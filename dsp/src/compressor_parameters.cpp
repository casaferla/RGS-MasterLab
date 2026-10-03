#include <rgsml/dsp/compressor_parameters.hpp>

#include <rgsml/core/error.hpp>

#include <cmath>
#include <string>
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

}  // namespace

Result<CompressorParameters> CompressorParameters::create(
    CompressorDetectorMode detector_mode,
    CompressorChannelLink channel_link,
    double threshold_dbfs,
    double ratio,
    double knee_db,
    double attack_ms,
    double release_ms,
    double rms_time_constant_ms,
    double look_ahead_ms,
    double mix_percent,
    double makeup_gain_db)
{
    if (detector_mode != CompressorDetectorMode::PEAK
        && detector_mode != CompressorDetectorMode::RMS) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::InvalidArgument,
            "INVALID_COMPRESSOR_PARAMETER",
            "Compressor detector mode is unrecognized."));
    }

    if (channel_link != CompressorChannelLink::LINKED_MAX
        && channel_link != CompressorChannelLink::LINKED_MEAN
        && channel_link != CompressorChannelLink::DUAL_MONO) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::InvalidArgument,
            "INVALID_COMPRESSOR_PARAMETER",
            "Compressor channel link mode is unrecognized."));
    }

    if (!std::isfinite(threshold_dbfs) || !std::isfinite(ratio)
        || !std::isfinite(knee_db) || !std::isfinite(attack_ms)
        || !std::isfinite(release_ms) || !std::isfinite(rms_time_constant_ms)
        || !std::isfinite(look_ahead_ms) || !std::isfinite(mix_percent)
        || !std::isfinite(makeup_gain_db)) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::InvalidArgument,
            "INVALID_COMPRESSOR_PARAMETER",
            "Compressor parameters must be finite numbers."));
    }

    if (threshold_dbfs < -120.0 || threshold_dbfs > 0.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "threshold_dbfs must be in [-120.0, 0.0]."));
    }
    if (ratio < 1.0 || ratio > 20.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "ratio must be in [1.0, 20.0]."));
    }
    if (knee_db < 0.0 || knee_db > 24.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "knee_db must be in [0.0, 24.0]."));
    }
    if (attack_ms < 0.1 || attack_ms > 500.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "attack_ms must be in [0.1, 500.0]."));
    }
    if (release_ms < 1.0 || release_ms > 5000.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "release_ms must be in [1.0, 5000.0]."));
    }
    if (rms_time_constant_ms < 1.0 || rms_time_constant_ms > 500.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "rms_time_constant_ms must be in [1.0, 500.0]."));
    }
    if (look_ahead_ms < 0.0 || look_ahead_ms > 20.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "look_ahead_ms must be in [0.0, 20.0]."));
    }
    if (mix_percent < 0.0 || mix_percent > 100.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "mix_percent must be in [0.0, 100.0]."));
    }
    if (makeup_gain_db < -24.0 || makeup_gain_db > 24.0) {
        return Result<CompressorParameters>::failure(param_error(
            ErrorCode::OutOfRange,
            "COMPRESSOR_PARAMETER_OUT_OF_RANGE",
            "makeup_gain_db must be in [-24.0, 24.0]."));
    }

    return Result<CompressorParameters>::success(CompressorParameters{
        detector_mode,
        channel_link,
        threshold_dbfs,
        ratio,
        knee_db,
        attack_ms,
        release_ms,
        rms_time_constant_ms,
        look_ahead_ms,
        mix_percent,
        makeup_gain_db});
}

Result<CompressorParameters> CompressorParameters::create_default() noexcept
{
    return create();
}

CompressorParameters::CompressorParameters(
    CompressorDetectorMode detector_mode,
    CompressorChannelLink channel_link,
    double threshold_dbfs,
    double ratio,
    double knee_db,
    double attack_ms,
    double release_ms,
    double rms_time_constant_ms,
    double look_ahead_ms,
    double mix_percent,
    double makeup_gain_db) noexcept
    : detector_mode_(detector_mode)
    , channel_link_(channel_link)
    , threshold_dbfs_(threshold_dbfs)
    , ratio_(ratio)
    , knee_db_(knee_db)
    , attack_ms_(attack_ms)
    , release_ms_(release_ms)
    , rms_time_constant_ms_(rms_time_constant_ms)
    , look_ahead_ms_(look_ahead_ms)
    , mix_percent_(mix_percent)
    , makeup_gain_db_(makeup_gain_db)
{
}

CompressorDetectorMode CompressorParameters::detector_mode() const noexcept { return detector_mode_; }
CompressorChannelLink CompressorParameters::channel_link() const noexcept { return channel_link_; }
double CompressorParameters::threshold_dbfs() const noexcept { return threshold_dbfs_; }
double CompressorParameters::ratio() const noexcept { return ratio_; }
double CompressorParameters::knee_db() const noexcept { return knee_db_; }
double CompressorParameters::attack_ms() const noexcept { return attack_ms_; }
double CompressorParameters::release_ms() const noexcept { return release_ms_; }
double CompressorParameters::rms_time_constant_ms() const noexcept { return rms_time_constant_ms_; }
double CompressorParameters::look_ahead_ms() const noexcept { return look_ahead_ms_; }
double CompressorParameters::mix_percent() const noexcept { return mix_percent_; }
double CompressorParameters::makeup_gain_db() const noexcept { return makeup_gain_db_; }

}  // namespace rgsml::dsp
