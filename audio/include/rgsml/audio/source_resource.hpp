#pragma once

#include <rgsml/audio/wav_format.hpp>
#include <rgsml/core/resource_io.hpp>
#include <rgsml/core/result.hpp>

#include <memory>

namespace rgsml::audio {

// Runtime-only aggregate for one read-only Source whose WAV container metadata
// has been probed. It intentionally owns neither an open reader nor decoded PCM.
class SourceResource final {
public:
    [[nodiscard]] static core::Result<SourceResource> probe(
        std::unique_ptr<core::IResourceReader> reader);

    [[nodiscard]] const core::ResourceReference& reference() const noexcept;
    [[nodiscard]] const WavStreamInfo& wav_info() const noexcept;

private:
    SourceResource(
        core::ResourceReference reference,
        WavStreamInfo wavInfo) noexcept;

    core::ResourceReference reference_;
    WavStreamInfo wavInfo_;
};

}  // namespace rgsml::audio
