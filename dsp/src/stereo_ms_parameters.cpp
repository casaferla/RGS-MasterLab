#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <rgsml/core/error.hpp>

#include <cmath>
#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;

[[nodiscard]] Error parameter_error(
    ErrorCode code,
    std::string category,
    std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] double canonical_zero(double value) noexcept
{
    return value == 0.0 ? 0.0 : value;
}

}  // namespace

Result<StereoMsParameters> StereoMsParameters::create(
    double mid_gain_db,
    double side_gain_db,
    bool side_muted,
    MonoBassMode mono_bass_mode,
    double mono_bass_cutoff_hz,
    double low_band_width_percent)
{
    // Validate stored fields even when temporarily non-effective (OFF, Side
    // muted, or mono input). Invalid state is never silently repaired.
    if (mono_bass_mode != MonoBassMode::OFF
        && mono_bass_mode != MonoBassMode::LR12
        && mono_bass_mode != MonoBassMode::LR24) {
        return Result<StereoMsParameters>::failure(parameter_error(
            ErrorCode::InvalidArgument,
            "INVALID_STEREO_MS_MODE",
            "Mono Bass mode must be OFF, LR12 or LR24."));
    }

    if (!std::isfinite(mid_gain_db) || !std::isfinite(side_gain_db)
        || !std::isfinite(mono_bass_cutoff_hz)
        || !std::isfinite(low_band_width_percent)) {
        return Result<StereoMsParameters>::failure(parameter_error(
            ErrorCode::InvalidArgument,
            "INVALID_STEREO_MS_PARAMETER",
            "Stereo/M-S parameters must be finite binary64 values."));
    }

    if (mid_gain_db < -12.0 || mid_gain_db > 12.0) {
        return Result<StereoMsParameters>::failure(parameter_error(
            ErrorCode::OutOfRange,
            "STEREO_MS_PARAMETER_OUT_OF_RANGE",
            "midGainDb must be within inclusive [-12, +12] dB."));
    }
    if (side_gain_db < -24.0 || side_gain_db > 12.0) {
        return Result<StereoMsParameters>::failure(parameter_error(
            ErrorCode::OutOfRange,
            "STEREO_MS_PARAMETER_OUT_OF_RANGE",
            "sideGainDb must be within inclusive [-24, +12] dB."));
    }
    if (mono_bass_cutoff_hz < 40.0 || mono_bass_cutoff_hz > 300.0) {
        return Result<StereoMsParameters>::failure(parameter_error(
            ErrorCode::OutOfRange,
            "STEREO_MS_PARAMETER_OUT_OF_RANGE",
            "monoBassCutoffHz must be within inclusive [40, 300] Hz."));
    }
    if (low_band_width_percent < 0.0 || low_band_width_percent > 100.0) {
        return Result<StereoMsParameters>::failure(parameter_error(
            ErrorCode::OutOfRange,
            "STEREO_MS_PARAMETER_OUT_OF_RANGE",
            "lowBandWidthPercent must be within inclusive [0, 100]%."));
    }

    // The sample-rate predicate cutoff < 0.45*sampleRate belongs to a later
    // prepare-time check. Non-effective stored values are never discarded.
    return Result<StereoMsParameters>::success(StereoMsParameters{
        canonical_zero(mid_gain_db),
        canonical_zero(side_gain_db),
        side_muted,
        mono_bass_mode,
        mono_bass_cutoff_hz,
        canonical_zero(low_band_width_percent)});
}

Result<StereoMsParameters> StereoMsParameters::create_default() noexcept
{
    return create();
}

StereoMsParameters::StereoMsParameters(
    double mid_gain_db,
    double side_gain_db,
    bool side_muted,
    MonoBassMode mono_bass_mode,
    double mono_bass_cutoff_hz,
    double low_band_width_percent) noexcept
    : mid_gain_db_(mid_gain_db)
    , side_gain_db_(side_gain_db)
    , side_muted_(side_muted)
    , mono_bass_mode_(mono_bass_mode)
    , mono_bass_cutoff_hz_(mono_bass_cutoff_hz)
    , low_band_width_percent_(low_band_width_percent)
{
}

double StereoMsParameters::mid_gain_db() const noexcept { return mid_gain_db_; }
double StereoMsParameters::side_gain_db() const noexcept { return side_gain_db_; }
bool StereoMsParameters::side_muted() const noexcept { return side_muted_; }
MonoBassMode StereoMsParameters::mono_bass_mode() const noexcept { return mono_bass_mode_; }
double StereoMsParameters::mono_bass_cutoff_hz() const noexcept { return mono_bass_cutoff_hz_; }
double StereoMsParameters::low_band_width_percent() const noexcept
{
    return low_band_width_percent_;
}

}  // namespace rgsml::dsp
