#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/audio_playback_service.hpp>

#include <memory>

namespace rgsml::platform::windows {

// Windows owner-thread adapter for the frozen playback control-plane port.
// Qt Multimedia and native device state are hidden in the implementation.
class WindowsAudioPlaybackService final : public core::IAudioPlaybackService {
public:
    WindowsAudioPlaybackService();
    ~WindowsAudioPlaybackService() noexcept override;

    WindowsAudioPlaybackService(const WindowsAudioPlaybackService&) = delete;
    WindowsAudioPlaybackService& operator=(const WindowsAudioPlaybackService&) = delete;
    WindowsAudioPlaybackService(WindowsAudioPlaybackService&&) = delete;
    WindowsAudioPlaybackService& operator=(WindowsAudioPlaybackService&&) = delete;

    [[nodiscard]] core::Status prepare(
        const core::ResourceReference& source) override;
    // Windows-specific borrowed canonical-PCM seam. The caller must keep the
    // backing AudioBuffer alive until clear() returns.
    [[nodiscard]] core::Status prepare_pcm(audio::AudioBufferView source);
    [[nodiscard]] core::Status clear() override;
    [[nodiscard]] core::Status play() override;
    [[nodiscard]] core::Status pause() override;
    [[nodiscard]] core::Status stop() override;
    [[nodiscard]] core::Status seek(core::FrameIndex position) override;
    [[nodiscard]] core::Status set_loop(
        std::optional<core::FrameRange> loop) override;
    [[nodiscard]] core::Result<core::PlaybackSnapshot> snapshot() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rgsml::platform::windows
