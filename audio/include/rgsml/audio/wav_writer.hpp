#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/wav_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/resource_io.hpp>
#include <rgsml/core/result.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace rgsml::audio {

struct WavWriteSpec final {
    AudioFormat audio_format;
    core::FrameCount frame_count;
    WavSampleFormat sample_format;

    friend bool operator==(const WavWriteSpec&, const WavWriteSpec&) = default;
};

// Deterministic same-rate IEEE_F64 RIFF/RF64 writer over the frozen core byte
// port. It owns the destination and publishes no filesystem policy.
class WavWriter final {
public:
    [[nodiscard]] static core::Result<std::unique_ptr<WavWriter>> open(
        std::unique_ptr<core::IResourceWriter> destination,
        WavWriteSpec spec);

    WavWriter(WavWriter&&) = delete;
    WavWriter& operator=(WavWriter&&) = delete;
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;
    ~WavWriter() noexcept;

    [[nodiscard]] const WavWriteSpec& spec() const noexcept;
    [[nodiscard]] WavContainerKind container_kind() const noexcept;
    [[nodiscard]] std::uint64_t expected_file_size_bytes() const noexcept;
    [[nodiscard]] core::FrameCount frames_written() const noexcept;

    [[nodiscard]] core::Status write_frames(AudioBufferView source);
    [[nodiscard]] core::Status finalize();
    [[nodiscard]] core::Status close();

private:
    struct Layout final {
        WavContainerKind container_kind;
        std::uint16_t channel_count;
        std::uint16_t block_align;
        std::uint32_t sample_rate;
        std::uint32_t byte_rate;
        std::uint64_t data_bytes;
        std::uint64_t file_size;
    };

    [[nodiscard]] static core::Result<Layout> make_layout(
        const WavWriteSpec& spec);
    [[nodiscard]] static std::vector<std::byte> make_header(
        const WavWriteSpec& spec,
        const Layout& layout);

    WavWriter(
        std::unique_ptr<core::IResourceWriter> destination,
        WavWriteSpec spec,
        Layout layout) noexcept;

    std::unique_ptr<core::IResourceWriter> destination_;
    WavWriteSpec spec_;
    Layout layout_;
    std::int64_t frames_written_{0};
    bool finalized_{false};
    bool closed_{false};
    bool failed_{false};
};

}  // namespace rgsml::audio
