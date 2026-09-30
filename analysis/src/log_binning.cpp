#include <rgsml/analysis/log_binning.hpp>

#include <algorithm>
#include <cmath>

namespace rgsml::analysis {

LogGrid LogGrid::create(double sampleRate, std::size_t pointCount)
{
    LogGrid g;
    g.point_count = pointCount;
    g.fmin = 20.0;
    g.fmax = std::min(20000.0, 0.45 * sampleRate);

    g.node_centers.resize(pointCount);
    g.cell_boundaries.resize(pointCount + 1);

    const double M = static_cast<double>(pointCount);
    const double ratio = g.fmax / g.fmin;

    for (std::size_t i = 0; i < pointCount; ++i) {
        g.node_centers[i] = g.fmin * std::pow(ratio, static_cast<double>(i) / (M - 1.0));
    }

    for (std::size_t i = 1; i < pointCount; ++i) {
        g.cell_boundaries[i] = std::sqrt(g.node_centers[i - 1] * g.node_centers[i]);
    }
    g.cell_boundaries[0] = g.node_centers[0] * std::sqrt(g.node_centers[0] / g.node_centers[1]);
    g.cell_boundaries[pointCount] = g.node_centers[pointCount - 1] * std::sqrt(g.node_centers[pointCount - 1] / g.node_centers[pointCount - 2]);

    return g;
}

void reduce_to_log_grid(
    std::span<const double> binPowerA2,
    double sampleRate,
    const LogGrid& grid,
    std::span<double> outNodePowerA2)
{
    const std::size_t fftSize = (binPowerA2.size() - 1) * 2;
    const double binWidth = sampleRate / static_cast<double>(fftSize);

    for (std::size_t i = 0; i < grid.point_count; ++i) {
        const double cellMin = grid.cell_boundaries[i];
        const double cellMax = grid.cell_boundaries[i + 1];

        const std::size_t kStart = static_cast<std::size_t>(std::ceil(cellMin / binWidth));
        const std::size_t kEnd = static_cast<std::size_t>(std::floor(cellMax / binWidth));

        double maxPower = 0.0;
        bool foundBin = false;

        if (kStart <= kEnd && kEnd < binPowerA2.size()) {
            for (std::size_t k = kStart; k <= kEnd; ++k) {
                if (!foundBin || binPowerA2[k] > maxPower) {
                    maxPower = binPowerA2[k];
                    foundBin = true;
                }
            }
        }

        if (foundBin) {
            outNodePowerA2[i] = maxPower;
        } else {
            const double centerFreq = grid.node_centers[i];
            const double exactK = centerFreq / binWidth;
            const auto k0 = static_cast<std::size_t>(std::floor(exactK));
            const std::size_t k1 = std::min(k0 + 1, binPowerA2.size() - 1);

            if (k0 >= binPowerA2.size() - 1) {
                outNodePowerA2[i] = binPowerA2.back();
            } else if (k0 == k1) {
                outNodePowerA2[i] = binPowerA2[k0];
            } else {
                const double f0 = static_cast<double>(k0) * binWidth;
                const double f1 = static_cast<double>(k1) * binWidth;
                const double frac = (centerFreq - f0) / (f1 - f0);
                outNodePowerA2[i] = binPowerA2[k0] + frac * (binPowerA2[k1] - binPowerA2[k0]);
            }
        }
    }
}

}  // namespace rgsml::analysis
