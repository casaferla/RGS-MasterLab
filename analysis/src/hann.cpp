#include <rgsml/analysis/hann.hpp>

#include <cmath>
#include <numbers>

namespace rgsml::analysis {

HannWindow HannWindow::create(std::size_t windowSize)
{
    HannWindow h;
    h.window.resize(windowSize, 0.0);
    h.sum_w = 0.0;
    h.sum_w2 = 0.0;

    const double N = static_cast<double>(windowSize);
    for (std::size_t n = 0; n < windowSize; ++n) {
        const double w = 0.5 - 0.5 * std::cos((2.0 * std::numbers::pi * static_cast<double>(n)) / N);
        h.window[n] = w;
        h.sum_w += w;
        h.sum_w2 += w * w;
    }

    return h;
}

}  // namespace rgsml::analysis
