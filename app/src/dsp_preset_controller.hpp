#pragma once
// User-initiated Open/Save preset operations. No M12 ownership change.
// A file load is a reviewable MANUAL state change; one native M18
// mutation admission commits the entire validated after-state.
#include "mastering_chain_state.hpp"
#include "internal/dsp_manual_preset_bundle.hpp"
#include <QObject>
#include <QUrl>
#include <QString>
#include <optional>
#include <cstddef>

namespace rgsml::app {
class MasteringPreviewController;
class DspChainAdapterModel;
class DspPresetController final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QString errorText READ error_text NOTIFY changed)
public:
    explicit DspPresetController(
        MasteringChainState* state,
        MasteringPreviewController* preview,
        DspChainAdapterModel* chainAdapter,
        QObject* parent=nullptr);
    [[nodiscard]] QString error_text()const{return errorText_;}
    Q_INVOKABLE bool saveChainPreset(const QUrl& destination);
    Q_INVOKABLE bool openChainPreset(const QUrl& source);
    Q_INVOKABLE bool saveModulePreset(
        const QUrl& destination,const QString& moduleInstanceId);
    Q_INVOKABLE bool openModulePreset(
        const QUrl& source,const QString& moduleInstanceId);
    // A future Product AutoMatch action can call this synchronously
    // before starting its worker; it is NOT an M12 takeover permission.
    [[nodiscard]] bool checkpointManualBeforeAutoMatch();
    [[nodiscard]] bool hasAutomaticManualCheckpoint()const noexcept {
        return checkpoint_.has_value();
    }
signals:
    void changed();
private:
    [[nodiscard]] std::optional<std::size_t>
    indexFor(const QString& instanceId);
    [[nodiscard]] bool save(
        const QUrl& target,std::optional<std::size_t> moduleIndex);
    [[nodiscard]] bool open(
        const QUrl& source,std::optional<std::size_t> moduleIndex);
    [[nodiscard]] bool error(const QString& what);
    void clearError();
    MasteringChainState* state_;
    MasteringPreviewController* preview_;
    DspChainAdapterModel* adapter_;
    QString errorText_;
    std::optional<dsp_presets::ManualPreset> checkpoint_;
};
} // namespace rgsml::app
