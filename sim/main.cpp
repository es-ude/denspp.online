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

bool checkFileType(const std::string& filepath) {
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

matvar_t* handle_mat_file(mat_t* matfile) {
    if (!matfile) {
        throw std::runtime_error("Could not open .mat file");
    }

    matvar_t *raw_data = Mat_VarRead(matfile, "rawdata");
    if (!raw_data) {
        throw std::runtime_error("Could not open .mat file");
    }

    matvar_t *spikeVar = Mat_VarGetStructFieldByName(raw_data, "spike", 0);
    if (!spikeVar) {
        throw std::runtime_error("Could not open .mat file");
    }
    return spikeVar;
}


int main(int argc, char* argv[]) {
    std::string config_file_path = "../../config/default.yaml";

    //handle cmd line args:
    if (argc >= 2){
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

    std::string path = cfg.sim_data_path;  // File path to sim data
    path = "../../data/sim_data/utah_dataset_snippet.mat";  // USE this path if simulation is launched from sim directory
    //path = "../../data/save_data/test.xdf";

    // Check if provided Data File is in .mat or .xdf format:
    bool isXdf = checkFileType(path);

    // memory variables for mat/xdf files
    int16_t* data;
    std::vector<std::vector<std::variant<int,float,double,long, std::__cxx11::basic_string<char>>>> samples;
    size_t numRows;
    double sim_data_s_rate;
    if (!isXdf) {
        mat_t *matfp = Mat_Open(path.c_str(), MAT_ACC_RDONLY);
        matvar_t* spikeVar = handle_mat_file(matfp);

        size_t* dims = spikeVar->dims;
        sim_data_s_rate = 30000;  // currently only mat files are from Utah Array, 30khz
        numRows = dims[0];

        std::cout << "'spike' matrix dimensions: " << dims[0] << " x " << dims[1] << std::endl;
        data = static_cast<int16_t*>(spikeVar->data);
    }

    if(isXdf) {
        try {
            Xdf xdf;
            xdf.load_xdf(path);

            samples = xdf.streams[0].time_series;

            sim_data_s_rate = xdf.sampleRateMap.begin().operator*();
            numRows = samples[0].size();

        } catch (const std::exception &ex) {
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }
    }

    // Create LSL stream info
    lsl::stream_info info(cfg.stream_name, "EEG", cfg.n_channel, cfg.sampling_rate, lsl::cf_double64, "myuid34234");
    lsl::stream_outlet outlet(info);

    std::cout << "Now sending data..." << std::endl;

    std::vector sample(cfg.n_channel,0);

    int ts = 0;
    int step_size = sim_data_s_rate/cfg.sampling_rate;  // calculate step size, original data is recorded at 30khz
    double sleep_duration = 1.0/cfg.sampling_rate * 1000000.0 * 0.85;

    std::cout << "Step Size: " << step_size << "\nSleep Duration: " << sleep_duration << std::endl;

    long global_duration = 1000000;  // how long the last dataset second (s_rate samples) as a metric of how close we are to real time
    auto start = std::chrono::high_resolution_clock::now();

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
