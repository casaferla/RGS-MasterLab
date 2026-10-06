#include "compressor_view_model.hpp"
#include "audition_source_selector.hpp"
#include "playback_transport_view_model.hpp"

#include <rgsml/dsp/module_registry.hpp>

#include <QUuid>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace rgsml::app {
namespace {

[[nodiscard]] QString detector_mode_to_string(dsp::CompressorDetectorMode mode)
{
    switch (mode) {
    case dsp::CompressorDetectorMode::PEAK: return QStringLiteral("PEAK");
    case dsp::CompressorDetectorMode::RMS: return QStringLiteral("RMS");
    }
    return QStringLiteral("RMS");
}

[[nodiscard]] std::optional<dsp::CompressorDetectorMode> string_to_detector_mode(const QString& str)
{
    if (str == QStringLiteral("PEAK")) return dsp::CompressorDetectorMode::PEAK;
    if (str == QStringLiteral("RMS")) return dsp::CompressorDetectorMode::RMS;
    return std::nullopt;
}

[[nodiscard]] QString channel_link_to_string(dsp::CompressorChannelLink link)
{
    switch (link) {
    case dsp::CompressorChannelLink::LINKED_MAX: return QStringLiteral("LINKED_MAX");
    case dsp::CompressorChannelLink::LINKED_MEAN: return QStringLiteral("LINKED_MEAN");
    case dsp::CompressorChannelLink::DUAL_MONO: return QStringLiteral("DUAL_MONO");
    }
    return QStringLiteral("LINKED_MAX");
}

[[nodiscard]] std::optional<dsp::CompressorChannelLink> string_to_channel_link(const QString& str)
{
    if (str == QStringLiteral("LINKED_MAX")) return dsp::CompressorChannelLink::LINKED_MAX;
    if (str == QStringLiteral("LINKED_MEAN")) return dsp::CompressorChannelLink::LINKED_MEAN;
    if (str == QStringLiteral("DUAL_MONO")) return dsp::CompressorChannelLink::DUAL_MONO;
    return std::nullopt;
}

[[nodiscard]] double compute_gain_reduction_db(
    double x_db,
    double threshold,
    double ratio,
    double knee) noexcept
{
    if (ratio == 1.0) {
        return 0.0;
    }
    const double inv_ratio_sub_one = (1.0 / ratio) - 1.0;
    const double one_sub_inv_ratio = 1.0 - (1.0 / ratio);

    if (knee == 0.0) {
        if (x_db <= threshold) {
            return 0.0;
        }
        return (x_db - threshold) * one_sub_inv_ratio;
    }

    const double diff = x_db - threshold;
    if (2.0 * diff < -knee) {
        return 0.0;
    }
    if (2.0 * std::abs(diff) <= knee) {
        const double term = diff + (knee * 0.5);
        const double y_db = x_db + (inv_ratio_sub_one * term * term) / (2.0 * knee);
        return x_db - y_db;
    }
    return diff * one_sub_inv_ratio;
}

[[nodiscard]] QString user_validation_message(const QString& fieldName, bool parseFailure = false)
{
    QString label;
    QString range;

    if (fieldName == QStringLiteral("thresholdDbfs") || fieldName == QStringLiteral("threshold")) {
        label = QStringLiteral("THRESHOLD");
        range = QStringLiteral("-120.0 to 0.0 dBFS");
    } else if (fieldName == QStringLiteral("ratio")) {
        label = QStringLiteral("RATIO");
        range = QStringLiteral("1.00:1 to 20.00:1");
    } else if (fieldName == QStringLiteral("kneeDb") || fieldName == QStringLiteral("knee")) {
        label = QStringLiteral("KNEE");
        range = QStringLiteral("0.0 to 24.0 dB");
    } else if (fieldName == QStringLiteral("attackMs") || fieldName == QStringLiteral("attack")) {
        label = QStringLiteral("ATTACK");
        range = QStringLiteral("0.1 to 500.0 ms");
    } else if (fieldName == QStringLiteral("releaseMs") || fieldName == QStringLiteral("release")) {
        label = QStringLiteral("RELEASE");
        range = QStringLiteral("1.0 to 5000.0 ms");
    } else if (fieldName == QStringLiteral("rmsTimeConstantMs") || fieldName == QStringLiteral("rmsTime")) {
        label = QStringLiteral("RMS TIME");
        range = QStringLiteral("1.0 to 500.0 ms");
    } else if (fieldName == QStringLiteral("lookAheadMs") || fieldName == QStringLiteral("lookAhead")) {
        label = QStringLiteral("LOOKAHEAD");
        range = QStringLiteral("0.0 to 20.0 ms");
    } else if (fieldName == QStringLiteral("mixPercent") || fieldName == QStringLiteral("mix")) {
        label = QStringLiteral("MIX");
        range = QStringLiteral("0.0% to 100.0%");
    } else if (fieldName == QStringLiteral("makeupGainDb") || fieldName == QStringLiteral("makeup")) {
        label = QStringLiteral("MAKE-UP");
        range = QStringLiteral("-24.0 to +24.0 dB");
    } else {
        return QStringLiteral("Invalid Compressor parameter");
    }

    if (parseFailure) {
        return label + QStringLiteral(" must be a number");
    }
    return label + QStringLiteral(" must be within ") + range;
}

}  // namespace

