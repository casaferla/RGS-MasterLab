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
constexpr std::size_t kResponseGridPoints = 100;

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
    emit changed();
}

void EqViewModel::addBand()
{
    if (committedBands_.size() >= 6U) {
        return;
    }
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
    committedParams_ = *paramsRes.value();
    selectedIndex_ = committedBands_.size() - 1U;
    draftBand_ = defaultDraft;

    ++previewGeneration_;
    update_response_grid();
    emit changed();
    request_preview();
}

void EqViewModel::removeSelectedBand()
{
    if (committedBands_.size() <= 1U) {
        return;
    }
    committedBands_.erase(committedBands_.begin() + selectedIndex_);
    if (selectedIndex_ >= committedBands_.size()) {
        selectedIndex_ = committedBands_.size() - 1U;
    }
    auto paramsRes = dsp::ParametricEqParameters::create(committedBands_);
    if (!paramsRes) {
        return;
    }
    committedParams_ = *paramsRes.value();
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
    bypass_ = bypass;
    ++previewGeneration_;
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
    emit changed();
}

bool EqViewModel::commitDraft()
{
    // Parse raw text for applicable fields
    bool ok = false;

    const double freq = draftBand_.frequency_text.toDouble(&ok);
    if (!ok || !std::isfinite(freq)) {
        return false; // Raw text invalid: reject commit, preserve raw draft text, no preview
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

    committedBands_ = std::move(candidateBands);
    committedParams_ = std::move(*candidateParamsRes.value());

    // Format raw text fields canonically after valid commit
    draftBand_.frequency_text = QString::number(draftBand_.frequency_hz);
    draftBand_.gain_text = QString::number(draftBand_.gain_db);
    draftBand_.q_text = QString::number(draftBand_.q);
    draftBand_.shelf_slope_text = QString::number(draftBand_.shelf_slope);

    ++previewGeneration_;
    update_response_grid();
    emit changed();
    request_preview();
    return true;
}

void EqViewModel::cancelDraft()
{
    draftBand_ = band_to_draft(committedBands_[selectedIndex_]);
    update_response_grid();
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
    emit changed();
}

void EqViewModel::graphRelease()
{
    static_cast<void>(commitDraft());
}

void EqViewModel::trigger_preview()
{
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
    freqs.reserve(kResponseGridPoints);
    for (std::size_t i = 0; i < kResponseGridPoints; ++i) {
        const double frac = static_cast<double>(i) / static_cast<double>(kResponseGridPoints - 1);
        freqs.push_back(std::exp(logMin + frac * (logMax - logMin)));
    }

    auto pointsRes = dsp::evaluate_band_response(*bandOpt, freqs, current_sample_rate());
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
