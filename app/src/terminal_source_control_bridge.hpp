#pragma once

#include "terminal_candidate_gate.hpp"
#include "terminal_playback_frame_map.hpp"

#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/result.hpp>

#include <memory>
#include <optional>
#include <utility>

namespace rgsml::app {

// PRIVATE preproduct adapter: SOURCE-oriented user commands for one
// already-installed, identity-stamped OUTPUT_RATE playback session.
// The real application does NOT instantiate this adapter or publish M17.
// It never allocates a RealizationId or prepares any device audio.
class TerminalSourceControlBridge final {
public:
    struct SourceSnapshot final {
        core::PlaybackState state;
        core::FrameIndex position;
        core::FrameCount duration;
        std::optional<core::FrameRange> loop;
    };

    // 'terminal_id' must come from a future independent publisher; it
    // cannot alias the manual PROCESSED realization identity.  No ID is
    // manufactured from candidate ownership or frame counts.
    [[nodiscard]] static core::Result<TerminalSourceControlBridge> create(
        TerminalCandidateGate& gate,
        core::IAudioPlaybackService& playback,
        core::RealizationId terminal_id);
    // Separate opt-in nonconforming audition: never flags audio VERIFIED.
    [[nodiscard]] static core::Result<TerminalSourceControlBridge> create_measured(
        TerminalCandidateGate& gate,
        core::IAudioPlaybackService& playback,
        core::RealizationId terminal_id);

    // SOURCE frames are mapped only after a fresh identity/ownership check.
    [[nodiscard]] core::Status seek_source(core::FrameIndex source_position);
    [[nodiscard]] core::Status set_source_loop(
        std::optional<core::FrameRange> source_loop);
    // Display-only inverse (not M16 sink-consumption/GR authority).
    [[nodiscard]] core::Result<SourceSnapshot> source_snapshot() const;
    // No audio state or another owner's prepared source is modified.
    void detach() noexcept { active_=false; }

private:
    TerminalSourceControlBridge(
        TerminalCandidateGate* gate,
        core::IAudioPlaybackService* playback,
        std::shared_ptr<const TerminalCandidateGate::StagedCandidate> owner,
        TerminalPlaybackFrameMap map,
        core::RealizationId terminal_id) noexcept
      : gate_(gate),playback_(playback),accepted_(std::move(owner)),
        map_(std::move(map)),terminalId_(terminal_id) {}

    TerminalSourceControlBridge(
        TerminalCandidateGate* gate,
        core::IAudioPlaybackService* playback,
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate> owner,
        TerminalPlaybackFrameMap map,core::RealizationId terminal_id) noexcept
      : gate_(gate),playback_(playback),measured_(std::move(owner)),
        map_(std::move(map)),terminalId_(terminal_id) {}

    [[nodiscard]] core::Result<core::PlaybackSnapshot>
    checked_output_snapshot() const;

    TerminalCandidateGate* gate_{nullptr};
    core::IAudioPlaybackService* playback_{nullptr};
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> accepted_;
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate> measured_;
    TerminalPlaybackFrameMap map_;
    core::RealizationId terminalId_;
    std::optional<core::FrameRange> sourceLoop_;
    bool active_{true};
};

} // namespace rgsml::app