CompressorViewModel::CompressorViewModel(
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
        const auto comp_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()).value()).value();
        auto defaultState = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id, comp_id);
        ownedChainState_ = std::make_unique<MasteringChainState>(std::move(*defaultState.value()));
    }

    if (!externalPreviewController_) {
        ownedPreviewController_ = std::make_unique<MasteringPreviewController>(&active_chain_state());
    } else if (externalChainState_ && !externalPreviewController_->preview_generation()) {
        externalPreviewController_->set_chain_state(externalChainState_);
    }

    connect(&active_preview_controller(), &MasteringPreviewController::changed, this, &CompressorViewModel::changed);

    connect(&telemetryTimer_, &QTimer::timeout, this, &CompressorViewModel::poll_telemetry);
    telemetryTimer_.start(33);

    refreshFromAuthority();
}

void CompressorViewModel::set_audition_selector(QObject* selector) noexcept
{
    auditionSelector_ = selector;
}

void CompressorViewModel::set_playback_transport(QObject* transport) noexcept
{
    playbackTransport_ = transport;
}

void CompressorViewModel::poll_telemetry()
{
    auto* audition = qobject_cast<AuditionSourceSelector*>(auditionSelector_);
    auto* transport = qobject_cast<PlaybackTransportViewModel*>(playbackTransport_);

    if (!audition) {
        liveGrState_ = QStringLiteral("UNAVAILABLE");
        emit changed();
        return;
    }

    const bool is_processed = (audition->active_target() == AuditionTarget::PROCESSED);
    const bool is_playing = transport ? transport->is_playing() : false;
    const bool is_paused = transport ? transport->is_paused() : false;
    const std::int64_t current_frame = transport ? transport->position_frames() : 0;

    const render::CompressorTelemetrySidecar* sidecar{nullptr};
    const auto snapshot = audition->processed_realization_snapshot();
    if (snapshot && snapshot->compressor_telemetry_sidecar().has_value()) {
        sidecar = &(*snapshot->compressor_telemetry_sidecar());
    }

    bool is_audible_bypassed = false;
    bool is_audible_dry_only = false;
    if (snapshot != nullptr) {
        for (const auto& sig : snapshot->signatures()) {
            if (sig.type_id == "rgsml.dsp.compressor") {
                if (sig.disposition == render::ModuleExecutionDisposition::BYPASS_IDENTITY) {
                    is_audible_bypassed = true;
                } else if (const auto* compPayload = std::get_if<render::CompressorExecutionSignaturePayload>(&sig.payload)) {
                    if (compPayload->mix_percent == 0.0) {
                        is_audible_dry_only = true;
                    }
                }
            }
        }
    }

    update_telemetry_observation(
        current_frame,
        is_playing,
        is_paused,
        false,
        is_processed,
        is_audible_bypassed,
        is_audible_dry_only,
        sidecar);
}

MasteringChainState& CompressorViewModel::active_chain_state() const noexcept
{
    return externalChainState_ ? *externalChainState_ : *ownedChainState_;
}

MasteringPreviewController& CompressorViewModel::active_preview_controller() const noexcept
{
    return externalPreviewController_ ? *externalPreviewController_ : *ownedPreviewController_;
}

bool CompressorViewModel::is_mono_prepared() const noexcept
{
    const auto& provider = active_preview_controller().snapshot_provider();
    if (provider) {
        const auto snapshot = provider();
        if (snapshot) {
            return snapshot->view().format().channel_layout() == audio::ChannelLayout::MONO_C;
        }
    }
    return false;
}

void CompressorViewModel::request_preview()
{
    active_preview_controller().request_preview();
}

QString CompressorViewModel::detector_mode() const
{
    return detector_mode_to_string(draftDetectorMode_);
}

QString CompressorViewModel::channel_link() const
{
    return channel_link_to_string(draftChannelLink_);
}

double CompressorViewModel::threshold_dbfs() const noexcept
{
    return active_chain_state().compressor_parameters().threshold_dbfs();
}

double CompressorViewModel::ratio() const noexcept
{
    return active_chain_state().compressor_parameters().ratio();
}

double CompressorViewModel::knee_db() const noexcept
{
    return active_chain_state().compressor_parameters().knee_db();
}

double CompressorViewModel::attack_ms() const noexcept
{
    return active_chain_state().compressor_parameters().attack_ms();
}

double CompressorViewModel::release_ms() const noexcept
{
    return active_chain_state().compressor_parameters().release_ms();
}

double CompressorViewModel::rms_time_constant_ms() const noexcept
{
    return active_chain_state().compressor_parameters().rms_time_constant_ms();
}

double CompressorViewModel::look_ahead_ms() const noexcept
{
    return active_chain_state().compressor_parameters().look_ahead_ms();
}

double CompressorViewModel::mix_percent() const noexcept
{
    return active_chain_state().compressor_parameters().mix_percent();
}

double CompressorViewModel::makeup_gain_db() const noexcept
{
    return active_chain_state().compressor_parameters().makeup_gain_db();
}

QString CompressorViewModel::threshold_text() const
{
    return draftThresholdText_;
}

QString CompressorViewModel::ratio_text() const
{
    return draftRatioText_;
}

QString CompressorViewModel::knee_text() const
{
    return draftKneeText_;
}

