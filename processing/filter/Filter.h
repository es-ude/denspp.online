#ifndef FILTER_H
#define FILTER_H

#include <string>

class Filter {
private:
    int order;
    double sampling_rate;
    std::string data_type;
    std::string filter_type;
    double low_cut_off;
    double high_cut_off;


public:
    Filter(int order, double sampling_rate, std::string  data_type, std::string filter_type, double low_cut_off, double high_cut_off);

    [[nodiscard]] int getOrder() const;

    [[nodiscard]] double getSamplingRate() const;

    [[nodiscard]] std::string getDataType() const;

    [[nodiscard]] std::string getFilterType() const;

    [[nodiscard]] double getHighCutOff() const;

    [[nodiscard]] double getLowCutOff() const;


    // Methods
    virtual void calculateCoefficients();
    virtual ~Filter() = default;
    virtual double calculateOutput(double data_in) = 0;
};



#endif //FILTER_H
