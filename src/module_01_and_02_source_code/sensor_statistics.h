#ifndef SENSOR_STATISTICS_H
#define SENSOR_STATISTICS_H

#include <vector>

namespace telemetry {

double mean(const std::vector<double>& samples);
double peak(const std::vector<double>& samples);

}  // namespace telemetry

#endif  // SENSOR_STATISTICS_H
