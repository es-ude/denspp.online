#include <deque>

#include "lsl_cpp.h"
#include <iostream>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "lib/Filter.h"
#include "lib/FIR_Filter.h"
#include "lib/IIR_Filter.h"
#include "lib/Biquad.h"
#include "../lib/xdfwriter.h"
#include "../lib/config.h"
#include "../lib/xdf_writer_template.h"


int main(int argc,char* argv[]) {
    std::string config_file_path = "../../config/default.yaml";
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
    //cfg.recording.path = "../../data/save_data";

    // create the outlet for the filtered DataStream:
    std::string name = "BioSemiFiltered";  // Stream name for filtered data
    std::string type = "EEG";  // Stream type
    int n_out_channel = 2 * cfg.n_channel;  //output count raw and filtered data
    // Create LSL stream info
    lsl::stream_info info(name, type, n_out_channel, cfg.sampling_rate, lsl::cf_int16, "myuid34234filtered");

    // Create a new outlet to send data
    lsl::stream_outlet outlet(info);

    // generate Filters:
    std::vector<std::unique_ptr<Filter>> filters;  // OWN FIR FILTERS
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

    // generate filters:
    std::vector<std::unique_ptr<Biquad>> biQfilters;
    biQfilters.reserve(cfg.n_channel);
    for(int i = 0; i< cfg.n_channel; i++) {
        biQfilters.emplace_back(new Biquad(2,((cfg.filter.highcut - cfg.filter.lowcut)/2)/cfg.sampling_rate, 0.707,0));
    }

    // setup the xdf file writer:
    std::string filename = cfg.recording.path + "/" + cfg.recording.file_name;
    //filename = "../../data/save_data/test3.xdf";
    std::cout << filename << std::endl;
    XDFWriter w(filename);

    if(cfg.recording.do_record){
        write_header(&w, cfg);
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

        double exact_ts = 0;

        // create buffer for previous seconds
        const size_t window_size = 5; // keep 5 seconds stored
        std::vector<std::pair<double,std::vector<double>>> samples;
        std::deque<std::vector<std::pair<double, std::vector<double>>>> buffer;


        while (true) {
            inlet.pull_sample(sample);
            for (int channel = 0; channel < cfg.n_channel; channel++) {
                filtered_values[channel] = filters[channel]->calculateOutput(sample[channel]);
                //filtered_values[channel] = biQfilters[channel]->process(sample[channel]);
            }

            // write sample to save file if necessary // TODO: save samples in vector and save them in bundles
            exact_ts = (static_cast<double>(time_stamp)/cfg.sampling_rate);
            samples.emplace_back(exact_ts, sample);

            if(cfg.recording.do_record){
                if (time_stamp <= cfg.recording.duration * cfg.sampling_rate) {
                    w.write_data_chunk(0, {exact_ts}, sample, 96);
                }
                if (time_stamp == cfg.recording.duration * cfg.sampling_rate) {
                    write_footer(&w, cfg,exact_ts, time_stamp);
                }
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

                buffer.emplace_back(samples);
                samples.clear();

                if(buffer.size() > window_size) buffer.pop_front();

            }
            ++time_stamp;
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}