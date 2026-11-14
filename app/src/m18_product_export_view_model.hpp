#pragma once
// M18 Product UI adapter. QML can only request the Product-owned action;
// it cannot mint consent tickets, mutate verified PCM or publish a WAV.
#include "internal/m18_private_product_export_action.hpp"
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <cstdint>
#include <optional>

namespace rgsml::app {
class M18ProductExportViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canExport READ can_export NOTIFY changed)
    Q_PROPERTY(QString exportReadinessReason READ export_readiness_reason NOTIFY changed)
    Q_PROPERTY(bool canCancel READ can_cancel NOTIFY changed)
    Q_PROPERTY(bool canAcknowledge READ can_acknowledge NOTIFY changed)
    Q_PROPERTY(bool canApplyMicroTrim READ can_apply_micro_trim NOTIFY changed)
    Q_PROPERTY(bool exportInProgress READ export_in_progress NOTIFY changed)
    Q_PROPERTY(double encodedProgress READ encoded_progress NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(QString errorText READ error_text NOTIFY changed)
    // M23 read-only, sanitized failure code. Never authorizes export.
    Q_PROPERTY(QString diagnosticSummary READ diagnostic_summary NOTIFY changed)
    Q_PROPERTY(QString verifiedSha256 READ verified_sha256 NOTIFY changed)
    Q_PROPERTY(qulonglong verifiedBytes READ verified_bytes NOTIFY changed)
public:
    using Action=internal::M18PrivateProductExportAction;
    using Controllers=internal::M18ProductCompleteBinding::Controllers;
    explicit M18ProductExportViewModel(Action& action,Controllers controllers,
        QObject* parent=nullptr);

    [[nodiscard]] bool can_export() const;
    [[nodiscard]] QString export_readiness_reason() const;
    [[nodiscard]] bool can_cancel() const;
    [[nodiscard]] bool can_acknowledge() const;
    [[nodiscard]] bool can_apply_micro_trim() const;
    [[nodiscard]] bool export_in_progress() const;
    [[nodiscard]] double encoded_progress() const;
    [[nodiscard]] QString status_text() const;
    [[nodiscard]] QString error_text() const;
    [[nodiscard]] QString diagnostic_summary() const;
    [[nodiscard]] QString verified_sha256() const;
    [[nodiscard]] qulonglong verified_bytes() const;

    // User must explicitly choose a destination and PCM24/Float32.
    // Format is NOT a second sonic stage: accepted M17 Ceiling is immutable.
    Q_INVOKABLE bool beginExport(const QUrl& destination,int formatIndex);
    // Only a post-verification eligible FAILED result unlocks this click.
    Q_INVOKABLE bool applyMicroTrimAndExport();
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE bool acknowledgeExport();
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    void poll();
    [[nodiscard]] bool owner_thread() const noexcept;
    Action* action_;
    Controllers controllers_;
    QTimer pollTimer_;
    QString lastError_;
    QUrl initialDestination_;
    int initialFormatIndex_{-1};
    std::optional<std::uint64_t> initialSeed_;
    double initialCeilingDbTp_{0.0};
};
} // namespace rgsml::app
