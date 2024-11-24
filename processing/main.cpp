#include "lsl_cpp.h"
#include <iostream>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "lib/Filter.h"
#include "lib/FIR_Filter.h"
#include "lib/IIR_Filter.h"

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



int main(int argc,char* argv[]) {
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

    // create the outlet for the filtered DataStream:
    std::string name = "BioSemiFiltered";  // Stream name for filtered data
    std::string type = "EEG";  // Stream type
    int n_out_channel = 2 * cfg.n_channel;  //output count raw and filtered data
    // Create LSL stream info
    lsl::stream_info info(name, type, n_out_channel, cfg.sampling_rate, lsl::cf_int16, "myuid34234filtered");

    // Create a new outlet to send data
    lsl::stream_outlet outlet(info);

    // generate Filters:
    std::vector<std::unique_ptr<Filter>> filters;

    filters.reserve(cfg.n_channel);
    for (int i=0;i<cfg.n_channel;i++) {
        if (cfg.filter.filter_class == "iir") {
            filters.emplace_back(std::make_unique<IIR_Filter>(cfg.filter.order, cfg.sampling_rate,
                                                                "double", cfg.filter.type,
                                                                cfg.filter.lowcut, cfg.filter.highcut));
        }else {
            filters.emplace_back(std::make_unique<FIR_Filter>(cfg.filter.order, cfg.sampling_rate,
                                                                "double", cfg.filter.type,
                                                                cfg.filter.lowcut, cfg.filter.highcut));
        }
    }



    try {

        // create inlet and connect to lsl stream by name
        std::cout << "Looking for an LSL stream..." << std::endl;
        std::vector<lsl::stream_info> streams = lsl::resolve_stream("name", cfg.stream_name);
        lsl::stream_inlet inlet(streams[0]);
        std::cout << "Connected to stream: " << streams[0].name() << std::endl;
        std::cout << "Datatype: " << streams[0].channel_format() << std::endl;
        std::vector<double> sample;
        sample.resize(streams[0].channel_count());

        int time_stamp = 0;
        int sim_seconds = 0;


        // initialise vector of filters (for each channel)
        std::vector<int16_t> filtered_values(cfg.n_channel, 0);  // Store filtered output for each channel
        std::vector<int16_t> outputSample(n_out_channel);


        while (true) {
            inlet.pull_sample(sample);
            for (int channel = 0; channel < cfg.n_channel; channel++) {
                filtered_values[channel] = filters[channel]->calculateOutput(sample[channel]);
            }

            //prepare output sample:
            for(int i=0; i<cfg.n_channel; i++) {
                outputSample[2*i] = sample[i];
                outputSample[2*i+1] = filtered_values[i];
            }
            // push filtered samples to LSL outlet
            outlet.push_sample(outputSample);

            if (time_stamp % cfg.sampling_rate == 0) {
                std::cout << "P: Time passed: " << ++sim_seconds << "s" << std::endl;
                std::cout << std::endl;
            }
            ++time_stamp;
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}