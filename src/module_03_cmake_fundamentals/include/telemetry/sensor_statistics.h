#ifndef TELEMETRY_SENSOR_STATISTICS_H
#define TELEMETRY_SENSOR_STATISTICS_H

#include <vector>

namespace telemetry {

double mean(const std::vector<double>& samples);
double peak(const std::vector<double>& samples);

}  // namespace telemetry

#endif  // TELEMETRY_SENSOR_STATISTICS_H
