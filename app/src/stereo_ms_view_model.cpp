#include "stereo_ms_view_model.hpp"

#include <utility>

namespace rgsml::app {

namespace {
[[nodiscard]] QString label(dsp::MonoBassMode mode)
{
    switch (mode) {
    case dsp::MonoBassMode::OFF: return QStringLiteral("OFF");
    case dsp::MonoBassMode::LR12: return QStringLiteral("LR12");
    case dsp::MonoBassMode::LR24: return QStringLiteral("LR24");
    }
    return QStringLiteral("INVALID");
}
}

StereoMsViewModel::StereoMsViewModel(
    MasteringChainState* chainState,
    MasteringPreviewController* previewController,
    QObject* parent)
    : QObject(parent)
    , chainState_(chainState)
    , previewController_(previewController)
{
    if (previewController_) {
        connect(previewController_, &MasteringPreviewController::changed,
                this, &StereoMsViewModel::changed);
    }
}

const dsp::StereoMsParameters* StereoMsViewModel::committed() const noexcept
{
    if (!chainState_ || !chainState_->stereo_ms_parameters()) {
        return nullptr;
    }
    return &*chainState_->stereo_ms_parameters();
}

const dsp::StereoMsParameters* StereoMsViewModel::editing() const noexcept
{
    return draft_ ? &*draft_ : committed();
}

bool StereoMsViewModel::available() const noexcept
{
    return chainState_ && chainState_->stereo_ms_instance_id().has_value()
        && committed() != nullptr;
}

double StereoMsViewModel::mid_gain_db() const noexcept
{
    return editing() ? editing()->mid_gain_db() : 0.0;
}

double StereoMsViewModel::side_gain_db() const noexcept
{
    return editing() ? editing()->side_gain_db() : 0.0;
}

double StereoMsViewModel::width_percent() const noexcept
{
    // Use the existing frozen M15 coordinate realization: never recreate
    // derived Width arithmetic in QML or in an alternate application helper.
    return committed()
        ? dsp::stereo_ms_width_coordinates(*committed()).current_width_percent
        : 100.0;
}

double StereoMsViewModel::draft_width_percent() const noexcept
{
    return editing()
        ? dsp::stereo_ms_width_coordinates(*editing()).current_width_percent
        : 100.0;
}

bool StereoMsViewModel::side_muted() const noexcept
{
    return editing() ? editing()->side_muted() : false;
}

QString StereoMsViewModel::mono_bass_mode() const
{
    return editing() ? label(editing()->mono_bass_mode()) : QStringLiteral("OFF");
}

double StereoMsViewModel::mono_bass_cutoff_hz() const noexcept
{
    return editing() ? editing()->mono_bass_cutoff_hz() : 120.0;
}

double StereoMsViewModel::low_band_width_percent() const noexcept
{
    return editing() ? editing()->low_band_width_percent() : 100.0;
}

bool StereoMsViewModel::mono_bass_controls_effective() const noexcept
{
    const auto* params = editing();
    // Stored non-effective values must remain intact. Disabled does not
    // mean reset, and LR12/LR24 at 100% is not equivalent to OFF.
    return available() && !bypass() && params && !params->side_muted()
        && params->mono_bass_mode() != dsp::MonoBassMode::OFF;
}

bool StereoMsViewModel::bypass() const noexcept
{
    if (!available()) return false;
    const auto status = chainState_->is_bypassed(*chainState_->stereo_ms_instance_id());
    return status ? *status.value() : false;
}

QString StereoMsViewModel::validation_field() const { return validationField_; }
QString StereoMsViewModel::validation_message() const { return validationMessage_; }

quint64 StereoMsViewModel::preview_generation() const noexcept
{
    return previewController_ ? previewController_->preview_generation() : 0;
}

QString StereoMsViewModel::preview_status() const
{
    return previewController_ ? previewController_->preview_status() : QStringLiteral("IDLE");
}

QString StereoMsViewModel::preview_error() const
{
    return previewController_ ? previewController_->preview_error() : QString{};
}

void StereoMsViewModel::set_error(const QString& field, const QString& message)
{
    validationField_ = field;
    validationMessage_ = message;
    emit changed();
}

void StereoMsViewModel::clear_error()
{
    validationField_.clear();
    validationMessage_.clear();
}

bool StereoMsViewModel::stage(
    double mid, double side, bool mute, dsp::MonoBassMode mode,
    double cutoff, double lowWidth, const QString& field)
{
    if (!available()) {
        set_error(field, QStringLiteral("Stereo/M-S is not present in this chain."));
        return false;
    }
    const auto candidate = dsp::StereoMsParameters::create(
        mid, side, mute, mode, cutoff, lowWidth);
    if (!candidate) {
        set_error(field, QString::fromStdString(candidate.error()->message()));
        return false;
    }
    draft_ = *candidate.value();
    clear_error();
    emit changed();
    return true;
}

bool StereoMsViewModel::setDraftMidGainDb(double value)
{
    const auto* p = editing();
    return p && stage(value, p->side_gain_db(), p->side_muted(),
        p->mono_bass_mode(), p->mono_bass_cutoff_hz(),
        p->low_band_width_percent(), QStringLiteral("midGainDb"));
}

bool StereoMsViewModel::setDraftSideGainDb(double value)
{
    const auto* p = editing();
    return p && stage(p->mid_gain_db(), value, p->side_muted(),
        p->mono_bass_mode(), p->mono_bass_cutoff_hz(),
        p->low_band_width_percent(), QStringLiteral("sideGainDb"));
}

bool StereoMsViewModel::setDraftSideMuted(bool value)
{
    const auto* p = editing();
    return p && stage(p->mid_gain_db(), p->side_gain_db(), value,
        p->mono_bass_mode(), p->mono_bass_cutoff_hz(),
        p->low_band_width_percent(), QStringLiteral("sideMuted"));
}

bool StereoMsViewModel::setDraftMonoBassCutoffHz(double value)
{
    const auto* p = editing();
    return p && stage(p->mid_gain_db(), p->side_gain_db(), p->side_muted(),
        p->mono_bass_mode(), value,
        p->low_band_width_percent(), QStringLiteral("monoBassCutoffHz"));
}

bool StereoMsViewModel::setDraftLowBandWidthPercent(double value)
{
    const auto* p = editing();
    return p && stage(p->mid_gain_db(), p->side_gain_db(), p->side_muted(),
        p->mono_bass_mode(), p->mono_bass_cutoff_hz(),
        value, QStringLiteral("lowBandWidthPercent"));
}

bool StereoMsViewModel::setDraftMonoBassMode(const QString& mode)
{
    const auto* p = editing();
    if (!p) return false;
    dsp::MonoBassMode typed{};
    if (mode == QStringLiteral("OFF")) typed = dsp::MonoBassMode::OFF;
    else if (mode == QStringLiteral("LR12")) typed = dsp::MonoBassMode::LR12;
    else if (mode == QStringLiteral("LR24")) typed = dsp::MonoBassMode::LR24;
    else {
        set_error(QStringLiteral("monoBassMode"),
                  QStringLiteral("Mode must be OFF, LR12 or LR24."));
        return false;
    }
    return stage(p->mid_gain_db(), p->side_gain_db(), p->side_muted(),
        typed, p->mono_bass_cutoff_hz(),
        p->low_band_width_percent(), QStringLiteral("monoBassMode"));
}

bool StereoMsViewModel::setDraftWidthPercent(double value)
{
    const auto* p = editing();
    if (!p) return false;
    const auto result = dsp::stereo_ms_edit_width_preserving_common_gain(*p, value);
    if (!result) {
        set_error(QStringLiteral("widthPercent"),
                  QString::fromStdString(result.error()->message()));
        return false;
    }
    draft_ = *result.value();
    clear_error();
    emit changed();
    return true;
}

bool StereoMsViewModel::commitDraft()
{
    if (!available() || !draft_) {
        // With no draft, nothing was committed and no preview should occur.
        return available() && validationMessage_.isEmpty();
    }
    if (*draft_ == *committed()) {
        draft_.reset();
        clear_error();
        emit changed();
        return true;
    }

    // One immutable chain write, then one preview invalidation. Never
    // publish partially validated graph/slider movement to the audio path.
    const auto status = chainState_->set_stereo_ms_parameters(*draft_);
    if (!status) {
        set_error(QStringLiteral("commit"),
                  QString::fromStdString(status.error()->message()));
        return false;
    }
    draft_.reset();
    clear_error();
    emit changed();
    request_preview();
    return true;
}

void StereoMsViewModel::cancelDraft()
{
    draft_.reset();
    clear_error();
    emit changed();
}

void StereoMsViewModel::resetToDefault()
{
    if (!available()) return;
    const auto defaults = dsp::StereoMsParameters::create_default();
    if (!defaults) return;
    draft_ = *defaults.value();
    // Reset changes parameters only; it never implicitly toggles bypass.
    static_cast<void>(commitDraft());
}

void StereoMsViewModel::setBypass(bool desired)
{
    if (!available() || desired == bypass()) return;
    const auto status = chainState_->set_user_bypass(
        *chainState_->stereo_ms_instance_id(), desired);
    if (!status) {
        set_error(QStringLiteral("bypass"),
                  QString::fromStdString(status.error()->message()));
        return;
    }
    clear_error();
    emit changed();
    request_preview();
}

void StereoMsViewModel::refreshFromAuthority()
{
    draft_.reset();
    clear_error();
    emit changed();
}

void StereoMsViewModel::resetForNewSource()
{
    if (!available()) return;
    const auto defaults = dsp::StereoMsParameters::create_default();
    if (!defaults) return;
    // Source changes must not retain an unfinished pointer/slider draft.
    draft_.reset();
    const auto status = chainState_->set_stereo_ms_parameters(*defaults.value());
    if (!status) {
        set_error(QStringLiteral("reset"),
                  QString::fromStdString(status.error()->message()));
        return;
    }
    clear_error();
    emit changed();
}

void StereoMsViewModel::request_preview()
{
    if (previewController_) previewController_->request_preview();
}

}  // namespace rgsml::app
