#include "processing.h"
#include <yaml-cpp/yaml.h>
#include <torch/script.h>
#include "filter/FIR_Filter.h"
#include "filter/IIR_Filter.h"
#include "../lib/xdf_writer_template.h"

Processing::Processing(const std::string &config_path){
    loadConfig(config_path);
}


void Processing::run() {
    loadModel();
    generateFilters();
    generateRunningStdDev();
    auto inlet = setupLSLInlet();
    auto outlet = setupLSLOutlet();
    auto spike_outlet = setupLSLSpikeOutlet();
    processData(&inlet, &outlet, &spike_outlet);
}


void Processing::processData(lsl::stream_inlet *inlet, lsl::stream_outlet *outlet, lsl::stream_outlet *spike_outlet ) {
    std::vector<double> sample(cfg.n_channel,0);
    std::vector<double> filtered_values(cfg.n_channel, 0);
    std::vector<double> outputSample(2 * cfg.n_channel, 0);
    long sampleIdx = 0;
    long sim_seconds = 0;
    double exact_ts = 0.0;

    uint8_t spike_cut_out_len = cfg.buffer.waveform_size;  // input size of classifier model
    window.reserve(cfg.buffer.window_size);

    if(cfg.recording.do_record) {
        xdf_writer = load_xdf_writer();
        write_header(xdf_writer.get(), cfg);
    }

    // track the time for real time factore estimates
    auto start = std::chrono::high_resolution_clock::now();



    while(true) {
        inlet->pull_sample(sample);

        // filtering and spike detection
        for(int channel = 0; channel < sample.size(); channel++) {
            //filtered_values[channel] = filters[channel]->calculateOutput(sample[channel]);
            filtered_values[channel] = biQfilters[channel]->process(sample[channel]);
            runningStdDev_calcs[channel]->update(filtered_values[channel]);

            detect_spikes(filtered_values[channel], sampleIdx, channel);
        }
        window.emplace_back(sampleIdx, filtered_values);

        // handle spike events
        for(auto &spike_event : spike_events) {
            int pos_in_win = spike_event.timestamp % cfg.buffer.window_size;
            int frame_start = int(pos_in_win) - int(spike_cut_out_len)/2;
            int frame_end = int(pos_in_win) + int(spike_cut_out_len)/2;

            // extract waveform
            auto waveform = extract_waveform(&spike_event,frame_start, frame_end, pos_in_win);

            // do inference
            if(!waveform.empty()) {
                torch::Tensor input = torch::tensor(waveform);
                input = input.view({1,-1});
                auto output = model.forward({input});
                std::cout << "Model output: " << output << std::endl;
                waveform.clear();
            }

            spike_events.pop_front();
        }

        // adjust buffered data
        if(sampleIdx % cfg.buffer.window_size == 0 and sampleIdx > 0){
            if(window_buffer.size() >= cfg.buffer.size) window_buffer.pop_front();
            window_buffer.emplace_back(0,window);
            window.clear();
        }

        // prepare samples for output stream
        for(int i=0; i<cfg.n_channel; i++) {
            outputSample[2*i] = sample[i];
            outputSample[2*i+1] = filtered_values[i];
        }
        outlet->push_sample(outputSample);

        // log every second
        if (sampleIdx % cfg.sampling_rate == 0) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            start = end;

            std::cout << "P: Time passed: " << ++sim_seconds << "s (computed in: "<< duration.count() << "us)" <<std::endl;
            std::cout << "Std Dev: " << runningStdDev_calcs[0]->getStandardDeviation();
            std::cout << std::endl;
        }

        // handle recording of neural device
        if(cfg.recording.do_record){
            // seconds instead of sample count
            exact_ts = (static_cast<double>(sampleIdx)/cfg.sampling_rate);
            if (sampleIdx <= cfg.recording.duration * cfg.sampling_rate) {
                xdf_writer->write_data_chunk(0, {exact_ts}, sample, cfg.n_channel);
            }
            if (sampleIdx == cfg.recording.duration * cfg.sampling_rate) {
                write_footer(xdf_writer.get(), cfg,exact_ts, sampleIdx);
                cfg.recording.do_record = false;
            }
        }

        sampleIdx++;
    }
}


void Processing::loadConfig(const std::string &config_path) {
    try {
        cfg = readConfig(config_path);
    } catch (const YAML::Exception &e) {
        throw std::runtime_error("Error parsing YAML file: " + std::string(e.what()));
    }
}

