#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

namespace rgsml::dsp {

// Derived editor coordinates, never persistent DSP parameters.
// Normal editor range 0..200% is NOT a hard DSP/edit-legal range.
// True fixed-common-gain bounds may extend beyond 200%.
struct StereoMsWidthCoordinates final {
    double common_gain_db;
    double current_width_percent;
    double minimum_positive_width_percent;
    double maximum_positive_width_percent;
};

// Observe an already valid canonical parameter object. In particular,
// never reject or clamp a display because the binary64 width rounds
// outward from an analytically representable parameter boundary.
[[nodiscard]] StereoMsWidthCoordinates stereo_ms_width_coordinates(
    const StereoMsParameters& parameters) noexcept;

// This is the Width macro edit, not an additional DSP gain or stored field.
// Width 0 toggles exact Side mute while preserving all six stored values.
// Positive Width un-mutes Side, preserving the existing common gain and
// stored Mono Bass fields; unrepresentable requests fail without mutation.
// Explicit common-gain adjustments are a separate future confirmed action.
[[nodiscard]] rgsml::core::Result<StereoMsParameters>
stereo_ms_edit_width_preserving_common_gain(
    const StereoMsParameters& current,
    double requested_width_percent);

}  // namespace rgsml::dsp
