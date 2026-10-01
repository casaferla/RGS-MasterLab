#pragma once

#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"

#include <rgsml/dsp/gain_parameters.hpp>

#include <QObject>
#include <QString>

#include <memory>

namespace rgsml::app {

class GainViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double gainDb READ gain_db NOTIFY changed)
    Q_PROPERTY(QString gainDbText READ gain_db_text NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass NOTIFY changed)
    Q_PROPERTY(QString validationError READ validation_error NOTIFY changed)

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
    [[nodiscard]] QString validation_error() const;

    Q_INVOKABLE bool setGainDb(double gainDb);
    Q_INVOKABLE bool setGainDbText(const QString& text);
    Q_INVOKABLE void setBypass(bool bypass);
    Q_INVOKABLE void resetToDefault();
    Q_INVOKABLE void resetForNewSource();

signals:
    void changed();

private:
    [[nodiscard]] MasteringChainState& active_chain_state() const noexcept;
    void request_preview();

    MasteringChainState* externalChainState_{nullptr};
    std::unique_ptr<MasteringChainState> ownedChainState_;
    MasteringPreviewController* externalPreviewController_{nullptr};
    std::unique_ptr<MasteringPreviewController> ownedPreviewController_;

    QString validationError_;
    QString draftGainDbText_;
};

}  // namespace rgsml::app
