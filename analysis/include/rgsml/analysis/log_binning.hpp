#ifndef RGSML_ANALYSIS_LOG_BINNING_HPP
#define RGSML_ANALYSIS_LOG_BINNING_HPP

#include <cstddef>
#include <span>
#include <vector>

namespace rgsml::analysis {

struct LogGrid final {
    std::size_t point_count{512};
    double fmin{20.0};
    double fmax{20000.0};
    std::vector<double> node_centers;
    std::vector<double> cell_boundaries;

    static LogGrid create(double sampleRate, std::size_t pointCount = 512);
};

void reduce_to_log_grid(
    std::span<const double> binPowerA2,
    double sampleRate,
    const LogGrid& grid,
    std::span<double> outNodePowerA2);

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_LOG_BINNING_HPP
