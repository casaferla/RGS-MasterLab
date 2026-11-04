#include "terminal_private_product_session.hpp"
#include <QThread>
#include "final_src_candidate_policy.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/audio/master_monitoring_rate_plan.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/audio/monitoring_frame_map.hpp>
#include <algorithm>
#include <exception>
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

bool TerminalPrivateProductSession::current_selection_for_pending() const
{
    return selected_ && (selected_()
        || (pending_ && pending_->offlineNonAudibleSelection
            && offlineSelected_ && offlineSelected_()));
}

void TerminalPrivateProductSession::failed(const core::Error& error) {
    if(installedMeasuredTicket_ ||measuredSourceRecovery_) {
        status_=measuredSourceRecovery_
            ?QStringLiteral("MEASURED_DEVICE_SOURCE_RECOVERY_REQUIRED")
            :QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
        error_=QString::fromStdString(error.message());
        emit changed();
        return;
    }
    // Destructive hard failures must never leave an apparently usable
    // post-retire recovery token referring to revoked PCM.
    returnRecovery_.reset();
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
    if(installedMeasuredTicket_ ||measuredSourceRecovery_) {
        status_=measuredSourceRecovery_
            ?QStringLiteral("MEASURED_DEVICE_SOURCE_RECOVERY_REQUIRED")
            :QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
        emit changed();
        return;
    }
    returnRecovery_.reset();
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
    return request_private_impl(rate,deferred,false);
}

core::Status TerminalPrivateProductSession::request_private_impl(
    core::SampleRate rate, bool deferred, bool masterOnly)
{
    if(pending_ || privateId_ ||installedMonitor_ ||monitorTransitioning_)
        return core::Status::failure(rejected(
            "A private M17 transaction is already pending or installed."));
    if(!prepared_||!manual_||!manualId_||!sourceLoop_||!selected_)
        return core::Status::failure(rejected(
            "Private M17 requires an authoritative PROCESSED source."));
    auto prepared=prepared_(),manual=manual_();
    auto manualId=manualId_();
    if(!prepared||!manual||!manualId||manualId->value==0)
        return core::Status::failure(rejected(
            "Private M17 requires a published manual predecessor."));
    // Product admission is conditioned on all 14 authentic original Q0
    // FIR binaries being available locally; exact requested SHA is checked
    // again by the worker launcher before any M17 DSP can begin.
    const auto sourceRate=manual->view().format().sample_rate().value();
    if(!m17_final_src_candidate_admissible(sourceRate,rate.value(),
           renderer_->full_original_q0_family_ready()))
        return core::Status::failure(core::Error{
            core::ErrorCode::UnsupportedOperation,
            "M17_FINAL_OUTPUT_SRC_CANDIDATE_NOT_QUALIFIED"});
    auto snapshot=transport_->playback_snapshot();
    if(!snapshot)return core::Status::failure(*snapshot.error());
    // ONLY a private final-master request may render without the playback
    // device. Both a clean NO_SOURCE state and the published MANUAL
    // predecessor must be proven; alien, stale or anonymous sinks fail.
    // Ordinary M17 Active/Play retains the strict MANUAL audible contract.
    const bool offlineNoSink=masterOnly
        &&snapshot.value()->state==core::PlaybackState::NO_SOURCE
        &&snapshot.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::UNAVAILABLE
        &&!snapshot.value()->audibleRealization.realizationId
        &&snapshot.value()->position==core::FrameIndex{0}
        &&!snapshot.value()->duration
        &&!snapshot.value()->loop;
    const bool exactManual=snapshot.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::NEW
        &&snapshot.value()->audibleRealization.realizationId==manualId
        &&snapshot.value()->duration
            ==std::optional{manual->view().frame_count()}
        &&(snapshot.value()->state==core::PlaybackState::STOPPED
            ||(deferred
                &&(snapshot.value()->state==core::PlaybackState::PLAYING
                    ||snapshot.value()->state==core::PlaybackState::PAUSED)));
    const bool audibleSelected=selected_();
    const bool nonAudibleOfflineSelected=offlineNoSink
        &&offlineSelected_&&offlineSelected_();
    if(!audibleSelected&&!nonAudibleOfflineSelected)
        return core::Status::failure(rejected(
            "Private M17 requires a selected PROCESSED source or explicit non-audible MASTER provenance."));
    if(!offlineNoSink&&!exactManual)
        return core::Status::failure(rejected(
            "M17 requires exact manual DEVICE ownership; only a private MASTER may render with NO_SOURCE."));
    auto loop=sourceLoop_();
    if(loop && (loop->begin().value()<0
       ||loop->end()>manual->view().absolute_range().end()))
        return core::Status::failure(rejected(
            "M17 SOURCE loop lies outside the full manual programme."));
    // request_private internally invalidates the gate, then emits RENDERING;
    // pending ownership is captured before any queued worker completion.
    pending_.emplace(Pending{prepared,manual,*manualId,loop,rate,
                             deferred,masterOnly,offlineNoSink,
                             nonAudibleOfflineSelected&&!audibleSelected});
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
    // A private MASTER is a render operation, not a DEVICE installation.
    // It can run after the output vanished, as long as immutable MANUAL
    // provenance and the clean empty-sink snapshot both hold. Do not relax
    // request_private() or the audible terminal handoff contract.
    return request_private_impl(selectedFinalMasterRate,true,true);
}

std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
TerminalPrivateProductSession::current_verified_private_master() const
{
    // Normal M17 Prepare already produced an authoritative, M16-verified
    // immutable MASTER. It may be borrowed only while MANUAL is the exact
    // audible owner: never run another full-resource Prepare for monitoring.
    if(pending_ && !pending_->masterOnly) {
        // Retired installed M17: exact MANUAL is restored and the checked
        // MASTER remains in the unchanged COMPARISON_VERIFIED gate. Only the
        // owner-exact borrowing path below can read it; never borrow from a
        // still-installed M17 device.
        if(status_!=QStringLiteral("READY_TO_INSTALL")
           &&status_!=QStringLiteral("COMPARISON_VERIFIED"))
            return {};
        return borrow_verified_m17_master_for_device(pending_->outputRate);
    }
    if(!pending_ ||!pending_->masterOnly ||privateId_
       ||status_!=QStringLiteral("MASTER_READY_MONITOR_PENDING")
       ||renderer_->status()!=QStringLiteral("READY_PRIVATE")
       ||!current_selection_for_pending() ||!prepared_||!manual_||!manualId_
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
    if(!audible)return {};
    if(pending_->offlineNoSink) {
        // An empty endpoint must remain empty until an explicit,
        // independently checked monitoring installation. A successor
        // source device would invalidate this private master ticket.
        if(audible.value()->state!=core::PlaybackState::NO_SOURCE
           ||audible.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::UNAVAILABLE
           ||audible.value()->audibleRealization.realizationId
           ||audible.value()->position!=core::FrameIndex{0}
           ||audible.value()->duration
           ||audible.value()->loop)
            return {};
    } else if(audible.value()->audibleRealization.phase
                  !=core::AudibleHandoffPhase::NEW
              ||audible.value()->audibleRealization.realizationId
                  !=std::optional{pending_->manualId}
              ||audible.value()->duration
                  !=std::optional{pending_->manual->view().frame_count()})
        return {};
    return stage;
}

std::optional<core::SampleRate>
TerminalPrivateProductSession::verified_master_rate_for_monitoring() const
{
    const auto stage=current_verified_private_master();
    return stage ? std::optional{stage->rendered.output_rate} : std::nullopt;
}

bool TerminalPrivateProductSession::current_master_matches_device_for_preview() const
{
    const auto stage=current_verified_private_master();
    if(!stage||!rateProbe_||!manual_)return false;
    const auto manual=manual_();
    if(!manual)return false;
    const auto rate=rateProbe_(manual->view().format().channel_layout());
    return rate && *rate.value()==stage->rendered.output_rate
        &&current_verified_private_master()==stage;
}

bool TerminalPrivateProductSession::current_measured_matches_device_for_preview() const
{
    // No fallback to Windows' paired Source frequency is authorized for
    // measured NON-CONFORMING terminal PCM. The verified-master monitoring
    // SRC cannot lend its distinct certificate to this measured candidate.
    const auto stage=measured_candidate();
    if(!stage||!rateProbe_||!manual_)return false;
    const auto manual=manual_();
    if(!manual)return false;
    const auto rate=rateProbe_(manual->view().format().channel_layout());
    return rate && *rate.value()==stage->rendered.output_rate
        &&measured_candidate()==stage;
}

TerminalPrivateProductSession::MeasuredMasterCapture
TerminalPrivateProductSession::
capture_measured_master_for_device_conversion() const
{
    // Offline FIR workers have no access to QObjects or mutable owner
    // state. Capture must happen on this session's Qt owner thread.
    if(QThread::currentThread()!=thread())return {};
    return measured_candidate();
}

core::Result<MeasuredDeviceAuditionTicket>
TerminalPrivateProductSession::admit_measured_device_conversion(
    MeasuredMasterCapture captured,
    std::shared_ptr<const audio::MonitoringPcmCandidate> device,
    bool explicitUserRequest,
    const MeasuredDeviceEndpointObservation& freshEndpoint) const
{
    if(QThread::currentThread()!=thread())
        return core::Result<MeasuredDeviceAuditionTicket>::failure(
            core::Error{core::ErrorCode::InvalidState,
                "M17_MEASURED_DEVICE_ADMISSION_WRONG_THREAD"});
    // Re-read the exact live measured stage AFTER conversion. No verified
    // Master Audition, M16 LIVE, export or sink authority is minted here.
    return admit_measured_device_audition_ticket(
        std::move(captured),measured_candidate(),std::move(device),
        explicitUserRequest,freshEndpoint);
}

core::Status TerminalPrivateProductSession::recheck_measured_device_preinstall(
    const MeasuredDeviceAuditionTicket& ticket,
    const MeasuredDeviceEndpointObservation& latestEndpoint,
    bool explicitUserRequest) const
{
    const auto refused=[](const char* message)->core::Status {
        return core::Status::failure(rejected(message));
    };
    if(QThread::currentThread()!=thread())
        return refused("M17_MEASURED_PREINSTALL_REQUIRES_QT_OWNER_THREAD");
    if(monitorTransitioning_||installedMonitor_||returnRecovery_||privateId_
       ||!pending_||!manual_||!manualId_)
        return refused("M17_MEASURED_PREINSTALL_EXCLUSIVE_OWNER_REQUIRED");
    const auto measured=measured_candidate();
    if(!measured)
        return refused("M17_MEASURED_PREINSTALL_STAGE_NOT_CURRENT");
    const auto before=transport_->playback_snapshot();
    if(!before)return core::Status::failure(*before.error());
    const auto state=before.value()->state;
    if((state!=core::PlaybackState::STOPPED
        &&state!=core::PlaybackState::PLAYING
        &&state!=core::PlaybackState::PAUSED)
       ||before.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::NEW
       ||before.value()->audibleRealization.realizationId
            !=std::optional{pending_->manualId}
       ||before.value()->duration
            !=std::optional{pending_->manual->view().frame_count()}
       ||before.value()->loop!=pending_->loop
       ||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId})
        return refused("M17_MEASURED_PREINSTALL_MANUAL_TRANSPORT_CHANGED");
    return ticket.recheck_preinstall(
        measured,pending_->manualId,latestEndpoint,explicitUserRequest);
}

