#ifndef IIR_FILTER_H
#define IIR_FILTER_H

#include "Filter.h"
#include <vector>

class IIR_Filter : public Filter {
public:
    IIR_Filter(int order, double sampling_rate, std::string data_type, std::string filter_type, double low_cut_off, double high_cut_off);

    // Calculates the output for the given input
    double calculateOutput(double data_in) override;

    // Returns the filter coefficients
    std::vector<double> getNumeratorCoefficients();
    std::vector<double> getDenominatorCoefficients();

private:
    std::vector<double> numerator;     // b coefficients
    std::vector<double> denominator;  // a coefficients
    std::vector<double> taps;          // Past inputs
    std::vector<double> out_taps;   // Past outputs
    int input_index;                 // Index for input ring buffer
    int output_index;
    void calculateCoefficients() override; // Calculate filter coefficients
};

#endif // IIR_FILTER_H
