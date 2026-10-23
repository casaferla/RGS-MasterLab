#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>

#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/core/error.hpp>

#include <cmath>
#include <chrono>
#include <cstdint>
#include <limits>
#include <utility>

namespace rgsml::render {
namespace {
using core::Result;
using core::ErrorCode;
using core::Error;
[[nodiscard]] Error verification_error(
    ErrorCode code, const char* category, const char* message)
{
    return Error{code,message,{{"category",category}}};
}

// Exact frozen verification guard: 0.10 dB expressed as its binary64
// lower-bound linear gain. Compare binary64 directly, NEVER rounded dB.
inline constexpr double kMinimumVerificationGain=0x1.fa23a197d2819p-1;

[[nodiscard]] bool allowed_gain(double value) noexcept
{
    return std::isfinite(value)
        && value>=kMinimumVerificationGain && value<=1.0;
}
} // namespace

Result<TruePeakLimiterCheckedCandidate> render_true_peak_limiter_checked_candidate(
    audio::AudioBufferView source,
    dsp::TruePeakLimiterParameters params,
    const dsp::ModuleRegistry& registry,
    core::FrameCount max_block,
    bool bypassed,
    const TerminalRenderCancel& cancelled,
    TerminalRenderStageTimings* stage_timings,
    bool use_experimental_offline_accelerator)
{
    // No clock call inside the high-frequency DSP controller. These
    // boundaries measure complete real production stages, even on failures.
    const auto m17_started=std::chrono::steady_clock::now();
    auto rendered=render_true_peak_limiter_terminal_candidate(
        source,params,registry,max_block,bypassed,cancelled,
        stage_timings ? stage_timings->progress : TerminalRenderProgressSink{},
        use_experimental_offline_accelerator);
    if(stage_timings)
        stage_timings->m17_core_us=std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now()-m17_started).count();
    if(!rendered)
        return Result<TruePeakLimiterCheckedCandidate>::failure(*rendered.error());

    const auto wasCancelled=[&] {
        return cancelled && cancelled();
    };
    const auto cancelFailure=[] {
        return Result<TruePeakLimiterCheckedCandidate>::failure(
            verification_error(ErrorCode::InvalidState,"M17_RENDER_CANCELLED",
                "Stale terminal render was cancelled before M16 verification."));
    };
    if(wasCancelled()) return cancelFailure();
    if(bypassed) {
        // Inactive means NO verification pass, no added latency, no global
        // gain, and no artificial GR; even over-Ceiling source remains raw.
        return Result<TruePeakLimiterCheckedCandidate>::success({
            std::move(*rendered.value()),std::nullopt});
    }

    auto& pcm=rendered.value()->compensated_pcm;
    if (pcm.view().timebase().frame_domain_id()
        != audio::FrameDomainId::OUTPUT_RATE) {
        return Result<TruePeakLimiterCheckedCandidate>::failure(
            verification_error(ErrorCode::InvalidArgument,
                "M17_VERIFICATION_WRONG_DOMAIN",
                "M17 full-resource verification requires OUTPUT_RATE."));
    }

