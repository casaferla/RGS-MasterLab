#include "terminal_audio_session_gate.hpp"

#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::app {
namespace {
[[nodiscard]] core::Error denied(const char* category,const char* reason)
{
    return core::Error{
        core::ErrorCode::InvalidState,reason,{{"category",category}}};
}
} // namespace

core::Status TerminalAudioSessionGate::prepare_private_session(
    std::shared_ptr<const render::RenderResult> prepared,
    std::shared_ptr<const render::RenderResult> manual,
    std::optional<core::RealizationId> manualId,
    core::RealizationId terminalId)
{
    // It would be unsafe for an unsupported early caller to leave a prior
    // logical terminal control usable while attempting a replacement.
    detach();
    auto accepted=gate_->staged();
    if(!accepted || !prepared || !manual || !manualId
       ||manualId->value==0 ||terminalId.value==0
       ||terminalId==*manualId
       ||accepted->original_manual_realization_id!=*manualId
       ||accepted->prepared_owner!=prepared
       ||accepted->rendered.manual_owner!=manual
       ||!prepare_)
        return core::Status::failure(denied(
            "M17_SESSION_AUTHORITY_MISSING",
            "The terminal installer requires a current accepted owner and distinct external ID."));
    auto mapping=TerminalPlaybackFrameMap::create(*accepted);
    if(!mapping)
        return core::Status::failure(*mapping.error());
    auto before=playback_->snapshot();
    if(!before)return core::Status::failure(*before.error());
    if(before.value()->state!=core::PlaybackState::STOPPED
       ||before.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||before.value()->audibleRealization.realizationId!=manualId
       ||!before.value()->duration
       ||*before.value()->duration!=mapping.value()->source_frames())
        return core::Status::failure(denied(
            "M17_SESSION_REQUIRES_STOPPED_MANUAL",
            "A stopped audible manual PROCESSED owner is required before terminal session installation."));
    const auto pcm=accepted->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    const auto installed=prepare_(pcm,accepted,terminalId);
    if(!installed) {
        clear_owned_failed_install(terminalId);
        return installed;
    }
    // A callback may synchronously switch Source/Ceiling or an output
    // device owner. No candidate survives such a race.
    if(gate_->staged()!=accepted) {
        clear_owned_failed_install(terminalId);
        return core::Status::failure(denied(
            "M17_SESSION_STALE_DURING_INSTALL",
            "Accepted terminal owner changed during output device installation."));
    }
    auto controller=TerminalSourceControlBridge::create(
        *gate_,*playback_,terminalId);
    if(!controller) {
        clear_owned_failed_install(terminalId);
        return core::Status::failure(*controller.error());
    }
    owner_=std::move(accepted);
    control_.emplace(std::move(*controller.value()));
    sessionLease_ = std::make_shared<int>(0);
    installedTerminalId_ = terminalId;
    return core::Status::success();
}

void TerminalAudioSessionGate::clear_owned_failed_install(
    core::RealizationId attempted) noexcept
{
    const auto snap=playback_->snapshot();
    // A third-party handoff may already be in progress; touching it would
    // violate the separately published RealizationId authority.
    if(!snap
       ||snap.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||snap.value()->audibleRealization.realizationId
           !=std::optional{attempted})
        return;
    if(snap.value()->state==core::PlaybackState::PLAYING
       ||snap.value()->state==core::PlaybackState::PAUSED)
        static_cast<void>(playback_->stop());
    static_cast<void>(playback_->clear());
}

core::Status TerminalAudioSessionGate::retire_owned_session()
{
    core::Status result=core::Status::success();
    if (installedTerminalId_) {
        auto current=playback_->snapshot();
        if (!current) {
            result=core::Status::failure(*current.error());
        } else if (current.value()->audibleRealization.phase
                   ==core::AudibleHandoffPhase::NEW
                   &&current.value()->audibleRealization.realizationId
                       ==installedTerminalId_) {
            if (current.value()->state==core::PlaybackState::PLAYING
                ||current.value()->state==core::PlaybackState::PAUSED) {
                result=playback_->stop();
            }
            if (result) result=playback_->clear();
        }
    }
    // Retire logical authority even if the physical device reports error.
    // Any failure is propagated; another owner is never erased.
    detach();
    return result;
}

void TerminalAudioSessionGate::detach() noexcept
{
    sessionLease_.reset();
    installedTerminalId_.reset();
    if(control_)control_->detach();
    control_.reset();
    owner_.reset();
}

core::Result<core::SampleRate> TerminalAudioSessionGate::source_rate() const
{
    auto checked=source_snapshot();
    if (!checked || !owner_ || !owner_->rendered.manual_owner)
        return core::Result<core::SampleRate>::failure(denied(
            "M17_SESSION_NOT_OWNED","No authoritative terminal SOURCE rate."));
    return core::Result<core::SampleRate>::success(
        owner_->rendered.manual_owner->view().format().sample_rate());
}

core::Result<TerminalSourceTransportPort::DisplaySnapshot>
TerminalAudioSessionGate::source_display_snapshot() const
{
    auto source=source_snapshot();
    if (!source)
        return core::Result<DisplaySnapshot>::failure(*source.error());
    return core::Result<DisplaySnapshot>::success(DisplaySnapshot{
        source.value()->state, source.value()->position,
        source.value()->duration, source.value()->loop});
}

core::Status TerminalAudioSessionGate::seek_source(core::FrameIndex source)
{
    if(!control_)return core::Status::failure(denied(
        "M17_SESSION_NOT_OWNED","No valid terminal session was installed."));
    return control_->seek_source(source);
}

core::Status TerminalAudioSessionGate::set_source_loop(
    std::optional<core::FrameRange> loop)
{
    if(!control_)return core::Status::failure(denied(
        "M17_SESSION_NOT_OWNED","No valid terminal session was installed."));
    return control_->set_source_loop(loop);
}

core::Result<TerminalSourceControlBridge::SourceSnapshot>
TerminalAudioSessionGate::source_snapshot() const
{
    if(!control_)
        return core::Result<TerminalSourceControlBridge::SourceSnapshot>::failure(
            denied("M17_SESSION_NOT_OWNED",
                   "No valid terminal session was installed."));
    return control_->source_snapshot();
}
} // namespace rgsml::app
