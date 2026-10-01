#include "gain_view_model.hpp"

#include <rgsml/dsp/module_registry.hpp>

#include <QUuid>

#include <cmath>
#include <utility>

namespace rgsml::app {

GainViewModel::GainViewModel(
    MasteringChainState* chainState,
    MasteringPreviewController* previewController,
    QObject* parent)
    : QObject(parent)
    , externalChainState_(chainState)
    , externalPreviewController_(previewController)
{
    if (!externalChainState_) {
        auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
        const auto chain_id = *core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value();
        const auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value()).value();
        const auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value()).value();
        auto defaultState = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
        ownedChainState_ = std::make_unique<MasteringChainState>(std::move(*defaultState.value()));
    }

    if (!externalPreviewController_) {
        ownedPreviewController_ = std::make_unique<MasteringPreviewController>(&active_chain_state());
    } else if (externalChainState_ && !externalPreviewController_->preview_generation()) {
        externalPreviewController_->set_chain_state(externalChainState_);
    }
}

MasteringChainState& GainViewModel::active_chain_state() const noexcept
{
    return externalChainState_ ? *externalChainState_ : *ownedChainState_;
}

double GainViewModel::gain_db() const noexcept
{
    return active_chain_state().gain_parameters().gain_db();
}

QString GainViewModel::gain_db_text() const
{
    if (!validationError_.isEmpty() && !draftGainDbText_.isEmpty()) {
        return draftGainDbText_;
    }
    return QString::number(gain_db(), 'f', 1);
}

bool GainViewModel::bypass() const noexcept
{
    const auto& state = active_chain_state();
    const auto res = state.is_bypassed(state.gain_instance_id());
    return res ? *res.value() : false;
}

QString GainViewModel::validation_error() const
{
    return validationError_;
}

bool GainViewModel::setGainDb(double gainDb)
{
    if (std::isnan(gainDb) || std::isinf(gainDb)) {
        validationError_ = QStringLiteral("Gain value must be a valid number.");
        emit changed();
        return false;
    }

    if (gainDb == -0.0 || (gainDb == 0.0 && std::signbit(gainDb))) {
        gainDb = 0.0;
    }

    auto params = dsp::GainParameters::create(gainDb);
    if (!params) {
        validationError_ = QString::fromStdString(params.error()->message());
        emit changed();
        return false;
    }

    validationError_.clear();
    draftGainDbText_.clear();

    auto& state = active_chain_state();
    static_cast<void>(state.set_gain_parameters(*params.value()));

    emit changed();
    request_preview();
    return true;
}

bool GainViewModel::setGainDbText(const QString& text)
{
    draftGainDbText_ = text;
    bool ok = false;
    const double val = text.toDouble(&ok);
    if (!ok) {
        validationError_ = QStringLiteral("Invalid gain text format.");
        emit changed();
        return false;
    }
    return setGainDb(val);
}

void GainViewModel::setBypass(bool bypass)
{
    auto& state = active_chain_state();
    static_cast<void>(state.set_user_bypass(state.gain_instance_id(), bypass));

    emit changed();
    request_preview();
}

void GainViewModel::resetToDefault()
{
    setGainDb(0.0);
}

void GainViewModel::resetForNewSource()
{
    validationError_.clear();
    draftGainDbText_.clear();

    auto& state = active_chain_state();
    auto defaultGain = dsp::GainParameters::create(0.0);
    if (defaultGain) {
        static_cast<void>(state.set_gain_parameters(*defaultGain.value()));
    }
    static_cast<void>(state.set_user_bypass(state.gain_instance_id(), false));

    emit changed();
}

void GainViewModel::request_preview()
{
    if (externalPreviewController_) {
        externalPreviewController_->request_preview();
    } else if (ownedPreviewController_) {
        ownedPreviewController_->request_preview();
    }
}

}  // namespace rgsml::app
