#include "lsl_cpp.h"
#include <iostream>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "lib/Filter.h"
#include "lib/FIR_Filter.h"
#include "lib/IIR_Filter.h"
#include "lib/Biquad.h"
#include "lib/xdfwriter.h"
#include "Iir.h"

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
void write_header(XDFWriter* writer, const Config& cfg ) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?>"
        << "<info>"
        << "<name>SaveUtahData</name>"
        << "<type>EEG</type>"
        << "<channel_count>" << cfg.n_channel << "</channel_count>"
        << "<nominal_srate>" << cfg.sampling_rate << "</nominal_srate>"
        << "<channel_format>" << "double64" << "</channel_format>"
        << "<created_at>50942.723319709003</created_at>"
        << "</info>";

    // Convert to string
    std::string content = xml.str();
    writer->write_stream_header(0,content);
}
void write_footer(XDFWriter* writer,const Config& cfg ,double exact_ts, double time_stamp) {
    std::cout << "Finished Recording all Samples" << std::endl;
    std::cout << "Final Timestamp: " << exact_ts << std::endl;
    std::cout << "Final Sample Count: "<< time_stamp <<std::endl;

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\"?>"
        << "<info>"
        << "<first_timestamp>0.0</first_timestamp>"
        << "<last_timestamp>" << cfg.recording.duration << "</last_timestamp>"
        << "<sample_count>" << cfg.recording.duration * cfg.sampling_rate << "</sample_count>"
        << "<clock_offsets>"
        << "<offset><time>0</time><value>0</value></offset>"
        << "</clock_offsets>"
        << "</info>";

    std::string footer = xml.str();
    writer->write_boundary_chunk();
    writer->write_stream_footer(0, footer);
}


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

        while (true) {
            inlet.pull_sample(sample);
            for (int channel = 0; channel < cfg.n_channel; channel++) {
                filtered_values[channel] = filters[channel]->calculateOutput(sample[channel]);
                //filtered_values[channel] = biQfilters[channel]->process(sample[channel]);
            }

            // write sample to save file if necessary // TODO: save samples in vector and save them in bundles
            exact_ts = (static_cast<double>(time_stamp)/cfg.sampling_rate);

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
            }
            ++time_stamp;
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}