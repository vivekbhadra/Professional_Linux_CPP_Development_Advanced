#include "sensor_statistics.h"

#include <iostream>
#include <vector>

int main()
{
    const std::vector<double> readings{21.4, 22.9, 23.7, 22.1, 24.8};

    std::cout << "samples : " << readings.size() << '\n';
    std::cout << "mean    : " << telemetry::mean(readings) << '\n';
    std::cout << "peak    : " << telemetry::peak(readings) << '\n';

    return 0;
}
