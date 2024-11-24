// Example program to demonstrate how to send a multi-channel time series to LSL in C++
#include <lsl_cpp.h>  // LSL C++ API
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <matio.h>
#include <cstdint>
#include <yaml-cpp/yaml.h>

struct FilterConfig {
    std::string filter_class;
    int order;
    double lowcut;
    double highcut;
    std::string type;
};

struct Config {
    int n_channel;
    int sampling_rate;
    bool do_plot;
    bool use_hw;
    bool use_lsl;
    std::string stream_name;
    std::string sim_data_path;
    bool save_recording;
    std::string save_data_path;
    FilterConfig filter;
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
    cfg.save_recording = config["save_recording"].as<bool>();
    cfg.save_data_path = config["save_data_path"].as<std::string>();

    // Load filter settings
    YAML::Node filter = config["filter"];
    cfg.filter.filter_class = filter["class"].as<std::string>();
    cfg.filter.order = filter["order"].as<int>();
    cfg.filter.lowcut = filter["lowcut"].as<double>();
    cfg.filter.highcut = filter["highcut"].as<double>();
    cfg.filter.type = filter["type"].as<std::string>();

    return cfg;
}


int main(int argc, char* argv[]) {
    std::string config_file_path = "config/default.yaml";
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
    char path[] = "/media/k-dawg/Volume/PraxisProjektRohdaten/rps_20131016-115158-NSP1-001_MERGED.mat";  // File path to data


    // Open the .mat file
    mat_t *matfp = Mat_Open(path, MAT_ACC_RDONLY);
    if (!matfp) {
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
    size_t numRows = dims[0];
    size_t numCols = dims[1];

    std::cout << "'spike' matrix dimensions: " << numRows << " x " << numCols << std::endl;

    auto* data = static_cast<int16_t*>(spikeVar->data);

    // prints time step 2 of Electrode 11
    std::cout << data[2 + numRows * 11] << std::endl;

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

    while (true) {
        // Prepare sample (vector of ints)
        for(int j = 0; j < cfg.n_channel; j++) {
            sample[j] = data[numRows*j + ts];
        }
        // Push the sample to the outlet
        outlet.push_sample(sample);

        ts += step_size; // down sampling of utah array data according to new sampling rate
        if((ts/step_size) % cfg.sampling_rate == 0) {
            std::cout<< "S: Time passed: " << (ts/step_size)/(cfg.sampling_rate) << "s" << std::endl;

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            start = end;
            std::cout << "Sim second duration: "<< duration.count() << std::endl;

        }
        if (ts >= numRows) {
            std::cout << ts << std::endl;
            ts=0;
        }
        // Sleep for the duration of the sampling period (adjusted for 2 kHz = 1/2000 sec -> 5 milli
        usleep(sleep_duration);
    }

    return 0;
}
