#ifndef RGSML_ANALYSIS_SMOOTHING_HPP
#define RGSML_ANALYSIS_SMOOTHING_HPP

#include <cstddef>
#include <span>

namespace rgsml::analysis {

void smooth_frequency_triangular(
    std::span<const double> inputPower,
    std::span<double> outputPower);

class TemporalSmoother final {
public:
    TemporalSmoother() = default;

    void configure(double hopSize, double sampleRate, std::size_t pointCount);
    void reset();

    void process(
        std::span<const double> newPower,
        std::span<double> stateAndOutput,
        bool isFreshRestart);

private:
    double alpha_attack_{0.0};
    double alpha_release_{0.0};
    std::size_t point_count_{0};
};

void power_to_dbfs(
    std::span<const double> linearPower,
    std::span<double> outDbfs);

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_SMOOTHING_HPP
