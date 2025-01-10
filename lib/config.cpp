#include "config.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
Config readConfig(const std::string& filename) {
    YAML::Node config = YAML::LoadFile(filename);

    Config cfg;
    cfg.n_channel = config["n_channel"].as<int>();
    cfg.sampling_rate = config["sampling_rate"].as<int>();
    cfg.do_plot = config["do_plot"].as<bool>();
    cfg.use_hw = config["use_hw"].as<bool>();
    cfg.stream_name = config["stream_name"].as<std::string>();
    cfg.sim_data_path = config["sim_data_path"].as<std::string>();
    cfg.use_layout = config["use_layout"].as<bool>();
    cfg.mapping_path = config["mapping_path"].as<std::string>();

    // Load filter settings
    YAML::Node filter = config["filter"];
    cfg.filter.filter_class = filter["class"].as<std::string>();
    cfg.filter.order = filter["order"].as<int>();
    cfg.filter.lowcut = filter["lowcut"].as<double>();
    cfg.filter.highcut = filter["highcut"].as<double>();
    cfg.filter.type = filter["type"].as<std::string>();

    // Load recording settings
    YAML::Node recording = config["recording"];
    cfg.recording.do_record = recording["do_record"].as<bool>();
    cfg.recording.duration = recording["duration"].as<int>();
    cfg.recording.path = recording["path"].as<std::string>();
    cfg.recording.file_name = recording["filename"].as<std::string>();

    // Load buffer settings
    YAML::Node buffer = config["buffer"];
    cfg.buffer.size = buffer["size"].as<int>();
    cfg.buffer.window_size = buffer["window_size"].as<int>();

    YAML::Node model = config["model"];
    cfg.model.path = model["path"].as<std::string>();
    cfg.model.input_size = model["input_size"].as<int>();

    return cfg;
}

void printConfig(const Config& cfg) {
    std::cout << "n_channel: " << cfg.n_channel << std::endl;
    std::cout << "sampling_rate: " << cfg.sampling_rate << std::endl;
    std::cout << "do_plot: " << (cfg.do_plot ? "true" : "false") << std::endl;
    std::cout << "use_hw: " << (cfg.use_hw ? "true" : "false") << std::endl;
    std::cout << "stream_name: " << cfg.stream_name << std::endl;
    std::cout << "sim_data_path: " << cfg.sim_data_path << std::endl;
    std::cout << "use_layout: " << (cfg.use_layout ? "true" : "false") << std::endl;
    std::cout << "mapping_path: " << cfg.mapping_path << std::endl;

    std::cout << "Filter Settings:" << std::endl;
    std::cout << "  class: " << cfg.filter.filter_class << std::endl;
    std::cout << "  order: " << cfg.filter.order << std::endl;
    std::cout << "  lowcut: " << cfg.filter.lowcut << std::endl;
    std::cout << "  highcut: " << cfg.filter.highcut << std::endl;
    std::cout << "  type: " << cfg.filter.type << std::endl;

    std::cout << "Recording Settings:" << std::endl;
    std::cout << "  do_record: " << (cfg.recording.do_record ? "true" : "false") << std::endl;
    std::cout << "  duration: " << cfg.recording.duration << std::endl;
    std::cout << "  path: " << cfg.recording.path << std::endl;
    std::cout << "  filename: " << cfg.recording.file_name << std::endl;

    std::cout << "Buffer Settings:" << std::endl;
    std::cout << "  size: " << cfg.buffer.size << std::endl;
    std::cout << "  window_size: " << cfg.buffer.window_size << std::endl;

    std::cout << "Model Settings:" << std::endl;
    std::cout << "  path: " << cfg.model.path << std::endl;
    std::cout << "  input_size: " << cfg.model.input_size << std::endl;
}