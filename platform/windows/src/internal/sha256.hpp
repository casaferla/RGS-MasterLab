#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/sha256.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace rgsml::platform::windows::internal {

// Existing private callers retain their spelling; implementation and ownership
// are now solely in rgsml_core. No duplicate SHA implementation is kept here.
using core::Sha256;
using core::Sha256Digest;
using core::sha256_hex;

// Streaming implementation of rgsml.canonical-decoded-audio/1.0.0.
class CanonicalDecodedAudioHasher final {
public:
    [[nodiscard]] static core::Result<CanonicalDecodedAudioHasher> create(
        audio::AudioFormat format,
        core::FrameCount frameCount);

    [[nodiscard]] core::Status update(audio::AudioBufferView source);
    [[nodiscard]] core::Result<std::string> finalize();

private:
    CanonicalDecodedAudioHasher(
        audio::AudioFormat format,
        core::FrameCount frameCount,
        core::Sha256 sha) noexcept;

    audio::AudioFormat format_;
    core::FrameCount frame_count_;
    std::int64_t frames_hashed_{0};
    core::Sha256 sha_;
    bool finalized_{false};
};

[[nodiscard]] core::Result<std::string> canonical_decoded_audio_sha256(
    audio::AudioBufferView source);

}  // namespace rgsml::platform::windows::internal
