#include "eq_view_model.hpp"

#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_response.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QMetaObject>
#include <QUuid>
#include <QVariant>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace rgsml::app {
namespace {

constexpr double kMinFreq = 20.0;
constexpr double kMaxFreqCap = 20000.0;
constexpr double kMinGain = -18.0;
constexpr double kMaxGain = 18.0;
constexpr double kMinQ = 0.10;
constexpr double kMaxQ = 12.0;
constexpr double kMinSlope = 0.10;
constexpr double kMaxSlope = 1.0;
constexpr std::size_t kResponseGridPoints = 512;

[[nodiscard]] QString filter_to_string(dsp::EqFilterType type)
{
    switch (type) {
    case dsp::EqFilterType::BELL: return QStringLiteral("BELL");
    case dsp::EqFilterType::NOTCH: return QStringLiteral("NOTCH");
    case dsp::EqFilterType::LOW_SHELF: return QStringLiteral("LOW_SHELF");
    case dsp::EqFilterType::HIGH_SHELF: return QStringLiteral("HIGH_SHELF");
    case dsp::EqFilterType::HIGH_PASS: return QStringLiteral("HIGH_PASS");
    case dsp::EqFilterType::LOW_PASS: return QStringLiteral("LOW_PASS");
    }
    return QStringLiteral("BELL");
}

[[nodiscard]] std::optional<dsp::EqFilterType> string_to_filter(const QString& str)
{
    if (str == QStringLiteral("BELL")) return dsp::EqFilterType::BELL;
    if (str == QStringLiteral("NOTCH")) return dsp::EqFilterType::NOTCH;
    if (str == QStringLiteral("LOW_SHELF")) return dsp::EqFilterType::LOW_SHELF;
    if (str == QStringLiteral("HIGH_SHELF")) return dsp::EqFilterType::HIGH_SHELF;
    if (str == QStringLiteral("HIGH_PASS")) return dsp::EqFilterType::HIGH_PASS;
    if (str == QStringLiteral("LOW_PASS")) return dsp::EqFilterType::LOW_PASS;
    return std::nullopt;
}

[[nodiscard]] QString routing_to_string(dsp::EqRouting routing)
{
    switch (routing) {
    case dsp::EqRouting::STEREO: return QStringLiteral("STEREO");
    case dsp::EqRouting::MID: return QStringLiteral("MID");
    case dsp::EqRouting::SIDE: return QStringLiteral("SIDE");
    case dsp::EqRouting::LEFT: return QStringLiteral("LEFT");
    case dsp::EqRouting::RIGHT: return QStringLiteral("RIGHT");
    }
    return QStringLiteral("STEREO");
}

[[nodiscard]] std::optional<dsp::EqRouting> string_to_routing(const QString& str)
{
    if (str == QStringLiteral("STEREO")) return dsp::EqRouting::STEREO;
    if (str == QStringLiteral("MID")) return dsp::EqRouting::MID;
    if (str == QStringLiteral("SIDE")) return dsp::EqRouting::SIDE;
    if (str == QStringLiteral("LEFT")) return dsp::EqRouting::LEFT;
    if (str == QStringLiteral("RIGHT")) return dsp::EqRouting::RIGHT;
    return std::nullopt;
}

[[nodiscard]] EqViewModel::DraftBand band_to_draft(const dsp::EqBandParameters& band)
{
    EqViewModel::DraftBand draft;
    draft.band_id = band.band_id();
    draft.enabled = band.enabled();
    draft.filter_type = band.filter_type();
    draft.routing = band.routing();

    std::visit(
        [&draft](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, dsp::BellPayload>) {
                draft.frequency_hz = payload.frequency_hz;
                draft.gain_db = payload.gain_db;
                draft.q = payload.q;
            } else if constexpr (std::is_same_v<T, dsp::NotchPayload>) {
                draft.frequency_hz = payload.frequency_hz;
                draft.q = payload.q;
            } else if constexpr (std::is_same_v<T, dsp::ShelfPayload>) {
                draft.frequency_hz = payload.frequency_hz;
                draft.gain_db = payload.gain_db;
                draft.shelf_slope = payload.shelf_slope;
            } else if constexpr (std::is_same_v<T, dsp::PassPayload>) {
                draft.frequency_hz = payload.frequency_hz;
                draft.slope_db_per_octave = payload.slope_db_per_octave;
            }
        },
        band.payload());

    draft.frequency_text = QString::number(draft.frequency_hz);
    draft.gain_text = QString::number(draft.gain_db);
    draft.q_text = QString::number(draft.q);
    draft.shelf_slope_text = QString::number(draft.shelf_slope);

    return draft;
}

}  // namespace

