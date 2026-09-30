#include <rgsml/analysis/smoothing.hpp>

#include <algorithm>
#include <cmath>

namespace rgsml::analysis {

void smooth_frequency_triangular(
    std::span<const double> inputPower,
    std::span<double> outputPower)
{
    const std::size_t N = inputPower.size();
    if (N == 0) {
        return;
    }
    if (N == 1) {
        outputPower[0] = inputPower[0];
        return;
    }

    outputPower[0] = (0.50 * inputPower[0] + 0.25 * inputPower[1]) / 0.75;
    outputPower[N - 1] = (0.25 * inputPower[N - 2] + 0.50 * inputPower[N - 1]) / 0.75;

    for (std::size_t i = 1; i < N - 1; ++i) {
        outputPower[i] = 0.25 * inputPower[i - 1] + 0.50 * inputPower[i] + 0.25 * inputPower[i + 1];
    }
}

void TemporalSmoother::configure(double hopSize, double sampleRate, std::size_t pointCount)
{
    point_count_ = pointCount;
    const double dt = hopSize / sampleRate;
    alpha_attack_ = std::exp(-dt / 0.060);
    alpha_release_ = std::exp(-dt / 0.250);
}

void TemporalSmoother::reset()
{
}

void TemporalSmoother::process(
    std::span<const double> newPower,
    std::span<double> stateAndOutput,
    bool isFreshRestart)
{
    const std::size_t N = std::min(newPower.size(), stateAndOutput.size());

    if (isFreshRestart) {
        for (std::size_t i = 0; i < N; ++i) {
            stateAndOutput[i] = newPower[i];
        }
        return;
    }

    for (std::size_t i = 0; i < N; ++i) {
        const double pNew = newPower[i];
        const double pPrev = stateAndOutput[i];
        if (pNew >= pPrev) {
            stateAndOutput[i] = alpha_attack_ * pPrev + (1.0 - alpha_attack_) * pNew;
        } else {
            stateAndOutput[i] = alpha_release_ * pPrev + (1.0 - alpha_release_) * pNew;
        }
    }
}

void power_to_dbfs(
    std::span<const double> linearPower,
    std::span<double> outDbfs)
{
    const std::size_t N = std::min(linearPower.size(), outDbfs.size());
    for (std::size_t i = 0; i < N; ++i) {
        const double p = std::max(linearPower[i], 1e-16);
        outDbfs[i] = 10.0 * std::log10(p);
    }
}

}  // namespace rgsml::analysis
