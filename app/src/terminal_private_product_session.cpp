#include "terminal_private_product_session.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/audio/master_monitoring_rate_plan.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/audio/monitoring_frame_map.hpp>
#include <limits>
#include <new>
#include <utility>

namespace rgsml::app {
namespace {
[[nodiscard]] core::Error rejected(const char* reason) {
    return core::Error{core::ErrorCode::InvalidState,reason};
}
}
TerminalPrivateProductSession::TerminalPrivateProductSession(
    TerminalPrivateRenderController& renderer,
    TerminalPrivatePublicationCoordinator& publisher,
    TerminalCandidateGate& candidates,
    PlaybackTransportViewModel& transport,
    Snapshot prepared,Snapshot manual,ManualId manualId,
    Loop sourceLoop,Selected selected,QObject* parent)
    : QObject(parent),renderer_(&renderer),publisher_(&publisher),
      candidates_(&candidates),transport_(&transport),
      prepared_(std::move(prepared)),manual_(std::move(manual)),
      manualId_(std::move(manualId)),sourceLoop_(std::move(sourceLoop)),
      selected_(std::move(selected))
{
    QObject::connect(renderer_,&TerminalPrivateRenderController::changed,
                     this,&TerminalPrivateProductSession::on_renderer_changed);
}

void TerminalPrivateProductSession::failed(const core::Error& error) {
    approvedMonitor_.reset();
    approvedMonitorMaster_.reset();
    pending_.reset();
    status_=QStringLiteral("ERROR");
    error_=QString::fromStdString(error.message());
    for(const auto& detail:error.details()) {
        if(detail.key=="category" && !detail.value.empty()) {
            error_=QStringLiteral("[%1] %2")
                .arg(QString::fromStdString(detail.value),error_);
            break;
        }
    }
    emit changed();
}
void TerminalPrivateProductSession::stale() {
    approvedMonitor_.reset();
    approvedMonitorMaster_.reset();
    pending_.reset();
    status_=QStringLiteral("STALE");
    error_.clear();
    emit changed();
}

core::Status TerminalPrivateProductSession::request_private(core::SampleRate rate,
                                                           bool deferred)
{
    if(pending_ || privateId_ ||installedMonitor_ ||monitorTransitioning_)
        return core::Status::failure(rejected(
            "A private M17 transaction is already pending or installed."));
    if(!prepared_||!manual_||!manualId_||!sourceLoop_||!selected_
       ||!selected_())
        return core::Status::failure(rejected(
            "Private M17 requires a selected PROCESSED source."));
    auto prepared=prepared_(),manual=manual_();
    auto manualId=manualId_();
    if(!prepared||!manual||!manualId||manualId->value==0)
        return core::Status::failure(rejected(
            "Private M17 requires a published manual predecessor."));
    auto snapshot=transport_->playback_snapshot();
    if(!snapshot) return core::Status::failure(*snapshot.error());
    if((snapshot.value()->state!=core::PlaybackState::STOPPED
        &&(!deferred || (snapshot.value()->state!=core::PlaybackState::PLAYING
                      &&snapshot.value()->state!=core::PlaybackState::PAUSED)))
       ||snapshot.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||snapshot.value()->audibleRealization.realizationId!=manualId
       ||!snapshot.value()->duration
       ||*snapshot.value()->duration!=manual->view().frame_count())
        return core::Status::failure(rejected(
            "M17 may render only a stopped exact audible manual predecessor."));
    auto loop=sourceLoop_();
    if(loop && (loop->begin().value()<0
       ||loop->end()>manual->view().absolute_range().end()))
        return core::Status::failure(rejected(
            "M17 SOURCE loop lies outside the full manual programme."));
    // request_private internally invalidates the gate, then emits RENDERING;
    // pending ownership is captured before any queued worker completion.
    pending_.emplace(Pending{prepared,manual,*manualId,loop,rate,deferred});
    approvedMonitor_.reset();
    approvedMonitorMaster_.reset();
    privateId_.reset();
    status_=QStringLiteral("RENDERING");
    error_.clear();
    auto submitted=renderer_->request_private(rate);
    if(!submitted) {
        failed(*submitted.error());
        return submitted;
    }
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::request_private_for_final_master(
    core::SampleRate selectedFinalMasterRate)
{
    // Pure rate validation: pass the same value twice ONLY to verify the
    // frozen six-rate MASTER set, not as a proxy for the actual device.
    const auto eligible=audio::plan_master_monitoring_rate(
        selectedFinalMasterRate,selectedFinalMasterRate);
    if(!eligible)return core::Status::failure(*eligible.error());
    // The normal snapshot/ticket/owner/cancel checks are reused, but
    // publication is intentionally never armed on the master-only path.
    auto submitted=request_private(selectedFinalMasterRate,true);
    if(!submitted)return submitted;
    if(!pending_)return core::Status::failure(rejected(
        "M17 private master submission lost its pending owner."));
    pending_->masterOnly=true;
    return core::Status::success();
}

std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
TerminalPrivateProductSession::current_verified_private_master() const
{
    if(!pending_ ||!pending_->masterOnly ||privateId_
       ||status_!=QStringLiteral("MASTER_READY_MONITOR_PENDING")
       ||renderer_->status()!=QStringLiteral("READY_PRIVATE")
       ||!selected_ ||!selected_() ||!prepared_||!manual_||!manualId_
       ||!sourceLoop_||prepared_()!=pending_->prepared
       ||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId}
       ||sourceLoop_()!=pending_->loop)
        return {};
    const auto stage=candidates_->staged();
    if(!stage||stage->prepared_owner!=pending_->prepared
       ||stage->rendered.manual_owner!=pending_->manual
       ||stage->original_manual_realization_id!=pending_->manualId
       ||stage->rendered.output_rate!=pending_->outputRate
       ||!stage->rendered.terminal.checked.final_check)
        return {};
    const auto master=stage->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    if(master.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||master.absolute_start_frame().value()!=0
       ||master.frame_count().value()<=0
       ||master.format().sample_rate()!=pending_->outputRate
       ||master.format().channel_layout()!=pending_->manual->view()
           .format().channel_layout())
        return {};
    const auto audible=transport_->playback_snapshot();
    if(!audible
       ||audible.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||audible.value()->audibleRealization.realizationId
           !=std::optional{pending_->manualId}
       ||audible.value()->duration
           !=std::optional{pending_->manual->view().frame_count()})
        return {};
    return stage;
}

core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>>
TerminalPrivateProductSession::materialize_private_final_master_monitoring(
    const MonitoringMaterializer& materializer)
{
    using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
    const auto refuse=[](const char* message)->core::Result<Owned> {
        return core::Result<Owned>::failure(
            core::Error{core::ErrorCode::InvalidState,message});
    };
    if(!materializer)return core::Result<Owned>::failure(core::Error{
        core::ErrorCode::InvalidArgument,
        "M17_MONITOR_EXPLICIT_MATERIALIZER_REQUIRED"});
    const auto stage=current_verified_private_master();
    if(!stage)return refuse("M17_MONITOR_VERIFIED_MASTER_NOT_CURRENT");
    // Holding the stage preserves the immutable MASTER allocation even if
    // a reentrant callback revokes its source transaction.
    const auto master=stage->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    auto result=[&]()->core::Result<Owned> {
        try {
            return materializer(master);
        } catch(const std::bad_alloc&) {
            return core::Result<Owned>::failure(core::Error{
                core::ErrorCode::IoFailure,
                "M17_MONITOR_MATERIALIZER_OUT_OF_MEMORY"});
        }
    }();
    // Fail closed AFTER an arbitrarily expensive conversion: a new source,
    // loop, M17 identity or canceled request invalidates its entire result.
    if(current_verified_private_master()!=stage)
        return refuse("M17_MONITOR_MASTER_REVOKED_DURING_MATERIALIZATION");
    if(!result)return result;
    if(!*result.value())
        return refuse("M17_MONITOR_DEVICE_PCM_OWNER_MISSING");
    const auto checked=audio::validate_monitoring_pcm_candidate(
        **result.value());
    if(!checked)return core::Result<Owned>::failure(*checked.error());
    if((*result.value())->immutable_master_rate!=master.format().sample_rate()
       ||(*result.value())->immutable_master_frames!=master.frame_count()
       ||(*result.value())->device_pcm.view().format().channel_layout()
           !=master.format().channel_layout())
        return refuse("M17_MONITOR_MASTER_DEVICE_PROVENANCE_MISMATCH");
    // Mint exact stage/PCM pointer authority ONLY after both the MASTER
    // and the independent monitoring provenance gates succeeded.
    approvedMonitor_=*result.value();
    approvedMonitorMaster_=stage;
    // Never install automatically or claim M16 LIVE.
    return result;
}

core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>>
TerminalPrivateProductSession::
materialize_private_final_master_monitoring_with_preflight(
    core::FrameCount maximumBlockFrames,
    std::uint64_t reservedOtherPrepareBytes,
    const MonitoringPreallocationDecision& explicitDecision)
{
    using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
    if(!explicitDecision)
        return core::Result<Owned>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "M17_MONITOR_EXPLICIT_MEMORY_DECISION_REQUIRED"});
    // Copy the configured service seam before the synchronous user callback.
    // Reentrant changes to the registration cannot replace an in-flight
    // materializer. The verified-MASTER bridge independently checks owner
    // identity both before and after this entire operation.
    const auto preflight=monitoringPreflight_;
    if(!preflight)
        return core::Result<Owned>::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M17_MONITOR_WINDOWS_PREFLIGHT_NOT_CONFIGURED"});
    return materialize_private_final_master_monitoring(
        [preflight,maximumBlockFrames,reservedOtherPrepareBytes,
         &explicitDecision](audio::AudioBufferView master) {
            return preflight(master,maximumBlockFrames,
                             reservedOtherPrepareBytes,explicitDecision);
        });
}


