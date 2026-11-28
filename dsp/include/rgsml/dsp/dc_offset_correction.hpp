#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <span>

namespace rgsml::dsp {

// Nonrelease M01 kernel. Caller must supply the complete source resource
// and an already authorized processing scope; a view cannot prove provenance.
// Offsets are explicit native-channel values, not detector output.
// Returns independently owned PCM; never modifies the source.
[[nodiscard]] core::Result<audio::AudioBuffer> correct_dc_offset(
    audio::AudioBufferView complete_source,
    core::FrameRange authorized_scope,
    std::span<const double> stored_offsets);

}  // namespace rgsml::dsp