core::Result<MeasuredDeviceTransportPlan>
TerminalPrivateProductSession::plan_measured_device_handoff(
    const MeasuredDeviceAuditionTicket& ticket,
    const MeasuredDeviceEndpointObservation& latestEndpoint,
    bool explicitUserRequest) const
{
    const auto checked=recheck_measured_device_preinstall(
        ticket,latestEndpoint,explicitUserRequest);
    if(!checked)
        return core::Result<MeasuredDeviceTransportPlan>::failure(*checked.error());
    const auto snapshot=transport_->playback_snapshot();
    if(!snapshot)
        return core::Result<MeasuredDeviceTransportPlan>::failure(*snapshot.error());
    return ticket.plan_handoff(
        measured_candidate(),*snapshot.value(),sourceLoop_(),
        latestEndpoint,explicitUserRequest);
}

core::Result<MeasuredDeviceTransportPlan>
TerminalPrivateProductSession::rebase_measured_device_handoff(
    const MeasuredDeviceAuditionTicket& ticket,
    const MeasuredDeviceTransportPlan& prior,
    const MeasuredDeviceEndpointObservation& latestEndpoint,
    bool explicitUserRequest) const
{
    const auto checked=recheck_measured_device_preinstall(
        ticket,latestEndpoint,explicitUserRequest);
    if(!checked)
        return core::Result<MeasuredDeviceTransportPlan>::failure(*checked.error());
    const auto snapshot=transport_->playback_snapshot();
    if(!snapshot)
        return core::Result<MeasuredDeviceTransportPlan>::failure(*snapshot.error());
    return ticket.rebase_handoff(
        prior,measured_candidate(),*snapshot.value(),sourceLoop_(),
        latestEndpoint,explicitUserRequest);
}


core::Status TerminalPrivateProductSession::install_measured_device_audition(
    const MeasuredDeviceAuditionTicket& ticket,
    const MeasuredDeviceEndpointObservation& currentEndpoint,
    bool explicitUserRequest)
{
    const auto deny=[](const char* why)->core::Status {
        return core::Status::failure(rejected(why));
    };
    if(QThread::currentThread()!=thread())
        return deny("M17_MEASURED_DEVICE_INSTALL_REQUIRES_QT_OWNER");
    if(monitorTransitioning_||installedMonitor_||installedMeasuredTicket_
       ||measuredSourceRecovery_||returnRecovery_||privateId_
       ||!pending_||!pending_->deferred
       ||status_!=QStringLiteral("MEASURED_NONCONFORMING_READY")
       ||!monitorInstall_||!monitorOwned_||!monitorRetire_
       ||!monitorControl_||!monitorDisplay_||!manualRestore_)
        return deny("M17_MEASURED_DEVICE_INSTALL_EXCLUSIVE_PORTS_REQUIRED");
    const auto plan=plan_measured_device_handoff(
        ticket,currentEndpoint,explicitUserRequest);
    if(!plan)return core::Status::failure(*plan.error());
    // Source transport can advance while the expensive DEVICE SRC worker
    // runs. Reobserve it before any sink mutation; a Seek/Loop transition
    // that raced the plan must not silently inherit old DEVICE coordinates.
    const auto actual=rebase_measured_device_handoff(
        ticket,*plan.value(),currentEndpoint,explicitUserRequest);
    if(!actual)return core::Status::failure(*actual.error());
    const auto original=transport_->playback_snapshot();
    if(!original)return core::Status::failure(*original.error());
    if(original.value()->seekSerial!=actual.value()->source_seek_serial
       ||original.value()->traversalSerial
            !=actual.value()->source_traversal_serial
       ||original.value()->loopWrapCount!=actual.value()->source_loop_wrap_count
       ||original.value()->state!=actual.value()->source_state
       ||original.value()->position!=actual.value()->source_cue
       ||original.value()->loop!=actual.value()->source_loop
       ||original.value()->audibleRealization.realizationId
           !=std::optional{actual.value()->manual_owner})
        return deny("M17_MEASURED_DEVICE_INSTALL_SOURCE_EPOCH_RACED");
    const auto device=ticket.audition_device_pcm();
    if(!device)return deny("M17_MEASURED_DEVICE_INSTALL_PCM_MISSING");
    monitorTransitioning_=true;
    // Keep a return-only ticket if Windows replaced MANUAL but then
    // refused Prepare, or if a post-install exact-owner proof fails.
    const auto retainOwnedRecovery=[&](const char* message)->core::Status {
        installedMonitor_=device;
        installedMeasuredTicket_=ticket;
        installedMeasuredPlan_=*actual.value();
        measuredOriginalManual_=*original.value();
        status_=QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
        monitorTransitioning_=false;
        emit changed();
        return deny(message);
    };
    const auto installed=[&]()->core::Status {
        try {
            return monitorInstall_(device);
        } catch(const std::bad_alloc&) {
            return deny("M17_MEASURED_DEVICE_INSTALL_WINDOWS_OUT_OF_MEMORY");
        } catch(const std::exception&) {
            return deny("M17_MEASURED_DEVICE_INSTALL_WINDOWS_EXCEPTION");
        }
    }();
    const auto ownership=[&]()->core::Result<bool> {
        try {
            return monitorOwned_(device);
        } catch(const std::exception&) {
            return core::Result<bool>::failure(rejected(
                "M17_MEASURED_DEVICE_INSTALL_OWNER_PROBE_EXCEPTION"));
        }
    }();
    // A failed owner probe is NOT proof that Windows left MANUAL intact:
    // retain a return-only lease until the same worker can be re-queried.
    if(!ownership)
        return retainOwnedRecovery(
            "M17_MEASURED_DEVICE_INSTALL_OWNERSHIP_UNKNOWN");
    if(!installed ||!*ownership.value()) {
        if(ownership&&*ownership.value())
            return retainOwnedRecovery(
                "M17_MEASURED_DEVICE_INSTALL_FAILED_WITH_OWNED_SINK");
        // Do not clear or retire any alien/successor PCM. A failed Qt
        // Prepare may have already removed MANUAL: retain recovery only
        // when the endpoint is provably empty, not when it is foreign.
        const auto observed=transport_->playback_snapshot();
        if(observed
           &&observed.value()->state==core::PlaybackState::NO_SOURCE
           &&observed.value()->audibleRealization.phase
               ==core::AudibleHandoffPhase::UNAVAILABLE
           &&!observed.value()->audibleRealization.realizationId
           &&!observed.value()->duration
           &&!observed.value()->loop) {
            installedMeasuredTicket_=ticket;
            installedMeasuredPlan_=*actual.value();
            measuredOriginalManual_=*original.value();
            measuredSourceRecovery_=true;
            measuredReturnCue_=original.value()->position;
            measuredReturnState_=original.value()->state;
            status_=QStringLiteral("MEASURED_DEVICE_SOURCE_RECOVERY_REQUIRED");
            monitorTransitioning_=false;
            emit changed();
            return installed?deny(
                "M17_MEASURED_DEVICE_INSTALL_DID_NOT_OWN_DEVICE")
                :installed;
        }
        monitorTransitioning_=false;
        return installed?deny(
            "M17_MEASURED_DEVICE_INSTALL_REFUSES_FOREIGN_OWNER")
            :installed;
    }
    const auto now=transport_->playback_snapshot();
    if(!now ||now.value()->state!=core::PlaybackState::STOPPED
       ||now.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::UNAVAILABLE
       ||now.value()->audibleRealization.realizationId
       ||now.value()->duration
            !=std::optional{device->device_pcm.view().frame_count()}
       ||!pending_||pending_->manual!=ticket.measured_master()->rendered.manual_owner
       ||pending_->manualId!=ticket.original_manual_realization_id()
       ||!manual_||manual_()!=pending_->manual
       ||!manualId_||manualId_()!=std::optional{pending_->manualId}
       ||!selected_||!selected_()
       ||!sourceLoop_||sourceLoop_()!=pending_->loop
       ||renderer_->status()!=QStringLiteral("MEASURED_NONCONFORMING_PRIVATE")
       ||candidates_->staged_measured()!=ticket.measured_master())
        return retainOwnedRecovery(
            "M17_MEASURED_DEVICE_INSTALL_POSTCHECK_REQUIRES_RECOVERY");
    using Action=platform::windows::MonitoringTransportAction;
    core::Status seeded=core::Status::success();
    if(actual.value()->master_loop)
        seeded=monitorControl_(device,{
            Action::SET_MASTER_LOOP,core::FrameIndex{0},
            actual.value()->master_loop});
    if(seeded&&actual.value()->master_cue!=core::FrameIndex{0})
        seeded=monitorControl_(device,{
            Action::SEEK_MASTER,actual.value()->master_cue,std::nullopt});
    if(seeded&&actual.value()->source_state!=core::PlaybackState::STOPPED)
        seeded=monitorControl_(device,{Action::PLAY,core::FrameIndex{0},std::nullopt});
    if(seeded&&actual.value()->source_state==core::PlaybackState::PAUSED)
        seeded=monitorControl_(device,{Action::PAUSE,core::FrameIndex{0},std::nullopt});
    if(!seeded)
        return retainOwnedRecovery(
            "M17_MEASURED_DEVICE_INSTALL_TRANSPORT_SEED_FAILED");
    const auto displayed=monitorDisplay_(device);
    if(!displayed
       ||displayed.value()->state!=actual.value()->source_state
       ||displayed.value()->master_rate!=device->immutable_master_rate
       ||displayed.value()->device_rate!=device->device_rate
       ||displayed.value()->master_duration!=device->immutable_master_frames
       ||displayed.value()->device_duration
            !=device->device_pcm.view().frame_count()
       ||displayed.value()->requested_master_loop!=actual.value()->master_loop)
        return retainOwnedRecovery(
            "M17_MEASURED_DEVICE_INSTALL_TRANSPORT_POSTCHECK_FAILED");
    installedMonitor_=device;
    installedMeasuredTicket_=ticket;
    installedMeasuredPlan_=*actual.value();
    measuredOriginalManual_=*original.value();
    status_=actual.value()->source_state==core::PlaybackState::PLAYING
        ?QStringLiteral("MEASURED_DEVICE_PLAYING")
        :actual.value()->source_state==core::PlaybackState::PAUSED
            ?QStringLiteral("MEASURED_DEVICE_PAUSED")
            :QStringLiteral("MEASURED_DEVICE_INSTALLED_STOPPED");
    error_.clear();
    monitorTransitioning_=false;
    emit changed();
    return core::Status::success();
}