core::Status TerminalPrivateProductSession::install_private_monitor_stopped(
    MonitoringOwner candidate)
{
    if(monitorTransitioning_||installedMonitor_||!candidate
       ||!monitorInstall_||!monitorOwned_||!monitorRetire_||!manualRestore_)
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_EXPLICIT_TRANSPORT_OR_OWNER_MISSING"));
    const auto masterStage=current_verified_private_master();
    if(!masterStage||masterStage!=approvedMonitorMaster_
       ||candidate!=approvedMonitor_)
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_UNMINTED_OR_STALE_MASTER_TICKET"));
    const auto coherent=audio::validate_monitoring_pcm_candidate(*candidate);
    if(!coherent)return coherent;
    const auto master=masterStage->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    if(candidate->immutable_master_rate!=master.format().sample_rate()
       ||candidate->immutable_master_frames!=master.frame_count()
       ||candidate->device_pcm.view().format().channel_layout()
           !=master.format().channel_layout())
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_MASTER_PROVENANCE_MISMATCH"));
    const auto before=transport_->playback_snapshot();
    if(!before||before.value()->state!=core::PlaybackState::STOPPED
       ||before.value()->position.value()!=0||before.value()->loop
       ||pending_->loop)
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_REQUIRES_STOPPED_ZERO_CUE_NO_LOOP"));
    const Pending predecessor=*pending_;
    monitorTransitioning_=true;
    const auto installed=monitorInstall_(candidate);
    if(!installed) {
        // Device creation/installation can fail AFTER replacing MANUAL.
        // An error is not proof that the original sink survived. Recover
        // only an exactly recognized candidate or a demonstrably empty sink;
        // never clear an alien anonymous monitor or a successor owner.
        const auto observed=monitorOwned_(candidate);
        if(!observed) {
            monitorTransitioning_=false;
            return core::Status::failure(*observed.error());
        }
        const auto owned=*observed.value();
        if(owned) {
            const auto retired=monitorRetire_(candidate);
            if(!retired) {
                // Exact candidate remains recoverable through the normal
                // conditional-return entry point, not an orphaned sink.
                installedMonitor_=candidate;
                status_=QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED");
                monitorTransitioning_=false;
                emit changed();
                return retired;
            }
        }
        const auto current=transport_->playback_snapshot();
        if(!current) {
            monitorTransitioning_=false;
            return core::Status::failure(*current.error());
        }
        const bool empty=current.value()->state==core::PlaybackState::NO_SOURCE
            &&current.value()->audibleRealization.phase
                ==core::AudibleHandoffPhase::UNAVAILABLE
            &&!current.value()->audibleRealization.realizationId;
        const bool original=current.value()->state==core::PlaybackState::STOPPED
            &&current.value()->position==core::FrameIndex{0}
            &&!current.value()->loop
            &&current.value()->audibleRealization.phase
                ==core::AudibleHandoffPhase::NEW
            &&current.value()->audibleRealization.realizationId
                ==std::optional{predecessor.manualId}
            &&current.value()->duration
                ==std::optional{predecessor.manual->view().frame_count()};
        if(!empty&&!original) {
            monitorTransitioning_=false;
            return core::Status::failure(rejected(
                "MONITOR_INSTALL_FAILURE_ALIEN_DEVICE_OWNER_UNTOUCHED"));
        }
        if(empty) {
            if(!selected_||!selected_()||!manual_||!manualId_
               ||manual_()!=predecessor.manual
               ||manualId_()!=std::optional{predecessor.manualId}
               ||!pending_||pending_->manual!=predecessor.manual
               ||pending_->manualId!=predecessor.manualId) {
                monitorTransitioning_=false;
                return core::Status::failure(rejected(
                    "MONITOR_INSTALL_FAILURE_MANUAL_OWNER_CHANGED"));
            }
            const auto restored=manualRestore_(
                predecessor.manual,predecessor.manualId,core::FrameIndex{0});
            if(!restored) {
                monitorTransitioning_=false;
                failed(*restored.error());
                return restored;
            }
            const auto after=transport_->playback_snapshot();
            if(!after||after.value()->state!=core::PlaybackState::STOPPED
               ||after.value()->position!=core::FrameIndex{0}
               ||after.value()->loop
               ||after.value()->audibleRealization.phase
                   !=core::AudibleHandoffPhase::NEW
               ||after.value()->audibleRealization.realizationId
                   !=std::optional{predecessor.manualId}
               ||after.value()->duration
                   !=std::optional{predecessor.manual->view().frame_count()}) {
                monitorTransitioning_=false;
                failed(rejected("MONITOR_INSTALL_FAILURE_MANUAL_RESTORE_UNPROVEN"));
                return core::Status::failure(rejected(
                    "MONITOR_INSTALL_FAILURE_MANUAL_RESTORE_UNPROVEN"));
            }
        }
        monitorTransitioning_=false;
        // No M17 realization or LIVE provenance was ever issued.
        return installed;
    }
    const auto ownership=monitorOwned_(candidate);
    const auto after=transport_->playback_snapshot();
    const bool valid=ownership&&*ownership.value()&&after
        &&after.value()->state==core::PlaybackState::STOPPED
        &&after.value()->duration
            ==std::optional{candidate->device_pcm.view().frame_count()}
        &&after.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::UNAVAILABLE
        &&!after.value()->audibleRealization.realizationId
        &&pending_&&pending_->manual==predecessor.manual
        &&pending_->prepared==predecessor.prepared
        &&pending_->manualId==predecessor.manualId
        &&pending_->outputRate==predecessor.outputRate
        &&!pending_->loop&&sourceLoop_&&!sourceLoop_()
        &&selected_&&selected_()&&manual_&&manual_()==predecessor.manual
        &&manualId_&&manualId_()==std::optional{predecessor.manualId}
        &&candidates_->staged()==masterStage
        &&renderer_->status()==QStringLiteral("READY_PRIVATE");
    if(!valid) {
        // Roll back only the exactly owned monitor. Never erase a successor.
        if(ownership&&*ownership.value()) {
            const auto retired=monitorRetire_(candidate);
            if(retired&&selected_&&selected_()
               &&manual_&&manual_()==predecessor.manual
               &&manualId_&&manualId_()==std::optional{predecessor.manualId})
                static_cast<void>(manualRestore_(
                    predecessor.manual,predecessor.manualId,
                    core::FrameIndex{0}));
        }
        monitorTransitioning_=false;
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_LOST_EXACT_STOPPED_OWNER"));
    }
    installedMonitor_=std::move(candidate);
    status_=QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED");
    monitorTransitioning_=false;
    emit changed();
    return core::Status::success();
}


