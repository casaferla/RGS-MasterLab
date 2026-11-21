#pragma once
// L3-M03 frozen P07, metadata/replay policy, NOT exact cropped-prefix
// equivalence and NOT an allocation size. Accounts for conditioning near
// silence and gain smoothing under worst-case finite binary64 amplitude.
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>

namespace rgsml::dsp::internal {
[[nodiscard]] core::Result<core::FrameCount>
transient_shaper_conservative_precontext_frames(
    std::uint32_t sample_rate_hz);
} // namespace rgsml::dsp::internal