EqViewModel::EqViewModel(
    PreparedSnapshotProvider snapshotProvider,
    ProcessedRealizationPublisher publisher,
    IdGenerator idGenerator,
    QObject* parent)
    : QObject(parent)
    , snapshotProvider_(std::move(snapshotProvider))
    , publisher_(std::move(publisher))
    , idGenerator_(std::move(idGenerator))
    , instanceId_(*dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("00000000-0000-0000-0000-000000000001").value()).value())
    , workerThread_([this] { worker_loop(); })
{
    if (!idGenerator_) {
        idGenerator_ = [] {
            const auto str = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            return *core::Uuid::parse(str).value();
        };
    }
    instanceId_ = *dsp::ModuleInstanceId::from_uuid(idGenerator_()).value();

    const auto firstBandId = idGenerator_();
    draftBand_ = make_default_band(firstBandId);
    auto firstBandParam = make_band_parameters(draftBand_, kMaxFreqCap);
    Q_ASSERT(firstBandParam.has_value());
    committedBands_.push_back(*firstBandParam);

    auto paramsRes = dsp::ParametricEqParameters::create(committedBands_);
    Q_ASSERT(paramsRes);
    committedParams_ = *paramsRes.value();

    update_response_grid();
    update_validation_state();
}

EqViewModel::~EqViewModel()
{
    {
        const std::scoped_lock lock{workerMutex_};
        workerStopping_ = true;
        pendingJob_.reset();
    }
    workerCond_.notify_one();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void EqViewModel::set_preview_executor(PreviewExecutor executor)
{
    const std::scoped_lock lock{workerMutex_};
    previewExecutor_ = std::move(executor);
}

int EqViewModel::band_count() const noexcept
{
    return static_cast<int>(committedBands_.size());
}

int EqViewModel::selected_index() const noexcept
{
    return static_cast<int>(selectedIndex_);
}

QString EqViewModel::selected_band_id() const
{
    return QString::fromStdString(draftBand_.band_id.to_string());
}

bool EqViewModel::enabled() const noexcept
{
    return draftBand_.enabled;
}

QString EqViewModel::filter_label() const
{
    return filter_to_string(draftBand_.filter_type);
}

QString EqViewModel::routing_label() const
{
    return routing_to_string(draftBand_.routing);
}

double EqViewModel::frequency() const noexcept
{
    return draftBand_.frequency_hz;
}

double EqViewModel::gain() const noexcept
{
    return draftBand_.gain_db;
}

double EqViewModel::q() const noexcept
{
    return draftBand_.q;
}

double EqViewModel::shelf_slope() const noexcept
{
    return draftBand_.shelf_slope;
}

QString EqViewModel::frequency_text() const
{
    return draftBand_.frequency_text;
}

QString EqViewModel::gain_text() const
{
    return draftBand_.gain_text;
}

QString EqViewModel::q_text() const
{
    return draftBand_.q_text;
}

QString EqViewModel::shelf_slope_text() const
{
    return draftBand_.shelf_slope_text;
}

int EqViewModel::slope_db_per_oct() const noexcept
{
    return static_cast<int>(draftBand_.slope_db_per_octave);
}

bool EqViewModel::gain_applicable() const noexcept
{
    return draftBand_.filter_type == dsp::EqFilterType::BELL
        || draftBand_.filter_type == dsp::EqFilterType::LOW_SHELF
        || draftBand_.filter_type == dsp::EqFilterType::HIGH_SHELF;
}

bool EqViewModel::q_applicable() const noexcept
{
    return draftBand_.filter_type == dsp::EqFilterType::BELL
        || draftBand_.filter_type == dsp::EqFilterType::NOTCH;
}

bool EqViewModel::shelf_slope_applicable() const noexcept
{
    return draftBand_.filter_type == dsp::EqFilterType::LOW_SHELF
        || draftBand_.filter_type == dsp::EqFilterType::HIGH_SHELF;
}

bool EqViewModel::slope_applicable() const noexcept
{
    return draftBand_.filter_type == dsp::EqFilterType::HIGH_PASS
        || draftBand_.filter_type == dsp::EqFilterType::LOW_PASS;
}

bool EqViewModel::add_available() const noexcept
{
    return committedBands_.size() < 6U;
}

bool EqViewModel::remove_available() const noexcept
{
    return committedBands_.size() > 1U;
}

bool EqViewModel::route_available() const noexcept
{
    return !is_mono_prepared();
}

bool EqViewModel::mixed_routing() const noexcept
{
    std::set<dsp::EqRouting> activeRoutings;
    for (const auto& band : committedBands_) {
        if (band.enabled()) {
            activeRoutings.insert(band.routing());
        }
    }
    return activeRoutings.size() > 1U;
}

bool EqViewModel::bypass() const noexcept
{
    return bypass_;
}

bool EqViewModel::can_undo() const noexcept
{
    return !undoStack_.empty();
}

bool EqViewModel::can_redo() const noexcept
{
    return !redoStack_.empty();
}

QVariantList EqViewModel::band_summaries() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(committedBands_.size()));
    for (std::size_t i = 0; i < committedBands_.size(); ++i) {
        const auto& band = committedBands_[i];
        QVariantMap map;
        map.insert(QStringLiteral("index"), static_cast<int>(i));
        map.insert(QStringLiteral("bandId"), QString::fromStdString(band.band_id().to_string()));
        map.insert(QStringLiteral("enabled"), band.enabled());
        map.insert(QStringLiteral("filter"), filter_to_string(band.filter_type()));
        map.insert(QStringLiteral("routing"), routing_to_string(band.routing()));

        double freq = 1000.0;
        double gain = 0.0;
        bool gainApp = false;

        std::visit(
            [&freq, &gain, &gainApp](const auto& payload) {
                using T = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<T, dsp::BellPayload>) {
                    freq = payload.frequency_hz;
                    gain = payload.gain_db;
                    gainApp = true;
                } else if constexpr (std::is_same_v<T, dsp::NotchPayload>) {
                    freq = payload.frequency_hz;
                    gainApp = false;
                } else if constexpr (std::is_same_v<T, dsp::ShelfPayload>) {
                    freq = payload.frequency_hz;
                    gain = payload.gain_db;
                    gainApp = true;
                } else if constexpr (std::is_same_v<T, dsp::PassPayload>) {
                    freq = payload.frequency_hz;
                    gainApp = false;
                }
            },
            band.payload());

        if (i == selectedIndex_) {
            freq = draftBand_.frequency_hz;
            gain = draftBand_.gain_db;
            gainApp = gain_applicable();
        }

        map.insert(QStringLiteral("frequency"), freq);
        map.insert(QStringLiteral("gain"), gain);
        map.insert(QStringLiteral("gainApplicable"), gainApp);

        list.append(map);
    }
    return list;
}