core::Result<audio::MonitoringMasterDisplaySnapshot>
TerminalPrivateProductSession::measured_device_snapshot() const
{
    using Result=core::Result<audio::MonitoringMasterDisplaySnapshot>;
    const auto deny=[](const char* message)->Result {
        return Result::failure(rejected(message));
    };
    if(QThread::currentThread()!=thread()||monitorTransitioning_
       ||!installedMeasuredTicket_||!installedMonitor_||!monitorOwned_
       ||!monitorDisplay_||!installedMeasuredPlan_||!pending_
       ||!selected_||!selected_()||!manual_||!manualId_
       ||!sourceLoop_||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId}
       ||sourceLoop_()!=pending_->loop
       ||renderer_->status()!=QStringLiteral("MEASURED_NONCONFORMING_PRIVATE")
       ||candidates_->staged_measured()!=installedMeasuredTicket_->measured_master()
       ||status_==QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED"))
        return deny("M17_MEASURED_DEVICE_DISPLAY_EXACT_OWNER_REQUIRED");
    if(installedMonitor_!=installedMeasuredTicket_->audition_device_pcm())
        return deny("M17_MEASURED_DEVICE_DISPLAY_PCM_CHANGED");
    const auto owned=monitorOwned_(installedMonitor_);
    if(!owned)return Result::failure(*owned.error());
    if(!*owned.value())
        return deny("M17_MEASURED_DEVICE_DISPLAY_FOREIGN_DEVICE");
    const auto display=monitorDisplay_(installedMonitor_);
    if(!display)return display;
    if(display.value()->master_rate!=installedMonitor_->immutable_master_rate
       ||display.value()->device_rate!=installedMonitor_->device_rate
       ||display.value()->master_duration!=installedMonitor_->immutable_master_frames
       ||display.value()->device_duration
            !=installedMonitor_->device_pcm.view().frame_count()
       ||display.value()->master_display_position.value()<0
       ||display.value()->master_display_position.value()
            >display.value()->master_duration.value()
       ||display.value()->device_position.value()<0
       ||display.value()->device_position.value()
            >display.value()->device_duration.value()
       ||(display.value()->state!=core::PlaybackState::STOPPED
           &&display.value()->state!=core::PlaybackState::PAUSED
           &&display.value()->state!=core::PlaybackState::PLAYING))
        return deny("M17_MEASURED_DEVICE_DISPLAY_TIMELINE_MISMATCH");
    return display;
}

