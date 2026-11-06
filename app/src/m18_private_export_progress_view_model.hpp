#pragma once
// M18 PREPRODUCTION ONLY. Real, testable Qt observability around the
// private worker, but intentionally not instantiated or registered by main.
// A future Product UI may use this only AFTER the single-writer epoch and
// explicit export authorization are installed across ALL mutators.
#include "internal/m18_private_async_export_session.hpp"
#include <QObject>
#include <QTimer>
#include <QString>
#include <cstdint>
#include <optional>

namespace rgsml::app {

class M18PrivateExportProgressViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(double encodedProgress READ encoded_progress NOTIFY changed)
    Q_PROPERTY(bool exportInProgress READ export_in_progress NOTIFY changed)
    Q_PROPERTY(bool verifiedDelivered READ verified_delivered NOTIFY changed)
    Q_PROPERTY(QString errorText READ error_text NOTIFY changed)
public:
    explicit M18PrivateExportProgressViewModel(QObject* parent=nullptr);
    ~M18PrivateExportProgressViewModel() override;

    // Typed C++ Product-only authorization. NOT Q_INVOKABLE: a QML menu
    // cannot mint its own M18 ticket or start work without owner preflight.
    [[nodiscard]] core::Status begin_private_export(
        const TerminalPrivateProductSession& session,
        const SourceSelectionViewModel& source,
        const MasterOutputRateState& rate,
        const TerminalCandidateGate& gate,
        const platform::windows::internal::
            M18ProductEpochTicketAuthority::Ticket& ticket,
        core::ResourceReference selectedSource,
        core::ResourceReference destination,
        audio::WavSampleFormat format,
        std::optional<std::uint64_t> seed,
        double ceilingDbTp);

    [[nodiscard]] QString status_text() const;
    [[nodiscard]] double encoded_progress() const noexcept;
    [[nodiscard]] bool export_in_progress() const noexcept;
    [[nodiscard]] bool verified_delivered() const noexcept;
    [[nodiscard]] QString error_text() const;

    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE bool acknowledgeExport();
signals:
    void changed();
private:
    void poll();
    internal::M18PrivateAsyncExportSession session_;
    internal::M18PrivateAsyncExportSession::Snapshot snapshot_;
    QTimer pollTimer_;
};

} // namespace rgsml::app