QString EqViewModel::validation_field() const
{
    return validationField_;
}

QString EqViewModel::validation_message() const
{
    return validationMessage_;
}

quint64 EqViewModel::preview_generation() const noexcept
{
    return previewGeneration_;
}

QString EqViewModel::preview_status() const
{
    return previewStatus_;
}

QString EqViewModel::preview_error() const
{
    return previewError_;
}

QVariantList EqViewModel::selected_band_response_points() const
{
    return responseGrid_;
}

dsp::ModuleInstanceId EqViewModel::instance_id() const noexcept
{
    return instanceId_;
}

const dsp::ParametricEqParameters& EqViewModel::committed_parameters() const noexcept
{
    return committedParams_;
}

std::uint64_t EqViewModel::stale_results_discarded() const noexcept
{
    return staleResultsDiscarded_;
}

void EqViewModel::selectBand(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= committedBands_.size()) {
        return;
    }
    if (static_cast<std::size_t>(index) == selectedIndex_) {
        return;
    }
    selectedIndex_ = static_cast<std::size_t>(index);
    draftBand_ = band_to_draft(committedBands_[selectedIndex_]);
    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::addBand()
{
    if (committedBands_.size() >= 6U) {
        return;
    }
    const auto preState = capture_current_snapshot();
    const auto newId = idGenerator_();
    auto defaultDraft = make_default_band(newId);
    auto newParam = make_band_parameters(defaultDraft, max_frequency_hz());
    if (!newParam) {
        return;
    }
    committedBands_.push_back(*newParam);
    auto paramsRes = dsp::ParametricEqParameters::create(committedBands_);
    if (!paramsRes) {
        committedBands_.pop_back();
        return;
    }
    push_undo_snapshot(preState);
    committedParams_ = *paramsRes.value();
    selectedIndex_ = committedBands_.size() - 1U;
    draftBand_ = defaultDraft;

    ++previewGeneration_;
    update_response_grid();
    update_validation_state();
    emit changed();
    request_preview();
}

void EqViewModel::removeSelectedBand()
{
    if (committedBands_.size() <= 1U) {
        return;
    }
    const auto preState = capture_current_snapshot();
    auto candidateBands = committedBands_;
    candidateBands.erase(candidateBands.begin() + selectedIndex_);
    auto paramsRes = dsp::ParametricEqParameters::create(candidateBands);
    if (!paramsRes) {
        return;
    }
    push_undo_snapshot(preState);
    committedBands_ = std::move(candidateBands);
    committedParams_ = *paramsRes.value();
    if (selectedIndex_ >= committedBands_.size()) {
        selectedIndex_ = committedBands_.size() - 1U;
    }
    draftBand_ = band_to_draft(committedBands_[selectedIndex_]);

    ++previewGeneration_;
    update_response_grid();
    emit changed();
    request_preview();
}

void EqViewModel::setEnabled(bool enabled)
{
    if (draftBand_.enabled == enabled) {
        return;
    }
    draftBand_.enabled = enabled;
    commitDraft();
}

void EqViewModel::setFilter(const QString& filterStr)
{
    const auto filterOpt = string_to_filter(filterStr);
    if (!filterOpt || *filterOpt == draftBand_.filter_type) {
        return;
    }

    draftBand_.filter_type = *filterOpt;
    switch (draftBand_.filter_type) {
    case dsp::EqFilterType::BELL:
        draftBand_.gain_db = 0.0;
        draftBand_.gain_text = QStringLiteral("0");
        draftBand_.q = 0.707;
        draftBand_.q_text = QStringLiteral("0.707");
        break;
    case dsp::EqFilterType::NOTCH:
        draftBand_.q = 0.707;
        draftBand_.q_text = QStringLiteral("0.707");
        break;
    case dsp::EqFilterType::LOW_SHELF:
    case dsp::EqFilterType::HIGH_SHELF:
        draftBand_.gain_db = 0.0;
        draftBand_.gain_text = QStringLiteral("0");
        draftBand_.shelf_slope = 1.0;
        draftBand_.shelf_slope_text = QStringLiteral("1");
        break;
    case dsp::EqFilterType::HIGH_PASS:
    case dsp::EqFilterType::LOW_PASS:
        draftBand_.slope_db_per_octave = dsp::SlopeDbPerOctave::DB_12;
        break;
    }

    commitDraft();
}

