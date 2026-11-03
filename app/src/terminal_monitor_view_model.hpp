#pragma once
#include "terminal_private_product_session.hpp"
#include "playback_transport_view_model.hpp"
#include <QObject>
#include <QTimer>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

// Explicit Windows-only experimental MASTER monitoring control surface.
// DEVICE PCM never receives a terminal RealizationId or an M16 LIVE claim.
namespace rgsml::app {
class TerminalMonitorViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(QString returnTargetText READ return_target_text NOTIFY changed)
    Q_PROPERTY(QString errorText READ error_text NOTIFY changed)
    Q_PROPERTY(bool monitorExclusive READ monitor_exclusive NOTIFY changed)
    Q_PROPERTY(bool monitorInstalled READ monitor_installed NOTIFY changed)
    Q_PROPERTY(bool monitorPreparing READ monitor_preparing NOTIFY changed)
    Q_PROPERTY(bool monitorCancelling READ monitor_cancelling NOTIFY changed)
    Q_PROPERTY(bool monitorControllable READ monitor_controllable NOTIFY changed)
    Q_PROPERTY(bool canPrepare READ can_prepare NOTIFY changed)
    Q_PROPERTY(bool canPrepareOffline READ can_prepare_offline NOTIFY changed)
    Q_PROPERTY(bool canDismiss READ can_dismiss NOTIFY changed)
    Q_PROPERTY(bool canInstall READ can_install NOTIFY changed)
    Q_PROPERTY(int masterRateHz READ master_rate_hz NOTIFY changed)
    Q_PROPERTY(int deviceRateHz READ device_rate_hz NOTIFY changed)
    Q_PROPERTY(qint64 masterPositionFrames READ master_position_frames NOTIFY changed)
    Q_PROPERTY(qint64 masterDurationFrames READ master_duration_frames NOTIFY changed)
    Q_PROPERTY(qint64 masterRegionStartFrames READ master_region_start_frames NOTIFY changed)
    Q_PROPERTY(qint64 masterRegionEndFrames READ master_region_end_frames NOTIFY changed)
    Q_PROPERTY(bool masterLoopEnabled READ master_loop_enabled NOTIFY changed)
    Q_PROPERTY(QString transportState READ transport_state NOTIFY changed)
public:
    TerminalMonitorViewModel(TerminalPrivateProductSession& session,
                             PlaybackTransportViewModel& transport,
                             QObject* parent=nullptr);
    ~TerminalMonitorViewModel() override;
    [[nodiscard]] QString status_text() const;
    [[nodiscard]] QString return_target_text() const {
        return session_->monitor_returns_to_empty_source()
            ? QStringLiteral("Close DEVICE · No MANUAL sink")
            : QStringLiteral("Return MANUAL");
    }
    [[nodiscard]] QString error_text() const {
        // Include asynchronous renderer rejection, not only direct UI calls.
        // A separate ordinary M17 session can fail independently of MONITOR.
        return !error_.isEmpty() ? error_
            : (monitorRequested_ || monitor_exclusive())
                ? session_->error() : QString{};
    }
    [[nodiscard]] bool monitor_exclusive() const noexcept;
    [[nodiscard]] bool monitor_installed() const noexcept;
    [[nodiscard]] bool monitor_preparing() const noexcept {return monitorPreparing_;}
    [[nodiscard]] bool monitor_cancelling() const noexcept {
        return !monitorPreparing_
            &&asyncWorkerRunning_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool monitor_controllable() const noexcept;
    [[nodiscard]] bool can_prepare() const noexcept;
    // Exceptional no-DEVICE fallback: ordinary audible MANUAL always
    // prepares through the single M17 Product control, not Monitor QML.
    [[nodiscard]] bool can_prepare_offline() const noexcept;
    [[nodiscard]] bool can_dismiss() const noexcept;
    [[nodiscard]] bool can_install() const noexcept;
    [[nodiscard]] int master_rate_hz() const noexcept {
        const auto ready=session_->verified_master_rate_for_monitoring();
        return ready ? static_cast<int>(ready->value()) : masterRateHz_;
    }
    [[nodiscard]] int device_rate_hz() const noexcept {return deviceRateHz_;}
    [[nodiscard]] qint64 master_position_frames() const noexcept {return position_;}
    [[nodiscard]] qint64 master_duration_frames() const noexcept {return duration_;}
    [[nodiscard]] qint64 master_region_start_frames() const noexcept {return masterRegionStart_;}
    [[nodiscard]] qint64 master_region_end_frames() const noexcept {return masterRegionEnd_;}
    [[nodiscard]] bool master_loop_enabled() const noexcept {return masterLoopEnabled_;}
    [[nodiscard]] QString transport_state() const {return transportState_;}

    // Optional app-owned preflight: creates an explicit offline SOURCE
    // provenance for the user-requested MASTER-only action, not playback.
    using OfflineMasterAuthorizer=std::function<core::Status()>;
    void set_offline_master_authorizer(OfflineMasterAuthorizer authorizer)
    {
        offlineMasterAuthorizer_=std::move(authorizer);
        lastCanPrepareOffline_=can_prepare_offline();
        emit changed();
    }
    // Independent conductor runs one exact M17 DEVICE retirement before
    // an ordinary Master Audition preflight. No M17 render is triggered.
    using InstalledHandoffEligible=std::function<bool()>;
    using InstalledHandoff=std::function<core::Status()>;
    using ReturnedReady=std::function<void()>;
    void set_installed_m17_handoff(InstalledHandoffEligible eligible,
        InstalledHandoff handoff,ReturnedReady returned)
    {
        installedHandoffEligible_=std::move(eligible);
        installedHandoff_=std::move(handoff);
        returnedReady_=std::move(returned);
        lastCanInstall_=can_install();
        emit changed();
    }
    // These two callbacks execute ONLY on the Qt owner thread. The async
    // FIR worker receives only immutable source-buffer owners and a frozen
    // endpoint ID + rate. Existing isolated tests may omit the callbacks
    // and retain their independently-tested synchronous fake seam.
    using AsyncDevicePlan=platform::windows::MonitoringAsyncDevicePlan;
    using AsyncProbe=std::function<core::Result<AsyncDevicePlan>(
        audio::AudioBufferView,bool)>;
    using AsyncRecheck=std::function<core::Status(
        const AsyncDevicePlan&,audio::AudioBufferView,bool)>;
    void set_async_monitoring_device(AsyncProbe probe,AsyncRecheck recheck)
    { asyncProbe_=std::move(probe);asyncRecheck_=std::move(recheck); }

    Q_INVOKABLE bool requestMaster(int chosenRateHz);
    Q_INVOKABLE bool installMonitor(bool explicitlyAllowCaution);
    Q_INVOKABLE bool returnManual();
    Q_INVOKABLE bool cancel();
    Q_INVOKABLE bool play();
    Q_INVOKABLE bool pause();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE bool seekMaster(qint64 frame);
    Q_INVOKABLE bool setMasterLoop(qint64 begin,qint64 end,bool enabled);
    Q_INVOKABLE bool setMasterRegion(qint64 begin,qint64 end);
    Q_INVOKABLE bool clearMasterRegion();
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    bool report(const core::Status& result);
    bool control(platform::windows::MonitoringTransportRequest request);
    void abort_async_conversion();
    bool monitorRequested_{false};
    bool monitorPreparing_{false};
    std::uint64_t monitorSerial_{0};
    std::atomic_bool asyncWorkerRunning_{false};
    std::shared_ptr<std::atomic_bool> asyncCancellation_;
    TerminalPrivateProductSession::MonitoringMasterCapture pendingMaster_;
    // Reuse only an exact verified MASTER/device PCM pair. No M17 rerender,
    // and the current Windows endpoint/RAM admission still runs each time.
    TerminalPrivateProductSession::MonitoringMasterCapture cachedMaster_;
    std::shared_ptr<const audio::MonitoringPcmCandidate> cachedDevicePcm_;
    AsyncProbe asyncProbe_;
    AsyncRecheck asyncRecheck_;
    std::jthread monitorWorker_;
    bool lastCanInstall_{false};
    bool lastCanPrepareOffline_{false};
    OfflineMasterAuthorizer offlineMasterAuthorizer_;
    InstalledHandoffEligible installedHandoffEligible_;
    InstalledHandoff installedHandoff_;
    ReturnedReady returnedReady_;
    TerminalPrivateProductSession* session_;
    PlaybackTransportViewModel* transport_;
    QTimer observationTimer_;
    QString error_;
    QString transportState_{QStringLiteral("UNAVAILABLE")};
    qint64 position_{0};
    qint64 duration_{0};
    qint64 masterRegionStart_{0};
    qint64 masterRegionEnd_{0};
    bool masterLoopEnabled_{false};
    bool masterRegionInitialized_{false};
    int masterRateHz_{0};
    int deviceRateHz_{0};
};
} // namespace rgsml::app
