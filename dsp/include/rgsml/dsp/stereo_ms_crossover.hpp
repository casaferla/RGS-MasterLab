#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <cstdint>

namespace rgsml::dsp {

// M15-A4a: coefficient and pole/settling contract only; no effective DSP
// crossover is enabled by merely including this type.
struct StereoMsFilterSection final {
    double b0;
    double b1;
    double b2;
    double a1;
    double a2;
};

struct StereoMsCrossoverDesign final {
    MonoBassMode mode;
    StereoMsFilterSection low_section;
    StereoMsFilterSection high_section;
    std::uint32_t sections_per_branch;
    bool invert_high_branch;
    double maximum_pole_magnitude;
    std::int64_t settling_frames;
};

// Prewarped Phase-5 Butterworth coefficient law (recovered Work authority).
// Only LR12 and LR24 are meaningful here; OFF has NO coefficient design.
// Validation uses the frozen cutoff domain and the strict prepare predicate.
[[nodiscard]] rgsml::core::Result<StereoMsCrossoverDesign>
design_stereo_ms_crossover(
    MonoBassMode mode, double cutoff_hz, double sample_rate_hz);

}  // namespace rgsml::dsp
