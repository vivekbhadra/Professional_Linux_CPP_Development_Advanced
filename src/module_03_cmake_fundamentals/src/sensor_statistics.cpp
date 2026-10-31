#include <telemetry/sensor_statistics.h>

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace telemetry {

double mean(const std::vector<double>& samples)
{
    if (samples.empty()) {
        throw std::invalid_argument("mean() requires at least one sample");
    }

    const double total = std::accumulate(samples.begin(), samples.end(), 0.0);
    return total / static_cast<double>(samples.size());
}

double peak(const std::vector<double>& samples)
{
    if (samples.empty()) {
        throw std::invalid_argument("peak() requires at least one sample");
    }

    return *std::max_element(samples.begin(), samples.end());
}

}  // namespace telemetry
