#ifndef CONFIG_H
#define CONFIG_H

#include <string>

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

struct BufferConfig {
    int size;
    int window_size;
};

struct ModelConfig {
    std::string path;
    int input_size;
};

struct Config {
    int n_channel;
    int sampling_rate;
    bool do_plot;
    bool use_hw;
    std::string stream_name;
    std::string sim_data_path;
    bool use_layout;
    std::string mapping_path;
    FilterConfig filter;
    RecordConfig recording;
    BufferConfig buffer;
    ModelConfig model;
};

Config readConfig(const std::string& filename);

void printConfig(const Config& cfg);

#endif //CONFIG_H
