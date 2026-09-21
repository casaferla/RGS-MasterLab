#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;

[[nodiscard]] Error param_error(ErrorCode code, std::string category, std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] bool is_valid_frequency(double f) noexcept
{
    return std::isfinite(f) && f >= 20.0 && f <= 20000.0;
}

[[nodiscard]] bool is_valid_gain(double g) noexcept
{
    return std::isfinite(g) && g >= -18.0 && g <= 18.0;
}

[[nodiscard]] bool is_valid_q(double q) noexcept
{
    return std::isfinite(q) && q >= 0.10 && q <= 12.0;
}

[[nodiscard]] bool is_valid_shelf_slope(double s) noexcept
{
    return std::isfinite(s) && s >= 0.10 && s <= 1.0;
}

}  // namespace

EqBandParameters::EqBandParameters(
    rgsml::core::Uuid band_id,
    bool enabled,
    EqFilterType filter_type,
    EqRouting routing,
    EqBandPayload payload) noexcept
    : band_id_(band_id)
    , enabled_(enabled)
    , filter_type_(filter_type)
    , routing_(routing)
    , payload_(std::move(payload))
{
}

Result<EqBandParameters> EqBandParameters::create(
    rgsml::core::Uuid band_id,
    bool enabled,
    EqFilterType filter_type,
    EqRouting routing,
    EqBandPayload payload)
{
    if (filter_type != EqFilterType::BELL &&
        filter_type != EqFilterType::NOTCH &&
        filter_type != EqFilterType::LOW_SHELF &&
        filter_type != EqFilterType::HIGH_SHELF &&
        filter_type != EqFilterType::HIGH_PASS &&
        filter_type != EqFilterType::LOW_PASS) {
        return Result<EqBandParameters>::failure(param_error(
            ErrorCode::InvalidArgument,
            "INVALID_FILTER_TYPE",
            "Filter type value is invalid or unrecognized."));
    }

    if (routing != EqRouting::STEREO &&
        routing != EqRouting::MID &&
        routing != EqRouting::SIDE &&
        routing != EqRouting::LEFT &&
        routing != EqRouting::RIGHT) {
        return Result<EqBandParameters>::failure(param_error(
            ErrorCode::InvalidArgument,
            "INVALID_ROUTING",
            "Routing value is invalid or unrecognized."));
    }

    switch (filter_type) {
    case EqFilterType::BELL: {
        const auto* p = std::get_if<BellPayload>(&payload);
        if (!p) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::InvalidArgument,
                "INVALID_BAND_PAYLOAD",
                "BELL filter type requires BellPayload."));
        }
        if (!is_valid_frequency(p->frequency_hz)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "FREQUENCY_OUT_OF_RANGE",
                "Frequency must be in range [20, 20000] Hz."));
        }
        if (!is_valid_gain(p->gain_db)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "GAIN_OUT_OF_RANGE",
                "Gain must be in range [-18, +18] dB."));
        }
        if (!is_valid_q(p->q)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "Q_OUT_OF_RANGE",
                "Q must be in range [0.10, 12]."));
        }
        BellPayload normalized = *p;
        if (normalized.gain_db == 0.0) {
            normalized.gain_db = 0.0;
        }
        payload = normalized;
        break;
    }
    case EqFilterType::NOTCH: {
        const auto* p = std::get_if<NotchPayload>(&payload);
        if (!p) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::InvalidArgument,
                "INVALID_BAND_PAYLOAD",
                "NOTCH filter type requires NotchPayload."));
        }
        if (!is_valid_frequency(p->frequency_hz)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "FREQUENCY_OUT_OF_RANGE",
                "Frequency must be in range [20, 20000] Hz."));
        }
        if (!is_valid_q(p->q)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "Q_OUT_OF_RANGE",
                "Q must be in range [0.10, 12]."));
        }
        break;
    }
    case EqFilterType::LOW_SHELF:
    case EqFilterType::HIGH_SHELF: {
        const auto* p = std::get_if<ShelfPayload>(&payload);
        if (!p) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::InvalidArgument,
                "INVALID_BAND_PAYLOAD",
                "SHELF filter type requires ShelfPayload."));
        }
        if (!is_valid_frequency(p->frequency_hz)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "FREQUENCY_OUT_OF_RANGE",
                "Frequency must be in range [20, 20000] Hz."));
        }
        if (!is_valid_gain(p->gain_db)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "GAIN_OUT_OF_RANGE",
                "Gain must be in range [-18, +18] dB."));
        }
        if (!is_valid_shelf_slope(p->shelf_slope)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "SHELF_SLOPE_OUT_OF_RANGE",
                "Shelf slope must be in range [0.10, 1.0]."));
        }
        ShelfPayload normalized = *p;
        if (normalized.gain_db == 0.0) {
            normalized.gain_db = 0.0;
        }
        payload = normalized;
        break;
    }
    case EqFilterType::HIGH_PASS:
    case EqFilterType::LOW_PASS: {
        const auto* p = std::get_if<PassPayload>(&payload);
        if (!p) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::InvalidArgument,
                "INVALID_BAND_PAYLOAD",
                "PASS filter type requires PassPayload."));
        }
        if (!is_valid_frequency(p->frequency_hz)) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::OutOfRange,
                "FREQUENCY_OUT_OF_RANGE",
                "Frequency must be in range [20, 20000] Hz."));
        }
        const auto s = p->slope_db_per_octave;
        if (s != SlopeDbPerOctave::DB_6 &&
            s != SlopeDbPerOctave::DB_12 &&
            s != SlopeDbPerOctave::DB_18 &&
            s != SlopeDbPerOctave::DB_24 &&
            s != SlopeDbPerOctave::DB_36 &&
            s != SlopeDbPerOctave::DB_48) {
            return Result<EqBandParameters>::failure(param_error(
                ErrorCode::InvalidArgument,
                "INVALID_SLOPE",
                "Slope value is invalid or unrecognized."));
        }
        break;
    }
    }

    return Result<EqBandParameters>::success(
        EqBandParameters{band_id, enabled, filter_type, routing, std::move(payload)});
}

