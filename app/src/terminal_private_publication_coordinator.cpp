#include "terminal_private_publication_coordinator.hpp"

#include <rgsml/core/error.hpp>

namespace rgsml::app {
namespace {
[[nodiscard]] core::Error refused(const char* message)
{
    return core::Error{core::ErrorCode::InvalidState,message};
}
} // namespace

core::Result<core::RealizationId>
TerminalPrivatePublicationCoordinator::install_private(
    std::shared_ptr<const render::RenderResult> prepared,
    std::shared_ptr<const render::RenderResult> manual,
    std::optional<core::RealizationId> manualId,
    std::optional<core::FrameRange> sourceLoop)
{
    using Result=core::Result<core::RealizationId>;
    auto staged=candidate_->staged();
    if (!issueId_ || !staged || !prepared || !manual || !manualId
        ||manualId->value==0
        ||staged->prepared_owner!=prepared
        ||staged->rendered.manual_owner!=manual
        ||staged->original_manual_realization_id!=*manualId)
        return Result::failure(refused(
            "Private M17 requires the current exact staged PREPARED and MANUAL owners."));

    const auto before=transport_->playback_snapshot();
    if (!before) return Result::failure(*before.error());
    if (before.value()->state!=core::PlaybackState::STOPPED
        ||before.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::NEW
        ||before.value()->audibleRealization.realizationId!=manualId
        ||!before.value()->duration
        ||*before.value()->duration!=manual->view().frame_count())
        return Result::failure(refused(
            "Private M17 installation requires stopped audible MANUAL ownership."));

    // Central selector reserves from its shared MANUAL/TERMINAL sequence.
    // A failed installation burns the ID but can NEVER publish it.
    auto reserved=issueId_(prepared,manual,manualId);
    if (!reserved) return reserved;
    if (reserved.value()->value==0 || *reserved.value()==*manualId
        ||candidate_->staged()!=staged)
        return Result::failure(refused(
            "Terminal realization ID invalid or candidate changed during reservation."));
    // The issuer may reenter the event loop; recheck the audible owner.
    const auto current=transport_->playback_snapshot();
    if (!current) return Result::failure(*current.error());
    if (current.value()->state!=before.value()->state
        ||current.value()->audibleRealization!=before.value()->audibleRealization
        ||current.value()->duration!=before.value()->duration)
        return Result::failure(refused(
            "Manual audible owner changed during terminal ID reservation."));

    auto installed=session_->prepare_private_session(
        prepared,manual,manualId,*reserved.value());
    if (!installed) return Result::failure(*installed.error());
    auto rollback=[this](const core::Error& failure) -> Result {
        auto retired=session_->retire_owned_session();
        return Result::failure(retired?failure:*retired.error());
    };
    auto bound=transport_->bind_private_terminal_source_controls(*session_);
    if (!bound) return rollback(*bound.error());
    auto sought=transport_->seek_source_frame(before.value()->position);
    if (!sought) return rollback(*sought.error());
    auto looped=transport_->set_loop_source_range(sourceLoop);
    if (!looped) return rollback(*looped.error());
    const auto sourceDisplay=transport_->source_display_snapshot();
    const auto installedOutput=transport_->playback_snapshot();
    if (!sourceDisplay || !installedOutput
        ||candidate_->staged()!=staged
        ||installedOutput.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::NEW
        ||installedOutput.value()->audibleRealization.realizationId
            !=std::optional{*reserved.value()}
        ||sourceDisplay.value()->loop!=sourceLoop)
        return rollback(refused(
            "Terminal SOURCE/output publication lost ownership during private commit."));

    // No selector mutation, PROCESSED UI publication or M16 LIVE claim.
    return Result::success(*reserved.value());
}

} // namespace rgsml::app