QString CompressorViewModel::attack_text() const
{
    return draftAttackText_;
}

QString CompressorViewModel::release_text() const
{
    return draftReleaseText_;
}

QString CompressorViewModel::rms_time_constant_text() const
{
    return draftRmsTimeConstantText_;
}

QString CompressorViewModel::look_ahead_text() const
{
    return draftLookAheadText_;
}

QString CompressorViewModel::mix_percent_text() const
{
    return draftMixPercentText_;
}

QString CompressorViewModel::makeup_gain_text() const
{
    return draftMakeupGainText_;
}

bool CompressorViewModel::bypass() const noexcept
{
    const auto& state = active_chain_state();
    const auto res = state.is_bypassed(state.compressor_instance_id());
    return res ? *res.value() : false;
}

bool CompressorViewModel::can_undo() const noexcept
{
    return !undoStack_.empty();
}

bool CompressorViewModel::can_redo() const noexcept
{
    return !redoStack_.empty();
}

QString CompressorViewModel::validation_field() const
{
    return validationField_;
}

QString CompressorViewModel::validation_message() const
{
    return validationMessage_;
}

quint64 CompressorViewModel::preview_generation() const noexcept
{
    return active_preview_controller().preview_generation();
}

QString CompressorViewModel::preview_status() const
{
    return active_preview_controller().preview_status();
}

QString CompressorViewModel::preview_error() const
{
    return active_preview_controller().preview_error();
}

bool CompressorViewModel::rms_time_effective() const noexcept
{
    return draftDetectorMode_ == dsp::CompressorDetectorMode::RMS;
}

bool CompressorViewModel::channel_link_effective() const noexcept
{
    return !is_mono_prepared();
}

QVariantList CompressorViewModel::transfer_curve_points() const
{
    QVariantList list;
    constexpr int kPoints = 191;
    constexpr double kMinDbfs = -120.0;
    constexpr double kMaxDbfs = 6.0;
    constexpr double kStep = (kMaxDbfs - kMinDbfs) / (kPoints - 1);

    list.reserve(kPoints);
    for (int i = 0; i < kPoints; ++i) {
        const double inDbfs = kMinDbfs + i * kStep;
        const double gr = compute_gain_reduction_db(inDbfs, draftThresholdDbfs_, draftRatio_, draftKneeDb_);
        const double outDbfs = inDbfs - gr + draftMakeupGainDb_;

        QVariantMap map;
        map.insert(QStringLiteral("inputDbfs"), inDbfs);
        map.insert(QStringLiteral("outputDbfs"), outDbfs);
        map.insert(QStringLiteral("gainReductionDb"), gr);
        map.insert(QStringLiteral("x"), inDbfs);
        map.insert(QStringLiteral("y"), outDbfs);
        list.append(map);
    }
    return list;
}

QVariantList CompressorViewModel::transfer_curve_handles() const
{
    QVariantList list;
    list.reserve(4);

    // 1. Threshold handle (#2ED3FF, horizontal drag)
    {
        const double x = draftThresholdDbfs_;
        const double gr = compute_gain_reduction_db(x, draftThresholdDbfs_, draftRatio_, draftKneeDb_);
        const double y = x - gr + draftMakeupGainDb_;

        QVariantMap map;
        map.insert(QStringLiteral("id"), QStringLiteral("threshold"));
        map.insert(QStringLiteral("color"), QStringLiteral("#2ED3FF"));
        map.insert(QStringLiteral("dragDirection"), QStringLiteral("horizontal"));
        map.insert(QStringLiteral("inputDbfs"), x);
        map.insert(QStringLiteral("outputDbfs"), y);
        map.insert(QStringLiteral("label"), QStringLiteral("Threshold"));
        list.append(map);
    }

    // 2. Ratio handle (#2FD98F, vertical drag)
    {
        double x = draftThresholdDbfs_ + 12.0;
        if (x > 6.0) x = 6.0;
        if (x < draftThresholdDbfs_) x = draftThresholdDbfs_;
        const double gr = compute_gain_reduction_db(x, draftThresholdDbfs_, draftRatio_, draftKneeDb_);
        const double y = x - gr + draftMakeupGainDb_;

        QVariantMap map;
        map.insert(QStringLiteral("id"), QStringLiteral("ratio"));
        map.insert(QStringLiteral("color"), QStringLiteral("#2FD98F"));
        map.insert(QStringLiteral("dragDirection"), QStringLiteral("vertical"));
        map.insert(QStringLiteral("inputDbfs"), x);
        map.insert(QStringLiteral("outputDbfs"), y);
        map.insert(QStringLiteral("label"), QStringLiteral("Ratio"));
        list.append(map);
    }

    // 3. Knee handle (#FFD84A, horizontal drag at lower knee boundary)
    {
        const double x = draftThresholdDbfs_ - (draftKneeDb_ * 0.5);
        const double gr = compute_gain_reduction_db(x, draftThresholdDbfs_, draftRatio_, draftKneeDb_);
        const double y = x - gr + draftMakeupGainDb_;

        QVariantMap map;
        map.insert(QStringLiteral("id"), QStringLiteral("knee"));
        map.insert(QStringLiteral("color"), QStringLiteral("#FFD84A"));
        map.insert(QStringLiteral("dragDirection"), QStringLiteral("horizontal"));
        map.insert(QStringLiteral("inputDbfs"), x);
        map.insert(QStringLiteral("outputDbfs"), y);
        map.insert(QStringLiteral("label"), QStringLiteral("Knee"));
        list.append(map);
    }

    // 4. Make-up handle (#FF6B6B, vertical drag)
    {
        double x = -48.0;
        if (x > draftThresholdDbfs_ - 6.0) x = draftThresholdDbfs_ - 6.0;
        if (x < -60.0) x = -60.0;
        const double gr = compute_gain_reduction_db(x, draftThresholdDbfs_, draftRatio_, draftKneeDb_);
        const double y = x - gr + draftMakeupGainDb_;

        QVariantMap map;
        map.insert(QStringLiteral("id"), QStringLiteral("makeup"));
        map.insert(QStringLiteral("color"), QStringLiteral("#FF6B6B"));
        map.insert(QStringLiteral("dragDirection"), QStringLiteral("vertical"));
        map.insert(QStringLiteral("inputDbfs"), x);
        map.insert(QStringLiteral("outputDbfs"), y);
        map.insert(QStringLiteral("label"), QStringLiteral("Make-up"));
        list.append(map);
    }

    return list;
}

