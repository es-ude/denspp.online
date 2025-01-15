//
// Created by k-dawg on 16.11.24.
//

#include "FIR_Filter.h"
#include <iostream>
#include <ranges>
#include <utility>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

namespace py = pybind11;
using namespace py::literals;

class PythonInterpreter {  // Singleton Interpreter (otherwise conflict bc theres only one main python interpreter -> TODO: Make coefficients static
public:
    PythonInterpreter() {
        static py::scoped_interpreter guard{};
    }
};


//  constructor
FIR_Filter::FIR_Filter(int order, double sampling_rate, std::string data_type, std::string filter_type, double low_cut_off, double high_cut_off)
    : Filter(order, sampling_rate, std::move(data_type), std::move(filter_type), low_cut_off, high_cut_off) {

    std::cout << "FIR_Filter created." << std::endl;
    input_index = 0;
    coefficients.resize(getOrder());
    taps.resize(getOrder(), 0.0);
    FIR_Filter::calculateCoefficients();
}


void FIR_Filter::calculateCoefficients() {
    static PythonInterpreter interpreter;

    std::string filter_type = getFilterType();

    auto locals = py::dict(
        "num_taps"_a=getOrder(),
        "low_cut_off"_a=getLowCutOff(),
        "high_cut_off"_a=getHighCutOff(),
        "sampling_rate"_a=getSamplingRate(),
        "filter_type"_a=filter_type
    );


    if (filter_type == "bandpass") {
        py::exec(R"(
        import scipy.signal
        filter_par = scipy.signal.firwin(numtaps=num_taps, cutoff=[low_cut_off,high_cut_off], fs=sampling_rate,pass_zero=filter_type)
        )", py::globals(), locals);
    }
    else if (filter_type == "lowpass") {
        py::exec(R"(
        import scipy.signal
        filter_par = scipy.signal.firwin(numtaps=num_taps, cutoff=low_cut_off, fs=sampling_rate,pass_zero=filter_type)
        )", py::globals(), locals);
    }
    else if (filter_type == "highpass") {
        py::exec(R"(
        import scipy.signal
        filter_par = scipy.signal.firwin(numtaps=num_taps, cutoff=high_cut_off, fs=sampling_rate,pass_zero=filter_type)
        )", py::globals(), locals);
    }

    coefficients = locals["filter_par"].cast<std::vector<double>>();

    //std::cout << "FIR filter coefficients calculated." << std::endl;
}

double FIR_Filter::calculateOutput(double data_in) {
    double output = 0.0;

    // newest element
    taps[input_index] = data_in;

    // feed forward term:
    for (int i = 0; i < taps.size(); ++i) {
        int index = (input_index - i + taps.size()) % taps.size();
        output += coefficients[i] * taps[index];
    }
    input_index = (input_index + 1) % taps.size();
    return output;
}

std::vector<double> FIR_Filter::getCoefficients() {
    return coefficients;
}