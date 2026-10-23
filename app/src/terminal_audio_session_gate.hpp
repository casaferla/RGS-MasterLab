#pragma once

#include "terminal_candidate_gate.hpp"
#include "terminal_source_control_bridge.hpp"
#include "terminal_source_transport_port.hpp"

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/realization_identity.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace rgsml::app {

// Private, pre-product two-phase M17 audio-session installer.
// It does NOT change AuditionSourceSelector, allocate a terminal RealizationId,
// or expose a new PROCESSED/UI destination. Actual publication is future-gated.
class TerminalAudioSessionGate final : public TerminalSourceTransportPort {
public:
    using DevicePrepare = std::function<core::Status(
        audio::AudioBufferView,
        std::shared_ptr<const void>,
        core::RealizationId)>;

    TerminalAudioSessionGate(
        TerminalCandidateGate& gate,
        core::IAudioPlaybackService& playback,
        DevicePrepare prepare)
      : gate_(&gate),playback_(&playback),prepare_(std::move(prepare)) {}

    // Requires a STOPPED manual PROCESSED session with matching NEW audible ID.
    // 'terminalId' is issued by a FUTURE distinct central publisher and
    // MUST NOT be the manual PROCESSED ID. Failed/stale installs are never
    // reported as available and cannot grant M16 LIVE consumption authority.
    [[nodiscard]] core::Status prepare_private_session(
        std::shared_ptr<const render::RenderResult> current_prepared,
        std::shared_ptr<const render::RenderResult> current_manual,
        std::optional<core::RealizationId> current_manual_id,
        core::RealizationId distinct_terminal_id);

    [[nodiscard]] bool has_session() const noexcept override
    { return control_.has_value(); }
    [[nodiscard]] std::weak_ptr<const void> session_liveness() const noexcept override
    { return sessionLease_; }
    [[nodiscard]] bool owns_playback(const core::IAudioPlaybackService& service)
        const noexcept override { return playback_ == &service; }
    [[nodiscard]] core::Result<core::SampleRate> source_rate() const override;
    [[nodiscard]] core::Result<DisplaySnapshot>
    source_display_snapshot() const override;

    [[nodiscard]] core::Status seek_source(core::FrameIndex source) override;
    [[nodiscard]] core::Status set_source_loop(
        std::optional<core::FrameRange> loop) override;
    [[nodiscard]] core::Result<TerminalSourceControlBridge::SourceSnapshot>
    source_snapshot() const;

    // Invalidates logical ownership, but never clears a different session.
    void detach() noexcept;
    // Retire only the exact ID installed by this gate. A newer audible
    // owner must NEVER be stopped or cleared by an obsolete M17 gate.
    [[nodiscard]] core::Status retire_owned_session();

private:
    // Reclaim only our own partial terminal install. If another owner took
    // over, leave that session untouched.
    void clear_owned_failed_install(core::RealizationId attempted_id) noexcept;

    TerminalCandidateGate* gate_;
    core::IAudioPlaybackService* playback_;
    DevicePrepare prepare_;
    std::optional<TerminalSourceControlBridge> control_;
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> owner_;
    std::shared_ptr<const void> sessionLease_;
    std::optional<core::RealizationId> installedTerminalId_;
};

} // namespace rgsml::app