QString CompressorViewModel::live_gr_state() const
{
    return liveGrState_;
}

QString CompressorViewModel::live_gr_db_text() const
{
    return QString::number(liveGrDb_, 'f', 1) + QStringLiteral(" dB");
}

QString CompressorViewModel::live_gr_db_text_r() const
{
    return QString::number(liveGrDbR_, 'f', 1) + QStringLiteral(" dB");
}

QVariantList CompressorViewModel::live_gr_history_l() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(historyL_.size()));
    for (const auto& b : historyL_) {
        QVariantMap map;
        map.insert(QStringLiteral("meanDb"), b.mean_reduction_db);
        map.insert(QStringLiteral("peakDb"), b.peak_reduction_db);
        map.insert(QStringLiteral("endDb"), b.end_reduction_db);
        map.insert(QStringLiteral("peakOffset"), b.peak_offset_frames);
        map.insert(QStringLiteral("attenuatedCount"), b.attenuated_frame_count);
        map.insert(QStringLiteral("beginFrame"), static_cast<qlonglong>(b.begin_frame));
        map.insert(QStringLiteral("endFrame"), static_cast<qlonglong>(b.end_frame));
        list.append(map);
    }
    return list;
}

QVariantList CompressorViewModel::live_gr_history_r() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(historyR_.size()));
    for (const auto& b : historyR_) {
        QVariantMap map;
        map.insert(QStringLiteral("meanDb"), b.mean_reduction_db);
        map.insert(QStringLiteral("peakDb"), b.peak_reduction_db);
        map.insert(QStringLiteral("endDb"), b.end_reduction_db);
        map.insert(QStringLiteral("peakOffset"), b.peak_offset_frames);
        map.insert(QStringLiteral("attenuatedCount"), b.attenuated_frame_count);
        map.insert(QStringLiteral("beginFrame"), static_cast<qlonglong>(b.begin_frame));
        map.insert(QStringLiteral("endFrame"), static_cast<qlonglong>(b.end_frame));
        list.append(map);
    }
    return list;
}

void CompressorViewModel::update_telemetry_observation(
    std::int64_t current_frame,
    bool is_playing,
    bool is_paused,
    bool is_transition,
    bool is_processed_audition,
    bool is_audible_bypassed,
    bool is_audible_dry_only,
    const render::CompressorTelemetrySidecar* sidecar)
{
    if (!is_processed_audition) {
        liveGrState_ = QStringLiteral("NOT AUDITIONED");
        emit changed();
        return;
    }

    if (is_transition) {
        liveGrState_ = QStringLiteral("TRANSITION");
        emit changed();
        return;
    }

    if (sidecar == nullptr || !sidecar->valid || sidecar->status != render::CompressorTelemetryStatus::OK) {
        liveGrState_ = QStringLiteral("UNAVAILABLE");
        emit changed();
        return;
    }

    if (is_audible_bypassed) {
        liveGrState_ = QStringLiteral("BYPASS");
        emit changed();
        return;
    }

    if (is_paused) {
        liveGrState_ = QStringLiteral("PAUSED");
    } else if (!is_playing) {
        liveGrState_ = QStringLiteral("STOPPED / END");
    } else if (is_audible_dry_only) {
        liveGrState_ = QStringLiteral("ACTIVE DRY ONLY");
    } else {
        liveGrState_ = QStringLiteral("ACTIVE WET");
    }

    if (current_frame < lastObservedFrame_ || sidecar->chain_revision != activeSidecarRevision_) {
        activeSidecarRevision_ = sidecar->chain_revision;
        consumedBucketIndexL_ = 0;
        consumedBucketIndexR_ = 0;
        historyL_.clear();
        historyR_.clear();
    }
    lastObservedFrame_ = current_frame;

    const auto& lanes = sidecar->channel_lanes;
    if (lanes.empty()) {
        emit changed();
        return;
    }

    isDualMonoTelemetry_ = (lanes.size() == 2);

    const auto& bucketsL = lanes[0].buckets;
    while (consumedBucketIndexL_ < bucketsL.size() && bucketsL[consumedBucketIndexL_].end_frame <= current_frame) {
        const auto& b = bucketsL[consumedBucketIndexL_];
        if (b.valid) {
            historyL_.push_back(b);
            if (historyL_.size() > 1000U) {
                historyL_.erase(historyL_.begin());
            }
            liveGrDb_ = b.end_reduction_db;
        }
        consumedBucketIndexL_++;
    }

    if (isDualMonoTelemetry_ && lanes.size() > 1) {
        const auto& bucketsR = lanes[1].buckets;
        while (consumedBucketIndexR_ < bucketsR.size() && bucketsR[consumedBucketIndexR_].end_frame <= current_frame) {
            const auto& b = bucketsR[consumedBucketIndexR_];
            if (b.valid) {
                historyR_.push_back(b);
                if (historyR_.size() > 1000U) {
                    historyR_.erase(historyR_.begin());
                }
                liveGrDbR_ = b.end_reduction_db;
            }
            consumedBucketIndexR_++;
        }
    }

    emit changed();
}

