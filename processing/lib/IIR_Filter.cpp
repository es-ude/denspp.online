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

    std::cout << "order: " << getOrder() << ", "
              << "low_cut_off: " << getLowCutOff() << ", "
              << "high_cut_off: " << getHighCutOff() << ", "
              << "sampling_rate: " << getSamplingRate() << std::endl;

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
    static int in_tap_len = numerator.size();
    static int out_tap_len = denominator.size();

    // previous inputs:
    for (int i = in_tap_len-1; i > 0; --i) {
        taps[i] = taps[i - 1];
    }
    taps[0] = data_in;
    for (int i = out_tap_len-1; i > 0; --i) {
        out_taps[i] = out_taps[i - 1];
    }

    for (int i = 0; i <= in_tap_len; ++i) {
        output += numerator[i] * taps[i];
    }
    for (int i = 1; i <= out_tap_len; ++i) {
        output -= denominator[i] * out_taps[i];
    }

    output /= denominator[0];
    out_taps[0] = output;
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
