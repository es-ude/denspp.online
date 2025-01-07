#include "online_std_dev.h"
#include "cmath"

OnlineStdDev::OnlineStdDev() : mean(0.0), m2(0.0), count(0) {}

void OnlineStdDev::update(double sample) {
    count++;
    double delta = sample - mean;
    mean += delta / count;
    double delta2 = sample - mean;
    m2 += delta * delta2;
}

double OnlineStdDev::getStandardDeviation() const {
    if (count < 2) return 0.0; // Not enough samples
    return std::sqrt(m2 / count); // Population standard deviation
}

