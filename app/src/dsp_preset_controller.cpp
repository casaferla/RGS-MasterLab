#include "dsp_preset_controller.hpp"
#include "mastering_preview_controller.hpp"
#include "dsp_chain_adapter_model.hpp"
#include <rgsml/core/uuid.hpp>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QByteArray>
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace rgsml::app {
DspPresetController::DspPresetController(
    MasteringChainState* state,MasteringPreviewController* preview,
    DspChainAdapterModel* adapter,QObject* parent)
    :QObject(parent),state_(state),preview_(preview),adapter_(adapter)
{
}
bool DspPresetController::error(const QString& what) {
    errorText_=what;emit changed();return false;
}
void DspPresetController::clearError() {
    errorText_.clear();emit changed();
}
std::optional<std::size_t> DspPresetController::indexFor(
    const QString& instanceId)
{
    if(!state_)return std::nullopt;
    const auto uuid=core::Uuid::parse(instanceId.toStdString());
    if(!uuid)return std::nullopt;
    const auto id=dsp::ModuleInstanceId::from_uuid(*uuid.value());
    if(!id)return std::nullopt;
    const auto all=state_->chain().instances();
    for(std::size_t i=0;i<all.size();++i)
        if(all[i].instance_id()==*id.value())return i;
    return std::nullopt;
}
bool DspPresetController::saveChainPreset(const QUrl& target) {
    return save(target,std::nullopt);
}
bool DspPresetController::openChainPreset(const QUrl& source) {
    return open(source,std::nullopt);
}
bool DspPresetController::saveModulePreset(
    const QUrl& target,const QString& id)
{
    const auto index=indexFor(id);
    return index?save(target,index):
        error(QStringLiteral("Module preset: selected DSP is not editable."));
}
bool DspPresetController::openModulePreset(
    const QUrl& source,const QString& id)
{
    const auto index=indexFor(id);
    return index?open(source,index):
        error(QStringLiteral("Module preset: selected DSP is not editable."));
}
bool DspPresetController::save(
    const QUrl& target,std::optional<std::size_t> moduleIndex)
{
    if(!state_||!target.isLocalFile()||target.toLocalFile().isEmpty())
        return error(QStringLiteral("Choose a local preset file."));
    const auto required=moduleIndex?QStringLiteral("rgsdsp"):
        QStringLiteral("rgschain");
    if(QFileInfo(target.toLocalFile()).suffix().compare(required,Qt::CaseInsensitive)!=0)
        return error(moduleIndex
            ?QStringLiteral("Save one DSP module as .rgsdsp.")
            :QStringLiteral("Save the DSP Chain as .rgschain."));
    const auto registry=state_->registry_snapshot();
    if(!registry)return error(QStringLiteral("DSP registry is unavailable."));
    const auto snapshot=dsp_presets::capture(
        state_->chain(),state_->execution_bindings(),*registry,moduleIndex);
    if(!snapshot)return error(QString::fromStdString(
        snapshot.error()->message()));
    const auto encoded=dsp_presets::encode_json(*snapshot.value());
    if(!encoded)return error(QString::fromStdString(
        encoded.error()->message()));
    QSaveFile file(target.toLocalFile());
    if(!file.open(QIODevice::WriteOnly))
        return error(QStringLiteral("Cannot open the preset destination."));
    const auto bytes=QByteArray::fromStdString(*encoded.value());
    if(file.write(bytes)!=bytes.size()||!file.commit())
        return error(QStringLiteral("Cannot atomically save the preset."));
    clearError();
    return true;
}
bool DspPresetController::open(
    const QUrl& source,std::optional<std::size_t> moduleIndex)
{
    if(!state_||!source.isLocalFile()||source.toLocalFile().isEmpty())
        return error(QStringLiteral("Choose a local preset file."));
    const auto suffix=QFileInfo(source.toLocalFile()).suffix().toLower();
    const bool legacy=suffix==QStringLiteral("rgspreset")||
        suffix==QStringLiteral("json");
    if(!legacy&&suffix!=(moduleIndex?QStringLiteral("rgsdsp"):
                                         QStringLiteral("rgschain")))
        return error(moduleIndex
            ?QStringLiteral("Choose a .rgsdsp module preset (or legacy .rgspreset).")
            :QStringLiteral("Choose a .rgschain chain preset (or legacy .rgspreset)."));
    QFile file(source.toLocalFile());
    if(!file.open(QIODevice::ReadOnly)||
       file.size()<=0||file.size()>4*1024*1024)
        return error(QStringLiteral("Preset is missing or too large."));
    const auto bytes=file.readAll();
    if(bytes.size()!=file.size())
        return error(QStringLiteral("Cannot read the full preset."));
    const auto registry=state_->registry_snapshot();
    if(!registry)return error(QStringLiteral("DSP registry is unavailable."));
    const auto loaded=dsp_presets::decode_json(
        bytes.toStdString(),*registry);
    if(!loaded)return error(QString::fromStdString(
        loaded.error()->message()));
    const auto bindings=state_->execution_bindings();
    const auto staged=dsp_presets::prepare_explicit_manual_preset_restore(
        *loaded.value(),state_->chain(),bindings,*registry,moduleIndex);
    if(!staged)return error(QString::fromStdString(
        staged.error()->message()));
    const auto accepted=state_->apply_explicit_manual_preset_draft(
        staged.value()->proposed_manual_chain,
        staged.value()->proposed_bindings);
    if(!accepted)return error(QString::fromStdString(
        accepted.error()->message()));
    if(adapter_)adapter_->refreshFromAuthority();
    if(preview_)preview_->request_preview();
    clearError();
    return true;
}
bool DspPresetController::checkpointManualBeforeAutoMatch() {
    if(!state_)return error(QStringLiteral("No MANUAL chain available."));
    const auto registry=state_->registry_snapshot();
    if(!registry)return error(QStringLiteral("DSP registry is unavailable."));
    const auto saved=dsp_presets::capture(
        state_->chain(),state_->execution_bindings(),*registry);
    if(!saved)return error(QString::fromStdString(
        saved.error()->message()));
    checkpoint_=std::move(*saved.value());
    clearError();
    return true;
}
} // namespace rgsml::app
