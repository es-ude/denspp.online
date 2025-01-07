#ifndef PROCESSING_H
#define PROCESSING_H
#include <string>

#include <lsl_cpp.h>
#include "../lib/config.h"
#include <torch/torch.h>

#include "../lib/xdfwriter.h"
#include "filter/Filter.h"
#include "filter/Biquad.h"
#include "spikesorting/online_std_dev.h"

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

class Processing {
public:
    explicit Processing(const std::string &config_path);
    void run();
private:
    Config cfg;
    torch::jit::script::Module model;
    std::vector<std::unique_ptr<Filter>> filters;
    std::vector<std::unique_ptr<Biquad>> biQfilters;
    std::vector<std::unique_ptr<OnlineStdDev>> runningStdDev_calcs;
    std::vector<std::pair<int,std::vector<double>>> waveforms;

    std::unique_ptr<XDFWriter> xdf_writer;
    std::deque<SpikeEvent> spike_events;
    std::vector<SampleData> window;
    std::deque<Window> window_buffer;
    void loadConfig(const std::string &config_path);
    void loadModel();
    void generateFilters();
    void generateRunningStdDev();
    void processData(lsl::stream_inlet *inlet, lsl::stream_outlet *outlet, lsl::stream_outlet *spike_outlet);
    void detect_spikes(double filtered_value, uint32_t sampleIdx, int channel);
    std::vector<double> extract_waveform(SpikeEvent *spike_event, int frame_start, int frame_end, int pos_in_win);
    lsl::stream_inlet setupLSLInlet() const;
    lsl::stream_outlet setupLSLOutlet() const;
    lsl::stream_outlet setupLSLSpikeOutlet() const;
    std::unique_ptr<XDFWriter> load_xdf_writer() const;
};
#endif //PROCESSING_H
