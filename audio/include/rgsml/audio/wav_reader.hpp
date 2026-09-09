#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/wav_format.hpp>
#include <rgsml/core/resource_io.hpp>
#include <rgsml/core/result.hpp>

#include <memory>

namespace rgsml::audio {

// Seekable, read-only WAV v1 decoder. It owns one Task 004 resource reader and
// never exposes or logs the resource locator.
class WavReader final {
public:
    [[nodiscard]] static core::Result<std::unique_ptr<WavReader>> open(
        std::unique_ptr<core::IResourceReader> source);

    WavReader(WavReader&&) noexcept = default;
    WavReader& operator=(WavReader&&) noexcept = default;
    WavReader(const WavReader&) = delete;
    WavReader& operator=(const WavReader&) = delete;
    ~WavReader() noexcept = default;

    [[nodiscard]] const WavStreamInfo& info() const noexcept;
    [[nodiscard]] core::Result<core::FrameCount> read_frames(
        core::FrameIndex absoluteStart,
        MutableAudioBufferView destination);
    [[nodiscard]] core::Status close();

private:
    WavReader(
        std::unique_ptr<core::IResourceReader> source,
        WavStreamInfo info) noexcept;

    [[nodiscard]] static core::Result<WavStreamInfo> parse(
        core::IResourceReader& source);

    std::unique_ptr<core::IResourceReader> source_;
    WavStreamInfo info_;
    bool closed_{false};
};

}  // namespace rgsml::audio
