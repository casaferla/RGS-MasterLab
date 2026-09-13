#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace rgsml::platform::windows::internal {

using Sha256Digest = std::array<std::byte, 32>;

class Sha256 final {
public:
    Sha256() noexcept;

    void update(std::span<const std::byte> bytes) noexcept;
    [[nodiscard]] Sha256Digest finalize() noexcept;

private:
    void transform(const std::byte* block) noexcept;

    std::array<std::uint32_t, 8> state_{};
    std::array<std::byte, 64> buffer_{};
    std::uint64_t total_bytes_{0};
    std::size_t buffered_{0};
    bool finalized_{false};
};

[[nodiscard]] std::string sha256_hex(const Sha256Digest& digest);
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);

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
        Sha256 sha) noexcept;

    audio::AudioFormat format_;
    core::FrameCount frame_count_;
    std::int64_t frames_hashed_{0};
    Sha256 sha_;
    bool finalized_{false};
};

[[nodiscard]] core::Result<std::string> canonical_decoded_audio_sha256(
    audio::AudioBufferView source);

}  // namespace rgsml::platform::windows::internal
