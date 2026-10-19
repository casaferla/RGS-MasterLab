#pragma once

#include "terminal_candidate_gate.hpp"
#include "terminal_audio_session_gate.hpp"
#include "playback_transport_view_model.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace rgsml::app {

// PRIVATE M17 preproduct handoff transaction: central selector identity
// reservation -> exact OUTPUT_RATE device prepare -> SOURCE transport bind,
// seek and loop. This is not a PROCESSED UI publisher or M16 LIVE authority.
class TerminalPrivatePublicationCoordinator final {
public:
    using IssueId = std::function<core::Result<core::RealizationId>(
        std::shared_ptr<const render::RenderResult>,
        std::shared_ptr<const render::RenderResult>,
        std::optional<core::RealizationId>)>;

    TerminalPrivatePublicationCoordinator(
        TerminalCandidateGate& candidate,
        TerminalAudioSessionGate& session,
        PlaybackTransportViewModel& transport,
        IssueId issuer)
        : candidate_(&candidate), session_(&session),
          transport_(&transport), issueId_(std::move(issuer)) {}

    [[nodiscard]] core::Result<core::RealizationId> install_private(
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const render::RenderResult> manual,
        std::optional<core::RealizationId> manualId,
        std::optional<core::FrameRange> sourceLoop);

private:
    TerminalCandidateGate* candidate_;
    TerminalAudioSessionGate* session_;
    PlaybackTransportViewModel* transport_;
    IssueId issueId_;
};

} // namespace rgsml::app
