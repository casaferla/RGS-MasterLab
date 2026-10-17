#pragma once
// Independent test-only direct finite-resource convolution. It uses the
// protected M16 authority coefficients, NEVER the M17 copied coefficients or
// M17 detector's ring-buffer implementation. OUTPUT_RATE is supplied by test.
#include "internal/true_peak_itu4x_coefficients.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace rgsml::tests::m17_oracle {
struct Event final {
    std::int64_t tick;
    double peak;
};
// Frozen binary64 midpoint tap literals independently copied from PO delta.
inline constexpr std::array<double, 16> kReferenceMidpoint{{
    -2.239776377948551e-06, 0.00017165472521812235,
    -0.0015146051755935781, 0.0071952023595316566,
    -0.024200410441139873, 0.066061419666173099,
    -0.16826720648957741, 0.62055618513176591,
    0.62055618513176591, -0.16826720648957741,
    0.066061419666173099, -0.024200410441139873,
    0.0071952023595316566, -0.0015146051755935781,
    0.00017165472521812235, -2.239776377948551e-06
}};

// O(N) storage, O(16N) direct convolution, independent of streaming state.
[[nodiscard]] inline std::vector<Event> direct_events(
    std::span<const double> left, std::span<const double> right = {})
{
    const auto count = static_cast<std::int64_t>(left.size());
    const auto qcount = 4*count + 59;
    std::array<std::vector<double>,2> anchors{
        std::vector<double>(static_cast<std::size_t>(qcount),0.0),
        std::vector<double>(static_cast<std::size_t>(qcount),0.0)};
    const std::array<std::span<const double>,2> pcm{left,right};
    const auto nChannels = right.empty() ? 1U : 2U;
    for (std::int64_t j=0; j<qcount; ++j) {
        const std::int64_t frame=j/4;
        const std::size_t phase=static_cast<std::size_t>(j%4);
        for (std::size_t ch=0; ch<nChannels; ++ch) {
            double value=+0.0;
            for (std::int64_t lag=0;lag<12;++lag) {
                const auto inputFrame=frame-lag;
                const double x=(inputFrame>=0 && inputFrame<count)
                    ? pcm[ch][static_cast<std::size_t>(inputFrame)] : +0.0;
                const double product=
                    rgsml::analysis::internal::kTruePeakItu4xPhaseMajor[
                        12*phase+static_cast<std::size_t>(lag)] * x;
                value += product;
            }
            anchors[ch][static_cast<std::size_t>(j)]=value;
        }
    }
    std::vector<Event> emitted;
    emitted.reserve(static_cast<std::size_t>(qcount*2));
    for (std::int64_t j=0;j<qcount;++j) {
        if (j>=8) {
            const auto idx=static_cast<std::size_t>(j-8);
            double peak=std::abs(anchors[0][idx]);
            if(nChannels==2)peak=std::max(peak,std::abs(anchors[1][idx]));
            emitted.push_back(Event{2*j-63,peak});
        }
        double peak=+0.0;
        for(std::size_t ch=0;ch<nChannels;++ch) {
            double v=+0.0;
            for(std::int64_t lag=0;lag<16;++lag) {
                const double a=j>=lag
                    ? anchors[ch][static_cast<std::size_t>(j-lag)] : +0.0;
                v += kReferenceMidpoint[static_cast<std::size_t>(lag)]*a;
            }
            peak=std::max(peak,std::abs(v));
        }
        emitted.push_back(Event{2*j-62,peak});
    }
    return emitted;
}
} // namespace rgsml::tests::m17_oracle
