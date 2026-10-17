#pragma once
// Test-only verifier: NEVER compiled into production DSP. M16 remains the
// global true-peak measurement authority for the exact compensated N frames.
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>

namespace rgsml::tests::m17_verifier {

inline constexpr double kMinimumGlobalTrimGain=0x1.fa23a197d2819p-1;

[[nodiscard]] inline bool trim_within_guard(double g) noexcept
{
    return std::isfinite(g) && g>=kMinimumGlobalTrimGain && g<=1.0;
}
struct VerificationResult final {
    double before_peak{0.0};
    double after_peak{0.0};
    double global_trim_gain{1.0};
    bool trim_used{false};
    analysis::TruePeakQualificationStatus status{
        analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED};
};
[[nodiscard]] inline core::Error error(core::ErrorCode code,
    const char* category,const char* message)
{
    return core::Error{code,message,{{"category",category}}};
}
[[nodiscard]] inline core::Result<VerificationResult>
verify_full_resource(audio::AudioBuffer& resource,double ceiling_db_tp)
{
    using core::Result;
    if(!std::isfinite(ceiling_db_tp)
        || ceiling_db_tp< -12.0 || ceiling_db_tp>0.0)
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidArgument,"INVALID_M17_CEILING",
            "M17 test-only verifier requires a valid Ceiling."));
    auto measured=analysis::measure_true_peak_full_resource(resource);
    if(!measured)return Result<VerificationResult>::failure(*measured.error());
    if(measured.value()->qualification_status()
        ==analysis::TruePeakQualificationStatus::UNSUPPORTED)
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidState,"M16_MEASUREMENT_UNSUPPORTED",
            "Unsupported M16 result must not masquerade as qualified."));
    const double before=measured.value()->global_linear_peak();
    const double ceiling=std::pow(10.0,ceiling_db_tp/20.0);
    if(!std::isfinite(before) || before<0.0 || !std::isfinite(ceiling)
        || ceiling<=0.0)
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidState,"M16_MEASUREMENT_INVALID",
            "M16 numeric result is invalid."));
    VerificationResult result{before,before,1.0,false,
        measured.value()->qualification_status()};
    if(before<=ceiling)
        return Result<VerificationResult>::success(result);
    const double gain=std::nextafter(ceiling/before,0.0);
    // Decision is exact binary64, not 20log10(gain) with rounded boundary.
    if(!trim_within_guard(gain))
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidState,"LIMITER_VERIFICATION_GUARD_EXCEEDED",
            "Global verification trim would exceed frozen 0.10 dB guard."));
    // A single global scalar for the whole stereo resource. Test-only caller
    // owns a materialized, unquantized binary64 OUTPUT_RATE audio buffer.
    for(std::size_t ch=0;ch<resource.view().format().channel_count();++ch) {
        auto writable=resource.mutable_view().channel(ch);
        if(!writable)return Result<VerificationResult>::failure(*writable.error());
        for(double& v:*writable.value()) {
            v = v*gain;
            if(!std::isfinite(v))
                return Result<VerificationResult>::failure(error(
                    core::ErrorCode::InvalidAudioSample,
                    "LIMITER_VERIFICATION_NUMERIC_FAILURE",
                    "Global trim produced nonfinite PCM."));
        }
    }
    auto after=analysis::measure_true_peak_full_resource(resource);
    if(!after)return Result<VerificationResult>::failure(*after.error());
    if(after.value()->global_linear_peak()>ceiling)
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidState,
            "LIMITER_VERIFICATION_NUMERIC_FAILURE",
            "One allowed trim still exceeds Ceiling under M16."));
    result.after_peak=after.value()->global_linear_peak();
    result.global_trim_gain=gain;
    result.trim_used=true;
    result.status=after.value()->qualification_status();
    return Result<VerificationResult>::success(result);
}

} // namespace rgsml::tests::m17_verifier
