#pragma once

#include "terminal_limiter_state.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace rgsml::app {

// M17 typed Ceiling editor authority. The VM owns only a UI draft;
// TerminalLimiterState and its immutable bindings are the DSP truth.
// Merely constructing this VM does not publish audio or certify M17.
class TerminalLimiterViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double ceilingDbTp READ ceiling_db_tp NOTIFY changed)
    Q_PROPERTY(QString ceilingDbTpText READ ceiling_db_tp_text NOTIFY changed)
    Q_PROPERTY(double draftCeilingDbTp READ draft_ceiling_db_tp NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass NOTIFY changed)
    Q_PROPERTY(QString audioStatus READ audio_status NOTIFY changed)
    Q_PROPERTY(QString validationField READ validation_field NOTIFY changed)
    Q_PROPERTY(QString validationMessage READ validation_message NOTIFY changed)
public:
    using AcceptedStateChange = std::function<void()>;

    explicit TerminalLimiterViewModel(
        TerminalLimiterState* state,
        AcceptedStateChange stateChanged = {},
        QObject* parent = nullptr);

    [[nodiscard]] double ceiling_db_tp() const noexcept;
    [[nodiscard]] QString ceiling_db_tp_text() const;
    [[nodiscard]] double draft_ceiling_db_tp() const noexcept;
    [[nodiscard]] bool bypass() const noexcept;
    [[nodiscard]] QString audio_status() const {return audioStatus_;}
    [[nodiscard]] QString validation_field() const { return validationField_; }
    [[nodiscard]] QString validation_message() const { return validationMessage_; }

    // Common StudioNumericField/StudioParameterSlider contract.
    // Drag/text changes are staged; a committed edit is validated again
    // by the canonical DSP constructor and invokes the accepted callback
    // only once. No per-pointer-move render is permitted.
    Q_INVOKABLE bool setDraftFieldValue(const QString& field, double value);
    Q_INVOKABLE bool setDraftFieldText(const QString& field, const QString& text);
    Q_INVOKABLE bool commitDraft();
    Q_INVOKABLE void cancelDraft();
    // Canonical -1.0 dBTP default, independent of user bypass.
    Q_INVOKABLE void resetToDefault();
    Q_INVOKABLE void setBypass(bool bypass);
    Q_INVOKABLE void refreshFromAuthority();
    // Fail-closed device/PCM transition error in the existing fixed footer.
    // Never replace the typed Ceiling range error with a fictitious success.
    void reportAudioError(const QString& message);
    void setAudioStatus(const QString& value);
    void clearAudioError();

signals:
    // Authored/draft/validation changed: repaint ONLY.
    void changed();
    // Semantic state is committed by TerminalLimiterState. Never emitted
    // for a draft, failed validation, no-op or refresh.
    void acceptedStateChanged();

private:
    void set_validation(const QString& message);
    void clear_validation();
    TerminalLimiterState* state_{nullptr};
    std::optional<double> draftCeilingDbTp_;
    QString validationField_;
    QString validationMessage_;
    AcceptedStateChange stateChanged_;
    QString audioStatus_{QStringLiteral("BYPASS")};
};

} // namespace rgsml::app
