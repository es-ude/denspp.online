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
    taps.resize(getOrder() + 1, 0.0);
    IIR_Filter::calculateCoefficients();
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

    /*std::cout << "IIR filter coefficients calculated." << std::endl;

    std::cout << "numerator: " << std::endl;
    for(auto num : numerator) {
        std::cout << num << " ";
    }
    std::cout << std::endl;

    std::cout << "demoninator: " << std::endl;
    for(auto den : denominator) {
        std::cout << den << " ";
    }
    std::cout << std::endl; */

}

// Calculate output for the given input
double IIR_Filter::calculateOutput(double data_in) {
    double output = 0.0;

    // hier vielleicht lieber ein Ringbuffer ums schneller zu machen
    for (int i = getOrder(); i > 0; --i) {
        taps[i] = taps[i - 1];
    }
    taps[0] = data_in;

    for (int i = 0; i <= getOrder(); ++i) {
        output += numerator[i] * taps[i];
    }
    for (int i = 1; i <= getOrder(); ++i) {
        output -= denominator[i] * taps[i];
    }

    output /= denominator[0];

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