core::Status TerminalPrivateProductSession::control_measured_device_audition(
    platform::windows::MonitoringTransportRequest request)
{
    const auto deny=[](const char* reason)->core::Status {
        return core::Status::failure(rejected(reason));
    };
    const auto before=measured_device_snapshot();
    if(!before)return core::Status::failure(*before.error());
    if(!monitorControl_)return deny("M17_MEASURED_DEVICE_CONTROL_PORT_MISSING");
    const auto owner=installedMonitor_;
    monitorTransitioning_=true;
    const auto applied=[&]()->core::Status {
        try {
            return monitorControl_(owner,request);
        } catch(const std::bad_alloc&) {
            return deny("M17_MEASURED_DEVICE_CONTROL_OUT_OF_MEMORY");
        } catch(const std::exception&) {
            return deny("M17_MEASURED_DEVICE_CONTROL_WINDOWS_EXCEPTION");
        }
    }();
    monitorTransitioning_=false;
    if(!applied||installedMonitor_!=owner) {
        // A Windows transport callback can partially mutate its sink before
        // it reports a refusal. Never keep PLAY/PAUSE buttons advertised as
        // authoritative after that uncertainty; preserve the exact DEVICE
        // owner for conditional Return and non-destructive recovery.
        status_=QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
        emit changed();
        return applied?deny("M17_MEASURED_DEVICE_CONTROL_LOST_EXACT_OWNER")
            :applied;
    }
    const auto after=measured_device_snapshot();
    if(!after) {
        status_=QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
        emit changed();
        return core::Status::failure(*after.error());
    }
    using Action=platform::windows::MonitoringTransportAction;
    // Rational SOURCE/MASTER/DEVICE frame conversion deliberately rounds
    // UP. A 192-kHz MASTER seek at frame 1 onto 48 kHz DEVICE targets
    // DEVICE frame 1, whose displayed MASTER cursor is 4 rather than 1.
    // Compare exact DEVICE coordinates, not a fabricated +/-1 MASTER
    // sample tolerance that would reject legitimate cross-rate seeks.
    const auto mapping=audio::MonitoringFrameMap::create(*owner);
    if(!mapping) {
        status_=QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
        emit changed();
        return core::Status::failure(*mapping.error());
    }
    const auto expectedSeek=request.action==Action::SEEK_MASTER
        ?mapping.value()->master_to_device(request.master_frame)
        :core::Result<core::FrameIndex>::success(core::FrameIndex{0});
    if((request.action==Action::PLAY
            &&after.value()->state!=core::PlaybackState::PLAYING)
       ||(request.action==Action::PAUSE
            &&after.value()->state!=core::PlaybackState::PAUSED)
       ||(request.action==Action::STOP
            &&(after.value()->state!=core::PlaybackState::STOPPED
                ||after.value()->device_position!=core::FrameIndex{0}))
       ||(request.action==Action::SEEK_MASTER
            &&(!expectedSeek
                ||after.value()->device_position!=*expectedSeek.value()))
       ||(request.action==Action::SET_MASTER_LOOP
            &&after.value()->requested_master_loop!=request.master_loop)) {
        // A Windows control may have mutated the physical sink before
        // reporting a failure. Never keep active transport permissions
        // after postcondition loss; offer only exact-owner Return.
        status_=QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
        emit changed();
        return deny("M17_MEASURED_DEVICE_CONTROL_POSTCHECK_FAILED");
    }
    status_=after.value()->state==core::PlaybackState::PLAYING
        ?QStringLiteral("MEASURED_DEVICE_PLAYING")
        :after.value()->state==core::PlaybackState::PAUSED
            ?QStringLiteral("MEASURED_DEVICE_PAUSED")
            :QStringLiteral("MEASURED_DEVICE_INSTALLED_STOPPED");
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::return_measured_device_audition()
{
    const auto deny=[](const char* reason)->core::Status {
        return core::Status::failure(rejected(reason));
    };
    if(QThread::currentThread()!=thread()||monitorTransitioning_
       ||!installedMeasuredTicket_||!installedMeasuredPlan_
       ||!measuredOriginalManual_||!pending_
       ||!monitorRetire_||!monitorOwned_||!manualRestore_)
        return deny("M17_MEASURED_DEVICE_RETURN_REQUIRES_EXACT_OWNER");
    const auto plan=*installedMeasuredPlan_;
    const auto original=*measuredOriginalManual_;
    if(!measuredSourceRecovery_) {
        if(!installedMonitor_||!monitorDisplay_)
            return deny("M17_MEASURED_DEVICE_RETURN_MONITOR_MISSING");
        const auto owned=monitorOwned_(installedMonitor_);
        if(!owned)return core::Status::failure(*owned.error());
        if(!*owned.value()) {
            const auto now=transport_->playback_snapshot();
            if(!now)return core::Status::failure(*now.error());
            const bool originalStillOwned=
                now.value()->audibleRealization.phase
                    ==core::AudibleHandoffPhase::NEW
                &&now.value()->audibleRealization.realizationId
                    ==std::optional{pending_->manualId}
                &&now.value()->duration
                    ==std::optional{pending_->manual->view().frame_count()}
                &&now.value()->loop==plan.source_loop
                &&(now.value()->state==core::PlaybackState::STOPPED
                    ||now.value()->state==core::PlaybackState::PAUSED
                    ||now.value()->state==core::PlaybackState::PLAYING);
            // If the attempted install never displaced the exact MANUAL,
            // clear only our own failed ticket. No SOURCE reinstallation.
            if(originalStillOwned) {
                installedMonitor_.reset();
                installedMeasuredTicket_.reset();
                installedMeasuredPlan_.reset();
                measuredOriginalManual_.reset();
                measuredSourceRecovery_=false;
                status_=QStringLiteral("MEASURED_NONCONFORMING_READY");
                error_.clear();
                emit changed();
                return core::Status::success();
            }
            const bool empty=now.value()->state==core::PlaybackState::NO_SOURCE
                &&now.value()->audibleRealization.phase
                    ==core::AudibleHandoffPhase::UNAVAILABLE
                &&!now.value()->audibleRealization.realizationId
                &&!now.value()->duration &&!now.value()->loop;
            if(!empty)
                return deny("M17_MEASURED_DEVICE_RETURN_FOREIGN_OWNER_UNTOUCHED");
            // A hotplug may already have removed the exact anonymous sink.
            // Only a proven NO_SOURCE endpoint can enter SOURCE-only retry.
            installedMonitor_.reset();
            measuredSourceRecovery_=true;
            measuredReturnCue_=original.position;
            measuredReturnState_=original.state;
            status_=QStringLiteral("MEASURED_DEVICE_SOURCE_RECOVERY_REQUIRED");
            emit changed();
            return return_measured_device_audition();
        }
        core::FrameIndex cue=original.position;
        core::PlaybackState resume=original.state;
        if(status_!=QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED")) {
            const auto displayed=monitorDisplay_(installedMonitor_);
            if(!displayed)return core::Status::failure(*displayed.error());
            resume=displayed.value()->state;
            const auto m=displayed.value()->master_display_position.value();
            const auto masterHz=plan.master_rate.value();
            const auto sourceHz=plan.source_rate.value();
            if(m<0||masterHz<=0||sourceHz<=0
               ||m>installedMonitor_->immutable_master_frames.value())
                return deny("M17_MEASURED_DEVICE_RETURN_MASTER_CUE_INVALID");
            const auto q=m/masterHz;
            const auto rem=m%masterHz;
            if(q>(std::numeric_limits<std::int64_t>::max()
                    -(rem*sourceHz)/masterHz)/sourceHz)
                return deny("M17_MEASURED_DEVICE_RETURN_CUE_OVERFLOW");
            cue=core::FrameIndex{std::min<std::int64_t>(
                q*sourceHz+(rem*sourceHz)/masterHz,
                pending_->manual->view().frame_count().value())};
        }
        if(plan.source_loop
           &&(cue<plan.source_loop->begin()||cue>=plan.source_loop->end()))
            cue=plan.source_loop->begin();
        if(!manual_||!manualId_||!sourceLoop_||!selected_
           ||!selected_()||manual_()!=pending_->manual
           ||manualId_()!=std::optional{pending_->manualId}
           ||sourceLoop_()!=plan.source_loop)
            return deny("M17_MEASURED_DEVICE_RETURN_SOURCE_EPOCH_CHANGED");
        if(manualRestorePreflight_) {
            const auto preflight=[&]()->core::Status {
                try {
                    return manualRestorePreflight_(pending_->manual);
                } catch(const std::bad_alloc&) {
                    return deny("M17_MEASURED_RETURN_PREFLIGHT_OUT_OF_MEMORY");
                } catch(const std::exception&) {
                    return deny("M17_MEASURED_RETURN_PREFLIGHT_EXCEPTION");
                }
            }();
            if(!preflight)return preflight; // Retain audible monitor.
        }
        monitorTransitioning_=true;
        const auto retired=[&]()->core::Status {
            try {
                return monitorRetire_(installedMonitor_);
            } catch(const std::exception&) {
                return deny("M17_MEASURED_RETURN_RETIRE_THROWN");
            }
        }();
        if(!retired) {
            status_=QStringLiteral("MEASURED_DEVICE_RECOVERY_REQUIRED");
            monitorTransitioning_=false;
            emit changed();
            return retired;
        }
        installedMonitor_.reset();
        measuredSourceRecovery_=true;
        measuredReturnCue_=cue;
        measuredReturnState_=resume;
        status_=QStringLiteral("MEASURED_DEVICE_SOURCE_RECOVERY_REQUIRED");
        monitorTransitioning_=false;
        emit changed();
    }
    // The exact DEVICE lease has been retired; never overwrite a successor
    // sink or synthesize a MANUAL realization when a Windows install fails.
    const auto empty=transport_->playback_snapshot();
    if(!empty)return core::Status::failure(*empty.error());
    // Windows may have installed the exact original MANUAL but returned an
    // error before the transport-state proof. A retry must FINISH that
    // same owner, not attempt a second QAudioSink replacement or overwrite
    // another user's playback device.
    const bool exactReopenedManual=
        empty.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::NEW
        &&empty.value()->audibleRealization.realizationId
            ==std::optional{pending_->manualId}
        &&empty.value()->duration
            ==std::optional{pending_->manual->view().frame_count()}
        &&empty.value()->loop==plan.source_loop
        &&(empty.value()->state==core::PlaybackState::STOPPED
            ||empty.value()->state==measuredReturnState_)
        &&(empty.value()->state==core::PlaybackState::PLAYING
            ||empty.value()->position==measuredReturnCue_);
    if(measuredSourceRecovery_&&exactReopenedManual) {
        if(!manual_||!manualId_||!sourceLoop_||!selected_
           ||!selected_()||manual_()!=pending_->manual
           ||manualId_()!=std::optional{pending_->manualId}
           ||sourceLoop_()!=plan.source_loop)
            return deny("M17_MEASURED_RETURN_ALREADY_RESTORED_SOURCE_REVOKED");
        if(empty.value()->state==core::PlaybackState::STOPPED
           &&measuredReturnState_!=core::PlaybackState::STOPPED) {
            transport_->playOrResume();
            if(measuredReturnState_==core::PlaybackState::PAUSED)
                transport_->pause();
        }
        const auto final=transport_->playback_snapshot();
        if(!final||final.value()->state!=measuredReturnState_
           ||final.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::NEW
           ||final.value()->audibleRealization.realizationId
               !=std::optional{pending_->manualId}
           ||final.value()->duration
               !=std::optional{pending_->manual->view().frame_count()}
           ||final.value()->loop!=plan.source_loop)
            return deny("M17_MEASURED_RETURN_ALREADY_RESTORED_TRANSPORT_UNPROVEN");
        installedMeasuredTicket_.reset();
        installedMeasuredPlan_.reset();
        measuredOriginalManual_.reset();
        measuredSourceRecovery_=false;
        status_=QStringLiteral("MEASURED_NONCONFORMING_READY");
        error_.clear();
        emit changed();
        return core::Status::success();
    }
    if(empty.value()->state!=core::PlaybackState::NO_SOURCE
       ||empty.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::UNAVAILABLE
       ||empty.value()->audibleRealization.realizationId
       ||empty.value()->duration||empty.value()->loop)
        return deny("M17_MEASURED_RETURN_EMPTY_SINK_PROOF_REQUIRED");
    if(!manual_||!manualId_||!sourceLoop_||!selected_
       ||!selected_()||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId}
       ||sourceLoop_()!=plan.source_loop)
        return deny("M17_MEASURED_RETURN_MANUAL_EPOCH_REVOKED");
    monitorTransitioning_=true;
    const auto restored=[&]()->core::Status {
        try {
            return manualRestore_(
                pending_->manual,pending_->manualId,measuredReturnCue_);
        } catch(const std::bad_alloc&) {
            return deny("M17_MEASURED_RETURN_SOURCE_OUT_OF_MEMORY");
        } catch(const std::exception&) {
            return deny("M17_MEASURED_RETURN_SOURCE_REINSTALL_EXCEPTION");
        }
    }();
    if(!restored) {
        monitorTransitioning_=false;
        return restored; // Retry token remains; no alien sink mutation.
    }
    const auto stopped=transport_->playback_snapshot();
    if(!stopped||stopped.value()->state!=core::PlaybackState::STOPPED
       ||stopped.value()->position!=measuredReturnCue_
       ||stopped.value()->duration
            !=std::optional{pending_->manual->view().frame_count()}
       ||stopped.value()->loop!=plan.source_loop
       ||stopped.value()->audibleRealization.phase
            !=core::AudibleHandoffPhase::NEW
       ||stopped.value()->audibleRealization.realizationId
            !=std::optional{pending_->manualId}) {
        monitorTransitioning_=false;
        return deny("M17_MEASURED_RETURN_MANUAL_REINSTALL_UNPROVEN");
    }
    if(measuredReturnState_!=core::PlaybackState::STOPPED) {
        transport_->playOrResume();
        if(measuredReturnState_==core::PlaybackState::PAUSED)
            transport_->pause();
    }
    const auto after=transport_->playback_snapshot();
    if(!after||after.value()->state!=measuredReturnState_
       ||after.value()->audibleRealization.realizationId
            !=std::optional{pending_->manualId}) {
        monitorTransitioning_=false;
        return deny("M17_MEASURED_RETURN_MANUAL_TRANSPORT_UNPROVEN");
    }
    installedMeasuredTicket_.reset();
    installedMeasuredPlan_.reset();
    measuredOriginalManual_.reset();
    measuredSourceRecovery_=false;
    status_=QStringLiteral("MEASURED_NONCONFORMING_READY");
    error_.clear();
    monitorTransitioning_=false;
    emit changed();
    return core::Status::success();
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
    // The exact capture is preserved across conversion. The eventual
    // asynchronous path can use the SAME owning-thread-only admission, so
    // only one authoritative Q0/M17 monitor-ticket security decision exists.
    if(!result)return result;
    const auto admitted=admit_converted_monitor_owner(
        stage,*result.value());
    if(!admitted)return core::Result<Owned>::failure(*admitted.error());
    return result;
}

TerminalPrivateProductSession::MonitoringMasterCapture
TerminalPrivateProductSession::capture_monitor_master_for_conversion() const
{
    // Do not even inspect source/transport state from a DSP worker.
    if(QThread::currentThread()!=thread())return {};
    // Capture ONLY on the owning UI thread. This pointer owns the complete
    // immutable M17 MASTER PCM for the later worker's bounded lifetime.
    return current_verified_private_master();
}