void CompressorViewModel::commit_candidate_or_set_validation(
    dsp::CompressorDetectorMode detectorMode,
    dsp::CompressorChannelLink channelLink,
    double thresholdDbfs,
    double ratio,
    double kneeDb,
    double attackMs,
    double releaseMs,
    double rmsTimeConstantMs,
    double lookAheadMs,
    double mixPercent,
    double makeupGainDb,
    const QString& fieldName)
{
    // Update draft fields for UI representation
    draftDetectorMode_ = detectorMode;
    draftChannelLink_ = channelLink;
    draftThresholdDbfs_ = thresholdDbfs;
    draftThresholdText_ = QString::number(thresholdDbfs, 'f', 1);
    draftRatio_ = ratio;
    draftRatioText_ = QString::number(ratio, 'f', 2);
    draftKneeDb_ = kneeDb;
    draftKneeText_ = QString::number(kneeDb, 'f', 1);
    draftAttackMs_ = attackMs;
    draftAttackText_ = QString::number(attackMs, 'f', 1);
    draftReleaseMs_ = releaseMs;
    draftReleaseText_ = QString::number(releaseMs, 'f', 1);
    draftRmsTimeConstantMs_ = rmsTimeConstantMs;
    draftRmsTimeConstantText_ = QString::number(rmsTimeConstantMs, 'f', 1);
    draftLookAheadMs_ = lookAheadMs;
    draftLookAheadText_ = QString::number(lookAheadMs, 'f', 1);
    draftMixPercent_ = mixPercent;
    draftMixPercentText_ = QString::number(mixPercent, 'f', 1);
    draftMakeupGainDb_ = makeupGainDb;
    draftMakeupGainText_ = QString::number(makeupGainDb, 'f', 1);

    auto candidate = dsp::CompressorParameters::create(
        detectorMode,
        channelLink,
        thresholdDbfs,
        ratio,
        kneeDb,
        attackMs,
        releaseMs,
        rmsTimeConstantMs,
        lookAheadMs,
        mixPercent,
        makeupGainDb);

    if (!candidate) {
        // Validation failed: record validation field and message, do NOT update committed state, do NOT trigger preview
        validationField_ = fieldName;
        validationMessage_ = user_validation_message(fieldName);
        emit changed();
        return;
    }

    // Validation succeeded: clear errors
    validationField_.clear();
    validationMessage_.clear();

    const auto& currentCommitted = active_chain_state().compressor_parameters();
    if (*candidate.value() != currentCommitted) {
        const auto preSnapshot = capture_current_snapshot();
        push_undo_snapshot(preSnapshot);
        static_cast<void>(active_chain_state().set_compressor_parameters(*candidate.value()));
    }

    emit changed();
    request_preview();
}

void CompressorViewModel::setDetectorMode(const QString& modeStr)
{
    const auto modeOpt = string_to_detector_mode(modeStr);
    if (!modeOpt) {
        validationField_ = QStringLiteral("detectorMode");
        validationMessage_ = QStringLiteral("Invalid detector mode.");
        emit changed();
        return;
    }
    commit_candidate_or_set_validation(
        *modeOpt,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("detectorMode"));
}

void CompressorViewModel::setChannelLink(const QString& linkStr)
{
    const auto linkOpt = string_to_channel_link(linkStr);
    if (!linkOpt) {
        validationField_ = QStringLiteral("channelLink");
        validationMessage_ = QStringLiteral("Invalid channel link mode.");
        emit changed();
        return;
    }
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        *linkOpt,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("channelLink"));
}

void CompressorViewModel::setThresholdDbfs(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        val,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("thresholdDbfs"));
}

void CompressorViewModel::setRatio(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        val,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("ratio"));
}

void CompressorViewModel::setKneeDb(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        val,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("kneeDb"));
}

void CompressorViewModel::setAttackMs(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        val,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("attackMs"));
}

void CompressorViewModel::setReleaseMs(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        val,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("releaseMs"));
}

void CompressorViewModel::setRmsTimeConstantMs(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        val,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("rmsTimeConstantMs"));
}

void CompressorViewModel::setLookAheadMs(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        val,
        draftMixPercent_,
        draftMakeupGainDb_,
        QStringLiteral("lookAheadMs"));
}

void CompressorViewModel::setMixPercent(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        val,
        draftMakeupGainDb_,
        QStringLiteral("mixPercent"));
}

