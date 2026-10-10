#include "stereo_ms_view_model.hpp"

#include <utility>
#include <cmath>
#include <QVariantMap>
#include <algorithm>

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
    connect(&telemetryTimer_, &QTimer::timeout,
            this, &StereoMsViewModel::refreshTelemetry);
    telemetryTimer_.setInterval(33);
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


void StereoMsViewModel::setSignalFormat(
    double effectiveSampleRateHz, int channelCount)
{
    // Unverified/invalid source formats MUST disable response rendering,
    // not silently reuse the previous source's DSP sample rate.
    const bool valid = std::isfinite(effectiveSampleRateHz)
        && effectiveSampleRateHz > 0.0
        && (channelCount == 1 || channelCount == 2);
    const double newRate = valid ? effectiveSampleRateHz : 0.0;
    const int newChannels = valid ? channelCount : 0;
    if (newRate == effectiveSampleRateHz_ && newChannels == sourceChannelCount_)
        return;
    effectiveSampleRateHz_ = newRate;
    sourceChannelCount_ = newChannels;
    emit changed();
}

QString StereoMsViewModel::width_response_status() const
{
    if (!available()) return QStringLiteral("MODULE_UNAVAILABLE");
    if (sourceChannelCount_ == 0) return QStringLiteral("SOURCE_UNAVAILABLE");
    if (sourceChannelCount_ == 1) return QStringLiteral("MONO_INPUT");
    if (bypass()) return QStringLiteral("BYPASSED");

    const auto* params = editing();
    if (!params) return QStringLiteral("MODULE_UNAVAILABLE");
    // Use only the C++ DSP response evaluator, which consumes the actual
    // frozen crossover coefficients. QML will transform points to pixels.
    const auto response = dsp::stereo_ms_width_response_grid(
        *params, effectiveSampleRateHz_);
    return response ? QStringLiteral("READY")
                    : QStringLiteral("RESPONSE_UNAVAILABLE");
}

