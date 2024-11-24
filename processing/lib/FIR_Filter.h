//
// Created by k-dawg on 16.11.24.
//

#ifndef FIR_FILTER_H
#define FIR_FILTER_H

#include <vector>
#include "Filter.h"

class FIR_Filter : public Filter {
private:
    std::vector<double> coefficients;
    std::vector<double> taps;
    void calculateCoefficients() override;

public:
    FIR_Filter(int order, double sampling_rate, std::string data_type, std::string filter_type, double low_cut_off, double high_cut_off);

    double calculateOutput(double data_in) override;

    std::vector<double> getCoefficients();
};



#endif //FIR_FILTER_H
