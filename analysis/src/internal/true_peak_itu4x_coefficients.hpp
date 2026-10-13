#pragma once
#include <array>
#include <cstddef>

// Internal RGS M16 measurement asset, not a public DSP control/limiter.
// ITU-R BS.1770-5 Annex 2: 4 phases x 12 taps, phase-major binary64.
// Every rational numerator/8192 is exactly representable in binary64.
// Future analyzer must consume this bank unchanged, with zero boundary
// conditions and exactly 11 analyzer-only trailing zero frames.
namespace rgsml::analysis::internal {
inline constexpr std::size_t kTruePeakItu4xPhaseCount = 4;
inline constexpr std::size_t kTruePeakItu4xTapsPerPhase = 12;
inline constexpr std::array<double, 48> kTruePeakItu4xPhaseMajor{{
    // Phase 0, lags 0..11
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
    -68.0 / 8192.0,
    // Phase 1, lags 0..11
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
    -155.0 / 8192.0,
    // Phase 2, lags 0..11
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
    -239.0 / 8192.0,
    // Phase 3, lags 0..11
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
    14.0 / 8192.0
}};
inline constexpr char kTruePeakItu4xAssetSha256[] =
    "D1DF3373D3C5A82A3E4CFE3576BDDF97E153F3281FF9287554E024FA6CB0D316";
}  // namespace rgsml::analysis::internal
