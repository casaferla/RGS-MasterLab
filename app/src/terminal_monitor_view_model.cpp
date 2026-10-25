#include "terminal_monitor_view_model.hpp"

#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
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
    connect(session_,&TerminalPrivateProductSession::changed,this,[this] {
        if(monitor_installed()) {
            if(!observationTimer_.isActive())observationTimer_.start();
        } else {
            observationTimer_.stop();
            position_=0;
            duration_=0;
            deviceRateHz_=0;
            transportState_=QStringLiteral("UNAVAILABLE");
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
        return QStringLiteral("MASTER AUDITION IDLE · NO PRIVATE DEVICE PCM");
    if(status==QStringLiteral("RENDERING"))
        return QStringLiteral("PREPARING FINAL MASTER · NOT DEVICE AUDIO");
    if(status==QStringLiteral("MASTER_READY_MONITOR_PENDING"))
        return QStringLiteral("MASTER VERIFIED · DEVICE PCM NOT INSTALLED");
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
bool TerminalMonitorViewModel::can_prepare() const noexcept
{
    return session_->status()==QStringLiteral("IDLE") && !monitor_exclusive();
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
    return session_->status()==QStringLiteral("MASTER_READY_MONITOR_PENDING");
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
    const bool exactStopped=source.value()->state==core::PlaybackState::STOPPED
        &&source.value()->position==core::FrameIndex{0}
        &&!source.value()->loop;
    const bool cleanEmpty=source.value()->state==core::PlaybackState::NO_SOURCE
        &&source.value()->audibleRealization.phase
            ==core::AudibleHandoffPhase::UNAVAILABLE
        &&!source.value()->audibleRealization.realizationId
        &&source.value()->position==core::FrameIndex{0}
        &&!source.value()->duration
        &&!source.value()->loop;
    if(!exactStopped&&!cleanEmpty)
        return report(reject("AUDITION_MASTER_REQUIRES_STOPPED_OR_EMPTY_DEVICE"));
    // The actual Windows SOURCE may have been accepted and rendered
    // offline while its INITIAL device prepare was rejected. Never fake
    // an audible activeTarget/RealizationId to unlock the private MASTER.
    if(cleanEmpty && offlineMasterAuthorizer_) {
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
    const auto block=core::FrameCount::create(4096);
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
