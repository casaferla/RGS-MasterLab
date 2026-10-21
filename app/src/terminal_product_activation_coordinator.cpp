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
    // Render latency is observational. This timer never asserts that M17
    // owns the sink or shortens any protected FP/verification deadline.
    renderHeartbeat_.setInterval(1000);
    connect(&renderHeartbeat_,&QTimer::timeout,
            this,&TerminalProductActivationCoordinator::update_render_elapsed);
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
    renderHeartbeat_.stop();
    waitingForManualPreview_=false;
    manualPreviewInProgress_=false;
    transitioning_=true;
    static_cast<void>(session_->cancel());
    const auto sink=transport_->playback_snapshot();
    if(intent_ && sink
       &&sink.value()->state==core::PlaybackState::NO_SOURCE) {
        static_cast<void>(selector_->restore_manual_processed_after_terminal(
            intent_->manualOwner,intent_->manualId,intent_->sourceCue));
    }
    const auto restored=transport_->playback_snapshot();
    // Background render failure does not require restoring transport:
    // a playing or paused MANUAL PCM was never stopped.
    if(intent_ && (handoffStarted_||deviceActive_) && restored
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
    handoffStarted_=false;
    transitioning_=false;
}

void TerminalProductActivationCoordinator::begin_activation()
{
    if(transitioning_||intent_)return;
    auto captured=selector_->capture_terminal_activation_intent();
    if(!captured) {
        // Preserve the precise, SOURCE-owned capture refusal. A failed
        // published owner, device realization, unstable clock or stale Loop
        // has a different remediation; the old generic string obscured
        // the real Windows failure after an upstream Input Gain edit.
        // NEVER retry or publish M17 on a rejected capture.
        fail_closed(QStringLiteral("M17 activation refused: %1")
            .arg(captured.error()
                ? QString::fromStdString(captured.error()->message())
                : QStringLiteral("unknown source ownership failure")));
        return;
    }
    intent_=*captured.value();
    transitioning_=true;
    view_->clearAudioError();
    // Keep the DSP-chain summary readable and show time spent waiting for
    // the exact accepted terminal output. During this interval the actual
    // audio is still the original MANUAL source, never the M17 candidate.
    view_->setAudioStatus(QStringLiteral("Preparing audio"));
    renderElapsed_.start();
    renderHeartbeat_.start();
    // Keep current MANUAL audio and its playhead running during offline M17
    // output-rate preparation. Only the accepted final handoff may stop it.
    auto submitted=session_->request_private_for_device(true);
    transitioning_=false;
    if(!submitted)
        fail_closed(QStringLiteral("M17 audio preparation was refused: %1")
                        .arg(QString::fromStdString(submitted.error()->message())));
}

void TerminalProductActivationCoordinator::update_render_elapsed()
{
    if (!intent_ || !renderElapsed_.isValid() || deviceActive_
        || state_->bypassed()) {
        renderHeartbeat_.stop();
        return;
    }
    // No percentage is fabricated: we know elapsed wall-clock time only.
    const auto seconds=renderElapsed_.elapsed()/1000;
    view_->setAudioStatus(QStringLiteral("Preparing · %1s").arg(seconds));
}

void TerminalProductActivationCoordinator::commit_ready_output()
{
    if(transitioning_||!intent_||state_->bypassed())return;
    const auto before=transport_->playback_snapshot();
    const auto source=transport_->source_display_snapshot();
    const auto after=transport_->playback_snapshot();
    const bool stable=before && source && after
        &&before.value()->state==after.value()->state
        &&before.value()->audibleRealization==after.value()->audibleRealization
        &&before.value()->audibleRealization.phase==core::AudibleHandoffPhase::NEW
        &&before.value()->audibleRealization.realizationId
            ==std::optional{intent_->manualId}
        &&before.value()->duration
            ==std::optional{intent_->manualOwner->view().frame_count()}
        &&before.value()->loop==after.value()->loop
        &&before.value()->seekSerial==after.value()->seekSerial
        &&before.value()->traversalSerial==after.value()->traversalSerial
        &&before.value()->loopWrapCount==after.value()->loopWrapCount
        &&source.value()->state==before.value()->state
        &&source.value()->loop==intent_->sourceLoop
        &&source.value()->duration==intent_->manualOwner->view().frame_count()
        &&source.value()->position.value()>=0
        &&source.value()->position.value()<=source.value()->duration.value();
    if(!stable) {
        fail_closed(QStringLiteral("M17 handoff lost exact MANUAL source ownership."));
        return;
    }
    // Latest SOURCE cue and user play/pause/stop state, possibly after loop
    // wraps while rendering, supersede the initial activation snapshot.
    intent_->sourceCue=source.value()->position;
    intent_->previousPlaybackState=source.value()->state;
    transitioning_=true;
    handoffStarted_=true;
    if(source.value()->state!=core::PlaybackState::STOPPED)
        transport_->stop();
    const auto stopped=transport_->playback_snapshot();
    if(!stopped || stopped.value()->state!=core::PlaybackState::STOPPED
       ||stopped.value()->audibleRealization.realizationId
            !=std::optional{intent_->manualId}
       ||stopped.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::NEW
       ||stopped.value()->duration
            !=std::optional{intent_->manualOwner->view().frame_count()}) {
        transitioning_=false;
        fail_closed(QStringLiteral("M17 could not stop the exact manual owner."));
        return;
    }
    const auto sought=transport_->seek_source_frame(intent_->sourceCue);
    const auto current=transport_->source_display_snapshot();
    if(!sought || !current
       ||current.value()->position!=intent_->sourceCue
       ||current.value()->loop!=intent_->sourceLoop) {
        transitioning_=false;
        fail_closed(QStringLiteral("M17 could not restore the source cue/loop."));
        return;
    }
    const auto installed=session_->install_ready_private();
    transitioning_=false;
    if(!installed) {
        fail_closed(QStringLiteral("M17 accepted render install failed: %1")
                        .arg(QString::fromStdString(installed.error()->message())));
        return;
    }
    finish_activation();
}

