#include "Filter.h"
#include <iostream>
#include <utility>

// Constructor
Filter::Filter(const int order, const double sampling_rate, std::string  data_type,
            std::string  filter_type, const double low_cut_off, const double high_cut_off)
    : order(order), sampling_rate(sampling_rate), data_type(std::move(data_type)),
    filter_type(std::move(filter_type)), low_cut_off(low_cut_off), high_cut_off(high_cut_off) {}

// Getter and Setter methods
int Filter::getOrder() const {
    return order;
}

double Filter::getSamplingRate() const {
    return sampling_rate;
}

double Filter::getHighCutOff() const {
    return high_cut_off;
}

double Filter::getLowCutOff() const {
    return low_cut_off;
}

std::string Filter::getDataType() const {
    return data_type;
}

std::string Filter::getFilterType() const {
    return filter_type;
}

// Method implementations
void Filter::calculateCoefficients() {
    std::cout << "Calculating filter coefficients..." << std::endl;
}

double Filter::calculateOutput(double data_in) {
    std::cout << "Calculating filter output..." << std::endl;
    return 0;
}
