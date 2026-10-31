#pragma once
#include <array>
#include <cstddef>
#include <limits>

// M17 ACCEPTED_FROZEN 16-tap midpoint FIR; little-endian binary64, order 0..15.
// The asset is immutable and is not generated at runtime.
namespace rgsml::dsp::internal {
inline constexpr std::array<double, 16> kLimiterMidpointFir16{{
    -2.239776377948551e-06,
    0.00017165472521812235,
    -0.0015146051755935781,
    0.0071952023595316566,
    -0.024200410441139873,
    0.066061419666173099,
    -0.16826720648957741,
    0.62055618513176591,
    0.62055618513176591,
    -0.16826720648957741,
    0.066061419666173099,
    -0.024200410441139873,
    0.0071952023595316566,
    -0.0015146051755935781,
    0.00017165472521812235,
    -2.239776377948551e-06,
}};
inline constexpr char kLimiterMidpointAssetSha256[] =
    "C6E63340BE925C8A35136C78B1E6248B57813ED8C4A7B808A2E9F0F33F0DF4F7";
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "M17 requires IEEE-754 binary64.");
} // namespace rgsml::dsp::internal
