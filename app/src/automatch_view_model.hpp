#pragma once
#include "internal/l3_native_qt_product_action.hpp"
#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

namespace rgsml::app {
class AutoMatchViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canStart READ can_start NOTIFY changed)
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
    Q_INVOKABLE bool start(double total,double tonal,double dynamics,double stereo,
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