core::Status TerminalPrivateProductSession::admit_converted_monitor_owner(
    MonitoringMasterCapture captured,
    std::shared_ptr<const audio::MonitoringPcmCandidate> candidate)
{
    const auto refuse=[](const char* message)->core::Status {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,message});
    };
    // A posted asynchronous completion must return to the owning QObject
    // thread before touching source, transport or candidate gate. Checking
    // this FIRST avoids even a read of mutable app state from a DSP worker.
    if(QThread::currentThread()!=thread())
        return refuse("M17_MONITOR_ADMISSION_REQUIRES_OWNER_THREAD");
    // This method intentionally neither touches Windows hardware nor
    // constructs PCM. It must be invoked by the caller on the owning thread.
    if(!captured ||current_verified_private_master()!=captured)
        return refuse("M17_MONITOR_MASTER_REVOKED_DURING_MATERIALIZATION");
    if(!candidate)return refuse("M17_MONITOR_DEVICE_PCM_OWNER_MISSING");
    const auto checked=audio::validate_monitoring_pcm_candidate(*candidate);
    if(!checked)return checked;
    const auto master=captured->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    if(candidate->immutable_master_rate!=master.format().sample_rate()
       ||candidate->immutable_master_frames!=master.frame_count()
       ||candidate->device_pcm.view().format().channel_layout()
           !=master.format().channel_layout())
        return refuse("M17_MONITOR_MASTER_DEVICE_PROVENANCE_MISMATCH");
    approvedMonitor_=std::move(candidate);
    approvedMonitorMaster_=std::move(captured);
    // No automatic PLAY/DEVICE install, SOURCE retarget or M16 LIVE claim.
    return core::Status::success();
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
    // An initial Windows SOURCE-format rejection can leave a sound
    // complete SOURCE/MANUAL owner with NO audible MANUAL sink. It is NOT
    // legal to invent one. Only a previously explicit, current offline
    // provenance ticket may replace a *demonstrably empty* endpoint.
    const Pending predecessor=*pending_;
    const bool offlineOrigin=predecessor.offlineNoSink
        &&predecessor.offlineNonAudibleSelection;
    const auto before=transport_->playback_snapshot();
    if(!before)return core::Status::failure(rejected(
        "MONITOR_INSTALL_SOURCE_SNAPSHOT_UNAVAILABLE"));
    // Map the current MANUAL transport into MASTER frames before replacing
    // its DEVICE sink. Only the exact immutable, matched predecessor may
    // be handed off; SOURCE loops are not DSP/render ownership changes.
    const auto mapped=audio::MonitoringFrameMap::create(*candidate);
    if(!mapped)return core::Status::failure(*mapped.error());
    core::FrameIndex masterCue{0};
    std::optional<core::FrameRange> masterLoop;
    const auto state=before.value()->state;
    const bool sourceClock=(state==core::PlaybackState::STOPPED
        ||state==core::PlaybackState::PLAYING
        ||state==core::PlaybackState::PAUSED);
    if(sourceClock) {
        if(before.value()->position.value()<0
           ||before.value()->position.value()
                >predecessor.manual->view().frame_count().value()
           ||before.value()->loop!=predecessor.loop
           ||!sourceLoop_||sourceLoop_()!=predecessor.loop)
            return core::Status::failure(rejected(
                "MONITOR_INSTALL_SOURCE_CUE_OR_LOOP_EPOCH_CHANGED"));
        const auto hz=predecessor.manual->view().format().sample_rate();
        const auto n=predecessor.manual->view().frame_count();
        const auto cue=mapped.value()->source_to_master(
            before.value()->position,hz,n);
        if(!cue)return core::Status::failure(*cue.error());
        masterCue=*cue.value();
        // A SOURCE Loop is preserved solely for the eventual MANUAL
        // return. Master audition deliberately does NOT inherit Loop:
        // full-program listening and seek are the only monitor controls.
    }
    const bool requiresTransport=(sourceClock
        &&(state!=core::PlaybackState::STOPPED
            ||masterCue!=core::FrameIndex{0}||masterLoop));
    if(requiresTransport &&(!monitorControl_||!monitorDisplay_))
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_MAPPED_TRANSPORT_PORTS_REQUIRED"));
    const bool empty=before.value()->state==core::PlaybackState::NO_SOURCE
        &&before.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::UNAVAILABLE
        &&!before.value()->audibleRealization.realizationId
        &&before.value()->position==core::FrameIndex{0}
        &&!before.value()->duration &&!before.value()->loop;
    const bool stopped=sourceClock
        &&before.value()->loop==predecessor.loop
        &&before.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::NEW
        &&before.value()->audibleRealization.realizationId
            ==std::optional{predecessor.manualId}
        &&before.value()->duration
            ==std::optional{predecessor.manual->view().frame_count()};
    if(offlineOrigin
        ?(!empty||!offlineProvenance_||!offlineProvenance_())
        :!stopped)
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_REQUIRES_EXACT_EMPTY_OR_MANUAL_PREDECESSOR"));
    // The DEVICE callback may fail AFTER replacing the MANUAL sink.
    // Restore the original SOURCE cue, Loop and Play/Pause state, but only
    // after exact-owner DEVICE retirement and unchanged SOURCE provenance.
    const auto restoreOriginalManual=[&]()->core::Status {
        if(offlineOrigin||!sourceClock||!selected_||!selected_()
           ||!manual_||manual_()!=predecessor.manual
           ||!manualId_||manualId_()!=std::optional{predecessor.manualId}
           ||!sourceLoop_||sourceLoop_()!=predecessor.loop
           ||!pending_||pending_->manual!=predecessor.manual
           ||pending_->manualId!=predecessor.manualId)
            return core::Status::failure(rejected(
                "MONITOR_INSTALL_RECOVERY_MANUAL_EPOCH_CHANGED"));
        const auto restored=manualRestore_(
            predecessor.manual,predecessor.manualId,before.value()->position);
        if(!restored)return restored;
        const auto stoppedSnapshot=transport_->playback_snapshot();
        if(!stoppedSnapshot
           ||stoppedSnapshot.value()->state!=core::PlaybackState::STOPPED
           ||stoppedSnapshot.value()->position!=before.value()->position
           ||stoppedSnapshot.value()->loop!=predecessor.loop
           ||stoppedSnapshot.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::NEW
           ||stoppedSnapshot.value()->audibleRealization.realizationId
               !=std::optional{predecessor.manualId}
           ||stoppedSnapshot.value()->duration
               !=std::optional{predecessor.manual->view().frame_count()})
            return core::Status::failure(rejected(
                "MONITOR_INSTALL_RECOVERY_MANUAL_RESTORE_UNPROVEN"));
        if(state!=core::PlaybackState::STOPPED) {
            transport_->playOrResume();
            if(state==core::PlaybackState::PAUSED)transport_->pause();
        }
        const auto finalSnapshot=transport_->playback_snapshot();
        // PLAYING may have legitimately advanced after the exact-cue
        // stopped restore and Play command. PAUSED/STOPPED must stay at
        // the exact pre-install cue; the same return-MANUAL contract does
        // not demand a frozen PLAYING sample clock after resumption.
        if(!finalSnapshot||finalSnapshot.value()->state!=state
           ||(state!=core::PlaybackState::PLAYING
               &&finalSnapshot.value()->position!=before.value()->position)
           ||finalSnapshot.value()->loop!=predecessor.loop
           ||finalSnapshot.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::NEW
           ||finalSnapshot.value()->audibleRealization.realizationId
               !=std::optional{predecessor.manualId}
           ||finalSnapshot.value()->duration
               !=std::optional{predecessor.manual->view().frame_count()})
            return core::Status::failure(rejected(
                "MONITOR_INSTALL_RECOVERY_TRANSPORT_NOT_RESTORED"));
        return core::Status::success();
    };
    monitorRecoveryOriginal_.reset();
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
                monitorRecoveryOriginal_=*before.value();
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
        const bool emptyAfterFailure=current.value()->state==core::PlaybackState::NO_SOURCE
            &&current.value()->audibleRealization.phase
                ==core::AudibleHandoffPhase::UNAVAILABLE
            &&!current.value()->audibleRealization.realizationId
            &&current.value()->position==core::FrameIndex{0}
            &&!current.value()->duration &&!current.value()->loop;
        const bool original=current.value()->state==state
            &&current.value()->position==before.value()->position
            &&current.value()->loop==before.value()->loop
            &&current.value()->audibleRealization.phase
                ==core::AudibleHandoffPhase::NEW
            &&current.value()->audibleRealization.realizationId
                ==std::optional{predecessor.manualId}
            &&current.value()->duration
                ==std::optional{predecessor.manual->view().frame_count()};
        if((offlineOrigin&&!emptyAfterFailure)
           ||(!offlineOrigin&&!emptyAfterFailure&&!original)) {
            monitorTransitioning_=false;
            return core::Status::failure(rejected(
                "MONITOR_INSTALL_FAILURE_ALIEN_DEVICE_OWNER_UNTOUCHED"));
        }
        if(emptyAfterFailure && !offlineOrigin) {
            const auto restored=restoreOriginalManual();
            if(!restored) {
                monitorTransitioning_=false;
                failed(*restored.error());
                return restored;
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
        &&pending_->loop==predecessor.loop
        &&sourceLoop_&&sourceLoop_()==predecessor.loop
        &&(offlineOrigin
            ?(offlineProvenance_&&offlineProvenance_())
            :(selected_&&selected_()))
        &&manual_&&manual_()==predecessor.manual
        &&manualId_&&manualId_()==std::optional{predecessor.manualId}
        &&candidates_->staged()==masterStage
        &&renderer_->status()==QStringLiteral("READY_PRIVATE");
    if(!valid) {
        // Roll back only the exactly owned monitor. Never erase a successor.
        if(ownership&&*ownership.value()) {
            const auto retired=monitorRetire_(candidate);
            if(!retired) {
                // Positive exact ownership + failed retirement means the
                // DEVICE may still be active. Keep the same owner ticket
                // available for the explicit recovery/Return action.
                installedMonitor_=candidate;
                monitorRecoveryOriginal_=*before.value();
                status_=QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED");
                monitorTransitioning_=false;
                emit changed();
                return retired;
            }
            // Refuse a successor after retirement and propagate any
            // failed original MANUAL restoration rather than ignoring it.
            const auto vacant=transport_->playback_snapshot();
            if(!vacant
               ||vacant.value()->state!=core::PlaybackState::NO_SOURCE
               ||vacant.value()->audibleRealization.phase
                   !=core::AudibleHandoffPhase::UNAVAILABLE
               ||vacant.value()->audibleRealization.realizationId
               ||vacant.value()->position!=core::FrameIndex{0}
               ||vacant.value()->duration||vacant.value()->loop) {
                monitorTransitioning_=false;
                return core::Status::failure(rejected(
                    "MONITOR_INSTALL_POSTCHECK_RETIRE_NOT_EMPTY"));
            }
            if(!offlineOrigin) {
                const auto restored=restoreOriginalManual();
                if(!restored) {
                    monitorTransitioning_=false;
                    failed(*restored.error());
                    return restored;
                }
            }
        }
        monitorTransitioning_=false;
        return core::Status::failure(rejected(
            "MONITOR_INSTALL_LOST_EXACT_STOPPED_OWNER"));
    }
    // The Windows worker owns the new DEVICE PCM. Reapply the current
    // SOURCE cue and loop via the separate exact-owner monitoring control
    // seam; no second M17 job and no mutation of authoritative MASTER PCM.
    auto seeded=core::Status::success();
    if(requiresTransport) {
        using Action=platform::windows::MonitoringTransportAction;
        if(masterLoop)
            seeded=monitorControl_(candidate,{
                Action::SET_MASTER_LOOP,core::FrameIndex{0},masterLoop});
        if(seeded &&masterCue!=core::FrameIndex{0})
            seeded=monitorControl_(candidate,{
                Action::SEEK_MASTER,masterCue,std::nullopt});
        if(seeded &&state!=core::PlaybackState::STOPPED)
            seeded=monitorControl_(candidate,{
                Action::PLAY,core::FrameIndex{0},std::nullopt});
        if(seeded &&state==core::PlaybackState::PAUSED)
            seeded=monitorControl_(candidate,{
                Action::PAUSE,core::FrameIndex{0},std::nullopt});
        if(seeded) {
            const auto display=monitorDisplay_(candidate);
            if(!display ||display.value()->state!=state
               ||display.value()->requested_master_loop!=masterLoop
               ||display.value()->master_rate!=candidate->immutable_master_rate
               ||display.value()->device_rate!=candidate->device_rate)
                seeded=core::Status::failure(rejected(
                    "MONITOR_INSTALL_MAPPED_TRANSPORT_POSTCHECK_FAILED"));
        }
    }
    if(!seeded) {
        // Preserve a positive exact-owner recovery ticket. An incomplete
        // install must never be published as fully operational or clear an
        // unrelated successor device. Return MANUAL remains explicitly
        // available and independently validates current device ownership.
        installedMonitor_=std::move(candidate);
        monitorRecoveryOriginal_=*before.value();
        status_=QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED");
        monitorTransitioning_=false;
        emit changed();
        return seeded;
    }
    installedMonitor_=std::move(candidate);
    monitorRecoveryOriginal_.reset();
    status_=state==core::PlaybackState::PLAYING
        ?QStringLiteral("MONITOR_PRIVATE_PLAYING")
        :state==core::PlaybackState::PAUSED
            ?QStringLiteral("MONITOR_PRIVATE_PAUSED")
            :QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED");
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
       ||!(pending_->offlineNonAudibleSelection
            ?(offlineProvenance_&&offlineProvenance_())
            :(selected_&&selected_()))
       ||!manual_||!manualId_
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
    // A failed installation may leave an exact-owner recovery ticket but
    // NOT a complete DEVICE transport. Only conditional Return/retirement
    // may operate here; never expose a partial monitor for PLAY/SEEK/LOOP.
    if(status_!=QStringLiteral("MONITOR_PRIVATE_PLAYING")
       &&status_!=QStringLiteral("MONITOR_PRIVATE_PAUSED")
       &&status_!=QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"))
        return core::Status::failure(rejected(
            "MONITOR_CONTROL_REQUIRES_COMMITTED_DEVICE_TRANSPORT"));
    const auto owner=installedMonitor_;
    const auto before=private_monitor_master_snapshot();
    if(!before)return core::Status::failure(*before.error());
    // Without a prior audible MANUAL, a MASTER Loop Region has no safe
    // SOURCE loop to commit on return. Play/Pause/Stop/Seek remain
    // independently owner-checked by the Windows worker.
    if(pending_&&pending_->offlineNonAudibleSelection
       &&command.action==platform::windows::MonitoringTransportAction::SET_MASTER_LOOP
       &&command.master_loop)
        return core::Status::failure(rejected(
            "MONITOR_OFFLINE_ORIGIN_REJECTS_UNRESTORABLE_SOURCE_LOOP"));
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

core::Status
TerminalPrivateProductSession::retry_source_return_after_monitor_retirement()
{
    // This is an explicit recovery, NOT a new MASTER render and NOT a
    // second anonymous DEVICE realization. The original M17 PCM remains
    // held by pending_/candidate stage until the SOURCE is genuinely back.
    if(monitorTransitioning_ ||installedMonitor_ ||!returnRecovery_
       ||status_!=QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED")
       ||!pending_ ||!manualRestore_ ||!transport_)
        return core::Status::failure(rejected(
            "MONITOR_RETURN_RECOVERY_NOT_AVAILABLE"));
    const Pending predecessor=*pending_;
    const ReturnRecovery recovery=*returnRecovery_;
    if(!candidates_ ||!recovery.capturedVerifiedMaster
       ||candidates_->staged()!=recovery.capturedVerifiedMaster)
        return core::Status::failure(rejected(
            "MONITOR_RETURN_RECOVERY_VERIFIED_STAGE_REVOKED"));
    if(predecessor.offlineNoSink ||!manual_ ||!prepared_ ||!manualId_
       ||!sourceLoop_ ||!selected_
       ||manual_()!=predecessor.manual
       ||prepared_()!=predecessor.prepared
       ||manualId_()!=std::optional{predecessor.manualId}
       ||sourceLoop_()!=predecessor.loop
       ||!selected_())
        return core::Status::failure(rejected(
            "MONITOR_RETURN_RECOVERY_SOURCE_EPOCH_REVOKED"));
    const auto empty=transport_->playback_snapshot();
    if(!empty ||empty.value()->state!=core::PlaybackState::NO_SOURCE
       ||empty.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::UNAVAILABLE
       ||empty.value()->audibleRealization.realizationId
       ||empty.value()->position!=core::FrameIndex{0}
       ||empty.value()->duration ||empty.value()->loop)
        return core::Status::failure(rejected(
            "MONITOR_RETURN_RECOVERY_REFUSES_OCCUPIED_OR_ALIEN_DEVICE"));
    if(manualRestorePreflight_) {
        const auto preflight=[&]()->core::Status {
            try {
                return manualRestorePreflight_(predecessor.manual);
            } catch(const std::bad_alloc&) {
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "MONITOR_RETURN_RECOVERY_PREFLIGHT_OUT_OF_MEMORY"});
            } catch(const std::exception&) {
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "MONITOR_RETURN_RECOVERY_PREFLIGHT_EXCEPTION"});
            }
        }();
        if(!preflight)return preflight;
    }
    monitorTransitioning_=true;
    const auto failedRecovery=[&](const core::Error& error)->core::Status {
        monitorTransitioning_=false;
        status_=QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED");
        error_=QString::fromStdString(error.message());
        emit changed();
        return core::Status::failure(error);
    };
    const auto restored=[&]()->core::Status {
        try {
            return manualRestore_(predecessor.manual,predecessor.manualId,
                recovery.sourceCue);
        } catch(const std::bad_alloc&) {
            return core::Status::failure(core::Error{
                core::ErrorCode::IoFailure,
                "MONITOR_RETURN_RECOVERY_SOURCE_OUT_OF_MEMORY"});
        } catch(const std::exception&) {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "MONITOR_RETURN_RECOVERY_SOURCE_EXCEPTION"});
        }
    }();
    if(!restored)return failedRecovery(*restored.error());
    const auto verifyExactStoppedManual=
        [&](std::optional<core::FrameRange> loop)->core::Status {
        const auto proof=transport_->playback_snapshot();
        const bool exact=proof
           &&proof.value()->state==core::PlaybackState::STOPPED
           &&proof.value()->audibleRealization.phase
               ==core::AudibleHandoffPhase::NEW
           &&proof.value()->audibleRealization.realizationId
               ==std::optional{predecessor.manualId}
           &&proof.value()->duration
               ==std::optional{predecessor.manual->view().frame_count()}
           &&proof.value()->position==recovery.sourceCue
           &&proof.value()->loop==loop;
        if(exact)return core::Status::success();
        // Diagnostic must be just as precise on RETRY as on the initial
        // handoff. No PCM/user paths/realization IDs are logged, and the
        // strict proof is never bypassed to obtain a green test.
        const char* category=!proof
            ?"MONITOR_RETURN_SNAPSHOT_UNAVAILABLE"
            :proof.value()->state!=core::PlaybackState::STOPPED
                ?"MONITOR_RETURN_NOT_STOPPED"
            :proof.value()->audibleRealization.phase
                !=core::AudibleHandoffPhase::NEW
                ?"MONITOR_RETURN_PHASE_MISMATCH"
            :proof.value()->audibleRealization.realizationId
                !=std::optional{predecessor.manualId}
                ?"MONITOR_RETURN_REALIZATION_MISMATCH"
            :proof.value()->duration
                !=std::optional{predecessor.manual->view().frame_count()}
                ?"MONITOR_RETURN_DURATION_MISMATCH"
            :proof.value()->position!=recovery.sourceCue
                ?"MONITOR_RETURN_POSITION_MISMATCH"
                :"MONITOR_RETURN_LOOP_MISMATCH";
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            std::string("MONITOR_RETURN_MANUAL_SINK_NOT_EXACT:")+category,
            {{"category",category}}});
    };
    const auto originalProof=verifyExactStoppedManual(predecessor.loop);
    if(!originalProof)return failedRecovery(*originalProof.error());
    if(recovery.reconciledSourceLoop) {
        if(!sourceLoopReconcile_)return failedRecovery(rejected(
            "MONITOR_RETURN_RECOVERY_LOOP_CALLBACK_MISSING"));
        const auto committed=sourceLoopReconcile_(
            *recovery.reconciledSourceLoop);
        if(!committed)return failedRecovery(*committed.error());
        if(!sourceLoop_||sourceLoop_()
               !=recovery.reconciledSourceLoop)
            return failedRecovery(rejected(
                "MONITOR_RETURN_RECOVERY_RECONCILED_LOOP_NOT_EXACT"));
        const auto loopProof=verifyExactStoppedManual(
            recovery.reconciledSourceLoop);
        if(!loopProof)return failedRecovery(*loopProof.error());
    }
    if(recovery.priorTransport!=core::PlaybackState::STOPPED) {
        transport_->playOrResume();
        if(recovery.priorTransport==core::PlaybackState::PAUSED)
            transport_->pause();
        const auto final=transport_->playback_snapshot();
        if(!final ||final.value()->state!=recovery.priorTransport
           ||final.value()->audibleRealization.phase
                !=core::AudibleHandoffPhase::NEW
           ||final.value()->audibleRealization.realizationId
                !=std::optional{predecessor.manualId}
           ||final.value()->duration
                !=std::optional{predecessor.manual->view().frame_count()}
           ||final.value()->loop!=(recovery.reconciledSourceLoop
                ?recovery.reconciledSourceLoop:predecessor.loop)) {
            // Retry only if we can prove the device is later empty; a
            // partially installed or foreign sink must never be cleared.
            return failedRecovery(rejected(
                "MONITOR_RETURN_RECOVERY_TRANSPORT_NOT_EXACT"));
        }
    }
    monitorTransitioning_=false;
    monitorRecoveryOriginal_.reset();
    returnRecovery_.reset();
    if(recovery.reconciledSourceLoop) {
        pending_.reset();
        approvedMonitor_.reset();
        approvedMonitorMaster_.reset();
        status_=QStringLiteral("IDLE");
        renderer_->invalidate();
    } else {
        status_=predecessor.masterOnly
            ?QStringLiteral("MASTER_READY_MONITOR_PENDING")
            :QStringLiteral("READY_TO_INSTALL");
    }
    error_.clear();
    emit changed();
    return core::Status::success();
}

