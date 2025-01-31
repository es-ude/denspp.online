#ifndef SIMULATION_H
#define SIMULATION_H

#include <lsl_cpp.h>
#include <vector>
#include <thread>
#include <matio.h>
#include <xdf.h>
#include <cstdint>
#include "../lib/config.h"

class Simulation {
public:
    explicit Simulation(const std::string &config_path);

    void run();

private:
    Config cfg;
    std::string path;
    bool isXdf = false;

    int16_t *data = nullptr;
    double *quirogaData = nullptr;
    std::vector<std::vector<std::variant<int, float, double, long, std::string>>> samples;
    size_t numRows = 0, numCols = 0;
    double sim_data_s_rate = 0;
    uint16_t sim_channel_count = 0;

    void loadConfig(const std::string &config_path);
    void setupDataSource();
    void setupMatFile();
    void setupXdfFile();
    void setupQuiroga();
    [[nodiscard]] lsl::stream_outlet createLSLStream() const;
    void prepareSample(std::vector<double> &sample, int ts) const;
    [[noreturn]] void sendData() const;
    void manageSleep(int ts, int step_size, const double &sleep_duration) const;
    static void calculate_sleep_pd(int sleep_update_rate, double &sleep_duration, long global_duration) ;

};

#endif // SIMULATION_H