QVariantList StereoMsViewModel::width_response_points() const
{
    QVariantList points;
    if (width_response_status() != QStringLiteral("READY")) return points;
    const auto* params = editing();
    if (!params) return points;
    const auto response = dsp::stereo_ms_width_response_grid(
        *params, effectiveSampleRateHz_);
    if (!response) return points;
    points.reserve(static_cast<qsizetype>(response.value()->size()));
    for (const auto& point : *response.value()) {
        QVariantMap item;
        item.insert(QStringLiteral("frequencyHz"), point.frequency_hz);
        item.insert(QStringLiteral("widthPercent"), point.effective_width_percent);
        points.append(item);
    }
    return points;
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

bool StereoMsViewModel::setDraftFieldValue(const QString& field, double value)
{
    if (field == QStringLiteral("widthPercent")) return setDraftWidthPercent(value);
    if (field == QStringLiteral("midGainDb")) return setDraftMidGainDb(value);
    if (field == QStringLiteral("sideGainDb")) return setDraftSideGainDb(value);
    if (field == QStringLiteral("monoBassCutoffHz")) return setDraftMonoBassCutoffHz(value);
    if (field == QStringLiteral("lowBandWidthPercent")) return setDraftLowBandWidthPercent(value);
    set_error(field, QStringLiteral("Unsupported Stereo/M-S parameter field."));
    return false;
}

bool StereoMsViewModel::setDraftFieldText(const QString& field, const QString& text)
{
    bool converted = false;
    const double value = text.trimmed().toDouble(&converted);
    if (!converted || !std::isfinite(value)) {
        set_error(field, QStringLiteral("Enter a finite numeric value."));
        return false;
    }
    return setDraftFieldValue(field, value);
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
    // A partially typed invalid field must never commit a previously valid
    // staged graph/slider value merely because the field lost focus.
    if (!validationMessage_.isEmpty()) return false;
    if (!available() || !draft_) {
        // With no draft, nothing was committed and no preview should occur.
        return available();
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
    effectiveSampleRateHz_ = 0.0;
    sourceChannelCount_ = 0;
    const auto status = chainState_->set_stereo_ms_parameters(*defaults.value());
    if (!status) {
        set_error(QStringLiteral("reset"),
                  QString::fromStdString(status.error()->message()));
        return;
    }
    clear_error();
    emit changed();
}


void StereoMsViewModel::setTelemetryProviders(
    AcceptedRenderProvider acceptedRender,
    PlaybackSnapshotProvider playback,
    AuditionProcessedProvider auditionProcessed)
{
    telemetryTimer_.stop();
    telemetryResolver_.reset();
    acceptedRenderProvider_ = std::move(acceptedRender);
    playbackSnapshotProvider_ = std::move(playback);
    auditionProcessedProvider_ = std::move(auditionProcessed);
    refreshTelemetry();
    if (acceptedRenderProvider_ && playbackSnapshotProvider_ &&
        auditionProcessedProvider_) {
        telemetryTimer_.start();
    }
}

void StereoMsViewModel::refreshTelemetry()
{
    if (!acceptedRenderProvider_ || !playbackSnapshotProvider_ ||
        !auditionProcessedProvider_) {
        telemetryResolver_.reset();
    } else {
        const auto accepted = acceptedRenderProvider_();
        if (accepted.result && accepted.realization_id && chainState_ &&
            chainState_->stereo_ms_instance_id()) {
            const auto module_id = *chainState_->stereo_ms_instance_id();
            // Disposition comes from the immutable accepted render, NEVER
            // from an editor draft or the current chain bypass control.
            const auto& sigs = accepted.result->signatures();
            const auto sig = std::find_if(sigs.begin(), sigs.end(),
                [&module_id](const render::ModuleExecutionSignature& s) {
                    return s.instance_id == module_id &&
                        s.type_id == "rgsml.dsp.stereo-ms";
                });
            if (sig != sigs.end()) {
                if (sig->disposition ==
                    render::ModuleExecutionDisposition::BYPASS_IDENTITY) {
                    telemetryResolver_.register_bypass(*accepted.realization_id);
                } else if (sig->disposition ==
                           render::ModuleExecutionDisposition::PROCESSED) {
                    const auto& stages =
                        accepted.result->stereo_ms_stage_output_sidecars();
                    const auto hit = std::find_if(stages.begin(), stages.end(),
                        [&module_id, &accepted](const render::StereoMsStageOutputSidecar& s) {
                            return s.module_instance_id == module_id &&
                                s.realization_id == accepted.realization_id &&
                                s.chain_revision == accepted.result->chain_revision();
                        });
                    if (hit != stages.end()) {
                        // Aliasing ownership holds immutable stage evidence
                        // alive after the Processed candidate is superseded.
                        telemetryResolver_.register_sidecar(
                            std::shared_ptr<const render::StereoMsStageOutputSidecar>(
                                accepted.result, &*hit));
                    }
                }
            }
        }
        const auto snapshot = playbackSnapshotProvider_();
        if (snapshot) {
            telemetryResolver_.update(*snapshot.value(),
                auditionProcessedProvider_());
        } else {
            // Unknown audible clock => no ongoing sample attribution.
            telemetryResolver_.reset();
        }
    }

    QString status = QStringLiteral("UNAVAILABLE");
    using S = render::StereoMsAudibleStatus;
    switch (telemetryResolver_.status()) {
    case S::UNAVAILABLE: break;
    case S::NOT_AUDITIONED: status = QStringLiteral("NOT AUDITIONED"); break;
    case S::STOPPED: status = QStringLiteral("STOPPED / END"); break;
    case S::PAUSED: status = QStringLiteral("PAUSED"); break;
    case S::TRANSITION: status = QStringLiteral("TRANSITION"); break;
    case S::BYPASS: status = QStringLiteral("BYPASS"); break;
    case S::ACTIVE: status = QStringLiteral("ACTIVE"); break;
    }

    const bool active = telemetryResolver_.status() == S::ACTIVE;
    const bool gap = telemetryResolver_.gap_detected();
    const auto id = telemetryResolver_.active_realization_id();
    const QString realizationId = id
        ? QString::number(id->value) : QString{};

    QVariantList buckets;
    const auto& history = telemetryResolver_.density_history();
    buckets.reserve(static_cast<qsizetype>(history.size()));
    for (const auto& b : history) {
        QVariantMap item;
        item.insert(QStringLiteral("beginFrame"), static_cast<qlonglong>(b.begin_frame));
        item.insert(QStringLiteral("endFrame"), static_cast<qlonglong>(b.end_frame));
        item.insert(QStringLiteral("frameCount"), static_cast<qulonglong>(b.frame_count));
        item.insert(QStringLiteral("validFrameCount"), static_cast<qulonglong>(b.valid_frame_count));
        item.insert(QStringLiteral("zeroVectorCount"), static_cast<qulonglong>(b.zero_vector_count));
        item.insert(QStringLiteral("overflowCount"), static_cast<qulonglong>(b.overflow_count));
        QVariantList occupancy;
        occupancy.reserve(static_cast<qsizetype>(b.occupancy.size()));
        for (const auto n : b.occupancy) occupancy.append(static_cast<qulonglong>(n));
        item.insert(QStringLiteral("occupancy"), occupancy);
        buckets.append(item);
    }

    QVariantMap correlation;
    if (const auto& window = telemetryResolver_.correlation()) {
        correlation.insert(QStringLiteral("beginFrame"),
                           static_cast<qlonglong>(window->begin_frame));
        correlation.insert(QStringLiteral("endFrame"),
                           static_cast<qlonglong>(window->end_frame));
        // Undefined AC is represented by null, NEVER a fabricated 0.
        correlation.insert(QStringLiteral("valid"),
            window->validity == render::StereoMsCorrelationWindowValidity::VALID &&
            window->rho.has_value());
        if (window->validity == render::StereoMsCorrelationWindowValidity::VALID &&
            window->rho.has_value()) {
            correlation.insert(QStringLiteral("value"), *window->rho);
        }
    }
    QVariantMap sideLow;
    if (const auto& window = telemetryResolver_.side_low()) {
        sideLow.insert(QStringLiteral("beginFrame"),
                       static_cast<qlonglong>(window->begin_frame));
        sideLow.insert(QStringLiteral("endFrame"),
                       static_cast<qlonglong>(window->end_frame));
        sideLow.insert(QStringLiteral("before"), window->rms_before);
        sideLow.insert(QStringLiteral("after"), window->rms_after);
    }
    if (telemetryStatus_ != status || telemetryActive_ != active ||
        telemetryGap_ != gap || telemetryRealizationId_ != realizationId ||
        telemetryDensityBuckets_ != buckets ||
        telemetryCorrelation_ != correlation || telemetrySideLow_ != sideLow) {
        telemetryStatus_ = std::move(status);
        telemetryActive_ = active;
        telemetryGap_ = gap;
        telemetryRealizationId_ = realizationId;
        telemetryDensityBuckets_ = std::move(buckets);
        telemetryCorrelation_ = std::move(correlation);
        telemetrySideLow_ = std::move(sideLow);
        emit telemetryChanged();
    }
}

void StereoMsViewModel::request_preview()
{
    if (previewController_) previewController_->request_preview();
}

}  // namespace rgsml::app