    // Only the M16 production complete-resource meter owns the true-peak
    // estimate. These numeric readings are SUPPORTED_UNQUALIFIED until
    // official exact-SHA Gate G and Windows physical tests close.
    if(wasCancelled()) return cancelFailure();
    if(stage_timings && stage_timings->progress)
        stage_timings->progress({TerminalRenderProgressPhase::M16_PRIMARY});
    const auto m16_started=std::chrono::steady_clock::now();
    auto measured=analysis::measure_true_peak_full_resource(pcm);
    if(stage_timings)
        stage_timings->m16_primary_us=std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now()-m16_started).count();
    if(!measured)
        return Result<TruePeakLimiterCheckedCandidate>::failure(*measured.error());
    if(measured.value()->qualification_status()
        ==analysis::TruePeakQualificationStatus::UNSUPPORTED) {
        return Result<TruePeakLimiterCheckedCandidate>::failure(
            verification_error(ErrorCode::UnsupportedOperation,
                "M17_VERIFICATION_UNSUPPORTED_M16",
                "M16 has no supported true-peak evidence for this resource."));
    }
    if(wasCancelled()) return cancelFailure();
    const auto before=measured.value()->global_linear_peak();
    const auto ceiling=std::pow(10.0,params.ceiling_db_tp()/20.0);
    if(!std::isfinite(before) || before<0.0
        || !std::isfinite(ceiling) || ceiling<=0.0) {
        return Result<TruePeakLimiterCheckedCandidate>::failure(
            verification_error(ErrorCode::InvalidState,
                "M17_VERIFICATION_INVALID_MEASUREMENT",
                "True-peak verification requires finite linear values."));
    }
    TruePeakLimiterFinalCheck evidence{before,before,1.0,false,
        measured.value()->qualification_status()};
    if(before<=ceiling)
        return Result<TruePeakLimiterCheckedCandidate>::success({
            std::move(*rendered.value()),evidence});

    const auto gain=std::nextafter(ceiling/before,0.0);
    if(!allowed_gain(gain)) {
        return Result<TruePeakLimiterCheckedCandidate>::failure(
            verification_error(ErrorCode::InvalidState,
                "LIMITER_VERIFICATION_GUARD_EXCEEDED",
                "One verification trim would exceed the frozen 0.10 dB allowance."));
    }

    // Build a SEPARATE binary64 buffer; do not modify the core's output
    // before remeasurement. One common scalar for ALL channels and frames,
    // so stereo balance and the original applied-gain witness are preserved.
    // Witness remains BASE LIMITER gain, not the post-verification trim.
    if(stage_timings && stage_timings->progress)
        stage_timings->progress({TerminalRenderProgressPhase::VERIFICATION_TRIM});
    const auto trim_started=std::chrono::steady_clock::now();
    auto staging=audio::AudioBuffer::create(pcm.view().format(),
        audio::FrameDomainId::OUTPUT_RATE,
        pcm.view().absolute_start_frame(),pcm.view().frame_count());
    if(!staging)
        return Result<TruePeakLimiterCheckedCandidate>::failure(*staging.error());
    for(std::size_t ch=0;ch<pcm.view().format().channel_count();++ch) {
        if(wasCancelled()) return cancelFailure();
        const auto src=pcm.view().channel(ch);
        auto dst=staging.value()->mutable_view().channel(ch);
        if(!src||!dst)
            return Result<TruePeakLimiterCheckedCandidate>::failure(
                verification_error(ErrorCode::InvalidState,
                    "M17_VERIFICATION_CHANNEL_UNAVAILABLE",
                    "Global trim requires every PCM channel."));
        for(std::size_t i=0;i<src.value()->size();++i) {
            const double value=(*src.value())[i]*gain;
            if(!std::isfinite(value))
                return Result<TruePeakLimiterCheckedCandidate>::failure(
                    verification_error(ErrorCode::InvalidAudioSample,
                        "M17_VERIFICATION_NONFINITE_TRIM",
                        "Global verification trim produced nonfinite PCM."));
            (*dst.value())[i]=value;
        }
    }

    if(stage_timings)
        stage_timings->verification_trim_us=std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now()-trim_started).count();
    if(wasCancelled()) return cancelFailure();
    if(stage_timings && stage_timings->progress)
        stage_timings->progress({TerminalRenderProgressPhase::M16_SECONDARY});
    const auto recheck_started=std::chrono::steady_clock::now();
    const auto remeasured=analysis::measure_true_peak_full_resource(*staging.value());
    if(stage_timings)
        stage_timings->m16_secondary_us=std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now()-recheck_started).count();
    if(!remeasured)
        return Result<TruePeakLimiterCheckedCandidate>::failure(*remeasured.error());
    const auto after=remeasured.value()->global_linear_peak();
    if(!std::isfinite(after) || after<0 || after>ceiling
        || remeasured.value()->qualification_status()
            ==analysis::TruePeakQualificationStatus::UNSUPPORTED) {
        return Result<TruePeakLimiterCheckedCandidate>::failure(
            verification_error(ErrorCode::InvalidState,
                "M17_VERIFICATION_POST_TRIM_UNSAFE",
                "Global trim failed authoritative full-resource M16 remeasurement."));
    }
    evidence.after_linear_peak=after;
    evidence.global_trim_gain=gain;
    evidence.trim_used=true;
    evidence.qualification_status=remeasured.value()->qualification_status();
    if(wasCancelled()) return cancelFailure();
    rendered.value()->compensated_pcm=std::move(*staging.value());
    return Result<TruePeakLimiterCheckedCandidate>::success({
        std::move(*rendered.value()),evidence});
}
} // namespace rgsml::render