core::Status TerminalPrivateProductSession::return_private_monitor_to_manual()
{
    if(QThread::currentThread()!=thread())
        return core::Status::failure(rejected(
            "MONITOR_RETURN_REQUIRES_OWNER_THREAD"));
    // A monitored DEVICE was already proven retired, but a later Windows
    // SOURCE installation failed. This branch never adopts a foreign sink.
    if(returnRecovery_)
        return retry_source_return_after_monitor_retirement();
    if(monitorTransitioning_||!installedMonitor_||!pending_
       ||!monitorOwned_||!monitorRetire_||!manualRestore_)
        return core::Status::failure(rejected(
            "MONITOR_RETURN_HAS_NO_EXACT_INSTALLED_OWNER"));
    const auto candidate=installedMonitor_;
    const Pending predecessor=*pending_;
    if(predecessor.offlineNoSink&&predecessor.offlineNonAudibleSelection) {
        // Offline origin has NO installed MANUAL to return to. Retire ONLY
        // the exact anonymous monitor; never synthesize a NEW audible ID
        // or attempt to reinstall unsupported original-rate SOURCE PCM.
        // This recovery is allowed even if the SOURCE provenance expired.
        const auto owned=monitorOwned_(candidate);
        if(!owned)return core::Status::failure(*owned.error());
        if(!*owned.value())
            return core::Status::failure(rejected(
                "MONITOR_RETURN_OFFLINE_ALIEN_DEVICE_UNTOUCHED"));
        monitorTransitioning_=true;
        const auto retired=monitorRetire_(candidate);
        if(!retired) {
            status_=QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED");
            monitorTransitioning_=false;
            emit changed();
            return retired;
        }
        installedMonitor_.reset();
        monitorRecoveryOriginal_.reset();
        const auto after=transport_->playback_snapshot();
        if(!after||after.value()->state!=core::PlaybackState::NO_SOURCE
           ||after.value()->audibleRealization.phase
               !=core::AudibleHandoffPhase::UNAVAILABLE
           ||after.value()->audibleRealization.realizationId
           ||after.value()->position!=core::FrameIndex{0}
           ||after.value()->duration||after.value()->loop) {
            monitorTransitioning_=false;
            renderer_->invalidate();
            failed(rejected("MONITOR_RETURN_OFFLINE_DID_NOT_EMPTY_DEVICE"));
            return core::Status::failure(rejected(
                "MONITOR_RETURN_OFFLINE_DID_NOT_EMPTY_DEVICE"));
        }
        pending_.reset();
        approvedMonitor_.reset();
        approvedMonitorMaster_.reset();
        status_=QStringLiteral("IDLE");
        error_.clear();
        renderer_->invalidate();
        monitorTransitioning_=false;
        emit changed();
        return core::Status::success();
    }
    // The Windows MONITOR snapshot is in MASTER frames, not device or
    // SOURCE units. Convert the display cue to the original SOURCE domain
    // with bounded integer arithmetic. A stopped monitor without the new
    // controller retains the original legacy zero-cue return.
    core::FrameIndex sourceCue{0};
    std::optional<core::FrameRange> reconciledSourceLoop;
    core::PlaybackState priorMonitorState=core::PlaybackState::STOPPED;
    if(monitorRecoveryOriginal_) {
        // Recovery after failed SET_LOOP/SEEK/PLAY/PAUSE must use the
        // pre-install MANUAL snapshot, never the partial monitor clock.
        const auto& original=*monitorRecoveryOriginal_;
        if(!sourceLoop_||sourceLoop_()!=predecessor.loop
           ||original.loop!=predecessor.loop
           ||original.audibleRealization.phase!=core::AudibleHandoffPhase::NEW
           ||original.audibleRealization.realizationId
               !=std::optional{predecessor.manualId}
           ||original.duration
               !=std::optional{predecessor.manual->view().frame_count()}
           ||(original.state!=core::PlaybackState::STOPPED
               &&original.state!=core::PlaybackState::PLAYING
               &&original.state!=core::PlaybackState::PAUSED))
            return core::Status::failure(rejected(
                "MONITOR_RETURN_RECOVERY_ORIGINAL_EPOCH_MISMATCH"));
        sourceCue=original.position;
        priorMonitorState=original.state;
    } else if(monitorDisplay_) {
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
        if(!sourceLoop_||sourceLoop_()!=predecessor.loop)
            return core::Status::failure(rejected(
                "MONITOR_RETURN_SOURCE_LOOP_CHANGED_DURING_MONITOR"));
        if(display.value()->requested_master_loop) {
            const auto map=audio::MonitoringFrameMap::create(*candidate);
            if(!map)return core::Status::failure(*map.error());
            std::optional<core::FrameRange> inherited;
            if(predecessor.loop) {
                const auto original=map.value()->source_loop_to_master(
                    *predecessor.loop,
                    predecessor.manual->view().format().sample_rate(),
                    predecessor.manual->view().frame_count());
                if(!original)return core::Status::failure(*original.error());
                inherited=*original.value();
            }
            // Inherited SOURCE loop must survive a no-change round trip
            // bit-exactly, including when rational-rate ceil projection
            // would otherwise shift its boundary by one frame.
            if(display.value()->requested_master_loop!=inherited) {
                // A changed loop while auditioning is audition-local when
                // the MANUAL had a loop: return restores the exact original.
                // On a MANUAL without a loop, the existing explicit SOURCE
                // reconciliation transaction may adopt a new requested loop.
                if(!predecessor.loop) {
                    if(!sourceLoopReconcile_)
                        return core::Status::failure(rejected(
                            "MONITOR_RETURN_EXPLICIT_SOURCE_LOOP_TRANSACTION_REQUIRED"));
                    const auto projected=map.value()->master_loop_to_source(
                        *display.value()->requested_master_loop,
                        predecessor.manual->view().format().sample_rate(),
                        predecessor.manual->view().frame_count());
                    if(!projected)
                        return core::Status::failure(*projected.error());
                    reconciledSourceLoop=*projected.value();
                }
            }
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
    // The original SOURCE loop may restart playback at its beginning
    // when a returned MASTER cue lies outside its half-open interval.
    // Normalize BEFORE the strict MANUAL restoration proof rather than
    // blaming the genuinely restored sink for a legal loop clamp.
    if(predecessor.loop
       &&(sourceCue<predecessor.loop->begin()
          ||sourceCue>=predecessor.loop->end()))
        sourceCue=predecessor.loop->begin();
    // A failed Windows SOURCE/PROCESSED rate admission must not first
    // retire the exact MASTER monitor and leave the user with NO_SOURCE.
    // The callback probes endpoint formats and pinned FIR only, without
    // touching the running monitor or changing any SOURCE/MASTER revision.
    // This is NOT a guarantee against a later Windows hotplug; the old
    // strict post-restore owner/loop proof remains mandatory.
    if(manualRestorePreflight_) {
        const auto preflight=[&]()->core::Status {
            try {
                return manualRestorePreflight_(predecessor.manual);
            } catch(const std::bad_alloc&) {
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "MONITOR_RETURN_SOURCE_PREFLIGHT_OUT_OF_MEMORY"});
            } catch(const std::exception&) {
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "MONITOR_RETURN_SOURCE_PREFLIGHT_EXCEPTION"});
            }
        }();
        if(!preflight)return preflight;
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
    // All failures AFTER the exact monitor has retired must retain the
    // authoritative MASTER and typed timeline. Never fabricate a MANUAL
    // audible certificate, and never overwrite a successor DEVICE sink.
    const auto preserveReturnFailure=[&](const core::Error& error)
        ->core::Status {
        returnRecovery_=ReturnRecovery{
            sourceCue,priorMonitorState,reconciledSourceLoop,
            candidates_?candidates_->staged():nullptr};
        monitorTransitioning_=false;
        status_=QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED");
        error_=QString::fromStdString(error.message());
        emit changed();
        return core::Status::failure(error);
    };
    const auto vacant=transport_->playback_snapshot();
    if(!vacant||vacant.value()->state!=core::PlaybackState::NO_SOURCE
       ||vacant.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::UNAVAILABLE
       ||vacant.value()->audibleRealization.realizationId) {
        return preserveReturnFailure(rejected(
            "MONITOR_RETURN_RETIRE_DID_NOT_EMPTY_DEVICE"));
    }
    if(!selected_||!selected_()||!manual_||!manualId_
       ||manual_()!=predecessor.manual
       ||manualId_()!=std::optional{predecessor.manualId}) {
        return preserveReturnFailure(rejected(
            "MONITOR_RETURN_STALE_MANUAL_OWNER"));
    }
    const auto restored=[&]()->core::Status {
        try {
            return manualRestore_(
                predecessor.manual,predecessor.manualId,sourceCue);
        } catch(const std::bad_alloc&) {
            return core::Status::failure(core::Error{
                core::ErrorCode::IoFailure,
                "MONITOR_RETURN_SOURCE_INSTALL_OUT_OF_MEMORY_AFTER_RETIRE"});
        } catch(const std::exception&) {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "MONITOR_RETURN_SOURCE_INSTALL_EXCEPTION_AFTER_RETIRE"});
        }
    }();
    if(!restored)
        return preserveReturnFailure(*restored.error());
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
        // Diagnose the exact failed predicate without logging PCM, file
        // paths, channel samples or realisation IDs. This never weakens
        // the strict proof and retains the immutable MASTER for recovery.
        const char* category=!after
            ?"MONITOR_RETURN_SNAPSHOT_UNAVAILABLE"
            :after.value()->state!=core::PlaybackState::STOPPED
                ?"MONITOR_RETURN_NOT_STOPPED"
            :after.value()->audibleRealization.phase
                !=core::AudibleHandoffPhase::NEW
                ?"MONITOR_RETURN_PHASE_MISMATCH"
            :after.value()->audibleRealization.realizationId
                !=std::optional{predecessor.manualId}
                ?"MONITOR_RETURN_REALIZATION_MISMATCH"
            :after.value()->duration
                !=std::optional{predecessor.manual->view().frame_count()}
                ?"MONITOR_RETURN_DURATION_MISMATCH"
            :after.value()->position!=sourceCue
                ?"MONITOR_RETURN_POSITION_MISMATCH"
                :"MONITOR_RETURN_LOOP_MISMATCH";
        return preserveReturnFailure(core::Error{
            core::ErrorCode::InvalidState,
            std::string("MONITOR_RETURN_MANUAL_SINK_NOT_EXACT:")+category,
            {{"category",category}}});
    }
    if(reconciledSourceLoop) {
        // The callback must act on the restored original MANUAL, never on
        // the anonymous monitor. Its SOURCE region is independently checked.
        const auto committed=[&]()->core::Status {
            try {
                return sourceLoopReconcile_(*reconciledSourceLoop);
            } catch(const std::bad_alloc&) {
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "MONITOR_RETURN_LOOP_RECONCILE_OUT_OF_MEMORY"});
            } catch(const std::exception&) {
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "MONITOR_RETURN_LOOP_RECONCILE_EXCEPTION"});
            }
        }();
        if(!committed)
            return preserveReturnFailure(*committed.error());
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
            return preserveReturnFailure(rejected(
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
           ||resumed.value()->loop!=(reconciledSourceLoop
                ?reconciledSourceLoop:predecessor.loop)) {
            // Do not announce restored playback and do not clear a
            // successor. Retire only the exact MANUAL owner if still there.
            if(resumed
               &&resumed.value()->audibleRealization.phase
                   ==core::AudibleHandoffPhase::NEW
               &&resumed.value()->audibleRealization.realizationId
                   ==std::optional{predecessor.manualId})
                static_cast<void>(transport_->stop_and_clear());
            return preserveReturnFailure(rejected(
                "MONITOR_RETURN_MANUAL_TRANSPORT_NOT_RESTORED"));
        }
    }
    monitorTransitioning_=false;
    monitorRecoveryOriginal_.reset();
    returnRecovery_.reset();
    if(reconciledSourceLoop) {
        // This SOURCE loop differs from the captured M17 render epoch.
        // Revoke the private MASTER ticket: new audition requires new render.
        pending_.reset();
        approvedMonitor_.reset();
        approvedMonitorMaster_.reset();
        status_=QStringLiteral("IDLE");
        renderer_->invalidate();
    } else {
        // Returning from DEVICE Audition does not consume the original
        // M17 READY candidate or force a second whole-master Prepare.
        // Only the legacy MASTER-only lane retains its own status.
        status_=predecessor.masterOnly
            ?QStringLiteral("MASTER_READY_MONITOR_PENDING")
            :QStringLiteral("READY_TO_INSTALL");
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

std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
TerminalPrivateProductSession::borrow_verified_m17_master_for_device(
    core::SampleRate selectedMasterRate) const
{
    // A passive private ownership read, NOT a permission to play, export,
    // claim verified LIVE/GR, or bypass the separate device-RAM preflight.
    if((status_!=QStringLiteral("READY_TO_INSTALL")
        &&status_!=QStringLiteral("COMPARISON_VERIFIED"))
       ||!pending_ || pending_->masterOnly ||privateId_
       ||installedMonitor_ ||monitorTransitioning_
       ||renderer_->status()!=QStringLiteral("READY_PRIVATE")
       ||pending_->outputRate!=selectedMasterRate
       ||!current_selection_for_pending()
       ||!prepared_||!manual_||!manualId_||!sourceLoop_
       ||prepared_()!=pending_->prepared
       ||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId}
       ||sourceLoop_()!=pending_->loop)
        return {};
    auto stage=candidates_->staged();
    if(!stage
       ||(status_==QStringLiteral("COMPARISON_VERIFIED")
           &&stage!=comparisonVerified_)
       ||stage->prepared_owner!=pending_->prepared
       ||stage->rendered.manual_owner!=pending_->manual
       ||stage->original_manual_realization_id!=pending_->manualId
       ||stage->rendered.output_rate!=selectedMasterRate
       ||!stage->rendered.terminal.checked.final_check)
        return {};
    const auto master=stage->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    if(master.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||master.absolute_start_frame()!=core::FrameIndex{0}
       ||master.frame_count().value()<=0
       ||master.format().sample_rate()!=selectedMasterRate
       ||master.format().channel_layout()!=pending_->manual->view()
           .format().channel_layout())
        return {};
    // A pending M17 READY state still has exact original MANUAL audio.
    // An installed M17 terminal is intentionally excluded above until its
    // own verified-device owner release transaction is implemented.
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
        return {};
    return stage;
}

