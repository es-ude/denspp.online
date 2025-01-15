#include "IIR_Filter.h"
#include <iostream>
#include <utility>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

namespace py = pybind11;
using namespace py::literals;

class PythonInterpreter {  // Singleton Interpreter
public:
    PythonInterpreter() {
        static py::scoped_interpreter guard{};
    }
};

// Constructor
IIR_Filter::IIR_Filter(int order, double sampling_rate, std::string data_type, std::string filter_type, double low_cut_off, double high_cut_off)
    : Filter(order, sampling_rate, std::move(data_type), std::move(filter_type), low_cut_off, high_cut_off) {
    input_index = 0;
    output_index = 0;
    numerator.resize(getOrder() + 1);
    denominator.resize(getOrder() + 1);
    IIR_Filter::calculateCoefficients();
    taps.resize(numerator.size(), 0.0);
    out_taps.resize(denominator.size(), 0.0);
}

// Calculate IIR coefficients
void IIR_Filter::calculateCoefficients() {
    static PythonInterpreter interpreter;

    std::string filter_type = getFilterType();

    auto locals = py::dict(
        "order"_a = getOrder(),
        "low_cut_off"_a = getLowCutOff(),
        "high_cut_off"_a = getHighCutOff(),
        "sampling_rate"_a = getSamplingRate(),
        "filter_type"_a = filter_type
    );

    // Use Python to design the IIR filter
    if (filter_type == "bandpass") {
        py::exec(R"(
            import scipy.signal
            b, a = scipy.signal.butter(order, [low_cut_off, high_cut_off], fs=sampling_rate, btype="band")
        )", py::globals(), locals);
    } else {
        py::exec(R"(
            import scipy.signal
            b, a = scipy.signal.butter(order, low_cut_off if filter_type == "lowpass" else high_cut_off, fs=sampling_rate, btype=filter_type)
        )", py::globals(), locals);
    }

    numerator = locals["b"].cast<std::vector<double>>();
    denominator = locals["a"].cast<std::vector<double>>();
}

// Calculate output for the given input
double IIR_Filter::calculateOutput(double data_in) {
    double output = 0.0;

    // Write the current input to the ring buffer
    taps[input_index] = data_in;

    // Compute the feedforward part (numerator)
    for (size_t i = 0; i < numerator.size(); ++i) {
        int index = (input_index - i + taps.size()) % taps.size();
        output += numerator[i] * taps[index];
    }

    // Compute the feedback part (denominator)
    for (size_t i = 1; i < denominator.size(); ++i) {
        int index = (output_index - i + out_taps.size()) % out_taps.size();
        output -= denominator[i] * out_taps[index];
    }

    // Normalize the output
    if (denominator[0] == 0.0) {
        throw std::runtime_error("Denominator coefficient a[0] cannot be zero.");
    }
    output /= denominator[0];

    // Write the current output to the output ring buffer
    out_taps[output_index] = output;

    // Increment the ring buffer indices
    input_index = (input_index + 1) % taps.size();
    output_index = (output_index + 1) % out_taps.size();

    return output;
}

// Get numerator coefficients
std::vector<double> IIR_Filter::getNumeratorCoefficients() {
    return numerator;
}

// Get denominator coefficients
std::vector<double> IIR_Filter::getDenominatorCoefficients() {
    return denominator;
}