void TerminalProductActivationCoordinator::finish_activation()
{
    if(transitioning_||!intent_||state_->bypassed())return;
    renderHeartbeat_.stop();
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
    handoffStarted_=false;
    transitioning_=false;
}

void TerminalProductActivationCoordinator::deactivate(bool keepActiveIntent)
{
    if(transitioning_)return;
    renderHeartbeat_.stop();
    transitioning_=true;
    const bool hadActiveOutput=deviceActive_;
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
    if(intent_ && hadActiveOutput)resume_previous_state();
    intent_.reset();
    deviceActive_=false;
    handoffStarted_=false;
    view_->setAudioStatus(QStringLiteral("BYPASS · MANUAL PROCESSED"));
    transitioning_=false;
    if(keepActiveIntent)begin_activation();
}

void TerminalProductActivationCoordinator::limiter_accepted()
{
    if(transitioning_)return;
    if(state_->bypassed()) {
        waitingForManualPreview_=false;
        deactivate(false);
    } else if(manualPreviewInProgress_) {
        // Accepted Ceiling/ON during a new MANUAL render must not borrow
        // the predecessor: retain Active intent for the next exact owner.
        waitingForManualPreview_=true;
        view_->setAudioStatus(QStringLiteral("Preparing manual audio"));
    } else if(intent_) {
        deactivate(true); // Committed Ceiling change: rerender the whole PCM.
    } else {
        begin_activation();
    }
}

void TerminalProductActivationCoordinator::manual_preview_changed(
    const QString& previewStatus)
{
    if(previewStatus==QStringLiteral("RENDERING")) {
        manualPreviewInProgress_=true;
        if(state_->bypassed()||transitioning_||waitingForManualPreview_)return;
        // Retire any installed terminal BEFORE set_processed_realization()
        // attempts a SOURCE handoff. The previous accepted MANUAL audio is
        // restored exactly; no stale M17 session can obstruct its successor.
        if(intent_)deactivate(false);
        if(state_->bypassed())return; // A real retirement failure stays closed.
        waitingForManualPreview_=true;
        view_->clearAudioError();
        view_->setAudioStatus(QStringLiteral("Preparing manual audio"));
        return;
    }
    if(previewStatus==QStringLiteral("READY")) {
        manualPreviewInProgress_=false;
        if(!waitingForManualPreview_)return;
        waitingForManualPreview_=false;
        if(!state_->bypassed())begin_activation();
        return;
    }
    if(previewStatus==QStringLiteral("ERROR")
       ||previewStatus==QStringLiteral("IDLE")) {
        manualPreviewInProgress_=false;
        if(waitingForManualPreview_&&!state_->bypassed()) {
            waitingForManualPreview_=false;
            fail_closed(QStringLiteral(
                "M17 manual preparation failed or was invalidated."));
        }
    }
}

void TerminalProductActivationCoordinator::loop_context_changed()
{
    if(transitioning_||!intent_||state_->bypassed())return;
    const auto source=transport_->source_display_snapshot();
    if(!source) {
        fail_closed(QStringLiteral("M17 Loop Region owner unavailable."));
        return;
    }
    if(source.value()->loop==intent_->sourceLoop)return;
    if(deviceActive_) {
        // Installed OUTPUT_RATE PCM represents the whole programme; the
        // live SOURCE transport already applied this Loop Region change.
        intent_->sourceLoop=source.value()->loop;
    } else {
        // The pending job captured a different loop/owner. Cancel it and
        // request a fresh exact candidate while preserving the Active bit.
        deactivate(true);
    }
}

void TerminalProductActivationCoordinator::private_session_changed()
{
    if(transitioning_||!intent_)return;
    const auto status=session_->status();
    if(status==QStringLiteral("READY_TO_INSTALL"))
        commit_ready_output();
    else if(status==QStringLiteral("PRIVATE_INSTALLED"))
        finish_activation();
    else if(status==QStringLiteral("ERROR")||status==QStringLiteral("STALE")
            ||status==QStringLiteral("IDLE"))
        fail_closed(session_->error().isEmpty()
            ? QStringLiteral("M17 terminal session unavailable (%1).").arg(status)
            : QStringLiteral("M17 terminal session failed: %1").arg(session_->error()));
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