core::Result<audio::MonitoringMasterDisplaySnapshot>
TerminalPrivateProductSession::private_monitor_master_snapshot() const
{
    using MonitorDisplaySnapshot=audio::MonitoringMasterDisplaySnapshot;
    const auto refused=[](const char* why)->core::Result<MonitorDisplaySnapshot> {
        return core::Result<MonitorDisplaySnapshot>::failure(
            core::Error{core::ErrorCode::InvalidState,why});
    };
    if(monitorTransitioning_||!installedMonitor_||!approvedMonitor_
       ||installedMonitor_!=approvedMonitor_||!approvedMonitorMaster_
       ||!monitorOwned_||!monitorDisplay_||!pending_
       ||candidates_->staged()!=approvedMonitorMaster_
       ||renderer_->status()!=QStringLiteral("READY_PRIVATE")
       ||!selected_||!selected_()||!manual_||!manualId_
       ||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId})
        return refused("MONITOR_DISPLAY_EXACT_MASTER_AUTHORITY_REQUIRED");
    const auto observed=monitorOwned_(installedMonitor_);
    if(!observed||!*observed.value())
        return refused("MONITOR_DISPLAY_EXACT_DEVICE_OWNER_REQUIRED");
    auto display=monitorDisplay_(installedMonitor_);
    if(!display)return display;
    if(display.value()->master_rate!=installedMonitor_->immutable_master_rate
       ||display.value()->device_rate!=installedMonitor_->device_rate
       ||display.value()->master_duration!=installedMonitor_->immutable_master_frames
       ||display.value()->device_duration
           !=installedMonitor_->device_pcm.view().frame_count()
       ||display.value()->master_display_position.value()<0
       ||display.value()->master_display_position.value()
           >display.value()->master_duration.value())
        return refused("MONITOR_DISPLAY_MASTER_DEVICE_PROVENANCE_MISMATCH");
    return display;
}