void EqViewModel::setRouting(const QString& routingStr)
{
    const auto routeOpt = string_to_routing(routingStr);
    if (!routeOpt || *routeOpt == draftBand_.routing) {
        return;
    }
    if (is_mono_prepared() && *routeOpt != dsp::EqRouting::STEREO) {
        return;
    }
    draftBand_.routing = *routeOpt;
    commitDraft();
}

void EqViewModel::setBypass(bool bypass)
{
    if (bypass_ == bypass) {
        return;
    }
    const auto preState = capture_current_snapshot();
    push_undo_snapshot(preState);
    bypass_ = bypass;
    ++previewGeneration_;
    emit changed();
    request_preview();
}

void EqViewModel::undo()
{
    if (undoStack_.empty()) {
        return;
    }
    const auto preState = capture_current_snapshot();
    const auto prevSnapshot = undoStack_.back();
    undoStack_.pop_back();
    redoStack_.push_back(preState);

    restore_snapshot(prevSnapshot);
    ++previewGeneration_;
    emit changed();
    request_preview();
}

void EqViewModel::redo()
{
    if (redoStack_.empty()) {
        return;
    }
    const auto preState = capture_current_snapshot();
    const auto nextSnapshot = redoStack_.back();
    redoStack_.pop_back();
    undoStack_.push_back(preState);

    restore_snapshot(nextSnapshot);
    ++previewGeneration_;
    emit changed();
    request_preview();
}

void EqViewModel::resetToFlat()
{
    const auto preState = capture_current_snapshot();

    // Check if already canonical Flat
    const auto defaultId = idGenerator_();
    auto flatDraft = make_default_band(defaultId);
    auto flatParam = make_band_parameters(flatDraft, max_frequency_hz());

    if (committedBands_.size() == 1U
        && !bypass_
        && flatParam.has_value()
        && committedBands_[0] == *flatParam) {
        return; // Already flat: no-op
    }

    if (!flatParam) {
        return;
    }

    push_undo_snapshot(preState);

    committedBands_ = { *flatParam };
    auto paramsRes = dsp::ParametricEqParameters::create(committedBands_);
    Q_ASSERT(paramsRes);
    committedParams_ = *paramsRes.value();
    selectedIndex_ = 0;
    draftBand_ = flatDraft;
    bypass_ = false;

    ++previewGeneration_;
    update_response_grid();
    update_validation_state();
    emit changed();
    request_preview();
}

void EqViewModel::resetForNewSource()
{
    const auto defaultId = idGenerator_();
    draftBand_ = make_default_band(defaultId);
    auto firstBandParam = make_band_parameters(draftBand_, max_frequency_hz());
    Q_ASSERT(firstBandParam.has_value());

    committedBands_ = { *firstBandParam };
    auto paramsRes = dsp::ParametricEqParameters::create(committedBands_);
    Q_ASSERT(paramsRes);
    committedParams_ = *paramsRes.value();

    selectedIndex_ = 0;
    bypass_ = false;

    undoStack_.clear();
    redoStack_.clear();

    ++previewGeneration_;
    update_response_grid();
    update_validation_state();
    emit changed();
    request_preview();
}

void EqViewModel::setDraftFrequency(double frequency)
{
    setDraftFrequencyText(QString::number(frequency));
}

void EqViewModel::setDraftGain(double gain)
{
    setDraftGainText(QString::number(gain));
}

void EqViewModel::setDraftQ(double q)
{
    setDraftQText(QString::number(q));
}

void EqViewModel::setDraftShelfSlope(double shelfSlope)
{
    setDraftShelfSlopeText(QString::number(shelfSlope));
}

