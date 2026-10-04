#include "compressor_view_model.hpp"

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
    return draftThresholdDbfs_;
}

double CompressorViewModel::ratio() const noexcept
{
    return draftRatio_;
}

double CompressorViewModel::knee_db() const noexcept
{
    return draftKneeDb_;
}

double CompressorViewModel::attack_ms() const noexcept
{
    return draftAttackMs_;
}

double CompressorViewModel::release_ms() const noexcept
{
    return draftReleaseMs_;
}

double CompressorViewModel::rms_time_constant_ms() const noexcept
{
    return draftRmsTimeConstantMs_;
}

double CompressorViewModel::look_ahead_ms() const noexcept
{
    return draftLookAheadMs_;
}

double CompressorViewModel::mix_percent() const noexcept
{
    return draftMixPercent_;
}

double CompressorViewModel::makeup_gain_db() const noexcept
{
    return draftMakeupGainDb_;
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
    constexpr int kPoints = 101;
    constexpr double kMinDbfs = -60.0;
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
    draftRatio_ = ratio;
    draftKneeDb_ = kneeDb;
    draftAttackMs_ = attackMs;
    draftReleaseMs_ = releaseMs;
    draftRmsTimeConstantMs_ = rmsTimeConstantMs;
    draftLookAheadMs_ = lookAheadMs;
    draftMixPercent_ = mixPercent;
    draftMakeupGainDb_ = makeupGainDb;

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
        validationMessage_ = QString::fromStdString(candidate.error()->message());
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
