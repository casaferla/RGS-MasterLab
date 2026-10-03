#include "gain_view_model.hpp"

#include <rgsml/dsp/module_registry.hpp>

#include <QUuid>

#include <cmath>
#include <utility>

namespace rgsml::app {

namespace {
constexpr std::size_t kMaxGainHistoryDepth = 50;
}

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

    auto* controller = externalPreviewController_ ? externalPreviewController_ : ownedPreviewController_.get();
    if (controller != nullptr) {
        connect(controller, &MasteringPreviewController::changed, this, &GainViewModel::changed);
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

bool GainViewModel::can_undo() const noexcept
{
    return !undoStack_.empty();
}

bool GainViewModel::can_redo() const noexcept
{
    return !redoStack_.empty();
}

QString GainViewModel::validation_error() const
{
    return validationError_;
}

quint64 GainViewModel::preview_generation() const noexcept
{
    if (externalPreviewController_) {
        return externalPreviewController_->preview_generation();
    }
    if (ownedPreviewController_) {
        return ownedPreviewController_->preview_generation();
    }
    return 0;
}

QString GainViewModel::preview_status() const
{
    if (externalPreviewController_) {
        return externalPreviewController_->preview_status();
    }
    if (ownedPreviewController_) {
        return ownedPreviewController_->preview_status();
    }
    return QStringLiteral("IDLE");
}

QString GainViewModel::preview_error() const
{
    if (externalPreviewController_) {
        return externalPreviewController_->preview_error();
    }
    if (ownedPreviewController_) {
        return ownedPreviewController_->preview_error();
    }
    return {};
}

void GainViewModel::push_undo_snapshot(double previousGainDb)
{
    undoStack_.push_back(previousGainDb);
    if (undoStack_.size() > kMaxGainHistoryDepth) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
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

    const double currentGain = gain_db();
    if (currentGain == params.value()->gain_db()) {
        validationError_.clear();
        draftGainDbText_.clear();
        emit changed();
        return true;
    }

    push_undo_snapshot(currentGain);

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

void GainViewModel::undo()
{
    if (!can_undo()) {
        return;
    }

    const double targetGain = undoStack_.back();
    undoStack_.pop_back();
    redoStack_.push_back(gain_db());

    validationError_.clear();
    draftGainDbText_.clear();

    auto params = dsp::GainParameters::create(targetGain);
    if (params) {
        auto& state = active_chain_state();
        static_cast<void>(state.set_gain_parameters(*params.value()));
    }

    emit changed();
    request_preview();
}

void GainViewModel::redo()
{
    if (!can_redo()) {
        return;
    }

    const double targetGain = redoStack_.back();
    redoStack_.pop_back();
    undoStack_.push_back(gain_db());

    validationError_.clear();
    draftGainDbText_.clear();

    auto params = dsp::GainParameters::create(targetGain);
    if (params) {
        auto& state = active_chain_state();
        static_cast<void>(state.set_gain_parameters(*params.value()));
    }

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
    undoStack_.clear();
    redoStack_.clear();

    auto& state = active_chain_state();
    auto defaultGain = dsp::GainParameters::create(0.0);
    if (defaultGain) {
        static_cast<void>(state.set_gain_parameters(*defaultGain.value()));
    }
    static_cast<void>(state.set_user_bypass(state.gain_instance_id(), false));

    emit changed();
}

void GainViewModel::refreshFromAuthority()
{
    validationError_.clear();
    draftGainDbText_.clear();
    undoStack_.clear();
    redoStack_.clear();
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