void EqViewModel::setDraftFrequencyText(const QString& text)
{
    draftBand_.frequency_text = text;
    bool ok = false;
    const double val = text.toDouble(&ok);
    if (ok && std::isfinite(val)) {
        draftBand_.frequency_hz = val;
    }
    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::setDraftGainText(const QString& text)
{
    draftBand_.gain_text = text;
    bool ok = false;
    const double val = text.toDouble(&ok);
    if (ok && std::isfinite(val)) {
        draftBand_.gain_db = val;
    }
    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::setDraftQText(const QString& text)
{
    draftBand_.q_text = text;
    bool ok = false;
    const double val = text.toDouble(&ok);
    if (ok && std::isfinite(val)) {
        draftBand_.q = val;
    }
    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::setDraftShelfSlopeText(const QString& text)
{
    draftBand_.shelf_slope_text = text;
    bool ok = false;
    const double val = text.toDouble(&ok);
    if (ok && std::isfinite(val)) {
        draftBand_.shelf_slope = val;
    }
    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::setDraftSlopeDbPerOct(int slope)
{
    switch (slope) {
    case 6: draftBand_.slope_db_per_octave = dsp::SlopeDbPerOctave::DB_6; break;
    case 12: draftBand_.slope_db_per_octave = dsp::SlopeDbPerOctave::DB_12; break;
    case 18: draftBand_.slope_db_per_octave = dsp::SlopeDbPerOctave::DB_18; break;
    case 24: draftBand_.slope_db_per_octave = dsp::SlopeDbPerOctave::DB_24; break;
    case 36: draftBand_.slope_db_per_octave = dsp::SlopeDbPerOctave::DB_36; break;
    case 48: draftBand_.slope_db_per_octave = dsp::SlopeDbPerOctave::DB_48; break;
    default: return;
    }
    update_response_grid();
    update_validation_state();
    emit changed();
}

bool EqViewModel::commitDraft()
{
    update_validation_state();
    if (!validationField_.isEmpty()) {
        emit changed();
        return false;
    }

    bool ok = false;

    const double freq = draftBand_.frequency_text.toDouble(&ok);
    if (!ok || !std::isfinite(freq)) {
        return false;
    }
    draftBand_.frequency_hz = freq;

    if (gain_applicable()) {
        const double g = draftBand_.gain_text.toDouble(&ok);
        if (!ok || !std::isfinite(g)) {
            return false;
        }
        draftBand_.gain_db = g;
    }

    if (q_applicable()) {
        const double qVal = draftBand_.q_text.toDouble(&ok);
        if (!ok || !std::isfinite(qVal)) {
            return false;
        }
        draftBand_.q = qVal;
    }

    if (shelf_slope_applicable()) {
        const double sVal = draftBand_.shelf_slope_text.toDouble(&ok);
        if (!ok || !std::isfinite(sVal)) {
            return false;
        }
        draftBand_.shelf_slope = sVal;
    }

    auto bandParamOpt = make_band_parameters(draftBand_, max_frequency_hz());
    if (!bandParamOpt) {
        return false;
    }

    auto candidateBands = committedBands_;
    candidateBands[selectedIndex_] = *bandParamOpt;

    auto candidateParamsRes = dsp::ParametricEqParameters::create(candidateBands);
    if (!candidateParamsRes) {
        return false;
    }

    const auto preState = capture_current_snapshot();
    if (candidateBands != committedBands_) {
        push_undo_snapshot(preState);
    }

    committedBands_ = std::move(candidateBands);
    committedParams_ = std::move(*candidateParamsRes.value());

    // Format raw text fields canonically after valid commit
    draftBand_.frequency_text = QString::number(draftBand_.frequency_hz);
    draftBand_.gain_text = QString::number(draftBand_.gain_db);
    draftBand_.q_text = QString::number(draftBand_.q);
    draftBand_.shelf_slope_text = QString::number(draftBand_.shelf_slope);

    ++previewGeneration_;
    update_response_grid();
    update_validation_state();
    emit changed();
    request_preview();
    return true;
}

void EqViewModel::cancelDraft()
{
    draftBand_ = band_to_draft(committedBands_[selectedIndex_]);
    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::graphDrag(double frequency, double gain)
{
    draftBand_.frequency_hz = frequency;
    draftBand_.frequency_text = QString::number(frequency);
    if (gain_applicable()) {
        draftBand_.gain_db = gain;
        draftBand_.gain_text = QString::number(gain);
    }
    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::graphRelease()
{
    static_cast<void>(commitDraft());
}

void EqViewModel::adjustSecondaryParameter(int steps, bool shiftPressed)
{
    if (steps == 0) {
        return;
    }

    switch (draftBand_.filter_type) {
    case dsp::EqFilterType::BELL:
    case dsp::EqFilterType::NOTCH: {
        // Q multiplicative adjustment: normal ~3% per notch, Shift ~0.8% per notch
        const double factor = shiftPressed ? 1.008 : 1.03;
        double newQ = draftBand_.q * std::pow(factor, steps);
        newQ = std::clamp(newQ, kMinQ, kMaxQ);
        draftBand_.q = newQ;
        draftBand_.q_text = QString::number(newQ, 'f', 3);
        break;
    }
    case dsp::EqFilterType::LOW_SHELF:
    case dsp::EqFilterType::HIGH_SHELF: {
        // Shelf slope linear adjustment: normal ~0.05, Shift ~0.01
        const double stepVal = shiftPressed ? 0.01 : 0.05;
        double newSlope = draftBand_.shelf_slope + steps * stepVal;
        newSlope = std::clamp(newSlope, kMinSlope, kMaxSlope);
        draftBand_.shelf_slope = newSlope;
        draftBand_.shelf_slope_text = QString::number(newSlope, 'f', 2);
        break;
    }
    case dsp::EqFilterType::HIGH_PASS:
    case dsp::EqFilterType::LOW_PASS: {
        // Slope dB/oct discrete steps: 6, 12, 18, 24, 36, 48
        const std::array<int, 6> slopes{6, 12, 18, 24, 36, 48};
        int currentVal = static_cast<int>(draftBand_.slope_db_per_octave);
        auto it = std::find(slopes.begin(), slopes.end(), currentVal);
        int idx = (it != slopes.end()) ? static_cast<int>(std::distance(slopes.begin(), it)) : 1;
        idx = std::clamp(idx + (steps > 0 ? 1 : -1), 0, static_cast<int>(slopes.size() - 1));
        setDraftSlopeDbPerOct(slopes[static_cast<std::size_t>(idx)]);
        return;
    }
    }

    update_response_grid();
    update_validation_state();
    emit changed();
}

void EqViewModel::trigger_preview()
{
    update_response_grid();
    update_validation_state();
    ++previewGeneration_; // Increment generation on Source/PREPARED replacement
    request_preview();
}

core::SampleRate EqViewModel::current_sample_rate() const noexcept
{
    if (snapshotProvider_) {
        const auto snapshot = snapshotProvider_();
        if (snapshot) {
            return snapshot->view().format().sample_rate();
        }
    }
    return *core::SampleRate::create(48000).value();
}

bool EqViewModel::is_mono_prepared() const noexcept
{
    if (snapshotProvider_) {
        const auto snapshot = snapshotProvider_();
        if (snapshot) {
            return snapshot->view().format().channel_layout() == audio::ChannelLayout::MONO_C;
        }
    }
    return false;
}

double EqViewModel::max_frequency_hz() const noexcept
{
    const double Fs = static_cast<double>(current_sample_rate().value());
    return std::min(kMaxFreqCap, 0.45 * Fs);
}

EqViewModel::DraftBand EqViewModel::make_default_band(core::Uuid id) noexcept
{
    return DraftBand{
        .band_id = id,
        .enabled = true,
        .filter_type = dsp::EqFilterType::BELL,
        .routing = dsp::EqRouting::STEREO,
        .frequency_hz = 1000.0,
        .gain_db = 0.0,
        .q = 0.707,
        .shelf_slope = 1.0,
        .frequency_text = QStringLiteral("1000"),
        .gain_text = QStringLiteral("0"),
        .q_text = QStringLiteral("0.707"),
        .shelf_slope_text = QStringLiteral("1"),
        .slope_db_per_octave = dsp::SlopeDbPerOctave::DB_12,
    };
}

std::optional<dsp::EqBandParameters> EqViewModel::make_band_parameters(
    const DraftBand& draft,
    double max_freq)
{
    if (!std::isfinite(draft.frequency_hz)
        || draft.frequency_hz < kMinFreq
        || draft.frequency_hz > max_freq) {
        return std::nullopt;
    }

    dsp::EqBandPayload payload;
    switch (draft.filter_type) {
    case dsp::EqFilterType::BELL:
        if (!std::isfinite(draft.gain_db) || draft.gain_db < kMinGain || draft.gain_db > kMaxGain
            || !std::isfinite(draft.q) || draft.q < kMinQ || draft.q > kMaxQ) {
            return std::nullopt;
        }
        payload = dsp::BellPayload{draft.frequency_hz, draft.gain_db, draft.q};
        break;
    case dsp::EqFilterType::NOTCH:
        if (!std::isfinite(draft.q) || draft.q < kMinQ || draft.q > kMaxQ) {
            return std::nullopt;
        }
        payload = dsp::NotchPayload{draft.frequency_hz, draft.q};
        break;
    case dsp::EqFilterType::LOW_SHELF:
    case dsp::EqFilterType::HIGH_SHELF:
        if (!std::isfinite(draft.gain_db) || draft.gain_db < kMinGain || draft.gain_db > kMaxGain
            || !std::isfinite(draft.shelf_slope) || draft.shelf_slope < kMinSlope || draft.shelf_slope > kMaxSlope) {
            return std::nullopt;
        }
        payload = dsp::ShelfPayload{draft.frequency_hz, draft.gain_db, draft.shelf_slope};
        break;
    case dsp::EqFilterType::HIGH_PASS:
    case dsp::EqFilterType::LOW_PASS:
        payload = dsp::PassPayload{draft.frequency_hz, draft.slope_db_per_octave};
        break;
    }

    auto result = dsp::EqBandParameters::create(
        draft.band_id, draft.enabled, draft.filter_type, draft.routing, payload);
    if (!result) {
        return std::nullopt;
    }
    return *result.value();
}

void EqViewModel::update_response_grid()
{
    responseGrid_.clear();
    const auto bandOpt = make_band_parameters(draftBand_, max_frequency_hz());
    if (!bandOpt) {
        return;
    }

    const double minF = kMinFreq;
    const double maxF = max_frequency_hz();
    const double logMin = std::log(minF);
    const double logMax = std::log(maxF);

    std::vector<double> freqs;
    freqs.reserve(kResponseGridPoints + 128);

    // 1. Baseline log-spaced points
    for (std::size_t i = 0; i < kResponseGridPoints; ++i) {
        if (i == 0) {
            freqs.push_back(minF);
        } else if (i == kResponseGridPoints - 1) {
            freqs.push_back(maxF);
        } else {
            const double frac = static_cast<double>(i) / static_cast<double>(kResponseGridPoints - 1);
            const double f = std::exp(logMin + frac * (logMax - logMin));
            freqs.push_back(std::clamp(f, minF, maxF));
        }
    }

    // 2. ALWAYS inject exact f0
    const double f0 = draftBand_.frequency_hz;
    if (f0 >= minF && f0 <= maxF) {
        freqs.push_back(f0);
    }

    // 3. Add local refinement around f0 for High-Q Bell and Notch filters
    if ((draftBand_.filter_type == dsp::EqFilterType::BELL || draftBand_.filter_type == dsp::EqFilterType::NOTCH)
        && draftBand_.q > 1.0 && f0 >= minF && f0 <= maxF) {
        // Fractional offsets around f0 based on Q width
        const double bandwidthFrac = 1.0 / std::max(0.1, draftBand_.q);
        const std::array<double, 12> localRatios{
            1.0 - 0.5 * bandwidthFrac, 1.0 - 0.25 * bandwidthFrac,
            1.0 - 0.1 * bandwidthFrac, 1.0 - 0.05 * bandwidthFrac,
            1.0 - 0.02 * bandwidthFrac, 1.0 - 0.01 * bandwidthFrac,
            1.0 + 0.01 * bandwidthFrac, 1.0 + 0.02 * bandwidthFrac,
            1.0 + 0.05 * bandwidthFrac, 1.0 + 0.1 * bandwidthFrac,
            1.0 + 0.25 * bandwidthFrac, 1.0 + 0.5 * bandwidthFrac
        };
        for (const double ratio : localRatios) {
            const double rf = f0 * ratio;
            if (rf >= minF && rf <= maxF) {
                freqs.push_back(rf);
            }
        }
    }

    // 4. Sort and remove duplicates / near-duplicates
    std::sort(freqs.begin(), freqs.end());
    std::vector<double> uniqueFreqs;
    uniqueFreqs.reserve(freqs.size());
    for (const double f : freqs) {
        if (uniqueFreqs.empty() || std::abs(f - uniqueFreqs.back()) > 1e-6) {
            uniqueFreqs.push_back(f);
        }
    }

    // 5. Enforce hard point count cap <= 1024
    if (uniqueFreqs.size() > 1024) {
        uniqueFreqs.resize(1024);
    }

    auto pointsRes = dsp::evaluate_band_response(*bandOpt, uniqueFreqs, current_sample_rate());
    if (!pointsRes) {
        return;
    }

    for (const auto& pt : *pointsRes.value()) {
        QVariantMap pointMap;
        pointMap.insert(QStringLiteral("frequency"), pt.frequency_hz);
        pointMap.insert(QStringLiteral("magnitudeDb"), pt.magnitude_db);
        pointMap.insert(QStringLiteral("phaseRad"), pt.phase_rad);
        responseGrid_.append(pointMap);
    }
}

void EqViewModel::push_undo_snapshot(EqStateSnapshot previousSnapshot)
{
    undoStack_.push_back(std::move(previousSnapshot));
    if (undoStack_.size() > 50U) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
}

EqViewModel::EqStateSnapshot EqViewModel::capture_current_snapshot() const
{
    return EqStateSnapshot{
        .bands = committedBands_,
        .selectedIndex = selectedIndex_,
        .bypass = bypass_,
    };
}

void EqViewModel::restore_snapshot(const EqStateSnapshot& snapshot)
{
    committedBands_ = snapshot.bands;
    auto paramsRes = dsp::ParametricEqParameters::create(committedBands_);
    Q_ASSERT(paramsRes);
    committedParams_ = *paramsRes.value();

    selectedIndex_ = std::min(snapshot.selectedIndex, committedBands_.empty() ? 0U : committedBands_.size() - 1U);
    draftBand_ = band_to_draft(committedBands_[selectedIndex_]);
    bypass_ = snapshot.bypass;

    update_response_grid();
    update_validation_state();
}

void EqViewModel::update_validation_state()
{
    validationField_.clear();
    validationMessage_.clear();

    if (is_mono_prepared() && draftBand_.routing != dsp::EqRouting::STEREO) {
        validationField_ = QStringLiteral("routing");
        validationMessage_ = QStringLiteral("Mono sources only support STEREO routing.");
        return;
    }

    bool ok = false;
    const double freq = draftBand_.frequency_text.toDouble(&ok);
    if (!ok || !std::isfinite(freq)) {
        validationField_ = QStringLiteral("frequency");
        validationMessage_ = QStringLiteral("Invalid frequency numeric syntax.");
        return;
    }
    const double maxF = max_frequency_hz();
    if (freq < kMinFreq || freq > maxF) {
        validationField_ = QStringLiteral("frequency");
        validationMessage_ = QString::asprintf("Frequency must be between %.0f Hz and %.0f Hz.", kMinFreq, maxF);
        return;
    }

    if (gain_applicable()) {
        const double g = draftBand_.gain_text.toDouble(&ok);
        if (!ok || !std::isfinite(g)) {
            validationField_ = QStringLiteral("gain");
            validationMessage_ = QStringLiteral("Invalid gain numeric syntax.");
            return;
        }
        if (g < kMinGain || g > kMaxGain) {
            validationField_ = QStringLiteral("gain");
            validationMessage_ = QStringLiteral("Gain must be between -18 dB and +18 dB.");
            return;
        }
    }

    if (q_applicable()) {
        const double qVal = draftBand_.q_text.toDouble(&ok);
        if (!ok || !std::isfinite(qVal)) {
            validationField_ = QStringLiteral("q");
            validationMessage_ = QStringLiteral("Invalid Q numeric syntax.");
            return;
        }
        if (qVal < kMinQ || qVal > kMaxQ) {
            validationField_ = QStringLiteral("q");
            validationMessage_ = QStringLiteral("Q must be between 0.10 and 12.0.");
            return;
        }
    }

    if (shelf_slope_applicable()) {
        const double sVal = draftBand_.shelf_slope_text.toDouble(&ok);
        if (!ok || !std::isfinite(sVal)) {
            validationField_ = QStringLiteral("shelfSlope");
            validationMessage_ = QStringLiteral("Invalid shelf slope numeric syntax.");
            return;
        }
        if (sVal < kMinSlope || sVal > kMaxSlope) {
            validationField_ = QStringLiteral("shelfSlope");
            validationMessage_ = QStringLiteral("Shelf slope must be between 0.10 and 1.0.");
            return;
        }
    }
}

void EqViewModel::request_preview()
{
    if (!snapshotProvider_) {
        return;
    }
    auto preparedSnapshot = snapshotProvider_();
    if (!preparedSnapshot) {
        previewStatus_ = QStringLiteral("NO_PREPARED_REALIZATION");
        previewError_ = QStringLiteral("No PREPARED realization available for rendering.");
        emit changed();
        return;
    }

    previewStatus_ = QStringLiteral("RENDERING");
    previewError_.clear();
    emit changed();

    {
        const std::scoped_lock lock{workerMutex_};
        pendingJob_ = PreviewJob{
            .generation = previewGeneration_,
            .parameters = committedParams_,
            .bypass = bypass_,
            .preparedSnapshot = std::move(preparedSnapshot),
            .instanceId = instanceId_,
        };
    }
    workerCond_.notify_one();
}

void EqViewModel::worker_loop()
{
    while (true) {
        std::optional<PreviewJob> job;
        PreviewExecutor executor;
        {
            std::unique_lock lock{workerMutex_};
            workerCond_.wait(lock, [this] { return workerStopping_ || pendingJob_.has_value(); });
            if (workerStopping_) {
                return;
            }
            job = std::move(pendingJob_);
            pendingJob_.reset();
            executor = previewExecutor_;
        }

        auto result = [&]() -> core::Result<render::RenderResult> {
            if (executor) {
                return executor(*job);
            }
            try {
                auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
                if (!registry) {
                    return core::Result<render::RenderResult>::failure(*registry.error());
                }
                auto chain = dsp::ProcessingChain::create(
                    *registry.value(),
                    {dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
                if (!chain) {
                    return core::Result<render::RenderResult>::failure(*chain.error());
                }
                auto addStatus = chain.value()->add(job->instanceId, "rgsml.dsp.parametric-eq", 0);
                if (!addStatus) {
                    return core::Result<render::RenderResult>::failure(*addStatus.error());
                }
                auto bypassStatus = chain.value()->set_user_bypass(job->instanceId, job->bypass);
                if (!bypassStatus) {
                    return core::Result<render::RenderResult>::failure(*bypassStatus.error());
                }

                dsp::ModuleExecutionBinding binding{job->instanceId, job->parameters};
                auto request = render::RenderRequest::create(
                    job->preparedSnapshot->view(),
                    job->preparedSnapshot->view().absolute_range(),
                    *chain.value(),
                    {binding},
                    *core::FrameCount::create(4096).value());
                if (!request) {
                    return core::Result<render::RenderResult>::failure(*request.error());
                }

                return render::render_preview(*request.value(), *registry.value());
            } catch (...) {
                return core::Result<render::RenderResult>::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "EQ preview worker encountered an unexpected exception."});
            }
        }();

        auto outcome = std::make_shared<core::Result<render::RenderResult>>(std::move(result));

        static_cast<void>(QMetaObject::invokeMethod(
            this,
            [this, gen = job->generation, outcome] {
                publish_preview_result(gen, outcome);
            },
            Qt::QueuedConnection));
    }
}

void EqViewModel::publish_preview_result(
    std::uint64_t generation,
    std::shared_ptr<core::Result<render::RenderResult>> outcome)
{
    if (generation != previewGeneration_) {
        ++staleResultsDiscarded_;
        return;
    }

    if (!outcome || !*outcome) {
        previewStatus_ = QStringLiteral("ERROR");
        previewError_ = QString::fromStdString(
            outcome && outcome->error() ? outcome->error()->message() : "Preview rendering failed.");
        emit changed();
        return;
    }

    if (publisher_) {
        auto pubStatus = publisher_(std::move(*outcome->value()));
        if (!pubStatus) {
            previewStatus_ = QStringLiteral("ERROR");
            previewError_ = QString::fromStdString(pubStatus.error()->message());
            emit changed();
            return;
        }
    }

    previewStatus_ = QStringLiteral("READY");
    previewError_.clear();
    emit changed();
}

}  // namespace rgsml::app