core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>>
TerminalPrivateProductSession::materialize_borrowed_verified_m17_monitoring(
    core::SampleRate selectedMasterRate,
    const MonitoringMaterializer& materializer)
{
    using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
    auto refuse=[](const char* reason)->core::Result<Owned> {
        return core::Result<Owned>::failure(core::Error{
            core::ErrorCode::InvalidState,reason});
    };
    if(!materializer)
        return core::Result<Owned>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "M17_REUSE_EXPLICIT_MONITOR_MATERIALIZER_REQUIRED"});
    const auto stage=borrow_verified_m17_master_for_device(selectedMasterRate);
    if(!stage)return refuse("M17_REUSE_VERIFIED_MASTER_UNAVAILABLE");
    const auto master=stage->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    // The existing MASTER remains borrowed with shared lifetime; no fresh
    // full-resource M17 job and no SOURCE scratch copy is permitted here.
    auto converted=[&]()->core::Result<Owned> {
        try {
            return materializer(master);
        } catch(const std::bad_alloc&) {
            return core::Result<Owned>::failure(core::Error{
                core::ErrorCode::IoFailure,
                "M17_REUSE_MONITOR_OUT_OF_MEMORY"});
        }
    }();
    // An invalidating source/loop/limiter epoch during offline DEVICE SRC
    // must discard the outcome, even if the new candidate looks well formed.
    if(borrow_verified_m17_master_for_device(selectedMasterRate)!=stage)
        return refuse("M17_REUSE_MASTER_REVOKED_DURING_DEVICE_SRC");
    if(!converted)return converted;
    if(!*converted.value())
        return refuse("M17_REUSE_MONITOR_OWNER_MISSING");
    const auto coherent=audio::validate_monitoring_pcm_candidate(
        **converted.value());
    if(!coherent)return core::Result<Owned>::failure(*coherent.error());
    const auto& candidate=**converted.value();
    if(candidate.immutable_master_rate!=selectedMasterRate
       ||candidate.immutable_master_frames!=master.frame_count()
       ||candidate.device_pcm.view().format().channel_layout()
           !=master.format().channel_layout())
        return refuse("M17_REUSE_MONITOR_MASTER_PROVENANCE_MISMATCH");
    // No approvedMonitor_/installedMonitor_ mutation. The actual UI and
    // device install still require an independently designed P0 gate.
    return converted;
}

