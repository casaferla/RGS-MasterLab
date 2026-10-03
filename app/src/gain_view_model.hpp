#pragma once

#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"

#include <rgsml/dsp/gain_parameters.hpp>

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace rgsml::app {

class GainViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double gainDb READ gain_db NOTIFY changed)
    Q_PROPERTY(QString gainDbText READ gain_db_text NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass NOTIFY changed)
    Q_PROPERTY(bool canUndo READ can_undo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ can_redo NOTIFY changed)
    Q_PROPERTY(QString validationError READ validation_error NOTIFY changed)
    Q_PROPERTY(quint64 previewGeneration READ preview_generation NOTIFY changed)
    Q_PROPERTY(QString previewStatus READ preview_status NOTIFY changed)
    Q_PROPERTY(QString previewError READ preview_error NOTIFY changed)

public:
    explicit GainViewModel(
        MasteringChainState* chainState = nullptr,
        MasteringPreviewController* previewController = nullptr,
        QObject* parent = nullptr);
    ~GainViewModel() override = default;

    GainViewModel(const GainViewModel&) = delete;
    GainViewModel& operator=(const GainViewModel&) = delete;

    [[nodiscard]] double gain_db() const noexcept;
    [[nodiscard]] QString gain_db_text() const;
    [[nodiscard]] bool bypass() const noexcept;
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;
    [[nodiscard]] QString validation_error() const;
    [[nodiscard]] quint64 preview_generation() const noexcept;
    [[nodiscard]] QString preview_status() const;
    [[nodiscard]] QString preview_error() const;

    Q_INVOKABLE bool setGainDb(double gainDb);
    Q_INVOKABLE bool setGainDbText(const QString& text);
    Q_INVOKABLE void setBypass(bool bypass);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void resetToDefault();
    Q_INVOKABLE void resetForNewSource();
    Q_INVOKABLE void refreshFromAuthority();

signals:
    void changed();

private:
    [[nodiscard]] MasteringChainState& active_chain_state() const noexcept;
    void request_preview();
    void push_undo_snapshot(double previousGainDb);

    MasteringChainState* externalChainState_{nullptr};
    std::unique_ptr<MasteringChainState> ownedChainState_;
    MasteringPreviewController* externalPreviewController_{nullptr};
    std::unique_ptr<MasteringPreviewController> ownedPreviewController_;

    QString validationError_;
    QString draftGainDbText_;

    std::vector<double> undoStack_;
    std::vector<double> redoStack_;
};

}  // namespace rgsml::app
