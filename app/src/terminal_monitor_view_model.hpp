#pragma once
#include "terminal_private_product_session.hpp"
#include "playback_transport_view_model.hpp"
#include <QObject>
#include <QTimer>
#include <QString>
#include <functional>
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
    Q_PROPERTY(bool monitorControllable READ monitor_controllable NOTIFY changed)
    Q_PROPERTY(bool canPrepare READ can_prepare NOTIFY changed)
    Q_PROPERTY(bool canPrepareOffline READ can_prepare_offline NOTIFY changed)
    Q_PROPERTY(bool canDismiss READ can_dismiss NOTIFY changed)
    Q_PROPERTY(bool canInstall READ can_install NOTIFY changed)
    Q_PROPERTY(int masterRateHz READ master_rate_hz NOTIFY changed)
    Q_PROPERTY(int deviceRateHz READ device_rate_hz NOTIFY changed)
    Q_PROPERTY(qint64 masterPositionFrames READ master_position_frames NOTIFY changed)
    Q_PROPERTY(qint64 masterDurationFrames READ master_duration_frames NOTIFY changed)
    Q_PROPERTY(QString transportState READ transport_state NOTIFY changed)
public:
    TerminalMonitorViewModel(TerminalPrivateProductSession& session,
                             PlaybackTransportViewModel& transport,
                             QObject* parent=nullptr);
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
    [[nodiscard]] QString transport_state() const {return transportState_;}

    // Optional app-owned preflight: creates an explicit offline SOURCE
    // provenance for the user-requested MASTER-only action, not playback.
    using OfflineMasterAuthorizer=std::function<core::Status()>;
    void set_offline_master_authorizer(OfflineMasterAuthorizer authorizer)
    { offlineMasterAuthorizer_=std::move(authorizer); }
    Q_INVOKABLE bool requestMaster(int chosenRateHz);
    Q_INVOKABLE bool installMonitor(bool explicitlyAllowCaution);
    Q_INVOKABLE bool returnManual();
    Q_INVOKABLE bool cancel();
    Q_INVOKABLE bool play();
    Q_INVOKABLE bool pause();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE bool seekMaster(qint64 frame);
    Q_INVOKABLE bool setMasterLoop(qint64 begin,qint64 end,bool enabled);
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    bool report(const core::Status& result);
    bool control(platform::windows::MonitoringTransportRequest request);
    bool monitorRequested_{false};
    bool lastCanInstall_{false};
    OfflineMasterAuthorizer offlineMasterAuthorizer_;
    TerminalPrivateProductSession* session_;
    PlaybackTransportViewModel* transport_;
    QTimer observationTimer_;
    QString error_;
    QString transportState_{QStringLiteral("UNAVAILABLE")};
    qint64 position_{0};
    qint64 duration_{0};
    int masterRateHz_{0};
    int deviceRateHz_{0};
};
} // namespace rgsml::app
