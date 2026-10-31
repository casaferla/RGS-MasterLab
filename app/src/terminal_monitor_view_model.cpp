#include "terminal_monitor_view_model.hpp"

#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>

namespace rgsml::app {
namespace {
core::Status reject(const char* reason)
{
    return core::Status::failure(core::Error{
        core::ErrorCode::InvalidState,reason});
}
}

TerminalMonitorViewModel::TerminalMonitorViewModel(
    TerminalPrivateProductSession& session,PlaybackTransportViewModel& transport,
    QObject* parent)
    : QObject(parent),session_(&session),transport_(&transport)
{
    observationTimer_.setInterval(200);
    connect(&observationTimer_,&QTimer::timeout,this,&TerminalMonitorViewModel::refresh);
    // DEVICE admission and exceptional NO_SOURCE offline fallback both
    // depend on transport, even if the session does not change. Notify QML
    // on either eligibility transition, not on every timer tick.
    lastCanInstall_=can_install();
    lastCanPrepareOffline_=can_prepare_offline();
    connect(transport_,&PlaybackTransportViewModel::playbackChanged,
        this,[this] {
            const bool install=can_install();
            const bool offline=can_prepare_offline();
            if(install==lastCanInstall_
               &&offline==lastCanPrepareOffline_)return;
            lastCanInstall_=install;
            lastCanPrepareOffline_=offline;
            emit changed();
        });
    connect(session_,&TerminalPrivateProductSession::changed,this,[this] {
        lastCanInstall_=can_install();
        lastCanPrepareOffline_=can_prepare_offline();
        if(monitor_controllable()) {
            if(!observationTimer_.isActive())observationTimer_.start();
        } else {
            observationTimer_.stop();
            position_=0;
            duration_=0;
            deviceRateHz_=0;
            transportState_=QStringLiteral("UNAVAILABLE");
            // A canceled/returned MASTER is no longer an immutable
            // prepared realization. Do not retain its stale UI rate.
            if(session_->status()==QStringLiteral("IDLE"))
                masterRateHz_=0;
        }
        emit changed();
    });
}

QString TerminalMonitorViewModel::status_text() const
{
    const auto status=session_->status();
    // IDLE cannot attest audible MANUAL: SOURCE may have been refused
    // initially and an offline-origin monitor may have returned NO_SOURCE.
    if(status==QStringLiteral("IDLE"))
        return QStringLiteral("ASCOLTA MASTER IDLE · NO PRIVATE DEVICE PCM");
    if(status==QStringLiteral("RENDERING"))
        return QStringLiteral("PREPARING FINAL MASTER · NOT DEVICE AUDIO");
    if(status==QStringLiteral("MASTER_READY_MONITOR_PENDING")
       ||((status==QStringLiteral("READY_TO_INSTALL")
           ||status==QStringLiteral("COMPARISON_VERIFIED"))
          &&session_->verified_master_rate_for_monitoring()))
        return QStringLiteral("MASTER VERIFIED · DEVICE PCM NOT INSTALLED");
    if(status==QStringLiteral("PRIVATE_INSTALLED")
       &&installedHandoffEligible_&&installedHandoffEligible_())
        return QStringLiteral(
            "M17 PREVIEW ACTIVE · ASCOLTA MASTER RETAINS SAME PCM");
    if(status==QStringLiteral("MASTER_MEASURED_NONCONFORMING_PRIVATE"))
        return QStringLiteral("MASTER NON-CONFORMING · AUDITION BLOCKED");
    if(status==QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED"))
        return QStringLiteral("AUDITION RECOVERY REQUIRED · OWNER CHECK");
    if(status.startsWith(QStringLiteral("MONITOR_PRIVATE_"))) {
        return session_->monitor_returns_to_empty_source()
            ?QStringLiteral("DEVICE AUDITION · NO MANUAL SINK · RETURN CLEARS DEVICE")
            :QStringLiteral("AUDITION DEVICE PCM · UNQUALIFIED LIVE");
    }
    return status;
}

bool TerminalMonitorViewModel::monitor_exclusive() const noexcept
{
    return session_->monitoring_transaction_active();
}
bool TerminalMonitorViewModel::monitor_installed() const noexcept
{
    return session_->status().startsWith(QStringLiteral("MONITOR_PRIVATE_"));
}
bool TerminalMonitorViewModel::monitor_controllable() const noexcept
{
    // Recovery is an installed exact-owner ticket, not operational audio.
    // Keep Return enabled but do not offer user transport on incomplete
    // SOURCE->MASTER->DEVICE command seeding.
    const auto current=session_->status();
    return current==QStringLiteral("MONITOR_PRIVATE_PLAYING")
        ||current==QStringLiteral("MONITOR_PRIVATE_PAUSED")
        ||current==QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED");
}
bool TerminalMonitorViewModel::can_prepare() const noexcept
{
    return session_->status()==QStringLiteral("IDLE") && !monitor_exclusive();
}
bool TerminalMonitorViewModel::can_prepare_offline() const noexcept
{
    if(!can_prepare()||!offlineMasterAuthorizer_)return false;
    const auto source=transport_->playback_snapshot();
    if(!source)return false;
    // Do not create a second Prepare on normal MANUAL playback. The
    // exceptional source-verified but DEVICE-refused path has NO audible
    // MANUAL sink; the explicit authorizer still checks the SOURCE epoch
    // when the operator actually requests an offline fallback.
    return source.value()->state==core::PlaybackState::NO_SOURCE
        &&source.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::UNAVAILABLE
        &&!source.value()->audibleRealization.realizationId
        &&source.value()->position==core::FrameIndex{0}
        &&!source.value()->duration &&!source.value()->loop;
}
bool TerminalMonitorViewModel::can_dismiss() const noexcept
{
    // ERROR or STALE can outlive the private render ticket; the operator
    // still needs an explicit reset rather than a disabled Cancel button.
    // Reset only the MASTER request owned by this UI. A separate
    // ordinary M17 preview must not become cancellable from MONITOR.
    return monitor_exclusive()
        || (monitorRequested_ && session_->status()!=QStringLiteral("IDLE"))
        || !error_.isEmpty();
}
bool TerminalMonitorViewModel::can_install() const noexcept
{
    const auto status=session_->status();
    if(status==QStringLiteral("PRIVATE_INSTALLED"))
        return installedHandoff_ && installedHandoffEligible_
            && installedHandoffEligible_();
    if(status!=QStringLiteral("MASTER_READY_MONITOR_PENDING")
       &&status!=QStringLiteral("READY_TO_INSTALL")
       &&status!=QStringLiteral("COMPARISON_VERIFIED"))return false;
    if(!session_->verified_master_rate_for_monitoring())return false;
    // A verified immutable M17 MASTER is independent of Play/Pause,
    // source cue and Loop. Its DEVICE installation must still pass exact
    // owner/rate/memory and mapped-transport gates inside the session.
    const auto source=transport_->playback_snapshot();
    if(!source)return false;
    return source.value()->state==core::PlaybackState::STOPPED
        ||source.value()->state==core::PlaybackState::PLAYING
        ||source.value()->state==core::PlaybackState::PAUSED
        ||source.value()->state==core::PlaybackState::NO_SOURCE;
}
bool TerminalMonitorViewModel::report(const core::Status& result)
{
    if(!result) {
        error_=result.error()
            ?QString::fromStdString(result.error()->message())
            :QStringLiteral("MONITOR_OPERATION_FAILED");
        emit changed();
        return false;
    }
    error_.clear();
    emit changed();
    return true;
}
bool TerminalMonitorViewModel::requestMaster(int chosenRateHz)
{
    if(!can_prepare())return report(reject("MONITOR_REQUEST_REQUIRES_IDLE"));
    const auto selected=core::SampleRate::create(chosenRateHz);
    if(!selected)return report(core::Status::failure(*selected.error()));
    // Explicit MASTER-only PREPARE may run when the previous Windows sink
    // is proven absent. DEVICE install/return have their OWN strict guards:
    // the empty sink does not imply that monitoring is already audible.
    const auto source=transport_->playback_snapshot();
    if(!source)return report(core::Status::failure(*source.error()));
    // Offline MASTER preparation does not install/replace a sound device:
    // PLAYING, PAUSED and a real SOURCE Loop are all legitimate. The
    // private session separately validates exact MANUAL ownership, source
    // realization and full-resource snapshot before scheduling its worker.
    const bool exactManual=source.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::NEW
        &&source.value()->audibleRealization.realizationId
        &&source.value()->duration
        &&(source.value()->state==core::PlaybackState::STOPPED
            ||source.value()->state==core::PlaybackState::PLAYING
            ||source.value()->state==core::PlaybackState::PAUSED);
    const bool cleanEmpty=source.value()->state==core::PlaybackState::NO_SOURCE
        &&source.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::UNAVAILABLE
        &&!source.value()->audibleRealization.realizationId
        &&source.value()->position==core::FrameIndex{0}
        &&!source.value()->duration
        &&!source.value()->loop;
    if(!exactManual&&!cleanEmpty)
        return report(reject("AUDITION_MASTER_REQUIRES_OWNED_MANUAL_OR_EMPTY_DEVICE"));
    // The actual Windows SOURCE may have been accepted and rendered
    // offline while its INITIAL device prepare was rejected. Never fake
    // an audible activeTarget/RealizationId to unlock the private MASTER.
    if(cleanEmpty) {
        // UI offline fallback is an exceptional authorization path, not
        // another way to bypass initial SOURCE/DEVICE selection. Even
        // non-QML callers must present an explicit provenance authorizer.
        if(!offlineMasterAuthorizer_)
            return report(reject("MONITOR_OFFLINE_MASTER_AUTHORIZER_REQUIRED"));
        const auto authorized=offlineMasterAuthorizer_();
        if(!authorized)return report(authorized);
    }
    const auto result=session_->request_private_for_final_master(*selected.value());
    if(result) {
        masterRateHz_=chosenRateHz;
        monitorRequested_=true;
    }
    return report(result);
}
bool TerminalMonitorViewModel::installMonitor(bool explicitlyAllowCaution)
{
    if(!can_install())return report(reject("MONITOR_INSTALL_REQUIRES_VERIFIED_MASTER"));
    if(session_->status()==QStringLiteral("PRIVATE_INSTALLED")) {
        // One explicit user command; the conductor retires the exact M17
        // device and restores MANUAL. Only THEN may Windows allocate an
        // independent DEVICE-rate monitoring sink from the SAME MASTER.
        if(!installedHandoff_)return report(reject(
            "MONITOR_M17_INSTALLED_HANDOFF_UNAVAILABLE"));
        const auto handed=installedHandoff_();
        if(!report(handed))return false;
        if(!can_install())return report(reject(
            "MONITOR_M17_RETAINED_MASTER_NOT_ADMISSIBLE"));
    }
    // Playback SRC's hard block bound is 1024 OUTPUT_RATE frames. An
    // oversized block made every real DEVICE allocation fail.
    const auto block=core::FrameCount::create(
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    if(!block)return report(core::Status::failure(*block.error()));
    // Re-probe exact current DEVICE and Windows memory immediately before
    // allocating the full-length monitoring PCM. UNKNOWN/DISCOURAGED are
    // denied; CAUTION requires a separate, explicit user opt-in.
    const auto ready=session_->materialize_private_final_master_monitoring_with_preflight(
        *block.value(),0,
        [explicitlyAllowCaution](const audio::MonitoringPcmPreallocationAdvice& advice) {
            using Kind=audio::MonitoringMemoryAdviceKind;
            if(advice.kind==Kind::RECOMMENDED_MEMORY_ONLY
               ||(advice.kind==Kind::CAUTION_LIMITED_HEADROOM
                  &&explicitlyAllowCaution))
                return core::Status::success();
            return reject("MONITOR_MEMORY_NOT_APPROVED_OR_UNAVAILABLE");
        });
    if(!ready)return report(core::Status::failure(*ready.error()));
    const auto installed=session_->install_private_monitor_stopped(*ready.value());
    if(!report(installed))return false;
    refresh();
    return true;
}
bool TerminalMonitorViewModel::returnManual()
{
    if(!monitor_installed())
        return report(reject("MONITOR_RETURN_EXACT_DEVICE_OWNER_REQUIRED"));
    const auto result=session_->return_private_monitor_to_manual();
    if(!result)return report(result);
    // Clear private evidence after restoring the exact manual owner. The
    // device-rate PCM is never copied to the FINAL MASTER or SOURCE epoch.
    // A normal M17 READY was borrowed, not consumed. Its immutable PCM
    // survives a DEVICE audition return and remains available to Preview.
    if(session_->status()==QStringLiteral("READY_TO_INSTALL")) {
        monitorRequested_=false;
        // Normal M17 does not lose its master or become a second Prepare
        // after Return from the exact monitoring lease.
        if(returnedReady_)returnedReady_();
        return report(core::Status::success());
    }
    const auto cleared=session_->cancel();
    if(cleared)monitorRequested_=false;
    return report(cleared);
}
bool TerminalMonitorViewModel::cancel()
{
    if(monitor_installed())return returnManual();
    if(!monitor_exclusive() && (!monitorRequested_
         || session_->status()==QStringLiteral("IDLE"))) {
        monitorRequested_=false;
        return report(core::Status::success());
    }
    const auto revoked=session_->cancel();
    if(revoked)monitorRequested_=false;
    return report(revoked);
}
bool TerminalMonitorViewModel::control(
    platform::windows::MonitoringTransportRequest request)
{
    if(!monitor_installed())
        return report(reject("MONITOR_CONTROL_REQUIRES_EXACT_DEVICE_OWNER"));
    if(session_->status()==QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED"))
        return report(reject("MONITOR_RECOVERY_ONLY_RETURN_ALLOWED"));
    const auto result=session_->control_private_monitor(request);
    if(!report(result))return false;
    refresh();
    return true;
}
bool TerminalMonitorViewModel::play()
{
    return control({platform::windows::MonitoringTransportAction::PLAY});
}
bool TerminalMonitorViewModel::pause()
{
    return control({platform::windows::MonitoringTransportAction::PAUSE});
}
bool TerminalMonitorViewModel::stop()
{
    return control({platform::windows::MonitoringTransportAction::STOP});
}
bool TerminalMonitorViewModel::seekMaster(qint64 frame)
{
    return control({platform::windows::MonitoringTransportAction::SEEK_MASTER,
                    core::FrameIndex{frame},std::nullopt});
}
bool TerminalMonitorViewModel::setMasterLoop(qint64 begin,qint64 end,bool enabled)
{
    using Action=platform::windows::MonitoringTransportAction;
    if(!enabled)return control({Action::SET_MASTER_LOOP});
    const auto loop=core::FrameRange::create(
        core::FrameIndex{begin},core::FrameIndex{end});
    if(!loop)return report(core::Status::failure(*loop.error()));
    return control({Action::SET_MASTER_LOOP,core::FrameIndex{0},*loop.value()});
}
void TerminalMonitorViewModel::refresh()
{
    if(!monitor_installed())return;
    const auto observed=session_->private_monitor_master_snapshot();
    if(!observed) {
        // A lost/alien owner must not leave apparently live MASTER position
        // or endpoint readouts from the preceding, valid device epoch.
        position_=0;
        duration_=0;
        deviceRateHz_=0;
        transportState_=QStringLiteral("UNAVAILABLE");
        error_=observed.error()
            ?QString::fromStdString(observed.error()->message())
            :QStringLiteral("MONITOR_OWNER_SNAPSHOT_UNAVAILABLE");
        emit changed();
        return;
    }
    const auto& data=*observed.value();
    position_=data.master_display_position.value();
    duration_=data.master_duration.value();
    masterRateHz_=static_cast<int>(data.master_rate.value());
    deviceRateHz_=static_cast<int>(data.device_rate.value());
    switch(data.state) {
    case core::PlaybackState::PLAYING:transportState_=QStringLiteral("PLAYING");break;
    case core::PlaybackState::PAUSED:transportState_=QStringLiteral("PAUSED");break;
    case core::PlaybackState::STOPPED:transportState_=QStringLiteral("STOPPED");break;
    default:transportState_=QStringLiteral("UNAVAILABLE");break;
    }
    // Polling an unchanged snapshot is NOT user consent to dismiss a
    // rejected command. Only an accepted explicit command clears errors.
    emit changed();
}
} // namespace rgsml::app
