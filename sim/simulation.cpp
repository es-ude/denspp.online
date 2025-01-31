#include "simulation.h"

#include "../lib/sim_file_io.h"
#include <yaml-cpp/yaml.h>
#include <chrono>
#include <iostream>

Simulation::Simulation(const std::string &config_path) {
    loadConfig(config_path);
}

void Simulation::run() {
    setupDataSource();
    sendData();
}

void Simulation::loadConfig(const std::string &config_path) {
    try {
        cfg = readConfig(config_path);
        path = cfg.sim_data_path;
    } catch (const YAML::Exception &e) {
        throw std::runtime_error("Error parsing YAML file: " + std::string(e.what()));
    }
    printConfig(cfg);
}

void Simulation::setupDataSource() {
    isXdf = checkFileType(path);

    if (!isXdf) {
        setupMatFile();
        //setupQuiroga();

    } else {
        setupXdfFile();
    }
}

void Simulation::setupQuiroga() {
    mat_t *matfp = Mat_Open(path.c_str(), MAT_ACC_RDONLY);
    matvar_t *spikeVar = Mat_VarRead(matfp, "data");
    quirogaData = static_cast<double *>(spikeVar->data);

    numRows = spikeVar->dims[1];
    numCols = spikeVar->dims[0];
    sim_data_s_rate = 24000;
    sim_channel_count = numCols;
    std::cout << "Scaling the Quiroga Datasets... " << numRows << " x " << numCols << std::endl;
    for (int i = 0; i < numRows; i++) {
        quirogaData[i] *= 300;
    }

}

void Simulation::setupMatFile() {
    mat_t *matfp = Mat_Open(path.c_str(), MAT_ACC_RDONLY);
    if (!matfp) {
        throw std::runtime_error("Failed to open .mat file: " + path);
    }

    matvar_t *spikeVar = handle_mat_file(matfp);
    size_t *dims = spikeVar->dims;

    sim_data_s_rate = 30000;  // Utah Array sampling rate
    numRows = dims[0];
    numCols = dims[1];
    sim_channel_count = numCols;
    data = static_cast<int16_t *>(spikeVar->data);

    std::cout << "'spike' matrix dimensions: " << numRows << " x " << numCols << std::endl;
}

void Simulation::setupXdfFile() {
    try {
        Xdf xdf;
        xdf.load_xdf(path);

        samples = xdf.streams[0].time_series;
        sim_data_s_rate = xdf.sampleRateMap.begin().operator*();
        numRows = samples[0].size();
        numCols = samples.size();
        sim_channel_count = numCols;
    } catch (const std::exception &ex) {
        throw std::runtime_error("Error loading XDF file: " + std::string(ex.what()));
    }
}

lsl::stream_outlet Simulation::createLSLStream() const {
    lsl::stream_info info(cfg.stream_name, "EEG", cfg.n_channel, cfg.sampling_rate, lsl::cf_double64, "myuid34234");
    lsl::stream_outlet outlet(info);
    return outlet;
}

void Simulation::prepareSample(std::vector<double> &sample, const int ts) const {
    if (!isXdf) {  // == is Matfile
        for (int j = 0; j < cfg.n_channel; j++) {
            if(data!= nullptr) {
                const size_t i = j % sim_channel_count;
                sample[j] = data[numRows * i + ts];
            }
            if(quirogaData != nullptr) {
                const size_t offset = j * 800; // two seconds offset
                sample[j] = quirogaData[(ts + offset)%numRows];
            }
        }
    } else {
        for (int j = 0; j < cfg.n_channel; j++) {
            int i = j % sim_channel_count;
            sample[j] = static_cast<int>(std::get<double>(samples[i][ts]));
        }
    }
}

[[noreturn]] void Simulation::sendData() const {
    lsl::stream_outlet outlet = this->createLSLStream();

    std::vector<double> sample(cfg.n_channel, 0);
    int ts = 0;
    const int step_size = (sim_data_s_rate > cfg.sampling_rate) ? sim_data_s_rate / cfg.sampling_rate : 1;
    double sleep_duration = 1.0 / cfg.sampling_rate * 1000000.0 * 0.85;  // initial sleeping interval, does not matter too much

    constexpr int sleep_update_rate = 200; // PD controller update rate

    auto start_seconds = std::chrono::high_resolution_clock::now();  // measure for seconds
    auto start_PD_update = std::chrono::high_resolution_clock::now();  // measure for update intervals of PD controller

    while (true) {
        prepareSample(sample, ts);
        outlet.push_sample(sample);

        ts += step_size;

        if ((ts / step_size) % cfg.sampling_rate == 0) {
            // every second, print time and dataset + real time taken

            auto end = std::chrono::high_resolution_clock::now();
            auto real_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start_seconds);
            start_seconds = end;

            std::cout << "S: Time on dataset passed: " << (ts / step_size) / (cfg.sampling_rate)
                      << "s (computed in: " << real_time.count() << "us)" << std::endl;
        }


        if ((ts / step_size) % (cfg.sampling_rate/sleep_update_rate) == 0) {
            // every 1/sleep_update_rate update the PD parameters

            auto end_PD_update = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_PD_update - start_PD_update);
            start_PD_update = end_PD_update;

            calculate_sleep_pd(sleep_update_rate, sleep_duration, duration.count());
        }

        manageSleep(ts, step_size, sleep_duration);  // idle for the given amount of time

        if (ts >= numRows) {
            std::cout << ts << std::endl;
            ts = 0;  // loop back
        }
    }
}

void Simulation::manageSleep(const int ts, const int step_size, const double &sleep_duration) const{
    if (cfg.sampling_rate <= 10000) {
        // idles for every sample
        usleep(sleep_duration);
    }
    else{
        // idles after multiple samples
        if ((ts / step_size) % (cfg.sampling_rate / 1000) == 0)  usleep(sleep_duration);
    }
}


void Simulation::calculate_sleep_pd(const int sleep_update_rate, double &sleep_duration, const long global_duration) {
    static double prev_error = 0.0;

    // Expected duration per update call (1 / sleep_update_rate in microseconds)
    double expected_duration = 1000000.0 / sleep_update_rate;
    double error = expected_duration - global_duration;

    // PD Controller Gains
    const double Kp = 0.02;  // Proportional Gain
    const double Kd = 0.005; // Derivative Gain

    // Compute PD control term
    double dt = 1000000.0 / sleep_update_rate;  // Time step in microseconds
    double derivative = (error - prev_error) / dt;
    double adjustment = Kp * error + Kd * derivative;

    // Update sleep duration
    sleep_duration += adjustment;
    if (sleep_duration < 0) sleep_duration = 0;

    prev_error = error;
}
