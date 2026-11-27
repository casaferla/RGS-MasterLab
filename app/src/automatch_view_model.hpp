#pragma once
#include "internal/l3_native_qt_product_action.hpp"
#include "internal/l3_manual_match_transfer_project.hpp"
#include <future>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

namespace rgsml::app {
class AutoMatchViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canStart READ can_start NOTIFY changed)
    Q_PROPERTY(bool transferBusy READ transfer_busy NOTIFY changed)
    Q_PROPERTY(bool transferReady READ transfer_ready NOTIFY changed)
    Q_PROPERTY(bool canBeginTransfer READ can_begin_transfer NOTIFY changed)
    Q_PROPERTY(QString transferText READ transfer_text NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canAdopt READ can_adopt NOTIFY changed)
    Q_PROPERTY(bool canCancel READ can_cancel NOTIFY changed)
    Q_PROPERTY(bool canUndo READ can_undo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ can_redo NOTIFY changed)
    Q_PROPERTY(bool canResume READ can_resume NOTIFY changed)
    Q_PROPERTY(QString currentMatchText READ current_match_text NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(QString errorText READ error_text NOTIFY changed)
    Q_PROPERTY(QString reviewText READ review_text NOTIFY changed)
    Q_PROPERTY(QString reviewMetricsText READ review_metrics_text NOTIFY changed)
    Q_PROPERTY(QString failureDiagnosticsText READ failure_diagnostics_text NOTIFY changed)
    Q_PROPERTY(QString elapsedTimeText READ elapsed_time_text NOTIFY changed)
    Q_PROPERTY(QString verificationNote READ verification_note NOTIFY changed)
public:
    using Action=l3_automatch::NativeQtProductAction;
    AutoMatchViewModel(Action::Controllers product,const Action::Fanout& fanout,
        const AuditionSourceSelector& selector,const GoldSelectionViewModel& gold,
        QObject* parent=nullptr);
    ~AutoMatchViewModel() override;
    [[nodiscard]] bool transfer_busy()const;
    [[nodiscard]] bool transfer_ready()const;
    [[nodiscard]] bool can_begin_transfer()const;
    [[nodiscard]] QString transfer_text()const;
    Q_INVOKABLE bool beginTransfer(const QString& instanceId);
    Q_INVOKABLE void discardTransfer();
    Q_INVOKABLE bool applyTransfer();
    [[nodiscard]] bool can_start()const;
    [[nodiscard]] bool busy()const;
    [[nodiscard]] bool can_adopt()const;
    [[nodiscard]] bool can_cancel()const;
    [[nodiscard]] bool can_undo()const;
    [[nodiscard]] bool can_redo()const;
    [[nodiscard]] bool can_resume()const;
    [[nodiscard]] QString current_match_text()const;
    [[nodiscard]] QString status_text()const;
    [[nodiscard]] QString error_text()const;
    [[nodiscard]] QString review_text()const;
    [[nodiscard]] QString review_metrics_text()const;
    [[nodiscard]] QString failure_diagnostics_text()const;
    [[nodiscard]] QString elapsed_time_text()const;
    [[nodiscard]] QString verification_note()const;
    // All values are captured once before dispatch; the ceiling is the
    // independent AutoMatch/M16 safety limit, never the WAV Export setting.
    // Product presents only the five artistic amounts. The independent
    // M16 safety policy is fixed at 0 dBTP, not an artistic parameter.
    Q_INVOKABLE bool startProduct(double total,double tonal,double dynamics,double stereo,
        double loudness);
    // Explicit native entry for research and regression tests.
    bool start(double total,double tonal,double dynamics,double stereo,
        double loudness,double ceiling);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool adopt();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE bool resume();
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    [[nodiscard]] bool owner()const;
    Action::Controllers product_;
    const Action::Fanout& fanout_;
    const AuditionSourceSelector& selector_;
    const GoldSelectionViewModel& gold_;
    Action action_;
    using TransferResult=core::Result<l3_automatch::ManualMatchTransferAudition>;
    std::shared_ptr<std::atomic<bool>> transfer_cancel_;
    std::future<TransferResult> transfer_worker_;
    std::shared_ptr<const l3_automatch::ManualMatchTransferAudition> transfer_candidate_;
    QString transfer_text_;
    void poll_transfer();
    QTimer timer_;
    // Monotonic wall clock for this actual Product search, distinct from
    // native phase-duration accounting and later M13 verification.
    QElapsedTimer search_clock_;
    qint64 finished_search_ms_{-1};
    bool search_clock_running_{false};
    QString error_;
    std::uint64_t amount_revision_{};
    bool publishing_{};
};
} // namespace rgsml::app
