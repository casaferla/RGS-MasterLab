#pragma once

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <rgsml/render/render_result.hpp>

namespace rgsml::render {

// Isolated pure signature constructor. B2b does NOT activate M15 in
// render_preview: execution remains fail-closed pending B2c qualification.
// Semantic fields are conditional on the actual input layout, Side mute,
// Mono Bass OFF/active, and user bypass; all persisted fields stay untouched.
[[nodiscard]] rgsml::core::Result<ModuleExecutionSignature>
make_stereo_ms_execution_signature(
    const rgsml::dsp::ModuleDescriptor& descriptor,
    rgsml::dsp::ModuleInstanceId instance_id,
    const rgsml::dsp::StereoMsParameters& stored_parameters,
    rgsml::audio::ChannelLayout source_channel_layout,
    bool user_bypassed);

}  // namespace rgsml::render