void CompressorViewModel::setMakeupGainDb(double val)
{
    commit_candidate_or_set_validation(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        val,
        QStringLiteral("makeupGainDb"));
}

void CompressorViewModel::setDraftFieldText(const QString& fieldName, const QString& text)
{
    if (fieldName == QStringLiteral("thresholdDbfs") || fieldName == QStringLiteral("threshold")) {
        draftThresholdText_ = text;
    } else if (fieldName == QStringLiteral("ratio")) {
        draftRatioText_ = text;
    } else if (fieldName == QStringLiteral("kneeDb") || fieldName == QStringLiteral("knee")) {
        draftKneeText_ = text;
    } else if (fieldName == QStringLiteral("attackMs") || fieldName == QStringLiteral("attack")) {
        draftAttackText_ = text;
    } else if (fieldName == QStringLiteral("releaseMs") || fieldName == QStringLiteral("release")) {
        draftReleaseText_ = text;
    } else if (fieldName == QStringLiteral("rmsTimeConstantMs") || fieldName == QStringLiteral("rmsTime")) {
        draftRmsTimeConstantText_ = text;
    } else if (fieldName == QStringLiteral("lookAheadMs") || fieldName == QStringLiteral("lookAhead")) {
        draftLookAheadText_ = text;
    } else if (fieldName == QStringLiteral("mixPercent") || fieldName == QStringLiteral("mix")) {
        draftMixPercentText_ = text;
    } else if (fieldName == QStringLiteral("makeupGainDb") || fieldName == QStringLiteral("makeup")) {
        draftMakeupGainText_ = text;
    }

    bool tOk = false, rOk = false, kOk = false, aOk = false, relOk = false, rmsOk = false, laOk = false, mOk = false, mkOk = false;
    const double tVal = draftThresholdText_.toDouble(&tOk);
    const double rVal = draftRatioText_.toDouble(&rOk);
    const double kVal = draftKneeText_.toDouble(&kOk);
    const double aVal = draftAttackText_.toDouble(&aOk);
    const double relVal = draftReleaseText_.toDouble(&relOk);
    const double rmsVal = draftRmsTimeConstantText_.toDouble(&rmsOk);
    const double laVal = draftLookAheadText_.toDouble(&laOk);
    const double mVal = draftMixPercentText_.toDouble(&mOk);
    const double mkVal = draftMakeupGainText_.toDouble(&mkOk);

    if (tOk) draftThresholdDbfs_ = tVal;
    if (rOk) draftRatio_ = rVal;
    if (kOk) draftKneeDb_ = kVal;
    if (aOk) draftAttackMs_ = aVal;
    if (relOk) draftReleaseMs_ = relVal;
    if (rmsOk) draftRmsTimeConstantMs_ = rmsVal;
    if (laOk) draftLookAheadMs_ = laVal;
    if (mOk) draftMixPercent_ = mVal;
    if (mkOk) draftMakeupGainDb_ = mkVal;

    auto candidate = dsp::CompressorParameters::create(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_);

    const bool allParsed = tOk && rOk && kOk && aOk && relOk && rmsOk && laOk && mOk && mkOk;

    if (!candidate) {
        validationField_ = fieldName;
        validationMessage_ = user_validation_message(fieldName);
    } else if (!allParsed) {
        validationField_ = fieldName;
        validationMessage_ = user_validation_message(fieldName, true);
    } else {
        validationField_.clear();
        validationMessage_.clear();
    }

    emit changed();
}

void CompressorViewModel::setDraftFieldValue(const QString& fieldName, double value)
{
    if (fieldName == QStringLiteral("thresholdDbfs") || fieldName == QStringLiteral("threshold")) {
        draftThresholdDbfs_ = value;
        draftThresholdText_ = QString::number(value, 'f', 1);
    } else if (fieldName == QStringLiteral("ratio")) {
        draftRatio_ = value;
        draftRatioText_ = QString::number(value, 'f', 2);
    } else if (fieldName == QStringLiteral("kneeDb") || fieldName == QStringLiteral("knee")) {
        draftKneeDb_ = value;
        draftKneeText_ = QString::number(value, 'f', 1);
    } else if (fieldName == QStringLiteral("attackMs") || fieldName == QStringLiteral("attack")) {
        draftAttackMs_ = value;
        draftAttackText_ = QString::number(value, 'f', 1);
    } else if (fieldName == QStringLiteral("releaseMs") || fieldName == QStringLiteral("release")) {
        draftReleaseMs_ = value;
        draftReleaseText_ = QString::number(value, 'f', 1);
    } else if (fieldName == QStringLiteral("rmsTimeConstantMs") || fieldName == QStringLiteral("rmsTime")) {
        draftRmsTimeConstantMs_ = value;
        draftRmsTimeConstantText_ = QString::number(value, 'f', 1);
    } else if (fieldName == QStringLiteral("lookAheadMs") || fieldName == QStringLiteral("lookAhead")) {
        draftLookAheadMs_ = value;
        draftLookAheadText_ = QString::number(value, 'f', 1);
    } else if (fieldName == QStringLiteral("mixPercent") || fieldName == QStringLiteral("mix")) {
        draftMixPercent_ = value;
        draftMixPercentText_ = QString::number(value, 'f', 1);
    } else if (fieldName == QStringLiteral("makeupGainDb") || fieldName == QStringLiteral("makeup")) {
        draftMakeupGainDb_ = value;
        draftMakeupGainText_ = QString::number(value, 'f', 1);
    }

    auto candidate = dsp::CompressorParameters::create(
        draftDetectorMode_,
        draftChannelLink_,
        draftThresholdDbfs_,
        draftRatio_,
        draftKneeDb_,
        draftAttackMs_,
        draftReleaseMs_,
        draftRmsTimeConstantMs_,
        draftLookAheadMs_,
        draftMixPercent_,
        draftMakeupGainDb_);

    if (!candidate) {
        validationField_ = fieldName;
        validationMessage_ = user_validation_message(fieldName);
    } else {
        validationField_.clear();
        validationMessage_.clear();
    }

    emit changed();
}