core::Status TerminalPrivateProductSession::control_private_monitor(
    platform::windows::MonitoringTransportRequest command)
{
    if(monitorTransitioning_||!installedMonitor_||!monitorControl_)
        return core::Status::failure(rejected(
            "MONITOR_CONTROL_PRIVATE_OWNER_REQUIRED"));
    const auto owner=installedMonitor_;
    const auto before=private_monitor_master_snapshot();
    if(!before)return core::Status::failure(*before.error());
    // Do not allow a command to invent a new SOURCE processing history;
    // the device-only monitor is still unqualified for LIVE TP/GR.
    monitorTransitioning_=true;
    const auto executed=monitorControl_(owner,command);
    monitorTransitioning_=false;
    if(!executed)return executed;
    if(installedMonitor_!=owner)
        return core::Status::failure(rejected(
            "MONITOR_CONTROL_OWNER_REVOKED_DURING_COMMAND"));
    const auto after=private_monitor_master_snapshot();
    if(!after)return core::Status::failure(*after.error());
    using Action=platform::windows::MonitoringTransportAction;
    bool coherent=false;
    switch(command.action) {
    case Action::PLAY:
        coherent=after.value()->state==core::PlaybackState::PLAYING;break;
    case Action::PAUSE:
        coherent=after.value()->state==core::PlaybackState::PAUSED;break;
    case Action::STOP:
        coherent=after.value()->state==core::PlaybackState::STOPPED
            &&after.value()->device_position.value()==0;break;
    case Action::SEEK_MASTER: {
        auto map=audio::MonitoringFrameMap::create(*owner);
        if(!map)return core::Status::failure(*map.error());
        auto expected=map.value()->master_to_device(command.master_frame);
        coherent=expected
            &&after.value()->device_position==*expected.value();
        break;
    }
    case Action::SET_MASTER_LOOP:
        coherent=after.value()->requested_master_loop
            ==command.master_loop;
        break;
    }
    if(!coherent)return core::Status::failure(rejected(
        "MONITOR_CONTROL_POST_COMMAND_DEVICE_STATE_MISMATCH"));
    status_=after.value()->state==core::PlaybackState::PLAYING
        ?QStringLiteral("MONITOR_PRIVATE_PLAYING")
        :after.value()->state==core::PlaybackState::PAUSED
            ?QStringLiteral("MONITOR_PRIVATE_PAUSED")
            :QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED");
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::return_private_monitor_to_manual()
{
    if(monitorTransitioning_||!installedMonitor_||!pending_
       ||!monitorOwned_||!monitorRetire_||!manualRestore_)
        return core::Status::failure(rejected(
            "MONITOR_RETURN_HAS_NO_EXACT_INSTALLED_OWNER"));
    const auto candidate=installedMonitor_;
    const Pending predecessor=*pending_;
    // The Windows MONITOR snapshot is in MASTER frames, not device or
    // SOURCE units. Convert the display cue to the original SOURCE domain
    // with bounded integer arithmetic. A stopped monitor without the new
    // controller retains the original legacy zero-cue return.
    core::FrameIndex sourceCue{0};
    std::optional<core::FrameRange> reconciledSourceLoop;
    core::PlaybackState priorMonitorState=core::PlaybackState::STOPPED;
    if(monitorDisplay_) {
        const auto display=private_monitor_master_snapshot();
        if(!display)return core::Status::failure(*display.error());
        priorMonitorState=display.value()->state;
        if(priorMonitorState!=core::PlaybackState::STOPPED
           &&priorMonitorState!=core::PlaybackState::PLAYING
           &&priorMonitorState!=core::PlaybackState::PAUSED)
            return core::Status::failure(rejected(
                "MONITOR_RETURN_INVALID_TRANSPORT_STATE"));
        // Verify selected SOURCE epoch BEFORE retiring the device. An
        // explicitly supplied callback will commit only the projected loop
        // after original MANUAL restoration, not during MONITOR playback.
        if(predecessor.loop || (sourceLoop_&&sourceLoop_()))
            return core::Status::failure(rejected(
                "MONITOR_RETURN_SOURCE_LOOP_CHANGED_DURING_MONITOR"));
        if(display.value()->requested_master_loop) {
            if(!sourceLoopReconcile_||!sourceLoop_)
                return core::Status::failure(rejected(
                    "MONITOR_RETURN_EXPLICIT_SOURCE_LOOP_TRANSACTION_REQUIRED"));
            const auto map=audio::MonitoringFrameMap::create(*candidate);
            if(!map)return core::Status::failure(*map.error());
            auto projected=map.value()->master_loop_to_source(
                *display.value()->requested_master_loop,
                predecessor.manual->view().format().sample_rate(),
                predecessor.manual->view().frame_count());
            if(!projected)
                return core::Status::failure(*projected.error());
            reconciledSourceLoop=*projected.value();
        }
        const auto masterHz=display.value()->master_rate.value();
        const auto sourceHz=predecessor.manual->view().format()
            .sample_rate().value();
        const auto masterPos=display.value()->master_display_position.value();
        if(masterHz<=0||sourceHz<=0||masterPos<0)
            return core::Status::failure(rejected(
                "MONITOR_RETURN_INVALID_MASTER_SOURCE_TIMELINE"));
        const auto q=masterPos/masterHz;
        const auto rem=masterPos%masterHz;
        if(q>(std::numeric_limits<std::int64_t>::max()
                -(rem*sourceHz)/masterHz)/sourceHz)
            return core::Status::failure(rejected(
                "MONITOR_RETURN_SOURCE_CUE_OVERFLOW"));
        const auto value=q*sourceHz+(rem*sourceHz)/masterHz;
        sourceCue=core::FrameIndex{std::min(
            value,predecessor.manual->view().frame_count().value())};
        if(reconciledSourceLoop
           &&(sourceCue<reconciledSourceLoop->begin()
              ||sourceCue>=reconciledSourceLoop->end()))
            sourceCue=reconciledSourceLoop->begin();
    }
    const auto owned=monitorOwned_(candidate);
    if(!owned)return core::Status::failure(*owned.error());
    if(!*owned.value())
        return core::Status::failure(rejected(
            "MONITOR_RETURN_REFUSES_ALIEN_DEVICE_OWNER"));
    monitorTransitioning_=true;
    // The worker thread ATOMICALLY rechecks owner identity before clear.
    const auto retired=monitorRetire_(candidate);
    if(!retired){monitorTransitioning_=false;return retired;}
    installedMonitor_.reset();
    const auto vacant=transport_->playback_snapshot();
    if(!vacant||vacant.value()->state!=core::PlaybackState::NO_SOURCE
       ||vacant.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::UNAVAILABLE
       ||vacant.value()->audibleRealization.realizationId) {
        monitorTransitioning_=false;
        renderer_->invalidate();
        failed(rejected("MONITOR_RETURN_RETIRE_DID_NOT_EMPTY_DEVICE"));
        return core::Status::failure(rejected(
            "MONITOR_RETURN_RETIRE_DID_NOT_EMPTY_DEVICE"));
    }
    if(!selected_||!selected_()||!manual_||!manualId_
       ||manual_()!=predecessor.manual
       ||manualId_()!=std::optional{predecessor.manualId}) {
        monitorTransitioning_=false;
        renderer_->invalidate();
        failed(rejected("MONITOR_RETURN_STALE_MANUAL_OWNER"));
        return core::Status::failure(rejected(
            "MONITOR_RETURN_STALE_MANUAL_OWNER"));
    }
    const auto restored=manualRestore_(
        predecessor.manual,predecessor.manualId,sourceCue);
    if(!restored){
        monitorTransitioning_=false;
        renderer_->invalidate();
        failed(*restored.error());
        return restored;
    }
    const auto after=transport_->playback_snapshot();
    if(!after||after.value()->state!=core::PlaybackState::STOPPED
       ||after.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||after.value()->audibleRealization.realizationId
           !=std::optional{predecessor.manualId}
       ||after.value()->duration
           !=std::optional{predecessor.manual->view().frame_count()}
       ||after.value()->position!=sourceCue
       ||after.value()->loop!=predecessor.loop) {
        monitorTransitioning_=false;
        renderer_->invalidate();
        failed(rejected("MONITOR_RETURN_MANUAL_SINK_NOT_EXACT"));
        return core::Status::failure(rejected(
            "MONITOR_RETURN_MANUAL_SINK_NOT_EXACT"));
    }
    if(reconciledSourceLoop) {
        // The callback must act on the restored original MANUAL, never on
        // the anonymous monitor. Its SOURCE region is independently checked.
        const auto committed=sourceLoopReconcile_(*reconciledSourceLoop);
        if(!committed) {
            monitorTransitioning_=false;
            renderer_->invalidate();
            failed(*committed.error());
            return committed;
        }
        const auto region=sourceLoop_();
        const auto loopSink=transport_->playback_snapshot();
        if(!region||*region!=*reconciledSourceLoop
           ||!loopSink||loopSink.value()->state!=core::PlaybackState::STOPPED
           ||loopSink.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::NEW
           ||loopSink.value()->audibleRealization.realizationId
               !=std::optional{predecessor.manualId}
           ||loopSink.value()->position!=sourceCue
           ||loopSink.value()->duration
               !=std::optional{predecessor.manual->view().frame_count()}
           ||loopSink.value()->loop!=reconciledSourceLoop) {
            monitorTransitioning_=false;
            renderer_->invalidate();
            failed(rejected("MONITOR_RETURN_SOURCE_LOOP_EPOCH_MISMATCH"));
            return core::Status::failure(rejected(
                "MONITOR_RETURN_SOURCE_LOOP_EPOCH_MISMATCH"));
        }
    }
    // Restore the prior Play/Pause state only after the exact original
    // MANUAL PCM and clock have been installed. A paused monitor returns
    // to PAUSED by starting the MANUAL sink then suspending the same owner.
    if(priorMonitorState!=core::PlaybackState::STOPPED) {
        transport_->playOrResume();
        if(priorMonitorState==core::PlaybackState::PAUSED)
            transport_->pause();
        const auto resumed=transport_->playback_snapshot();
        if(!resumed
           ||resumed.value()->state!=priorMonitorState
           ||resumed.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::NEW
           ||resumed.value()->audibleRealization.realizationId
               !=std::optional{predecessor.manualId}
           ||resumed.value()->duration
               !=std::optional{predecessor.manual->view().frame_count()}
           ||resumed.value()->loop!=reconciledSourceLoop) {
            // Do not announce restored playback and do not clear a
            // successor. Retire only the exact MANUAL owner if still there.
            if(resumed
               &&resumed.value()->audibleRealization.phase
                   ==core::AudibleHandoffPhase::NEW
               &&resumed.value()->audibleRealization.realizationId
                   ==std::optional{predecessor.manualId})
                static_cast<void>(transport_->stop_and_clear());
            monitorTransitioning_=false;
            renderer_->invalidate();
            failed(rejected("MONITOR_RETURN_MANUAL_TRANSPORT_NOT_RESTORED"));
            return core::Status::failure(rejected(
                "MONITOR_RETURN_MANUAL_TRANSPORT_NOT_RESTORED"));
        }
    }
    monitorTransitioning_=false;
    if(reconciledSourceLoop) {
        // This SOURCE loop differs from the captured M17 render epoch.
        // Revoke the private MASTER ticket: new audition requires new render.
        pending_.reset();
        approvedMonitor_.reset();
        approvedMonitorMaster_.reset();
        status_=QStringLiteral("IDLE");
        renderer_->invalidate();
    } else {
        status_=QStringLiteral("MASTER_READY_MONITOR_PENDING");
    }
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::request_private_for_device(bool deferred)
{
    if(!rateProbe_ || !selected_ || !selected_() || !manual_)
        return core::Status::failure(rejected(
            "No selected manual programme or real output-rate probe for M17."));
    auto manual=manual_();
    if(!manual || manual->view().frame_count().value()<=0
       ||manual->frame_domain_id()!=audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return core::Status::failure(rejected(
            "M17 output-rate discovery requires complete SOURCE-domain manual PCM."));
    auto rate=rateProbe_(manual->view().format().channel_layout());
    if(!rate)return core::Status::failure(*rate.error());
    if(manual_()!=manual || !selected_())
        return core::Status::failure(rejected(
            "M17 manual owner or audition target changed during device probe."));
    // request_private rechecks stopped audible ownership and Loop Region;
    // device install independently rechecks the exact rate and channel format.
    return request_private(*rate.value(),deferred);
}

core::Result<TerminalPrivateProductSession::InstalledOutputIdentity>
TerminalPrivateProductSession::private_installed_output_identity() const
{
    using Identity=InstalledOutputIdentity;
    auto refused=[](const char* reason) -> core::Result<Identity> {
        return core::Result<Identity>::failure(
            core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!privateId_ || privateId_->value==0 || pending_
       ||status_!=QStringLiteral("PRIVATE_INSTALLED")
       ||!selected_ || !selected_() || !prepared_ || !manual_
       ||!manualId_)
        return refused("M17 is not a current installed PROCESSED terminal.");
    auto candidate=candidates_->staged();
    if(!candidate || candidate->prepared_owner!=prepared_()
       ||candidate->rendered.manual_owner!=manual_()
       ||candidate->original_manual_realization_id
           !=manualId_().value_or(core::RealizationId{0}))
        return refused("M17 immutable candidate no longer has its manual owner.");
    const auto pcm=candidate->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    if(pcm.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||pcm.frame_count().value()<=0
       ||pcm.format().sample_rate()!=candidate->rendered.output_rate)
        return refused("M17 candidate lacks exact OUTPUT_RATE PCM provenance.");
    auto audible=transport_->playback_snapshot();
    if(!audible) return core::Result<Identity>::failure(*audible.error());
    if(audible.value()->state==core::PlaybackState::NO_SOURCE
       ||audible.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||audible.value()->audibleRealization.realizationId
           !=std::optional{*privateId_}
       ||audible.value()->duration!=std::optional{pcm.frame_count()})
        return refused("M17 terminal is not the independently audible exact device owner.");
    auto source=transport_->source_display_snapshot();
    if(!source) return core::Result<Identity>::failure(*source.error());
    if(source.value()->duration
       !=candidate->rendered.manual_owner->view().frame_count()
       ||source.value()->position.value()<0
       ||source.value()->position.value()>source.value()->duration.value())
        return refused("M17 SOURCE clock is not mapped to the full predecessor.");
    return core::Result<Identity>::success(Identity{
        std::move(candidate),*privateId_,*audible.value(),*source.value()});
}

bool TerminalPrivateProductSession::comparison_owners_current() const
{
    if(!comparison_cached()||!pending_||!selected_||!selected_()
       ||!prepared_||!manual_||!manualId_||!sourceLoop_
       ||prepared_()!=pending_->prepared||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId}
       ||sourceLoop_()!=pending_->loop)
        return false;
    if(status_==QStringLiteral("COMPARISON_VERIFIED")) {
        const auto stage=candidates_->staged();
        return stage && stage==comparisonVerified_
            && stage->prepared_owner==pending_->prepared
            && stage->rendered.manual_owner==pending_->manual
            && stage->original_manual_realization_id==pending_->manualId
            && stage->rendered.output_rate==pending_->outputRate
            && renderer_->status()==QStringLiteral("READY_PRIVATE");
    }
    const auto stage=candidates_->staged_measured();
    return stage && stage==comparisonMeasured_
        && stage->prepared_owner==pending_->prepared
        && stage->rendered.manual_owner==pending_->manual
        && stage->original_manual_realization_id==pending_->manualId
        && stage->rendered.output_rate==pending_->outputRate
        && renderer_->status()==QStringLiteral("MEASURED_NONCONFORMING_PRIVATE");
}

core::Status TerminalPrivateProductSession::suspend_for_comparison()
{
    const bool verified=status_==QStringLiteral("READY_TO_INSTALL")
        ||status_==QStringLiteral("PRIVATE_INSTALLED");
    const bool measured=status_==QStringLiteral("MEASURED_NONCONFORMING_READY")
        ||status_==QStringLiteral("NONCONFORMING_INSTALLED");
    if(!verified&&!measured)
        return core::Status::failure(rejected(
            "M17 comparison requires a finished measured PCM, not an in-flight job."));
    auto stage=verified?candidates_->staged():nullptr;
    auto unsafe=measured?candidates_->staged_measured():nullptr;
    if((verified&&!stage)||(measured&&!unsafe))
        return core::Status::failure(rejected(
            "M17 comparison has no immutable verified/measured source candidate."));
    const auto& prepared=verified?stage->prepared_owner:unsafe->prepared_owner;
    const auto& manual=verified?stage->rendered.manual_owner:unsafe->rendered.manual_owner;
    const auto id=verified?stage->original_manual_realization_id
                          :unsafe->original_manual_realization_id;
    const auto rate=verified?stage->rendered.output_rate
                            :unsafe->rendered.output_rate;
    if(!prepared_||!manual_||!manualId_||!sourceLoop_||!selected_
       ||!selected_()||prepared_()!=prepared||manual_()!=manual
       ||manualId_()!=std::optional{id}
       ||(verified&&renderer_->status()!=QStringLiteral("READY_PRIVATE"))
       ||(measured&&renderer_->status()
            !=QStringLiteral("MEASURED_NONCONFORMING_PRIVATE")))
        return core::Status::failure(rejected(
            "M17 comparison source or measured candidate is no longer authoritative."));
    const auto loop=sourceLoop_();
    if(loop&&(loop->begin().value()<0
        ||loop->end()>manual->view().absolute_range().end()))
        return core::Status::failure(rejected(
            "M17 comparison SOURCE loop is outside the complete manual programme."));
    // Revoke device/SOURCE transport ownership, NOT the gate or renderer.
    // The coordinator alone restores the exact MANUAL sink after retirement.
    auto retired=publisher_->revoke_private();
    if(!retired)return retired;
    pending_.emplace(Pending{prepared,manual,id,loop,rate,true});
    privateId_.reset();
    comparisonVerified_=stage;
    comparisonMeasured_=unsafe;
    status_=verified?QStringLiteral("COMPARISON_VERIFIED")
                    :QStringLiteral("COMPARISON_NONCONFORMING");
    error_.clear();
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::resume_comparison()
{
    if(!comparison_owners_current()||!rateProbe_)
        return core::Status::failure(rejected(
            "M17 comparison PCM, measured evidence or MANUAL owner became stale."));
    // Never borrow a candidate prepared for a different physical device
    // format. No implicit resampling and absolutely NO new full-resource job.
    auto rate=rateProbe_(pending_->manual->view().format().channel_layout());
    if(!rate)return core::Status::failure(*rate.error());
    if(*rate.value()!=pending_->outputRate ||!comparison_owners_current())
        return core::Status::failure(rejected(
            "M17 comparison output rate or exact MANUAL authority changed."));
    const auto playback=transport_->playback_snapshot();
    if(!playback
       ||playback.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::NEW
       ||playback.value()->audibleRealization.realizationId
            !=std::optional{pending_->manualId}
       ||playback.value()->duration
            !=std::optional{pending_->manual->view().frame_count()}
       ||(playback.value()->state!=core::PlaybackState::STOPPED
           &&playback.value()->state!=core::PlaybackState::PLAYING
           &&playback.value()->state!=core::PlaybackState::PAUSED))
        return core::Status::failure(rejected(
            "M17 comparison cannot replace an unknown audible MANUAL owner."));
    status_=comparisonVerified_?QStringLiteral("READY_TO_INSTALL")
                              :QStringLiteral("MEASURED_NONCONFORMING_READY");
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::cancel()
{
    // A monitoring sink has no terminal RealizationId. The terminal gate
    // cannot retire it: require exact monitor ownership and MANUAL restore.
    if(installedMonitor_) {
        const auto restored=return_private_monitor_to_manual();
        if(!restored)return restored;
    }
    if(monitorTransitioning_)
        return core::Status::failure(rejected(
            "MONITOR_CANCEL_REENTRANT_TRANSITION"));
    approvedMonitor_.reset();
    approvedMonitorMaster_.reset();
    // Withdraw transaction first: renderer.invalidate() emits changed and
    // must not reinterpret its own invalidation as a new acceptance.
    pending_.reset();
    privateId_.reset();
    comparisonVerified_.reset();
    comparisonMeasured_.reset();
    auto retired=publisher_->revoke_private();
    renderer_->invalidate();
    if(!retired) {
        failed(*retired.error());
        return retired;
    }
    status_=QStringLiteral("IDLE");
    error_.clear();
    emit changed();
    return core::Status::success();
}

void TerminalPrivateProductSession::cancel_pending_on_region_change()
{
    // A synchronous SOURCE-region signal during exact MONITOR retirement
    // must never re-enter cancel() and disturb the owner transaction.
    if(monitorTransitioning_)return;
    if(pending_)static_cast<void>(cancel());
}

void TerminalPrivateProductSession::on_renderer_changed()
{
    if(!pending_)return;
    const QString state=renderer_->status();
    if(state==QStringLiteral("ERROR")) {
        // Never erase the diagnostic after a minutes-long Windows render:
        // e.g. M16 post-trim failure and verification-guard exceedance
        // are actionable, and categorically different from stale owners.
        auto reason=renderer_->last_error();
        failed(reason ? *reason
            : rejected("M17 private renderer returned no diagnostic."));
        return;
    }
    if(state==QStringLiteral("STALE")) {stale();return;}
    if(state==QStringLiteral("MEASURED_NONCONFORMING_PRIVATE")) {
        // Keep owner and PCM intact, but never invoke verified install.
        auto evidence=candidates_->staged_measured();
        if(!selected_()||prepared_()!=pending_->prepared
           ||manual_()!=pending_->manual
           ||manualId_()!=std::optional{pending_->manualId}
           ||sourceLoop_()!=pending_->loop
           ||!evidence||evidence->prepared_owner!=pending_->prepared
           ||evidence->rendered.manual_owner!=pending_->manual
           ||evidence->original_manual_realization_id!=pending_->manualId) {
            renderer_->invalidate();
            stale();
            return;
        }
        status_=pending_->masterOnly
            ?QStringLiteral("MASTER_MEASURED_NONCONFORMING_PRIVATE")
            :QStringLiteral("MEASURED_NONCONFORMING_READY");
        error_.clear();
        emit changed();
        return;
    }
    if(state!=QStringLiteral("READY_PRIVATE"))return;
    if(pending_->masterOnly) {
        // Master-rate rendering is complete and M16 checked. This is NOT
        // audition-ready: no device SRC, SOURCE transport or sink ownership.
        status_=QStringLiteral("MASTER_READY_MONITOR_PENDING");
        emit changed();
        return;
    }
    if(pending_->deferred) {
        // The MANUAL sink continues playing until accepted OUTPUT_RATE
        // audio is ready. Only its conductor may perform the handoff.
        status_=QStringLiteral("READY_TO_INSTALL");
        emit changed();
        return;
    }
    static_cast<void>(install_accepted_candidate());
}

core::Status TerminalPrivateProductSession::install_ready_private()
{
    if(!pending_ || !pending_->deferred
       ||status_!=QStringLiteral("READY_TO_INSTALL")
       ||renderer_->status()!=QStringLiteral("READY_PRIVATE"))
        return core::Status::failure(rejected(
            "M17 deferred installation has no exact accepted output candidate."));
    return install_accepted_candidate();
}

core::Status TerminalPrivateProductSession::install_measured_private()
{
    if(!pending_||!pending_->deferred
       ||status_!=QStringLiteral("MEASURED_NONCONFORMING_READY")
       ||renderer_->status()!=QStringLiteral("MEASURED_NONCONFORMING_PRIVATE"))
        return core::Status::failure(rejected(
            "M17 nonconforming preview was not explicitly available."));
    const Pending authority=*pending_;
    if(!selected_()||prepared_()!=authority.prepared
       ||manual_()!=authority.manual
       ||manualId_()!=std::optional{authority.manualId}
       ||sourceLoop_()!=authority.loop) {
        renderer_->invalidate();
        stale();
        return core::Status::failure(rejected(
            "Nonconforming preview source, owner or loop changed."));
    }
    const auto held=measured_candidate();
    if(!held||held->rendered.output_rate!=authority.outputRate)
        return core::Status::failure(rejected(
            "No current measured nonconforming output-rate PCM."));
    auto installed=publisher_->install_measured_private(
        authority.prepared,authority.manual,authority.manualId,authority.loop);
    if(!installed) {
        failed(*installed.error());
        renderer_->invalidate();
        return core::Status::failure(*installed.error());
    }
    pending_.reset();
    privateId_=*installed.value();
    status_=QStringLiteral("NONCONFORMING_INSTALLED");
    error_.clear();
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::install_accepted_candidate()
{
    if(!pending_)return core::Status::failure(rejected(
        "M17 cannot install without captured owners."));
    const Pending authority=*pending_;
    // No user-visible publication if input, target or Loop Region changed
    // while the worker calculated its OUTPUT_RATE candidate.
    if(!selected_()||prepared_()!=authority.prepared
       ||manual_()!=authority.manual
       ||manualId_()!=std::optional{authority.manualId}
       ||sourceLoop_()!=authority.loop) {
        renderer_->invalidate();
        stale();
        return core::Status::failure(rejected(
            "M17 deferred source/target/Loop ownership changed."));
    }
    auto accepted=candidates_->staged();
    if(!accepted || accepted->prepared_owner!=authority.prepared
       ||accepted->rendered.manual_owner!=authority.manual
       ||accepted->original_manual_realization_id!=authority.manualId
       ||accepted->rendered.output_rate!=authority.outputRate) {
        renderer_->invalidate();
        stale();
        return core::Status::failure(rejected(
            "M17 accepted OUTPUT_RATE candidate changed before installation."));
    }
    // The publisher and audio gate still independently verify STOPPED,
    // immutable MANUAL ownership before installing any output-rate PCM.
    auto installed=publisher_->install_private(authority.prepared,
        authority.manual,authority.manualId,authority.loop);
    if(!installed) {
        failed(*installed.error());
        renderer_->invalidate();
        return core::Status::failure(*installed.error());
    }
    pending_.reset();
    privateId_=*installed.value();
    status_=QStringLiteral("PRIVATE_INSTALLED");
    error_.clear();
    emit changed();
    return core::Status::success();
}
} // namespace rgsml::app
