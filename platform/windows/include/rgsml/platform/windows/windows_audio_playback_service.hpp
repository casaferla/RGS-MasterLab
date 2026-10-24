#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/platform/windows/windows_consumed_output_prefix.hpp>

#include <memory>
#include <cstdint>
#include <optional>

namespace rgsml::analysis {
class LiveSpectrumAnalyzer;
}

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
    [[nodiscard]] core::Status prepare_pcm(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime,
        std::optional<core::RealizationId> realizationId = std::nullopt);
    // M17 candidate-only: no PROCESSED publication or active UI yet.
    // Advisory owner-thread probe of the current device preferred exact
    // output format; install rechecks against the actual selected device.
    [[nodiscard]] core::Result<core::SampleRate>
    preferred_terminal_output_rate(audio::ChannelLayout layout) const;
    [[nodiscard]] core::Status prepare_terminal_output_pcm(
        audio::AudioBufferView pcm,
        std::shared_ptr<const void> lifetime,
        core::RealizationId candidateIdentity);
    [[nodiscard]] core::Status handoff_pcm(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime = nullptr,
        std::optional<core::RealizationId> realizationId = std::nullopt);
    [[nodiscard]] core::Status clear() override;
    [[nodiscard]] core::Status play() override;
    [[nodiscard]] core::Status pause() override;
    [[nodiscard]] core::Status stop() override;
    [[nodiscard]] core::Status seek(core::FrameIndex position) override;
    [[nodiscard]] core::Status set_loop(
        std::optional<core::FrameRange> loop) override;
    [[nodiscard]] core::Result<core::PlaybackSnapshot> snapshot() const override;
    // C2D6: serialized worker snapshot, never an audible DAC clock or
    // qualified LIVE True Peak. An expected stale generation is rejected.
    [[nodiscard]] core::Result<WindowsConsumedOutputPrefix>
    consumed_output_prefix(
        std::optional<std::uint64_t> expectedGeneration = std::nullopt) const;

    void attach_analyzer(analysis::LiveSpectrumAnalyzer* analyzer);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rgsml::platform::windows