void CompressorViewModel::setCurveHandleDraft(const QString& handleId, double inputDbfs, double outputDbfs)
{
    if (handleId == QStringLiteral("threshold")) {
        setDraftFieldValue(
            QStringLiteral("thresholdDbfs"),
            std::clamp(inputDbfs, -120.0, 0.0));
    } else if (handleId == QStringLiteral("ratio")) {
        double evalX = draftThresholdDbfs_ + 12.0;
        if (evalX > 6.0) evalX = 6.0;
        if (evalX < draftThresholdDbfs_) evalX = draftThresholdDbfs_;

        const double targetGr = evalX + draftMakeupGainDb_ - outputDbfs;
        const double maxGr = compute_gain_reduction_db(evalX, draftThresholdDbfs_, 20.0, draftKneeDb_);

        double newRatio = 1.0;
        if (targetGr <= 0.0) {
            newRatio = 1.0;
        } else if (targetGr >= maxGr) {
            newRatio = 20.0;
        } else {
            // Deterministic bisection solver over ratio in [1.0, 20.0]
            double lowR = 1.0;
            double highR = 20.0;
            for (int iter = 0; iter < 30; ++iter) {
                const double midR = 0.5 * (lowR + highR);
                const double grMid = compute_gain_reduction_db(evalX, draftThresholdDbfs_, midR, draftKneeDb_);
                if (grMid < targetGr) {
                    lowR = midR;
                } else {
                    highR = midR;
                }
            }
            newRatio = 0.5 * (lowR + highR);
        }
        setDraftFieldValue(QStringLiteral("ratio"), newRatio);
    } else if (handleId == QStringLiteral("knee")) {
        const double newKnee = std::clamp(
            2.0 * (draftThresholdDbfs_ - inputDbfs),
            0.0,
            24.0);
        setDraftFieldValue(QStringLiteral("kneeDb"), newKnee);
    } else if (handleId == QStringLiteral("makeup")) {
        double evalX = -48.0;
        if (evalX > draftThresholdDbfs_ - 6.0) evalX = draftThresholdDbfs_ - 6.0;
        if (evalX < -60.0) evalX = -60.0;
        const double gr = compute_gain_reduction_db(evalX, draftThresholdDbfs_, draftRatio_, draftKneeDb_);
        const double newMakeup = std::clamp(
            outputDbfs - evalX + gr,
            -24.0,
            24.0);
        setDraftFieldValue(QStringLiteral("makeupGainDb"), newMakeup);
    }
}

bool CompressorViewModel::commitDraft()
{
    bool tOk = false, rOk = false, kOk = false, aOk = false, relOk = false, rmsOk = false, laOk = false, mOk = false, mkOk = false;
    const double tVal = draftThresholdText_.toDouble(&tOk);
    const double rVal = draftRatioText_.toDouble(&rOk);
    const double kVal = draftKneeText_.toDouble(&kOk);
    const double aVal = draftAttackText_.toDouble(&aOk);
    const double relVal = draftReleaseText_.toDouble(&relOk);
    const double rmsVal = draftRmsTimeConstantText_.toDouble(&rmsOk);
    const double laVal = draftLookAheadText_.toDouble(&laOk);
    const double mVal = draftMixPercentText_.toDouble(&mOk);
    const double mkVal = draftMakeupGainText_.toDouble(&mkOk);

    if (!tOk || !rOk || !kOk || !aOk || !relOk || !rmsOk || !laOk || !mOk || !mkOk) {
        if (!tOk) validationField_ = QStringLiteral("thresholdDbfs");
        else if (!rOk) validationField_ = QStringLiteral("ratio");
        else if (!kOk) validationField_ = QStringLiteral("kneeDb");
        else if (!aOk) validationField_ = QStringLiteral("attackMs");
        else if (!relOk) validationField_ = QStringLiteral("releaseMs");
        else if (!rmsOk) validationField_ = QStringLiteral("rmsTimeConstantMs");
        else if (!laOk) validationField_ = QStringLiteral("lookAheadMs");
        else if (!mOk) validationField_ = QStringLiteral("mixPercent");
        else if (!mkOk) validationField_ = QStringLiteral("makeupGainDb");
        validationMessage_ = user_validation_message(validationField_, true);
        emit changed();
        return false;
    }

    auto candidate = dsp::CompressorParameters::create(
        draftDetectorMode_,
        draftChannelLink_,
        tVal,
        rVal,
        kVal,
        aVal,
        relVal,
        rmsVal,
        laVal,
        mVal,
        mkVal);

    if (!candidate) {
        if (validationField_.isEmpty()) {
            validationField_ = QStringLiteral("thresholdDbfs");
        }
        validationMessage_ = user_validation_message(validationField_);
        emit changed();
        return false;
    }

    validationField_.clear();
    validationMessage_.clear();

    const auto& currentCommitted = active_chain_state().compressor_parameters();
    if (*candidate.value() != currentCommitted) {
        const auto preSnapshot = capture_current_snapshot();
        push_undo_snapshot(preSnapshot);
        static_cast<void>(active_chain_state().set_compressor_parameters(*candidate.value()));
        refreshFromAuthority();
        request_preview();
    } else {
        refreshFromAuthority();
    }

    return true;
}

