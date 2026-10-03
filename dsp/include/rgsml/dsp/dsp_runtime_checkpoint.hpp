#pragma once

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rgsml::dsp {

struct DspRuntimeCheckpoint final {
    std::string module_type_id;
    std::string algorithm_version;
    std::string parameter_schema_id;
    std::string sonic_fingerprint;
    std::optional<rgsml::audio::AudioFormat> audio_format;
    rgsml::audio::FrameDomainId frame_domain_id{rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    std::string checkpoint_schema_version;
    std::optional<rgsml::core::FrameIndex> next_input_frame;
    std::string backend_identity;
    std::vector<std::uint8_t> payload;

    friend bool operator==(
        const DspRuntimeCheckpoint&,
        const DspRuntimeCheckpoint&) = default;
};

}  // namespace rgsml::dsp