core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>>
TerminalPrivateProductSession::
materialize_borrowed_verified_m17_monitoring_with_preflight(
    core::SampleRate selectedMasterRate,
    core::FrameCount maximumBlockFrames,
    std::uint64_t reservedOtherPrepareBytes,
    const MonitoringPreallocationDecision& explicitDecision)
{
    using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
    if(!explicitDecision)
        return core::Result<Owned>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "M17_REUSE_EXPLICIT_MEMORY_DECISION_REQUIRED"});
    const auto preflight=monitoringPreflight_;
    if(!preflight)
        return core::Result<Owned>::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M17_REUSE_WINDOWS_PREFLIGHT_NOT_CONFIGURED"});
    // Never invoke Windows/Qt memory code from an audio callback or under
    // a DSP lock. The underlying master-identity check is repeated AFTER
    // this potentially expensive device conversion.
    return materialize_borrowed_verified_m17_monitoring(
        selectedMasterRate,
        [preflight,maximumBlockFrames,reservedOtherPrepareBytes,
         &explicitDecision](audio::AudioBufferView master) {
            return preflight(master,maximumBlockFrames,
                reservedOtherPrepareBytes,explicitDecision);
        });
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
    if(installedMeasuredTicket_ ||measuredSourceRecovery_) {
        const auto returned=return_measured_device_audition();
        if(!returned)return returned;
    }
    // Generic Cancel must not secretly retry a physical Windows install.
    // Retain the M17 MASTER while a dedicated explicit Return is required.
    if(returnRecovery_)
        return core::Status::failure(rejected(
            "MONITOR_RETURN_RECOVERY_EXPLICIT_RETRY_REQUIRED"));
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

bool TerminalPrivateProductSession::adopt_pending_source_loop_without_rerender()
{
    if(monitorTransitioning_ ||returnRecovery_ ||installedMonitor_
       ||privateId_||!pending_
       ||!prepared_||!manual_||!manualId_||!sourceLoop_
       ||!current_selection_for_pending()
       ||prepared_()!=pending_->prepared||manual_()!=pending_->manual
       ||manualId_()!=std::optional{pending_->manualId})
        return false;
    const auto current=sourceLoop_();
    const auto full=pending_->manual->view().absolute_range();
    if(current &&(current->begin().value()<0 ||current->end()>full.end()))
        return false;
    // PCM is always the complete zero-origin SOURCE, never the active loop.
    // Only the handoff's SOURCE transport/cue mapping changes.
    pending_->loop=current;
    return true;
}

void TerminalPrivateProductSession::cancel_pending_on_region_change()
{
    // A synchronous SOURCE-region signal during exact MONITOR retirement
    // must never re-enter cancel() and disturb the owner transaction.
    // This signal can be synchronous/reentrant from SOURCE UI. A device
    // that already lost its monitor and is awaiting an explicit manual
    // recovery must NOT silently retry Windows QAudioSink installation here.
    // It also must not erase retained M17 PCM without a user decision.
    if(monitorTransitioning_ ||returnRecovery_)return;
    if(pending_)static_cast<void>(cancel());
}

void TerminalPrivateProductSession::on_renderer_changed()
{
    // A measured anonymous Windows DEVICE lease is a separate owned
    // transaction. Renderer notifications cannot revoke its return token.
    if(installedMeasuredTicket_||measuredSourceRecovery_)return;
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
        if(!current_selection_for_pending()||prepared_()!=pending_->prepared
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
