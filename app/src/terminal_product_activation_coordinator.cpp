#include "terminal_product_activation_coordinator.hpp"
#include "m17_prepare_live_eta_policy.hpp"
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <cmath>
#include <QMetaObject>
#include <QThread>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <new>
#include <exception>

namespace rgsml::app {
TerminalProductActivationCoordinator::TerminalProductActivationCoordinator(
    TerminalLimiterState& state,TerminalLimiterViewModel& view,
    AuditionSourceSelector& selector,PlaybackTransportViewModel& transport,
    TerminalPrivateProductSession& session,QObject* parent)
    : QObject(parent),state_(&state),view_(&view),selector_(&selector),
      transport_(&transport),session_(&session),
      lastParameterRevision_(state.parameter_revision())
{
    connect(view_,&TerminalLimiterViewModel::acceptedStateChanged,
            this,&TerminalProductActivationCoordinator::limiter_accepted);
    connect(session_,&TerminalPrivateProductSession::changed,
            this,&TerminalProductActivationCoordinator::private_session_changed);
    connect(view_,&TerminalLimiterViewModel::prepareRequested,
            this,&TerminalProductActivationCoordinator::prepare_requested);
    connect(view_,&TerminalLimiterViewModel::cancelRequested,
            this,&TerminalProductActivationCoordinator::cancel_requested);
    connect(view_,&TerminalLimiterViewModel::previewRequested,
            this,&TerminalProductActivationCoordinator::preview_requested);
    connect(view_,&TerminalLimiterViewModel::previewNonconformingRequested,
            this,&TerminalProductActivationCoordinator::preview_nonconforming_requested);
    connect(selector_,&AuditionSourceSelector::changed,
            this,&TerminalProductActivationCoordinator::external_context_changed);
    // Render latency is observational. This timer never asserts that M17
    // owns the sink or shortens any protected FP/verification deadline.
    measuredProgressTimer_.setInterval(300);
    connect(&measuredProgressTimer_,&QTimer::timeout,this,[this] {
        if(!measuredPreparing_)return;
        const auto total=measuredTotalFrames_.load(std::memory_order_acquire);
        const auto done=measuredCompletedFrames_.load(std::memory_order_acquire);
        view_->setPreparationProgress(
            QStringLiteral("Converting measured MASTER for DEVICE"),
            total>0?std::clamp(static_cast<double>(done)/total,0.0,1.0):0.0,
            total>0);
    });
    renderHeartbeat_.setInterval(1000);
    connect(&renderHeartbeat_,&QTimer::timeout,
            this,&TerminalProductActivationCoordinator::update_render_elapsed);
}

TerminalProductActivationCoordinator::~TerminalProductActivationCoordinator()
{
    cancel_measured_crossrate_audition();
    if(measuredWorker_.joinable()) {
        measuredWorker_.request_stop();
        measuredWorker_.join();
    }
}

void TerminalProductActivationCoordinator::cancel_measured_crossrate_audition()
{
    if(measuredCancel_)
        measuredCancel_->store(true,std::memory_order_release);
    ++measuredSerial_;
    measuredPreparing_=false;
    view_->setNonconformingDevicePreparing(false);
    pendingMeasured_.reset();
    measuredProgressTimer_.stop();
    measuredCompletedFrames_.store(0,std::memory_order_release);
    measuredTotalFrames_.store(0,std::memory_order_release);
}

void TerminalProductActivationCoordinator::refresh_measured_crossrate_action()
{
    if(measuredPreparing_
       ||measuredWorkerRunning_.load(std::memory_order_acquire)
       ||!intent_||state_->bypassed()
       ||session_->monitoring_transaction_active()
       ||session_->status()!=QStringLiteral("MEASURED_NONCONFORMING_READY"))
        return;
    const auto stage=session_->capture_measured_master_for_device_conversion();
    if(!stage||stage->rendered.manual_owner!=intent_->manualOwner
       ||stage->original_manual_realization_id!=intent_->manualId)return;
    // A failed/cancelled DEVICE FIR must never strand a retained measured
    // M17 MASTER behind a disabled button. Each next explicit user click
    // still receives a fresh Windows physical endpoint/RAM preflight.
    view_->setAudioStatus(QStringLiteral(
        "NON-CONFORMING · MEASURED PCM RETAINED"));
    view_->setNonconformingDirectPreviewAvailable(
        session_->current_measured_matches_device_for_preview());
    view_->setNonconformingDevicePreviewAvailable(
        static_cast<bool>(measuredProbe_&&measuredRecheck_));
}

void TerminalProductActivationCoordinator::set_master_rate_state(
    MasterOutputRateState& masterState)
{
    if(masterRateState_)return; // no live multi-owner retarget.
    masterRateState_=&masterState;
    connect(&masterState,&MasterOutputRateState::changed,
        this,&TerminalProductActivationCoordinator::master_rate_changed);
}

void TerminalProductActivationCoordinator::master_rate_changed()
{
    if(measuredPreparing_)cancel_measured_crossrate_audition();
    if(transitioning_)return;
    view_->clearVerificationTrim();
    if(session_->monitoring_transaction_active()) {
        view_->setAudioStatus(session_->measured_device_active()
            ?QStringLiteral("ACTIVE · NON-CONFORMING AUDITION")
            :QStringLiteral("MONITOR · PRIVATE PCM (NOT LIVE)"));
        return;
    }
    // A changed (even A->B->A) Project rate is a NEW render epoch.
    // Revoke only the exact current M17 candidate; never auto-prepare.
    if(intent_)deactivate(false);
    else if(session_->comparison_cached())
        static_cast<void>(session_->cancel());
    if(!state_->bypassed()) {
        view_->clearAudioError();
        view_->setAudioStatus(QStringLiteral("NEEDS PREPARATION"));
    }
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
    if(measuredPreparing_)cancel_measured_crossrate_audition();
    if(transitioning_)return;
    view_->clearVerificationTrim();
    renderHeartbeat_.stop();
    corePhaseObserved_=false;
    view_->clearPreparationProgress();
    activeWholeEtaUs_.reset();
    activeDspEtaUs_.reset();
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
    readyToPreview_=false;
    transitioning_=false;
}

void TerminalProductActivationCoordinator::begin_activation()
{
    if(transitioning_||intent_||state_->bypassed())return;
    if(session_->monitoring_transaction_active()) {
        view_->setAudioStatus(session_->measured_device_active()
            ?QStringLiteral("ACTIVE · NON-CONFORMING AUDITION")
            :QStringLiteral("MONITOR · PRIVATE PCM (NOT LIVE)"));
        return;
    }
    if(manualPreviewInProgress_) {
        view_->setAudioStatus(QStringLiteral("NEEDS PREPARATION"));
        return;
    }
    readyToPreview_=false;
    view_->clearVerificationTrim();
    // M17 belongs to PROCESSED. A normal PREPARED audition is a UI
    // selection, not a DSP failure: select current accepted PROCESSED
    // before capturing the exact MANUAL owner. Do not fabricate it.
    if(selector_->active_target()==AuditionTarget::PREPARED) {
        if(!selector_->processed_available()) {
            view_->clearAudioError();
            view_->setAudioStatus(QStringLiteral(
                "SELECT PROCESSED · No valid processed audio"));
            return;
        }
        transitioning_=true; // suppress recursive selector-change callbacks
        const auto selected=selector_->switch_to(AuditionTarget::PROCESSED);
        transitioning_=false;
        if(!selected) {
            view_->reportAudioError(QStringLiteral(
                "Unable to select PROCESSED: %1")
                .arg(QString::fromStdString(selected.error()->message())));
            view_->setAudioStatus(QStringLiteral("SELECT PROCESSED"));
            return;
        }
    }
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
    activeWholeEtaUs_.reset();
    activeDspEtaUs_.reset();
    if(masterRateState_) {
        if(hostTotalEtaProvider_)
            activeWholeEtaUs_=hostTotalEtaProvider_(
                masterRateState_->master_rate_hz());
        if(provisionalDspEtaProvider_)
            activeDspEtaUs_=provisionalDspEtaProvider_(
                masterRateState_->master_rate_hz());
    }
    transitioning_=true;
    view_->clearAudioError();
    // Keep the DSP-chain summary readable and show time spent waiting for
    // the exact accepted terminal output. During this interval the actual
    // audio is still the original MANUAL source, never the M17 candidate.
    view_->setAudioStatus(QStringLiteral("Preparing audio"));
    view_->setPreparationProgress(QStringLiteral("QUEUED"),0.0,false);
    corePhaseObserved_=false;
    srcPhaseObserved_=false;
    m16PhaseObserved_=false;
    observedCoreMs_=0;
    observedPrimaryM16Ms_=0;
    lastObservedM16Phase_=render::TerminalRenderProgressPhase::IDLE;
    renderElapsed_.start();
    renderHeartbeat_.start();
    // Keep current MANUAL audio and its playhead running during offline M17
    // output-rate preparation. Only the accepted final handoff may stop it.
    core::Status submitted=core::Status::failure(core::Error{
        core::ErrorCode::InvalidState,"M17_MASTER_RATE_NOT_INITIALIZED"});
    if(masterRateState_) {
        if(masterRateState_->has_supported_master_rate()) {
            const auto selected=core::SampleRate::create(
                masterRateState_->master_rate_hz());
            if(selected)
                submitted=session_->request_private(*selected.value(),true);
            else
                submitted=core::Status::failure(*selected.error());
        }
    } else {
        // Legacy isolated integration fixtures; product always binds
        // the Project MASTER authority at app startup.
        submitted=session_->request_private_for_device(true);
    }
    transitioning_=false;
    if(!submitted)
        fail_closed(QStringLiteral("M17 audio preparation was refused: %1")
                        .arg(QString::fromStdString(submitted.error()->message())));
}

void TerminalProductActivationCoordinator::prepare_requested()
{
    // No user Play / Active operation may prepare the whole master.
    if(!state_->bypassed() && !intent_)begin_activation();
}

void TerminalProductActivationCoordinator::cancel_requested()
{
    if(measuredPreparing_) {
        cancel_measured_crossrate_audition();
        view_->setAudioStatus(QStringLiteral(
            "NON-CONFORMING · DEVICE CONVERSION CANCELLED"));
        view_->clearPreparationProgress();
        return;
    }
    if(transitioning_)return;
    if(intent_)deactivate(false);
    else if(!state_->bypassed()) {
        readyToPreview_=false;
        view_->clearAudioError();
        view_->setAudioStatus(QStringLiteral("NEEDS PREPARATION"));
    }
}

void TerminalProductActivationCoordinator::preview_requested()
{
    // Master Audition owns a different anonymous DEVICE PCM while active.
    // A late M17 Preview click must not try to replace this sink, trigger
    // fail_closed or discard the immutable MASTER already being monitored.
    if(session_->monitoring_transaction_active())return;
    if(readyToPreview_ && intent_ && !state_->bypassed()
       &&session_->status()==QStringLiteral("READY_TO_INSTALL"))
        commit_ready_output();
}

void TerminalProductActivationCoordinator::preview_nonconforming_requested()
{
    if(!intent_ || state_->bypassed()
       ||session_->status()!=QStringLiteral("MEASURED_NONCONFORMING_READY"))
        return;
    // Direct-rate preview preserves the original, independent legacy
    // publication path. Cross-rate preview MUST instead use a distinct,
    // anonymous Windows DEVICE PCM and never claim M16 VERIFIED/LIVE.
    if(session_->current_measured_matches_device_for_preview()) {
        view_->clearAudioError();
        commit_ready_output(true);
        return;
    }
    begin_measured_crossrate_audition();
}


void TerminalProductActivationCoordinator::begin_measured_crossrate_audition()
{
    const auto report=[this](const core::Error& error) {
        view_->reportAudioError(QString::fromStdString(error.message()));
    };
    if(QThread::currentThread()!=thread()||transitioning_
       ||!intent_||state_->bypassed()
       ||session_->monitoring_transaction_active()
       ||session_->status()!=QStringLiteral("MEASURED_NONCONFORMING_READY")
       ||!measuredProbe_||!measuredRecheck_) {
        view_->reportAudioError(QStringLiteral(
            "M17_NONCONFORMING_DEVICE_PREVIEW_EXPLICIT_PORTS_UNAVAILABLE"));
        return;
    }
    if(measuredPreparing_
       ||measuredWorkerRunning_.load(std::memory_order_acquire)) {
        view_->reportAudioError(QStringLiteral(
            "M17_NONCONFORMING_DEVICE_PREVIEW_PREVIOUS_WORKER_ACTIVE"));
        return;
    }
    const auto captured=session_->capture_measured_master_for_device_conversion();
    if(!captured) {
        view_->reportAudioError(QStringLiteral(
            "M17_NONCONFORMING_DEVICE_PREVIEW_MEASURED_MASTER_STALE"));
        return;
    }
    const auto immutable=captured->rendered.terminal.terminal
        .compensated_pcm.view();
    const auto planned=measuredProbe_(immutable);
    if(!planned) {report(*planned.error());return;}
    if(planned.value()->opaque_endpoint_id.empty()
       ||session_->capture_measured_master_for_device_conversion()!=captured) {
        view_->reportAudioError(QStringLiteral(
            "M17_NONCONFORMING_DEVICE_PREVIEW_ENDPOINT_OR_STAGE_REVOKED"));
        return;
    }
    const auto block=core::FrameCount::create(
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    if(!block) {report(*block.error());return;}
    if(measuredWorker_.joinable())measuredWorker_.join();
    std::shared_ptr<std::atomic_bool> token;
    try {
        token=std::make_shared<std::atomic_bool>(false);
    } catch(const std::bad_alloc&) {
        view_->reportAudioError(QStringLiteral(
            "M17_NONCONFORMING_DEVICE_CANCEL_TOKEN_OUT_OF_MEMORY"));
        refresh_measured_crossrate_action();
        return;
    }
    const auto job=++measuredSerial_;
    measuredCancel_=token;
    pendingMeasured_=captured;
    measuredPreparing_=true;
    view_->setNonconformingDevicePreparing(true);
    measuredCompletedFrames_.store(0,std::memory_order_release);
    measuredTotalFrames_.store(0,std::memory_order_release);
    measuredProgressTimer_.start();
    view_->clearAudioError();
    view_->setNonconformingDevicePreviewAvailable(false);
    view_->setAudioStatus(QStringLiteral(
        "NON-CONFORMING · PREPARING SEPARATE DEVICE PCM"));
    try {
        using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
        using Result=core::Result<Owned>;
        auto outcome=std::make_shared<Result>(Result::failure(
            core::Error{core::ErrorCode::IoFailure,
                "M17_NONCONFORMING_DEVICE_SRC_ALLOCATION_FAILED"}));
        measuredWorkerRunning_.store(true,std::memory_order_release);
        measuredWorker_=std::jthread(
            [this,captured,plan=*planned.value(),block=*block.value(),
             token,job,outcome](std::stop_token stop) {
            using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
            using Result=core::Result<Owned>;
            *outcome=([&]()->Result {
                try {
                    const auto master=captured->rendered.terminal.terminal
                        .compensated_pcm.view();
                    auto rendered=audio::materialize_monitoring_pcm_candidate(
                        master,plan.device_rate,block,{},
                        [stop,token] {
                            return stop.stop_requested()
                                ||token->load(std::memory_order_acquire);
                        },
                        [this](std::int64_t done,std::int64_t total) {
                            measuredTotalFrames_.store(
                                total,std::memory_order_release);
                            measuredCompletedFrames_.store(
                                done,std::memory_order_release);
                        });
                    if(!rendered)return Result::failure(*rendered.error());
                    auto device=std::move(*rendered.value());
                    device.windows_prepared_endpoint_id=plan.opaque_endpoint_id;
                    return Result::success(
                        std::make_shared<const audio::MonitoringPcmCandidate>(
                            std::move(device)));
                } catch(const std::bad_alloc&) {
                    return Result::failure(core::Error{
                        core::ErrorCode::IoFailure,
                        "M17_NONCONFORMING_DEVICE_PCM_OUT_OF_MEMORY"});
                } catch(const std::exception&) {
                    return Result::failure(core::Error{
                        core::ErrorCode::InvalidState,
                        "M17_NONCONFORMING_DEVICE_SRC_WORKER_EXCEPTION"});
                } catch(...) {
                    return Result::failure(core::Error{
                        core::ErrorCode::InvalidState,
                        "M17_NONCONFORMING_DEVICE_SRC_UNKNOWN_EXCEPTION"});
                }
            }());
            measuredWorkerRunning_.store(false,std::memory_order_release);
            QMetaObject::invokeMethod(this,
                [this,captured,plan,token,job,outcome] {
                if(job!=measuredSerial_||!measuredPreparing_
                   ||token->load(std::memory_order_acquire)) {
                    refresh_measured_crossrate_action();
                    return;
                }
                measuredPreparing_=false;
                view_->setNonconformingDevicePreparing(false);
                pendingMeasured_.reset();
                measuredProgressTimer_.stop();
                view_->clearPreparationProgress();
                if(!*outcome) {
                    view_->reportAudioError(
                        QString::fromStdString(outcome->error()->message()));
                    refresh_measured_crossrate_action();
                    return;
                }
                if(!intent_||state_->bypassed()
                   ||session_->status()!=QStringLiteral(
                       "MEASURED_NONCONFORMING_READY")
                   ||session_->capture_measured_master_for_device_conversion()
                        !=captured) {
                    view_->reportAudioError(QStringLiteral(
                        "M17_NONCONFORMING_SOURCE_REVOKED_BEFORE_DEVICE"));
                    return;
                }
                const auto master=captured->rendered.terminal.terminal
                    .compensated_pcm.view();
                const auto checked=measuredRecheck_(plan,master);
                if(!checked) {
                    view_->reportAudioError(
                        QString::fromStdString(checked.error()->message()));
                    refresh_measured_crossrate_action();
                    return;
                }
                const MeasuredDeviceEndpointObservation latest{
                    plan.opaque_endpoint_id,plan.device_rate};
                const auto admitted=session_->admit_measured_device_conversion(
                    captured,*outcome->value(),true,latest);
                if(!admitted) {
                    view_->reportAudioError(
                        QString::fromStdString(admitted.error()->message()));
                    refresh_measured_crossrate_action();
                    return;
                }
                const auto installed=session_->install_measured_device_audition(
                    *admitted.value(),latest,true);
                if(!installed) {
                    view_->reportAudioError(
                        QString::fromStdString(installed.error()->message()));
                    // A partial Windows install may own an exact recovery
                    // ticket; NEVER cancel it or replace a successor sink.
                    refresh_measured_crossrate_action();
                    return;
                }
                readyToPreview_=false;
                view_->clearAudioError();
                view_->setAudioStatus(QStringLiteral(
                    "ACTIVE · NON-CONFORMING AUDITION"));
                // The shared Master Audition UI now owns Play/Pause/Stop,
                // seek and explicit Return; no terminal RealizationId exists.
            },Qt::QueuedConnection);
        });
    } catch(const std::exception&) {
        measuredWorkerRunning_.store(false,std::memory_order_release);
        cancel_measured_crossrate_audition();
        view_->clearPreparationProgress();
        view_->reportAudioError(QStringLiteral(
            "M17_NONCONFORMING_DEVICE_WORKER_START_REFUSED"));
        refresh_measured_crossrate_action();
    }
}

void TerminalProductActivationCoordinator::update_render_elapsed()
{
    if(!intent_||!renderElapsed_.isValid()||deviceActive_
       ||state_->bypassed()) {
        renderHeartbeat_.stop();
        return;
    }
    const auto progress=session_->render_progress();
    using Phase=render::TerminalRenderProgressPhase;
    const auto elapsed=renderElapsed_.elapsed();
    const auto fmt=[](std::int64_t ms) {
        const auto s=std::max<std::int64_t>(0,(ms+999)/1000);
        return QStringLiteral("%1m %2s")
            .arg(s/60).arg(s%60,2,10,QLatin1Char('0'));
    };
    const bool fractionKnown=progress.total_frames>0
        &&progress.processed_frames>=0
        &&progress.processed_frames<=progress.total_frames;
    const double f=fractionKnown
        ?static_cast<double>(progress.processed_frames)
            /static_cast<double>(progress.total_frames):0.0;
    QString stage=QStringLiteral("Queued");
    double fraction=0.0;
    bool determinate=false;
    std::optional<M17PrepareLivePhase> livePhase;
    std::int64_t livePhaseMs=0;

    switch(progress.phase) {
    case Phase::SRC:
        stage=QStringLiteral("Final SRC");
        if(!srcPhaseObserved_) {
            srcPhaseObserved_=true;
            srcPhaseElapsed_.start();
        }
        if(fractionKnown) {
            fraction=std::min(0.99,f);
            determinate=true;
        }
        livePhase=M17PrepareLivePhase::FINAL_SRC;
        livePhaseMs=srcPhaseElapsed_.elapsed();
        break;
    case Phase::M17_CORE:
        stage=QStringLiteral("M17 DSP");
        if(!corePhaseObserved_) {
            corePhaseObserved_=true;
            corePhaseElapsed_.start();
        }
        if(fractionKnown) {
            fraction=std::min(0.99,f);
            determinate=true;
        }
        livePhase=M17PrepareLivePhase::M17_CORE;
        livePhaseMs=corePhaseElapsed_.elapsed();
        break;
    case Phase::M16_PRIMARY:
    case Phase::M16_SECONDARY:
        stage=progress.phase==Phase::M16_PRIMARY
            ?QStringLiteral("M16 primary TP verification")
            :QStringLiteral("M16 final TP verification");
        if(progress.phase!=lastObservedM16Phase_) {
            if(lastObservedM16Phase_==Phase::M16_PRIMARY
               &&m16PhaseElapsed_.isValid())
                observedPrimaryM16Ms_=m16PhaseElapsed_.elapsed();
            lastObservedM16Phase_=progress.phase;
            m16PhaseElapsed_.start();
            m16PhaseObserved_=true;
            if(progress.phase==Phase::M16_PRIMARY)
                observedCoreMs_=corePhaseObserved_
                    ?corePhaseElapsed_.elapsed():0;
        }
        // M16 has no trusted frame progress denominator. Do not manufacture
        // a percentage. Use only elapsed phase time and prior measured
        // M17/primary M16 duration in a provisional whole-Prepare envelope.
        livePhase=progress.phase==Phase::M16_PRIMARY
            ?M17PrepareLivePhase::M16_PRIMARY
            :M17PrepareLivePhase::M16_SECONDARY;
        livePhaseMs=m16PhaseElapsed_.elapsed();
        break;
    case Phase::VERIFICATION_TRIM:
        stage=QStringLiteral("Verification trim");
        if(lastObservedM16Phase_!=Phase::VERIFICATION_TRIM) {
            // A real verification trim can require an extra M16 pass.
            // Snapshot the just-finished PRIMARY before replacing its
            // elapsed timer; this is observation, not another Prepare.
            if(lastObservedM16Phase_==Phase::M16_PRIMARY
               &&m16PhaseElapsed_.isValid())
                observedPrimaryM16Ms_=m16PhaseElapsed_.elapsed();
            lastObservedM16Phase_=Phase::VERIFICATION_TRIM;
            verificationTrimElapsed_.start();
        }
        livePhase=M17PrepareLivePhase::VERIFICATION_TRIM;
        livePhaseMs=verificationTrimElapsed_.elapsed();
        break;
    case Phase::FINISHING:
        stage=QStringLiteral("Finishing");
        livePhase=M17PrepareLivePhase::FINISHING;
        break;
    case Phase::IDLE:break;
    }

    // FIRST run is fully eligible: current progress and stage transitions
    // form the provisional ETA immediately when they provide evidence.
    // Previous accepted Prepares (3+ samples) are OPTIONAL context only,
    // never a gate for this calculation and never another calibration.
    const auto remaining=livePhase
        ?m17_provisional_live_remaining(M17PrepareLiveEvidence{
            *livePhase,livePhaseMs,f,fractionKnown,
            observedCoreMs_,observedPrimaryM16Ms_,activeDspEtaUs_})
        :std::nullopt;
    QString eta=QStringLiteral("Elapsed %1").arg(fmt(elapsed));
    if(remaining) {
        const auto low=remaining->low_ms;
        const auto high=std::max(low,remaining->high_ms);
        const auto centre=low+(high-low)/2;
        eta+=QStringLiteral(
            " · Provisional remaining ~%1 (%2–%3)")
            .arg(fmt(centre),fmt(low),fmt(high));
    } else if(activeWholeEtaUs_
              &&elapsed<activeWholeEtaUs_->second/1000
              &&progress.phase!=Phase::VERIFICATION_TRIM) {
        const auto low=std::max<std::int64_t>(
            0,activeWholeEtaUs_->first/1000-elapsed);
        const auto high=std::max<std::int64_t>(
            low,activeWholeEtaUs_->second/1000-elapsed);
        eta+=QStringLiteral(
            " · Historical remaining ~%1–%2").arg(fmt(low),fmt(high));
    } else {
        eta+=QStringLiteral(" · Measuring current phase");
    }
    // Progress fraction is CURRENT STAGE only. The remaining-time range
    // refers to the entire ongoing Prepare, and may legitimately WIDEN
    // if an actual trim/overrun reveals more work than previously observed.
    // It never changes the output PCM, Ceiling, DSP, Q0 or LIVE authority.
    view_->setPreparationProgress(stage,fraction,determinate,eta);
    view_->setAudioStatus(
        QStringLiteral("Preparing · %1s").arg(elapsed/1000));
}

void TerminalProductActivationCoordinator::commit_ready_output(bool nonconforming)
{
    if(transitioning_||!intent_||state_->bypassed()
       ||(nonconforming
          ? session_->status()!=QStringLiteral("MEASURED_NONCONFORMING_READY")
          : !readyToPreview_))return;
    readyToPreview_=false;
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
        // Diagnostic only: preserve ALL frozen fail-closed ownership checks.
        // Distinguish a changed MANUAL owner from a playhead/loop race
        // after a long Prepare, without logging user PCM or full job logs.
        QString category;
        if(!before || !source || !after)
            category=QStringLiteral("M17_HANDOFF_SNAPSHOT_UNAVAILABLE");
        else if(before.value()->state!=after.value()->state
                ||before.value()->audibleRealization
                    !=after.value()->audibleRealization
                ||before.value()->seekSerial!=after.value()->seekSerial
                ||before.value()->traversalSerial!=after.value()->traversalSerial
                ||before.value()->loopWrapCount!=after.value()->loopWrapCount)
            category=QStringLiteral("M17_HANDOFF_TRANSPORT_RACE");
        else if(before.value()->audibleRealization.phase
                    !=core::AudibleHandoffPhase::NEW
                ||before.value()->audibleRealization.realizationId
                    !=std::optional{intent_->manualId}
                ||before.value()->duration
                    !=std::optional{intent_->manualOwner->view().frame_count()})
            category=QStringLiteral("M17_HANDOFF_MANUAL_OWNER_CHANGED");
        else if(before.value()->loop!=after.value()->loop
                ||source.value()->loop!=intent_->sourceLoop)
            category=QStringLiteral("M17_HANDOFF_LOOP_CHANGED");
        else if(source.value()->state!=before.value()->state
                ||source.value()->duration
                    !=intent_->manualOwner->view().frame_count())
            category=QStringLiteral("M17_HANDOFF_SOURCE_CLOCK_MISMATCH");
        else
            category=QStringLiteral("M17_HANDOFF_SOURCE_POSITION_INVALID");
        fail_closed(QStringLiteral("%1: manual audition ownership could not be verified.")
            .arg(category));
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
    const auto installed=nonconforming
        ?session_->install_measured_private()
        :session_->install_ready_private();
    transitioning_=false;
    if(!installed) {
        fail_closed(QStringLiteral("M17 accepted render install failed: %1")
                        .arg(QString::fromStdString(installed.error()->message())));
        return;
    }
    finish_activation(nonconforming);
}

void TerminalProductActivationCoordinator::finish_activation(bool nonconforming)
{
    if(transitioning_||!intent_||state_->bypassed())return;
    renderHeartbeat_.stop();
    if(nonconforming) {
        // This is a genuinely distinct playback product state. Do NOT call
        // the VERIFIED-owned identity observer or assert an M16 certificate.
        const auto playback=transport_->playback_snapshot();
        const auto source=transport_->source_display_snapshot();
        const auto playbackId=session_->private_id();
        if(!playback || !source || !playbackId
           ||session_->status()!=QStringLiteral("NONCONFORMING_INSTALLED")
           ||playback.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::NEW
           ||playback.value()->audibleRealization.realizationId
               !=playbackId
           ||playback.value()->state!=core::PlaybackState::STOPPED
           ||source.value()->loop!=intent_->sourceLoop
           ||source.value()->duration!=intent_->manualOwner->view().frame_count()
           ||source.value()->position.value()<0
           ||(source.value()->position.value()>intent_->sourceCue.value()
               ?source.value()->position.value()-intent_->sourceCue.value()
               :intent_->sourceCue.value()-source.value()->position.value())>1) {
            fail_closed(QStringLiteral("M17 nonconforming playback lost exact output ownership."));
            return;
        }
        transitioning_=true;
        deviceActive_=true;
        resume_previous_state();
        const auto after=transport_->playback_snapshot();
        if(!after || after.value()->audibleRealization.realizationId
              !=playbackId
           ||(intent_->previousPlaybackState==core::PlaybackState::PLAYING
               &&after.value()->state!=core::PlaybackState::PLAYING)
           ||(intent_->previousPlaybackState==core::PlaybackState::PAUSED
               &&after.value()->state!=core::PlaybackState::PAUSED)) {
            transitioning_=false;
            fail_closed(QStringLiteral("M17 nonconforming playback did not preserve transport."));
            return;
        }
        view_->setAudioStatus(QStringLiteral("ACTIVE · NON-CONFORMING AUDITION"));
        readyToPreview_=false;
        handoffStarted_=false;
        transitioning_=false;
        return;
    }
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
    readyToPreview_=false;
    handoffStarted_=false;
    transitioning_=false;
}

bool TerminalProductActivationCoordinator::can_handoff_installed_to_monitor() const
{
    return !transitioning_ && deviceActive_ && intent_
        && !state_->bypassed()
        && session_->status()==QStringLiteral("PRIVATE_INSTALLED")
        && static_cast<bool>(session_->private_installed_output_identity());
}

core::Status TerminalProductActivationCoordinator::handoff_installed_to_monitor()
{
    if(!can_handoff_installed_to_monitor())
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M17_MONITOR_HANDOFF_REQUIRES_EXACT_INSTALLED_PREVIEW"});
    const auto identity=session_->private_installed_output_identity();
    if(!identity)return core::Status::failure(*identity.error());
    const auto original=identity.value()->owner;
    const auto rate=original->rendered.output_rate;
    // The existing A/B retirement transaction has already been tested
    // for current SOURCE cue/Loop and exact installed RealizationId.
    // It restores the MANUAL device before any unrelated monitor allocation.
    deactivate(true);
    if(!session_->comparison_owners_current()
       ||session_->status()!=QStringLiteral("COMPARISON_VERIFIED")
       ||session_->borrow_verified_m17_master_for_device(rate)!=original)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M17_MONITOR_HANDOFF_COULD_NOT_PROVE_RETAINED_MASTER"});
    // Re-arm the ORIGINAL verified M17 READY ticket while MANUAL is its
    // exact audible owner. If Windows RAM preflight, DEVICE-rate SRC or
    // monitor sink creation then fails, ordinary Preview Prepared remains
    // available: the user must never pay for another full Prepare.
    const auto resumed=session_->resume_comparison();
    if(!resumed)return resumed;
    if(session_->status()!=QStringLiteral("READY_TO_INSTALL")
       ||session_->borrow_verified_m17_master_for_device(rate)!=original)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M17_MONITOR_HANDOFF_REARM_LOST_VERIFIED_MASTER"});
    restore_ready_after_monitor_return();
    return core::Status::success();
}

void TerminalProductActivationCoordinator::restore_ready_after_monitor_return()
{
    if(transitioning_||intent_||deviceActive_||state_->bypassed()
       ||session_->monitoring_transaction_active()
       ||session_->status()!=QStringLiteral("READY_TO_INSTALL"))
        return;
    // The exact MANUAL was restored by the private session, not fabricated
    // from monitoring PCM. Re-arm M17's UI WITHOUT any new whole-track job.
    auto captured=selector_->capture_terminal_activation_intent();
    if(!captured)return;
    const auto rate=session_->verified_master_rate_for_monitoring();
    if(!rate || (masterRateState_
        &&masterRateState_->master_rate_hz()!=rate->value()))
        return;
    intent_=*captured.value();
    readyToPreview_=session_->current_master_matches_device_for_preview();
    view_->setPreparationProgress(
        QStringLiteral("Verified MASTER ready"),1.0,true);
    view_->setAudioStatus(readyToPreview_
        ?QStringLiteral("READY · PREVIEW AVAILABLE")
        :QStringLiteral("MASTER VERIFIED · LISTEN TO MASTER"));
}

void TerminalProductActivationCoordinator::deactivate(bool keepActiveIntent)
{
    if(measuredPreparing_)cancel_measured_crossrate_audition();
    if(transitioning_)return;
    view_->clearVerificationTrim();
    renderHeartbeat_.stop();
    activeWholeEtaUs_.reset();
    activeDspEtaUs_.reset();
    transitioning_=true;
    const bool hadActiveOutput=deviceActive_;
    if(intent_ && deviceActive_) {
        const auto installed=session_->private_installed_output_identity();
        if(installed) {
            intent_->sourceCue=installed.value()->source.position;
            intent_->sourceLoop=installed.value()->source.loop;
            intent_->previousPlaybackState=installed.value()->playback.state;
        } else if(session_->status()==QStringLiteral("NONCONFORMING_INSTALLED")) {
            const auto playback=transport_->playback_snapshot();
            const auto source=transport_->source_display_snapshot();
            if(playback && source && session_->private_id()
               &&playback.value()->audibleRealization.realizationId
                    ==session_->private_id()) {
                intent_->sourceCue=source.value()->position;
                intent_->sourceLoop=source.value()->loop;
                intent_->previousPlaybackState=source.value()->state;
            }
        }
    }
    // A/B retires the private device lease but RETAINS the immutable
    // verified or measured-nonconforming PCM for explicit re-audition.
    // Cancel, source/parameter changes and failure still invalidate it.
    auto retired=keepActiveIntent
        ?session_->suspend_for_comparison()
        :session_->cancel();
    if(!retired && keepActiveIntent)
        retired=session_->cancel(); // No stale comparison state on refusal.
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
    readyToPreview_=false;
    corePhaseObserved_=false;
    view_->clearPreparationProgress();
    view_->setAudioStatus(state_->bypassed()
        ? QStringLiteral("BYPASS · MANUAL PROCESSED")
        : QStringLiteral("NEEDS PREPARATION"));
    transitioning_=false;
}

void TerminalProductActivationCoordinator::limiter_accepted()
{
    if(measuredPreparing_)cancel_measured_crossrate_audition();
    if(transitioning_)return;
    const bool changedCeiling=state_->parameter_revision()!=lastParameterRevision_;
    lastParameterRevision_=state_->parameter_revision();
    // Never route ordinary ON/BYPASS into the separate exact MONITOR lease.
    if(session_->monitoring_transaction_active()) {
        view_->setAudioStatus(session_->measured_device_active()
            ?QStringLiteral("ACTIVE · NON-CONFORMING AUDITION")
            :QStringLiteral("MONITOR · PRIVATE PCM (NOT LIVE)"));
        return;
    }
    if(changedCeiling) {
        view_->clearVerificationTrim();
        // This includes Ceiling changes made while BYPASS is selected.
        // No A->B->A parameter revision may recover an obsolete result.
        if(intent_)deactivate(false);
        else if(session_->comparison_cached())
            static_cast<void>(session_->cancel());
        waitingForManualPreview_=manualPreviewInProgress_;
        view_->clearAudioError();
        view_->setAudioStatus(state_->bypassed()
            ? QStringLiteral("BYPASS · MANUAL PROCESSED")
            : QStringLiteral("NEEDS PREPARATION"));
        return;
    }
    if(state_->bypassed()) {
        waitingForManualPreview_=false;
        if(intent_)deactivate(true);
        else view_->setAudioStatus(QStringLiteral("BYPASS · MANUAL PROCESSED"));
        return;
    }
    if(session_->comparison_cached()) {
        // ON does not re-render or auto-play M17. Re-establish the exact
        // currently audible MANUAL owner; the user must click Preview
        // Prepared / Preview Anyway, respectively, to hear cached PCM.
        auto captured=selector_->capture_terminal_activation_intent();
        if(captured && session_->comparison_owners_current()) {
            intent_=*captured.value();
            auto resumed=session_->resume_comparison();
            if(resumed)return; // private_session_changed publishes READY.
            intent_.reset();
        }
        static_cast<void>(session_->cancel());
    }
    if(intent_)deactivate(false);
    waitingForManualPreview_=manualPreviewInProgress_;
    view_->clearAudioError();
    view_->setAudioStatus(QStringLiteral("NEEDS PREPARATION"));
}

void TerminalProductActivationCoordinator::manual_preview_changed(
    const QString& previewStatus)
{
    if(previewStatus==QStringLiteral("RENDERING")) {
        manualPreviewInProgress_=true;
        view_->clearVerificationTrim();
        // A fresh MANUAL render invalidates cached A/B even while bypassed.
        // Never keep a previous PCM after changing Input Gain or DSP chain.
        if(state_->bypassed()) {
            if(session_->comparison_cached())
                static_cast<void>(session_->cancel());
            return;
        }
        if(transitioning_)return;
        // Retire installed/READY terminal BEFORE upstream publication.
        if(intent_)deactivate(false);
        if(state_->bypassed())return;
        waitingForManualPreview_=true;
        view_->clearAudioError();
        view_->setAudioStatus(QStringLiteral("NEEDS PREPARATION"));
    } else if(previewStatus==QStringLiteral("READY")
              ||previewStatus==QStringLiteral("ERROR")
              ||previewStatus==QStringLiteral("IDLE")) {
        manualPreviewInProgress_=false;
        waitingForManualPreview_=false;
        // Upstream completion never triggers Prepare.
        if(!state_->bypassed()&&!intent_)
            view_->setAudioStatus(QStringLiteral("NEEDS PREPARATION"));
    }
}

void TerminalProductActivationCoordinator::loop_context_changed()
{
    // Loop reconciliation may synchronously emit AuditionRegion::changed.
    if(session_->monitoring_transaction_active())return;
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
        // A valid loop is not DSP input. Keep the ONE running/completed
        // offline full-track M17 Prepare; change only deferred transport
        // metadata. Refuse stale/alien SOURCE ownership, never invent PCM.
        if(session_->adopt_pending_source_loop_without_rerender())
            intent_->sourceLoop=source.value()->loop;
        else
            fail_closed(QStringLiteral(
                "M17 SOURCE loop changed but exact manual ownership was lost."));
    }
}

void TerminalProductActivationCoordinator::private_session_changed()
{
    if(measuredPreparing_
       &&session_->capture_measured_master_for_device_conversion()
            !=pendingMeasured_)
        cancel_measured_crossrate_audition();
    // A successfully installed Master Audition owns DEVICE exclusively.
    // Explicitly retire the stale Preview Prepared button presentation,
    // but retain the exact M17 MASTER and original intent for its return.
    if(session_->monitoring_transaction_active()) {
        readyToPreview_=false;
        renderHeartbeat_.stop();
        // A SOURCE recovery ticket is NOT an installed or audible
        // monitor. A generic "MONITOR" label would lie about Windows
        // output and hide why the only legal action is an explicit retry.
        const auto active=session_->status();
        if(session_->measured_device_active()) {
            const bool recovery=active==QStringLiteral(
                "MEASURED_DEVICE_RECOVERY_REQUIRED");
            const bool sourceRetry=active==QStringLiteral(
                "MEASURED_DEVICE_SOURCE_RECOVERY_REQUIRED");
            view_->setAudioStatus(sourceRetry
                ?QStringLiteral("NON-CONFORMING · RETRY SOURCE RETURN")
                :recovery
                    ?QStringLiteral("NON-CONFORMING · DEVICE RECOVERY REQUIRED")
                    :QStringLiteral("ACTIVE · NON-CONFORMING AUDITION"));
        } else {
            view_->setAudioStatus(active==QStringLiteral(
                "MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED")
                ?QStringLiteral(
                    "DEVICE UNAVAILABLE · MASTER RETAINED · RETRY SOURCE")
                :QStringLiteral("MONITOR · PRIVATE PCM (NOT LIVE)"));
        }
        return;
    }
    if(transitioning_||!intent_)return;
    const auto status=session_->status();
    if(status==QStringLiteral("READY_TO_INSTALL")) {
        // Only the current exact verified M17 MASTER can supply an M16
        // full-resource pre/post trim witness. This is NOT derived from
        // Ceiling, a historic PRE-LIMITER value or private DEVICE playback.
        const auto masterRate=session_->verified_master_rate_for_monitoring();
        const auto stage=masterRate
            ?session_->borrow_verified_m17_master_for_device(*masterRate)
            :nullptr;
        if(stage &&stage->rendered.terminal.checked.final_check) {
            const auto& check=*stage->rendered.terminal.checked.final_check;
            view_->setVerifiedTrimObservation(
                check.global_trim_gain,check.trim_used,
                check.before_linear_peak,check.after_linear_peak);
        } else {
            view_->clearVerificationTrim(); // never publish stale owner
        }
        // The authoritative MASTER rate may be 192k on a 48k DAC. Never
        // offer the legacy direct terminal install for an incompatible
        // DEVICE: use Master Audition's independently preflighted SRC.
        const bool direct=session_->current_master_matches_device_for_preview();
        readyToPreview_=direct;
        renderHeartbeat_.stop();
        view_->setPreparationProgress(
            QStringLiteral("Verified MASTER ready"),1.0,true);
        view_->setAudioStatus(direct
            ?QStringLiteral("READY · PREVIEW AVAILABLE")
            :QStringLiteral("MASTER VERIFIED · LISTEN TO MASTER"));
    } else if(status==QStringLiteral("MEASURED_NONCONFORMING_READY")) {
        view_->clearVerificationTrim(); // measured miss is never a verified trim
        readyToPreview_=false;
        renderHeartbeat_.stop();
        auto measured=session_->measured_candidate();
        if(!measured) {
            fail_closed(QStringLiteral("M17 measured result lost exact source ownership."));
            return;
        }
        const auto& evidence=measured->rendered.terminal;
        const double peak=20.0*std::log10(evidence.measured_linear_peak);
        // Honest, current and non-verifiable-as-success: true measured peak,
        // no automatic playback and no verified READY declaration.
        view_->setNonconformingObservation(
            evidence.measured_linear_peak,evidence.ceiling_linear,
            evidence.requested_ceiling_db_tp);
        view_->setPreparationProgress(
            QStringLiteral("Measured PCM retained · Ceiling not met"),0.99,false);
        view_->setAudioStatus(
            QStringLiteral("NON-CONFORMING · %1 dBTP (target %2)")
                .arg(QString::number(peak,'f',2))
                .arg(QString::number(evidence.requested_ceiling_db_tp,'f',2)));
        // Advisory UI availability. A device change after this observation
        // is still rejected by preview_nonconforming_requested() at click.
        view_->setNonconformingDirectPreviewAvailable(
            session_->current_measured_matches_device_for_preview());
        view_->setNonconformingDevicePreviewAvailable(
            static_cast<bool>(measuredProbe_&&measuredRecheck_)
            && !measuredPreparing_
            && !measuredWorkerRunning_.load(std::memory_order_acquire)
            && static_cast<bool>(
                session_->capture_measured_master_for_device_conversion()));
    } else if(status==QStringLiteral("PRIVATE_INSTALLED")) {
        finish_activation();
    } else if(status==QStringLiteral("STALE")
              ||status==QStringLiteral("IDLE")) {
        deactivate(false);
    } else if(status==QStringLiteral("ERROR")) {
        fail_closed(session_->error().isEmpty()
            ? QStringLiteral("M17 terminal session failed.")
            : QStringLiteral("M17 terminal session failed: %1").arg(session_->error()));
    }
}

void TerminalProductActivationCoordinator::source_committed_successfully()
{
    // A new SOURCE cannot inherit yesterday's ERROR/BYPASS diagnostic.
    // This is presentation recovery ONLY, never a terminal render or a
    // forged READY. Never clear a real installed/pending/private owner.
    if(transitioning_ || intent_ || deviceActive_ || handoffStarted_
       ||session_->monitoring_transaction_active() ||!state_->bypassed()
       ||!selector_->prepared_realization_snapshot())
        return;
    const auto status=session_->status();
    if(status!=QStringLiteral("IDLE")
       &&status!=QStringLiteral("ERROR")
       &&status!=QStringLiteral("STALE"))
        return;
    view_->clearAudioError();
    view_->clearPreparationProgress();
    view_->clearVerificationTrim();
    view_->setAudioStatus(QStringLiteral("BYPASS · NEW SOURCE"));
}

void TerminalProductActivationCoordinator::external_context_changed()
{
    if(measuredPreparing_
       &&session_->capture_measured_master_for_device_conversion()
            !=pendingMeasured_)
        cancel_measured_crossrate_audition();
    // Selector signals during MANUAL restoration cannot re-enter bypass.
    if(session_->monitoring_transaction_active())return;
    if(transitioning_)return;
    if(!intent_) {
        if(session_->comparison_cached()
           &&!session_->comparison_owners_current()) {
            static_cast<void>(session_->cancel());
            view_->clearVerificationTrim();
        }
        return;
    }
    // A new manual owner or selected target invalidates any terminal
    // publication without silently installing PCM over another owner.
    if(selector_->processed_realization_snapshot()!=intent_->manualOwner
       ||selector_->processed_realization_id()
           !=std::optional{intent_->manualId}
       ||selector_->active_target()!=AuditionTarget::PROCESSED) {
        if(deviceActive_)
            fail_closed(QStringLiteral("M17 installed source/target changed."));
        else
            deactivate(false); // Private stale candidate is not an error.
    }
}
} // namespace rgsml::app
