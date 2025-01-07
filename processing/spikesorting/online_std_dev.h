#ifndef ONLINE_STD_DEV_H
#define ONLINE_STD_DEV_H

class OnlineStdDev {
private:
    double mean;       // Running mean
    double m2;         // Sum of squared differences from the mean
    int count;         // Number of samples processed

public:
    OnlineStdDev();

    // Update the statistics with a new sample
    void update(double sample);

    // Get the current standard deviation
    [[nodiscard]] double getStandardDeviation() const;
};


#endif //ONLINE_STD_DEV_H
