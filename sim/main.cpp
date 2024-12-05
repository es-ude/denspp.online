#include <lsl_cpp.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <matio.h>
#include <xdf.h>

#include <cstdint>
#include <fstream>
#include <queue>
#include <yaml-cpp/yaml.h>

struct FilterConfig {
    std::string filter_class;
    int order;
    double lowcut;
    double highcut;
    std::string type;
};

struct RecordConfig {
    bool do_record;
    int duration;
    std::string path;
    std::string file_name;
};

struct Config {
    int n_channel;
    int sampling_rate;
    bool do_plot;
    bool use_hw;
    bool use_lsl;
    std::string stream_name;
    std::string sim_data_path;
    FilterConfig filter;
    RecordConfig recording;
};

Config readConfig(const std::string& filename) {
    YAML::Node config = YAML::LoadFile(filename);

    Config cfg;
    cfg.n_channel = config["n_channel"].as<int>();
    cfg.sampling_rate = config["sampling_rate"].as<int>();
    cfg.do_plot = config["do_plot"].as<bool>();
    cfg.use_hw = config["use_hw"].as<bool>();
    cfg.use_lsl = config["use_lsl"].as<bool>();
    cfg.stream_name = config["stream_name"].as<std::string>();
    cfg.sim_data_path = config["sim_data_path"].as<std::string>();

    // Load filter settings
    YAML::Node filter = config["filter"];
    cfg.filter.filter_class = filter["class"].as<std::string>();
    cfg.filter.order = filter["order"].as<int>();
    cfg.filter.lowcut = filter["lowcut"].as<double>();
    cfg.filter.highcut = filter["highcut"].as<double>();
    cfg.filter.type = filter["type"].as<std::string>();

    // Load recording settings;
    YAML::Node recording = config["recording"];
    cfg.recording.do_record = recording["do_record"].as<bool>();
    cfg.recording.duration = recording["duration"].as<int>();
    cfg.recording.path = recording["path"].as<std::string>();
    cfg.recording.file_name = recording["filename"].as<std::string>();

    return cfg;
}

bool checkFileType(const std::string &filepath) {
    // true: file ends on .xdf, false: file ends on .mat

    size_t dotPos = filepath.rfind('.');
    if (dotPos == std::string::npos) {
        throw std::runtime_error("No dot found :c"); //
    }
    std::string extension = filepath.substr(dotPos + 1);
    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == "mat") {
        return false;
    } else if (extension == "xdf") {
        return true;
    } else {
        throw std::runtime_error("Unsupported file extension");
    }
}


