#pragma once
#include <array>
#include <cstddef>

// Frozen, exactly representable rational ITU-R BS.1770-5 4x anchors.
// Source authority: protected analysis/src/internal/true_peak_itu4x_coefficients.hpp
// SHA-256 D1DF3373D3C5A82A3E4CFE3576BDDF97E153F3281FF9287554E024FA6CB0D316.
// This DSP-private copy avoids a DSP -> Analysis reverse dependency; an
// independent test must prove all 48 binary64 coefficients and every phase.
namespace rgsml::dsp::internal {
inline constexpr std::size_t kLimiterAnchorPhases = 4;
inline constexpr std::size_t kLimiterAnchorTaps = 12;
inline constexpr std::array<double, 48> kLimiterAnchorPhaseMajor{{
    14.0 / 8192.0,
    90.0 / 8192.0,
    -161.0 / 8192.0,
    272.0 / 8192.0,
    -487.0 / 8192.0,
    1125.0 / 8192.0,
    7964.0 / 8192.0,
    -838.0 / 8192.0,
    390.0 / 8192.0,
    -218.0 / 8192.0,
    122.0 / 8192.0,
    -68.0 / 8192.0, // phase 0
    -239.0 / 8192.0,
    240.0 / 8192.0,
    -424.0 / 8192.0,
    730.0 / 8192.0,
    -1364.0 / 8192.0,
    3810.0 / 8192.0,
    6388.0 / 8192.0,
    -1641.0 / 8192.0,
    832.0 / 8192.0,
    -477.0 / 8192.0,
    271.0 / 8192.0,
    -155.0 / 8192.0, // phase 1
    -155.0 / 8192.0,
    271.0 / 8192.0,
    -477.0 / 8192.0,
    832.0 / 8192.0,
    -1641.0 / 8192.0,
    6388.0 / 8192.0,
    3810.0 / 8192.0,
    -1364.0 / 8192.0,
    730.0 / 8192.0,
    -424.0 / 8192.0,
    240.0 / 8192.0,
    -239.0 / 8192.0, // phase 2
    -68.0 / 8192.0,
    122.0 / 8192.0,
    -218.0 / 8192.0,
    390.0 / 8192.0,
    -838.0 / 8192.0,
    7964.0 / 8192.0,
    1125.0 / 8192.0,
    -487.0 / 8192.0,
    272.0 / 8192.0,
    -161.0 / 8192.0,
    90.0 / 8192.0,
    14.0 / 8192.0, // phase 3
}};
inline constexpr char kLimiterAnchorAssetSha256[] =
    "D1DF3373D3C5A82A3E4CFE3576BDDF97E153F3281FF9287554E024FA6CB0D316";
} // namespace rgsml::dsp::internal