const rgsml::core::Uuid& EqBandParameters::band_id() const noexcept { return band_id_; }
bool EqBandParameters::enabled() const noexcept { return enabled_; }
EqFilterType EqBandParameters::filter_type() const noexcept { return filter_type_; }
EqRouting EqBandParameters::routing() const noexcept { return routing_; }
const EqBandPayload& EqBandParameters::payload() const noexcept { return payload_; }

ParametricEqParameters::ParametricEqParameters(std::vector<EqBandParameters> bands) noexcept
    : bands_(std::move(bands))
{
}

Result<ParametricEqParameters> ParametricEqParameters::create(
    std::vector<EqBandParameters> bands)
{
    if (bands.empty() || bands.size() > 6U) {
        return Result<ParametricEqParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "INVALID_BAND_COUNT",
            "Parametric EQ must have between 1 and 6 bands inclusive."));
    }

    for (std::size_t i = 0; i < bands.size(); ++i) {
        for (std::size_t j = i + 1; j < bands.size(); ++j) {
            if (bands[i].band_id() == bands[j].band_id()) {
                return Result<ParametricEqParameters>::failure(param_error(
                    ErrorCode::InvalidArgument,
                    "DUPLICATE_BAND_ID",
                    "Band IDs within a Parametric EQ parameter set must be unique."));
            }
        }
    }

    return Result<ParametricEqParameters>::success(
        ParametricEqParameters{std::move(bands)});
}

Result<ParametricEqParameters> ParametricEqParameters::create_legacy_default()
{
    const auto band_id = rgsml::core::Uuid::parse("00000000-0000-4000-8000-000000000011");
    if (!band_id) {
        return Result<ParametricEqParameters>::failure(*band_id.error());
    }
    auto band = EqBandParameters::create(
        *band_id.value(),
        true,
        EqFilterType::BELL,
        EqRouting::STEREO,
        BellPayload{1000.0, 0.0, 0.707});
    if (!band) {
        return Result<ParametricEqParameters>::failure(*band.error());
    }
    std::vector<EqBandParameters> bands;
    bands.push_back(std::move(*band.value()));
    return create(std::move(bands));
}

const std::vector<EqBandParameters>& ParametricEqParameters::bands() const noexcept
{
    return bands_;
}

}  // namespace rgsml::dsp