void Processing::loadModel() {
    // model path should be part of the config
    std::string modelpath = "../model.pt";
    model = torch::jit::load(modelpath);
    std::cout << "Loaded Torch Model successfully" << std::endl;

}


void Processing::generateFilters() {
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
    biQfilters.reserve(cfg.n_channel);
    for(int i = 0; i< cfg.n_channel; i++) {
        biQfilters.emplace_back(new Biquad(2,((cfg.filter.highcut + cfg.filter.lowcut)/2)/cfg.sampling_rate, 0.707,0));
    }
}


void Processing::generateRunningStdDev() {
    runningStdDev_calcs.reserve(cfg.n_channel);
    for(int i = 0; i< cfg.n_channel; i++) {
        runningStdDev_calcs.emplace_back(new OnlineStdDev());
    }
}

void Processing::detect_spikes(double filtered_value, uint32_t sampleIdx, int channel) {
    if(filtered_value < -10 * runningStdDev_calcs[channel]->getStandardDeviation() and sampleIdx > 5*cfg.sampling_rate) {
        if(!spike_events.empty()) {
            auto prev_event = spike_events.back();
            if(sampleIdx > prev_event.timestamp+10 and channel != prev_event.channel) {
                std::cout << "Spike detected in channel: " << channel << std::endl;
                spike_events.push_back(SpikeEvent(channel,sampleIdx));
            }
        }else {
            // first event
            std::cout << "Spike detected in channel: " << channel << std::endl;
            spike_events.push_back(SpikeEvent(channel,sampleIdx));

        }
    };
}

std::vector<double> Processing::extract_waveform(SpikeEvent *spike_event,int frame_start, int frame_end, int pos_in_win) {
    // trivial case when the frame is completely within the window
    std::vector<double> spike_waveform;
    static int window_size = cfg.buffer.window_size;
    static int spike_cut_out_len = cfg.buffer.waveform_size;

    if(frame_start >= 0 and frame_end <= window_size) {
        for (int i=0; i < cfg.buffer.waveform_size; i++) {
            spike_waveform.emplace_back(window[pos_in_win+i-spike_cut_out_len/2].sample[spike_event->channel]);
        }
    }

    // frame is at the start of the current frame
    if(frame_start < 0) {
        auto prev_window = window_buffer.back();
        for (int i=abs(frame_start); i > 0; i--) {
            spike_waveform.emplace_back(prev_window.data[window_size-i].sample[spike_event->channel]);
        }
        for (int i=0; i < spike_cut_out_len+frame_start; i++) {
            spike_waveform.emplace_back(window[i].sample[spike_event->channel]);
        }
    }

    // frame is at the end of the current and start of the following frame:
    // TODO

    return spike_waveform;
}

lsl::stream_inlet Processing::setupLSLInlet() const {
    std::cout << "Looking for an LSL stream..." << std::endl;
    std::vector<lsl::stream_info> streams = lsl::resolve_stream("name", cfg.stream_name);
    lsl::stream_inlet inlet(streams[0]);
    std::cout << "Connected to stream: " << streams[0].name() << std::endl;
    std::cout << "Datatype: " << streams[0].channel_format() << std::endl;
    return inlet;
}

lsl::stream_outlet Processing::setupLSLOutlet() const{
    int n_out_channel = 2 * cfg.n_channel;
    lsl::stream_info info("BioSemiFiltered", "EEG", n_out_channel, cfg.sampling_rate, lsl::cf_int16, "myuid34234filtered");
    lsl::stream_outlet outlet(info);
    std::cout << "Created LSL Outlet for raw and filtered data!" << std::endl;
    return outlet;
}

lsl::stream_outlet Processing::setupLSLSpikeOutlet() const{
    lsl::stream_info spike_info("spikes", "EEG", cfg.buffer.waveform_size + 1, lsl::IRREGULAR_RATE, lsl::cf_int16, "3113208");
    lsl::stream_outlet spike_outlet(spike_info);
    std::cout << "Created LSL Outlet for detected spikes" << std::endl;
    return spike_outlet;
}

std::unique_ptr<XDFWriter> Processing::load_xdf_writer() const {
    std::string filename = cfg.recording.path + "/" + cfg.recording.file_name;
    auto writer = std::make_unique<XDFWriter>(filename);
    return writer;
}