int main(int argc, char* argv[]) {
    std::string config_file_path = "../../config/layout_test.yaml";

    //handle cmd line args:
    if (argc >= 2) {
        config_file_path = std::string(argv[1]);
        std::cout << "Using Config / CLI Arguments! " << config_file_path << std::endl;
    } else {
        std::cout << "Using default Config: " << config_file_path << std::endl;
    }

    Config cfg;
    try {
        cfg = readConfig(config_file_path);
    } catch (const YAML::Exception& e) {
        std::cerr << "Error parsing YAML file: " << e.what() << std::endl;
        return 1;
    }

    std::string name = cfg.stream_name;  // Stream name
    std::string type = "EEG";  // Stream type
    std::string path = cfg.sim_data_path;  // File path to sim data
    //path = "../../data/sim_data/utah_dataset_snippet.mat";  // USE this path if simulation is launched from sim directory
    //path = "../../data/save_data/test.xdf";

    // Check if provided Data File is in .mat or .xdf format:
    bool isXdf = checkFileType(path);
    std::cout << isXdf << std::endl;
    int16_t* data;
    size_t numRows;

    if (!isXdf) {
        // MATFILE ROUTINE
        mat_t *matfp = Mat_Open(path.c_str(), MAT_ACC_RDONLY);
        if (!matfp) {
            std::cout << path.c_str() << std::endl;
            std::cerr << "Error: Could not open .mat file." << std::endl;
            return 1;
        }

        // Read the 'rawdata' structure from the .mat file
        matvar_t *raw_data = Mat_VarRead(matfp, "rawdata");
        if (!raw_data) {
            std::cerr << "Error: Could not read 'rawdata' from .mat file." << std::endl;
            Mat_Close(matfp);
            return 1;
        }

        matvar_t* spikeVar = Mat_VarGetStructFieldByName(raw_data, "spike", 0);

        if (!spikeVar) {
            std::cerr << "Error: Could not find 'spike' matrix in the file." << std::endl;
            return 1;
        }

        size_t* dims = spikeVar->dims;
        numRows = dims[0];
        size_t numCols = dims[1];

        std::cout << "'spike' matrix dimensions: " << numRows << " x " << numCols << std::endl;
        data = static_cast<int16_t*>(spikeVar->data);
    }

    std::vector<std::vector<std::variant<int,float,double,long, std::__cxx11::basic_string<char>>>> samples;
    if(isXdf) {
        // XDFFILE routine
        try {
            // Load the XDF file
            Xdf xdf;
            xdf.load_xdf(path);

            samples = xdf.streams[0].time_series;
            //auto timestamp = xdf.streams[0].time_stamps;   // Timestamps for all Channels


            /*for(int ts=0; ts < 10; ts++){
                for (int channel=0; channel<96; channel++) {
                    trouble[channel] = (std::get<double>(samples[channel][ts]));
                }
                std::cout << "ts: " << ts << std::endl;
                std::cout << "values: ";
                for(auto &value: trouble) std::cout << value << " ";
                std::cout << std::endl;
            }*/
            numRows = samples[0].size();


        } catch (const std::exception &ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }
    }


    // prints time step 2 of Electrode 11
    //std::cout << data[2 + numRows * 11] << std::endl;

    // Create LSL stream info
    lsl::stream_info info(cfg.stream_name, type, cfg.n_channel, cfg.sampling_rate, lsl::cf_double64, "myuid34234");

    // Create a new outlet to send data
    lsl::stream_outlet outlet(info);

    std::cout << "Now sending data..." << std::endl;

    std::vector sample(cfg.n_channel,0);
    int ts = 0;
    // calculate step size, original data is recorded at 30khz
    int step_size = 30000/cfg.sampling_rate;
    double sleep_duration = 1.0/cfg.sampling_rate * 1000000.0 * 0.85;

    std::cout << "Step Size: " << step_size << "\nSleep Duration: " << sleep_duration << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    long global_duration = 1000000;  // TRASH VARIABLE FOR PROOF OF CONCEPT

    while (true) {

        // Prepare sample (vector of ints)

        if(!isXdf){
            for(int j = 0; j < cfg.n_channel; j++) {
                sample[j] = data[numRows*j + ts];
            }
        }
        if(isXdf) {
            for(int j = 0; j < cfg.n_channel; j++) {
                sample[j] = int(std::get<double>(samples[j][ts]));
            }
        }


        outlet.push_sample(sample);

        ts += step_size; // down sampling of utah array data according to new sampling rate
        if((ts/step_size) % cfg.sampling_rate == 0) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            start = end;

            std::cout<< "S: Time on dataset passed: " << (ts/step_size)/(cfg.sampling_rate) << "s (computed in: " << duration.count() << "us)" << std::endl;
            global_duration = duration.count();
        }

        // idle / sleep logic:
        if (cfg.sampling_rate <= 10000) {  // sleeps after every sample (only feasible for lower sampling rates
            // recalculate sleeping duration every second:
            if((ts/step_size) % cfg.sampling_rate == 0){
                sleep_duration += 0.0005 * ((1000000 - global_duration));  // PID Controller without I and D :^)
            }
            usleep(sleep_duration);
        }
        else{
            if((ts/step_size) % (cfg.sampling_rate/1000) == 0) { // sleeps every 30 samples (for 30khz, 20 samples for 20khz...)
                // recalculate sleeping duration every second:
                if((ts/step_size) % cfg.sampling_rate == 0){
                    sleep_duration += 0.0005 * ((1000000 - global_duration));
                }
                usleep(sleep_duration);
            }
        }

        // loop the dataset
        if (ts >= numRows) {
            std::cout << ts << std::endl;
            ts=0;
        }
    }

    return 0;
}
