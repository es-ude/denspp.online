#include <deque>

#include "lsl_cpp.h"
#include <torch/torch.h>
#include <torch/script.h>
#include <cmath>
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

class OnlineStdDev {
private:
    double mean;       // Running mean
    double m2;         // Sum of squared differences from the mean
    int count;         // Number of samples processed

public:
    OnlineStdDev() : mean(0.0), m2(0.0), count(0) {}

    // Update the statistics with a new sample
    void update(double sample) {
        count++;
        double delta = sample - mean;
        mean += delta / count;
        double delta2 = sample - mean;
        m2 += delta * delta2;
    }

    // Get the current standard deviation
    [[nodiscard]] double getStandardDeviation() const {
        if (count < 2) return 0.0; // Not enough samples
        return std::sqrt(m2 / count); // Population standard deviation
    }
};

struct SpikeEvent {
    int channel;
    long timestamp;
};

struct SampleData {
    long timestamp;
    std::vector<double> sample;
};

struct Window {
    int idx;
    std::vector<SampleData> data;
};

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

    // generate Filters: IIR or FIR
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

    // generate filters: IIR Biquad
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

    // std dev calculator for each channel:
    std::vector<std::unique_ptr<OnlineStdDev>> stdCalcs;
    stdCalcs.reserve(cfg.n_channel);
    for(int i = 0; i< cfg.n_channel; i++) {
        stdCalcs.emplace_back(new OnlineStdDev());
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

        int sampleIdx = 0;
        int sim_seconds = 0;


        // initialise vector of filters (for each channel)
        std::vector<double> filtered_values(cfg.n_channel, 0);  // Store filtered output for each channel
        std::vector<double> outputSample(n_out_channel);

        double exact_ts = 0;

        // create buffer for previous seconds
        constexpr size_t max_buffer_size = 5; // how many windows to be stored
        constexpr size_t max_window_size = 1000; // how many samples in each window
        constexpr size_t spike_cut_out_len = 32;  // input size of classifier model

        std::vector<SampleData> window;
        std::deque<Window> window_buffer;
        // list of spike time stamps with channel
        std::deque<SpikeEvent> spike_events;
        std::vector<double> spike_waveform;
        spike_waveform.reserve(spike_cut_out_len);

        std::string modelpath = "../model.pt";
        torch::jit::script::Module model = torch::jit::load(modelpath);
        // begin of processing loop
        while (true) {
            inlet.pull_sample(sample);

            // for every channel in the lsl stream
            for (int channel = 0; channel < cfg.n_channel; channel++) {
                // update running stddev
                stdCalcs[channel]->update(sample[channel]);

                // calculate filter output
                //filtered_values[channel] = filters[channel]->calculateOutput(sample[channel]);
                filtered_values[channel] = biQfilters[channel]->process(sample[channel]);

                // check for spikes: // only begin detection after 5 seconds to let the stdDev accumulate
                if (fabs(filtered_values[channel]) > 3 * stdCalcs[channel]->getStandardDeviation() and sampleIdx > cfg.sampling_rate*5) {

                    // often the detection finds "several" spikes in the same channel close to each other
                    // but the waveform are actually from the same spike
                    // ignore the detected spike if the previous spike in the log are less than 10 indices away
                    if(!spike_events.empty()) {
                        // check if there's already an event
                        auto prev_event = spike_events.back();
                        if(sampleIdx > prev_event.timestamp+10 and channel == prev_event.channel) {
                            std::cout << "Spike detected in channel: " << channel << std::endl;
                            spike_events.push_back(SpikeEvent(channel,sampleIdx));
                        }
                    }else {
                        // first event
                        std::cout << "Spike detected in channel: " << channel << std::endl;
                        spike_events.push_back(SpikeEvent(channel,sampleIdx));
                    }
                }
            }
            window.emplace_back(sampleIdx, sample);


            // if max_window_size is reached:
            // +1 because if sampleIdx=0 the statement holds
            if ((sampleIdx+1) % max_window_size == 0){

                // iterate over all spike events
                for(auto& spike_event : spike_events) {
                    size_t pos_in_win = spike_event.timestamp % max_window_size;

                    // trivial case when the frame is completely within the window
                    if (spike_event.timestamp >= window[0].timestamp and spike_event.timestamp < sampleIdx -
                        spike_cut_out_len / 2) {
                        if((int(pos_in_win) - int(spike_cut_out_len)/2) >= 0 and (pos_in_win + spike_cut_out_len/2) <= max_window_size) {
                            std::cout << "Spike Waveform in Python Array Structure: \ndata = [";
                            for (int i=0; i < spike_cut_out_len; i++) {
                                if (i != 0) std::cout << ", ";
                                std::cout << window[pos_in_win+i-spike_cut_out_len/2].sample[spike_event.channel];
                                spike_waveform.emplace_back(window[pos_in_win+i-spike_cut_out_len/2].sample[spike_event.channel]);
                            }
                            std::cout << "] " << std::endl;
                        }
                        // Do model based inference:
                        if(!spike_waveform.empty()) {
                            torch::Tensor input = torch::tensor(spike_waveform);
                            input = input.view({1,-1});
                            auto output = model.forward({input});
                            std::cout << "Model output: " << output << std::endl;
                            spike_waveform.clear();
                        }
                    }
                    spike_events.pop_front();
                }

                // place full buffer at the end
                window_buffer.emplace_back(0,window);
                window.clear();

                // when buffer is full
                if(window_buffer.size() > max_buffer_size) window_buffer.pop_front();  // TODO: write front to disc when removed from buffer
            }




            // saving of data
            if(cfg.recording.do_record){
                // seconds instead of sample count
                exact_ts = (static_cast<double>(sampleIdx)/cfg.sampling_rate);
                if (sampleIdx <= cfg.recording.duration * cfg.sampling_rate) {
                    w.write_data_chunk(0, {exact_ts}, sample, cfg.n_channel);
                }
                if (sampleIdx == cfg.recording.duration * cfg.sampling_rate) {
                    write_footer(&w, cfg,exact_ts, sampleIdx);
                }
            }



            // prepare and push output sample:
            for(int i=0; i<cfg.n_channel; i++) {
                outputSample[2*i] = sample[i];
                outputSample[2*i+1] = filtered_values[i];
            }
            outlet.push_sample(outputSample);


            // logging
            if (sampleIdx % cfg.sampling_rate == 0) {
                std::cout << "P: Time passed: " << ++sim_seconds << "s" << std::endl;
                std::cout << "Std Dev: " << stdCalcs[34]->getStandardDeviation();
                std::cout << std::endl;
            }
            ++sampleIdx;
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}