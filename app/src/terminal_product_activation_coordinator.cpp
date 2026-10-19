#include "terminal_product_activation_coordinator.hpp"
#include <rgsml/core/error.hpp>

namespace rgsml::app {
TerminalProductActivationCoordinator::TerminalProductActivationCoordinator(
    TerminalLimiterState& state,TerminalLimiterViewModel& view,
    AuditionSourceSelector& selector,PlaybackTransportViewModel& transport,
    TerminalPrivateProductSession& session,QObject* parent)
    : QObject(parent),state_(&state),view_(&view),selector_(&selector),
      transport_(&transport),session_(&session)
{
    connect(view_,&TerminalLimiterViewModel::acceptedStateChanged,
            this,&TerminalProductActivationCoordinator::limiter_accepted);
    connect(session_,&TerminalPrivateProductSession::changed,
            this,&TerminalProductActivationCoordinator::private_session_changed);
    connect(selector_,&AuditionSourceSelector::changed,
            this,&TerminalProductActivationCoordinator::external_context_changed);
}

void TerminalProductActivationCoordinator::resume_previous_state()
{
    if(!intent_)return;
    if(intent_->previousPlaybackState==core::PlaybackState::PLAYING) {
        transport_->playOrResume();
    } else if(intent_->previousPlaybackState==core::PlaybackState::PAUSED) {
        // A freshly prepared PCM is STOPPED. Recreate PAUSED synchronously
        // instead of leaving the user in an unrequested stopped state.
        transport_->playOrResume();
        auto snapshot=transport_->playback_snapshot();
        if(snapshot && snapshot.value()->state==core::PlaybackState::PLAYING)
            transport_->pause();
    }
}

void TerminalProductActivationCoordinator::fail_closed(const QString& reason)
{
    if(transitioning_)return;
    transitioning_=true;
    static_cast<void>(session_->cancel());
    const auto sink=transport_->playback_snapshot();
    if(intent_ && sink
       &&sink.value()->state==core::PlaybackState::NO_SOURCE) {
        static_cast<void>(selector_->restore_manual_processed_after_terminal(
            intent_->manualOwner,intent_->manualId,intent_->sourceCue));
    }
    const auto restored=transport_->playback_snapshot();
    if(intent_ && restored
       &&restored.value()->audibleRealization.realizationId
           ==std::optional{intent_->manualId}
       &&restored.value()->audibleRealization.phase
           ==core::AudibleHandoffPhase::NEW)
        resume_previous_state();
    static_cast<void>(state_->set_user_bypass(true));
    view_->refreshFromAuthority();
    view_->reportAudioError(reason);
    view_->setAudioStatus(QStringLiteral("ERROR · BYPASS"));
    intent_.reset();
    deviceActive_=false;
    transitioning_=false;
}

void TerminalProductActivationCoordinator::begin_activation()
{
    if(transitioning_||intent_)return;
    auto captured=selector_->capture_terminal_activation_intent();
    if(!captured) {
        fail_closed(QStringLiteral(
            "M17 activation needs the exact selected PROCESSED source."));
        return;
    }
    intent_=*captured.value();
    transitioning_=true;
    view_->clearAudioError();
    view_->setAudioStatus(QStringLiteral("RENDERING · AUDITION ONLY"));
    if(intent_->previousPlaybackState!=core::PlaybackState::STOPPED)
        transport_->stop();
    auto stopped=transport_->playback_snapshot();
    if(!stopped || stopped.value()->state!=core::PlaybackState::STOPPED
       ||stopped.value()->audibleRealization.realizationId
           !=std::optional{intent_->manualId}
       ||stopped.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||stopped.value()->duration
           !=std::optional{intent_->manualOwner->view().frame_count()}) {
        transitioning_=false;
        fail_closed(QStringLiteral("M17 could not safely stop the manual source."));
        return;
    }
    // stop() resets the cursor. Restore SOURCE coordinates before the
    // owned terminal publisher captures the stopped manual transport.
    auto sought=transport_->seek_source_frame(intent_->sourceCue);
    if(!sought) {
        transitioning_=false;
        fail_closed(QStringLiteral("M17 cannot restore the SOURCE cue."));
        return;
    }
    const auto current=transport_->source_display_snapshot();
    if(!current || current.value()->position!=intent_->sourceCue
       ||current.value()->loop!=intent_->sourceLoop) {
        transitioning_=false;
        fail_closed(QStringLiteral("M17 source position or Loop Region changed."));
        return;
    }
    // DEVICE rate is audition-only; the independent final-master rate must
    // never be silently inferred from it.
    auto submitted=session_->request_private_for_device();
    transitioning_=false;
    if(!submitted)
        fail_closed(QStringLiteral("M17 audio preparation was refused."));
}

void TerminalProductActivationCoordinator::finish_activation()
{
    if(transitioning_||!intent_||state_->bypassed())return;
    auto accepted=session_->private_installed_output_identity();
    if(!accepted || selector_->processed_realization_snapshot()
            !=intent_->manualOwner
       ||selector_->processed_realization_id()
            !=std::optional{intent_->manualId}
       ||accepted.value()->source.position.value()<0
       ||(accepted.value()->source.position.value()
                >intent_->sourceCue.value()
            ?accepted.value()->source.position.value()
                -intent_->sourceCue.value()
            :intent_->sourceCue.value()
                -accepted.value()->source.position.value())>1
       ||accepted.value()->source.loop!=intent_->sourceLoop
       ||accepted.value()->playback.state!=core::PlaybackState::STOPPED) {
        fail_closed(QStringLiteral("M17 device/source ownership mismatch."));
        return;
    }
    transitioning_=true;
    deviceActive_=true;
    resume_previous_state();
    auto after=transport_->playback_snapshot();
    if(!after || after.value()->audibleRealization.realizationId
          !=std::optional{accepted.value()->realizationId}
       ||(intent_->previousPlaybackState==core::PlaybackState::PLAYING
             &&after.value()->state!=core::PlaybackState::PLAYING)
       ||(intent_->previousPlaybackState==core::PlaybackState::PAUSED
             &&after.value()->state!=core::PlaybackState::PAUSED)) {
        transitioning_=false;
        fail_closed(QStringLiteral("M17 device did not preserve playback state."));
        return;
    }
    view_->setAudioStatus(QStringLiteral("ACTIVE · AUDITION UNQUALIFIED"));
    transitioning_=false;
}

void TerminalProductActivationCoordinator::deactivate(bool keepActiveIntent)
{
    if(transitioning_)return;
    transitioning_=true;
    if(intent_ && deviceActive_) {
        const auto installed=session_->private_installed_output_identity();
        if(installed) {
            intent_->sourceCue=installed.value()->source.position;
            intent_->sourceLoop=installed.value()->source.loop;
            intent_->previousPlaybackState=installed.value()->playback.state;
        }
    }
    const auto retired=session_->cancel();
    if(!retired) {
        transitioning_=false;
        fail_closed(QStringLiteral("M17 terminal retirement failed."));
        return;
    }
    auto sink=transport_->playback_snapshot();
    if(intent_ && sink&&sink.value()->state==core::PlaybackState::NO_SOURCE) {
        auto restored=selector_->restore_manual_processed_after_terminal(
            intent_->manualOwner,intent_->manualId,intent_->sourceCue);
        if(!restored) {
            transitioning_=false;
            fail_closed(QStringLiteral("M17 bypass could not restore manual PCM."));
            return;
        }
    }
    if(intent_)resume_previous_state();
    intent_.reset();
    deviceActive_=false;
    view_->setAudioStatus(QStringLiteral("BYPASS · MANUAL PROCESSED"));
    transitioning_=false;
    if(keepActiveIntent)begin_activation();
}

void TerminalProductActivationCoordinator::limiter_accepted()
{
    if(transitioning_)return;
    if(state_->bypassed()) {
        deactivate(false);
    } else if(intent_) {
        deactivate(true); // Committed Ceiling change: rerender the whole PCM.
    } else {
        begin_activation();
    }
}

void TerminalProductActivationCoordinator::private_session_changed()
{
    if(transitioning_||!intent_)return;
    const auto status=session_->status();
    if(status==QStringLiteral("PRIVATE_INSTALLED"))
        finish_activation();
    else if(status==QStringLiteral("ERROR")||status==QStringLiteral("STALE")
            ||status==QStringLiteral("IDLE"))
        fail_closed(QStringLiteral("M17 terminal session became unavailable."));
}

void TerminalProductActivationCoordinator::external_context_changed()
{
    if(transitioning_||!intent_)return;
    // A new manual owner or selected target invalidates any terminal
    // publication without silently installing PCM over another owner.
    if(selector_->processed_realization_snapshot()!=intent_->manualOwner
       ||selector_->processed_realization_id()
           !=std::optional{intent_->manualId}
       ||selector_->active_target()!=AuditionTarget::PROCESSED)
        fail_closed(QStringLiteral("M17 source/target changed."));
}
} // namespace rgsml::app
