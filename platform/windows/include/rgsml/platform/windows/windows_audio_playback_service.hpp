#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/monitoring_frame_map.hpp>
#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/platform/windows/windows_consumed_output_prefix.hpp>

#include <memory>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::analysis {
class LiveSpectrumAnalyzer;
}
namespace rgsml::audio {
struct MonitoringPcmCandidate;
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
    // Prepare-start advisory: read current Windows memory headroom and the
    // independently negotiated monitoring-device rate. All six proposed
    // MASTER sample rates are evaluated WITHOUT modifying the selected
    // master, reserving memory, or declaring a CPU/hardware qualification.
    // The caller supplies estimated other Prepare/DSP allocation bytes.
    // Re-evaluate before costly allocations: memory availability changes.
    [[nodiscard]] core::Result<std::vector<audio::MonitoringRateMemoryAdvice>>
    advise_prepare_master_rate_memory(
        core::SampleRate sourceRate,core::FrameCount sourceFrames,
        audio::ChannelLayout layout,
        std::uint64_t otherPrepareReserveBytes=0) const;

    // EXPERIMENTAL device-monitoring seam: candidate PCM is separately owned
    // and already converted offline. Never substitute a new master rate or
    // claim M17 terminal/M16 LIVE ownership. No active UI publication.
    [[nodiscard]] core::Result<core::SampleRate>
    preferred_monitoring_output_rate(audio::ChannelLayout layout) const;
    [[nodiscard]] core::Status prepare_monitoring_output_pcm(
        std::shared_ptr<const audio::MonitoringPcmCandidate> candidate);
    // Monitoring-only master-coordinate controls. These are NOT the
    // IAudioPlaybackService SOURCE frame methods nor a qualified DAC clock.
    [[nodiscard]] core::Status seek_monitoring_master_frame(
        core::FrameIndex masterFrame);
    [[nodiscard]] core::Status set_monitoring_master_loop(
        std::optional<core::FrameRange> masterLoop);
    // Explicit MASTER-domain display. Generic snapshot() remains DEVICE
    // coordinates and must not be republished as master/audible proof.
    [[nodiscard]] core::Result<audio::MonitoringMasterDisplaySnapshot>
    monitoring_master_snapshot() const;
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