void CompressorViewModel::cancelDraft()
{
    refreshFromAuthority();
}

void CompressorViewModel::setBypass(bool bypassed)
{
    if (bypass() == bypassed) {
        return;
    }
    const auto preSnapshot = capture_current_snapshot();
    push_undo_snapshot(preSnapshot);

    auto& state = active_chain_state();
    static_cast<void>(state.set_user_bypass(state.compressor_instance_id(), bypassed));

    emit changed();
    request_preview();
}

void CompressorViewModel::undo()
{
    if (undoStack_.empty()) {
        return;
    }
    const auto preSnapshot = capture_current_snapshot();
    const auto prevSnapshot = undoStack_.back();
    undoStack_.pop_back();
    redoStack_.push_back(preSnapshot);

    restore_snapshot(prevSnapshot);
    emit changed();
    request_preview();
}

void CompressorViewModel::redo()
{
    if (redoStack_.empty()) {
        return;
    }
    const auto preSnapshot = capture_current_snapshot();
    const auto nextSnapshot = redoStack_.back();
    redoStack_.pop_back();
    undoStack_.push_back(preSnapshot);

    restore_snapshot(nextSnapshot);
    emit changed();
    request_preview();
}

void CompressorViewModel::resetToDefault()
{
    const auto defaultParams = *dsp::CompressorParameters::create_default().value();
    if (active_chain_state().compressor_parameters() == defaultParams && bypass()) {
        return; // Already default and bypassed
    }

    const auto preSnapshot = capture_current_snapshot();
    push_undo_snapshot(preSnapshot);

    auto& state = active_chain_state();
    static_cast<void>(state.set_compressor_parameters(defaultParams));
    static_cast<void>(state.set_user_bypass(state.compressor_instance_id(), true));

    refreshFromAuthority();
    request_preview();
}

void CompressorViewModel::resetForNewSource()
{
    const auto defaultParams = *dsp::CompressorParameters::create_default().value();
    auto& state = active_chain_state();
    static_cast<void>(state.set_compressor_parameters(defaultParams));
    static_cast<void>(state.set_user_bypass(state.compressor_instance_id(), true));

    undoStack_.clear();
    redoStack_.clear();

    refreshFromAuthority();
    request_preview();
}

void CompressorViewModel::refreshFromAuthority()
{
    const auto& params = active_chain_state().compressor_parameters();
    draftDetectorMode_ = params.detector_mode();
    draftChannelLink_ = params.channel_link();
    draftThresholdDbfs_ = params.threshold_dbfs();
    draftRatio_ = params.ratio();
    draftKneeDb_ = params.knee_db();
    draftAttackMs_ = params.attack_ms();
    draftReleaseMs_ = params.release_ms();
    draftRmsTimeConstantMs_ = params.rms_time_constant_ms();
    draftLookAheadMs_ = params.look_ahead_ms();
    draftMixPercent_ = params.mix_percent();
    draftMakeupGainDb_ = params.makeup_gain_db();

    draftThresholdText_ = QString::number(params.threshold_dbfs(), 'f', 1);
    draftRatioText_ = QString::number(params.ratio(), 'f', 2);
    draftKneeText_ = QString::number(params.knee_db(), 'f', 1);
    draftAttackText_ = QString::number(params.attack_ms(), 'f', 1);
    draftReleaseText_ = QString::number(params.release_ms(), 'f', 1);
    draftRmsTimeConstantText_ = QString::number(params.rms_time_constant_ms(), 'f', 1);
    draftLookAheadText_ = QString::number(params.look_ahead_ms(), 'f', 1);
    draftMixPercentText_ = QString::number(params.mix_percent(), 'f', 1);
    draftMakeupGainText_ = QString::number(params.makeup_gain_db(), 'f', 1);

    validationField_.clear();
    validationMessage_.clear();

    emit changed();
}

void CompressorViewModel::push_undo_snapshot(CompressorStateSnapshot previousSnapshot)
{
    undoStack_.push_back(std::move(previousSnapshot));
    if (undoStack_.size() > 50U) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

CompressorViewModel::CompressorStateSnapshot CompressorViewModel::capture_current_snapshot() const
{
    return CompressorStateSnapshot{
        .parameters = active_chain_state().compressor_parameters(),
        .bypass = bypass(),
    };
}

void CompressorViewModel::restore_snapshot(const CompressorStateSnapshot& snapshot)
{
    auto& state = active_chain_state();
    static_cast<void>(state.set_compressor_parameters(snapshot.parameters));
    static_cast<void>(state.set_user_bypass(state.compressor_instance_id(), snapshot.bypass));

    refreshFromAuthority();
}

}  // namespace rgsml::app
