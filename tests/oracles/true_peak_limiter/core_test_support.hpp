#pragma once
// Test-only verifier: NEVER compiled into production DSP. M16 remains the
// global true-peak measurement authority for the exact compensated N frames.
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/sha256.hpp>
#include <rgsml/dsp/true_peak_limiter_module.hpp>
#include <rgsml/core/result.hpp>

#include <bit>
#include <array>
#include <cstdint>
#include <utility>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <limits>
#include <span>
#include <string>

namespace rgsml::tests::m17_verifier {

// Pure test-harness envelope: the production DspProcessSpec has no
// immutable whole-resource owner, PCM identity or chain revision.
struct BoundCoreCheckpoint final {
    dsp::DspRuntimeCheckpoint checkpoint;
    std::string owner_resource_id;
    std::string full_pcm_sha256;
    std::int64_t programme_origin{0};
    std::int64_t programme_frames{0};
    audio::AudioFormat resource_format;
    audio::FrameDomainId resource_domain{audio::FrameDomainId::OUTPUT_RATE};
    std::uint64_t chain_revision{0};
    std::string backend_identity;
};

[[nodiscard]] inline core::Result<std::string> full_pcm_digest(
    const audio::AudioBuffer& resource)
{
    core::Sha256 sha;
    const auto view=resource.view();
    if(view.frame_count().value()<=0)
        return core::Result<std::string>::failure(core::Error{
            core::ErrorCode::InvalidArgument,"Empty full resource."});
    for(std::size_t ch=0;ch<view.format().channel_count();++ch) {
        const auto samples=view.channel(ch);
        if(!samples)return core::Result<std::string>::failure(*samples.error());
        for(const double value:*samples.value()) {
            if(!std::isfinite(value))
                return core::Result<std::string>::failure(core::Error{
                    core::ErrorCode::InvalidAudioSample,"Nonfinite full resource PCM."});
            const auto bits=std::bit_cast<std::uint64_t>(value);
            std::array<std::byte,8> le{};
            for(unsigned j=0;j<8;++j)
                le[j]=static_cast<std::byte>((bits>>(8*j))&0xffU);
            sha.update(le);
        }
    }
    return core::Result<std::string>::success(core::sha256_hex(sha.finalize()));
}

[[nodiscard]] inline core::Result<BoundCoreCheckpoint> bind_core_checkpoint(
    const dsp::DspRuntimeCheckpoint& cp,std::string owner_resource_id,
    const audio::AudioBuffer& full_programme,std::uint64_t chain_revision)
{
    const auto view=full_programme.view();
    const auto start=view.absolute_range().begin().value();
    const auto frames=view.frame_count().value();
    if(owner_resource_id.empty() || frames<=0
        ||view.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
        ||cp.audio_format!=view.format()
        ||cp.frame_domain_id!=audio::FrameDomainId::OUTPUT_RATE
        ||(cp.next_input_frame &&
            (cp.next_input_frame->value()<start
            ||cp.next_input_frame->value()-start>frames)))
        return core::Result<BoundCoreCheckpoint>::failure(core::Error{
            core::ErrorCode::InvalidArgument,"Invalid full-resource checkpoint binding."});
    const auto sha=full_pcm_digest(full_programme);
    if(!sha)return core::Result<BoundCoreCheckpoint>::failure(*sha.error());
    return core::Result<BoundCoreCheckpoint>::success(BoundCoreCheckpoint{
        cp,std::move(owner_resource_id),*sha.value(),start,frames,
        view.format(),view.timebase().frame_domain_id(),chain_revision,
        cp.backend_identity});
}

[[nodiscard]] inline core::Status restore_bound_checkpoint(
    dsp::TruePeakLimiterModule& module,const BoundCoreCheckpoint& bound,
    const std::string& claimed_resource_id,const audio::AudioBuffer& full_programme,
    std::uint64_t claimed_revision)
{
    const auto view=full_programme.view();
    if(claimed_resource_id!=bound.owner_resource_id
        ||claimed_revision!=bound.chain_revision
        ||view.absolute_range().begin().value()!=bound.programme_origin
        ||view.frame_count().value()!=bound.programme_frames
        ||view.format()!=bound.resource_format
        ||view.timebase().frame_domain_id()!=bound.resource_domain
        ||bound.checkpoint.audio_format!=bound.resource_format
        ||bound.checkpoint.frame_domain_id!=bound.resource_domain
        ||bound.checkpoint.backend_identity!=bound.backend_identity)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidArgument,"INCOMPATIBLE_BOUND_CORE_RESOURCE"});
    const auto sha=full_pcm_digest(full_programme);
    if(!sha || *sha.value()!=bound.full_pcm_sha256)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidArgument,"INCOMPATIBLE_BOUND_CORE_RESOURCE"});
    return module.restore_runtime_checkpoint(bound.checkpoint);
}


[[nodiscard]] inline core::Error error(core::ErrorCode code,
    const char* category,const char* message)
{
    return core::Error{code,message,{{"category",category}}};
}

// AC-06 inactive-path reference harness only. NO M17 instance, NO detector,
// NO D placeholder/drain, NO verification trim, NO gain callback. The
// product renderer's bypass is not part of this Core-first entry.
[[nodiscard]] inline core::Result<audio::AudioBuffer> bypass_full_resource(
    const audio::AudioBuffer& source)
{
    const auto view=source.view();
    if(view.frame_count().value()<=0 ||
        view.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE)
        return core::Result<audio::AudioBuffer>::failure(error(
            core::ErrorCode::InvalidArgument,"INVALID_M17_BYPASS_RESOURCE",
            "Inactive M17 test harness requires full OUTPUT_RATE PCM."));
    auto copy=audio::AudioBuffer::create(
        view.format(),audio::FrameDomainId::OUTPUT_RATE,
        view.absolute_range().begin(),view.frame_count());
    if(!copy)return core::Result<audio::AudioBuffer>::failure(*copy.error());
    for(std::size_t ch=0;ch<view.format().channel_count();++ch) {
        const auto from=view.channel(ch);
        const auto into=copy.value()->mutable_view().channel(ch);
        if(!from || !into)
            return core::Result<audio::AudioBuffer>::failure(error(
                core::ErrorCode::InvalidArgument,"INVALID_M17_BYPASS_BUFFER",
                "Inactive M17 reference requires complete PCM channels."));
        // Memcpy is deliberately a pure bitwise payload copy, including
        // -0.0. No float arithmetic or postprocessing occurs.
        std::memcpy(into.value()->data(),from.value()->data(),
            from.value()->size()*sizeof(double));
    }
    return copy;
}

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
[[nodiscard]] inline core::Result<VerificationResult>
verify_full_resource(audio::AudioBuffer& resource,double ceiling_db_tp)
{
    using core::Result;
    if(!std::isfinite(ceiling_db_tp)
        || ceiling_db_tp< -12.0 || ceiling_db_tp>0.0)
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidArgument,"INVALID_M17_CEILING",
            "M17 test-only verifier requires a valid Ceiling."));
    // This Core-first verifier owns precisely one materialized OUTPUT_RATE
    // resource. Reject other domains rather than silently relabeling PCM.
    if(resource.view().timebase().frame_domain_id()!=
        audio::FrameDomainId::OUTPUT_RATE)
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidArgument,"INVALID_M17_VERIFY_DOMAIN",
            "Full-resource M17 verification requires OUTPUT_RATE."));
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
    // Transactional one-shot trim: build a separate full-resource binary64
    // candidate; let the frozen M16 meter verify that *candidate* before
    // publishing any of its PCM back to the caller. If measurement, shape or
    // numeric guards fail, every bit of the original buffer remains intact.
    const auto original=resource.view();
    auto candidate=audio::AudioBuffer::create(original.format(),
        audio::FrameDomainId::OUTPUT_RATE,
        original.absolute_range().begin(),original.frame_count());
    if(!candidate)return Result<VerificationResult>::failure(*candidate.error());
    std::array<std::span<double>,2> originals{},staged{};
    const auto channels=original.format().channel_count();
    for(std::size_t ch=0;ch<channels;++ch) {
        const auto src=original.channel(ch);
        const auto dst=resource.mutable_view().channel(ch);
        const auto temporary=candidate.value()->mutable_view().channel(ch);
        if(!src || !dst || !temporary)
            return Result<VerificationResult>::failure(error(
                core::ErrorCode::InvalidState,
                "LIMITER_VERIFICATION_NUMERIC_FAILURE",
                "Full-resource trim could not bind every channel."));
        originals[ch]=*dst.value();
        staged[ch]=*temporary.value();
        for(std::size_t i=0;i<src.value()->size();++i) {
            const double value=(*src.value())[i]*gain;
            if(!std::isfinite(value))
                return Result<VerificationResult>::failure(error(
                    core::ErrorCode::InvalidAudioSample,
                    "LIMITER_VERIFICATION_NUMERIC_FAILURE",
                    "Global trim produced nonfinite PCM."));
            staged[ch][i]=value;
        }
    }
    auto after=analysis::measure_true_peak_full_resource(*candidate.value());
    if(!after)return Result<VerificationResult>::failure(*after.error());
    if(after.value()->global_linear_peak()>ceiling)
        return Result<VerificationResult>::failure(error(
            core::ErrorCode::InvalidState,
            "LIMITER_VERIFICATION_NUMERIC_FAILURE",
            "One allowed trim still exceeds Ceiling under M16."));
    // Commit only after successful M16 full-resource remeasurement; never
    // publish a partially trimmed left lane ahead of a failing right lane.
    for(std::size_t ch=0;ch<channels;++ch)
        std::memcpy(originals[ch].data(),staged[ch].data(),
            staged[ch].size()*sizeof(double));
    result.after_peak=after.value()->global_linear_peak();
    result.global_trim_gain=gain;
    result.trim_used=true;
    result.status=after.value()->qualification_status();
    return Result<VerificationResult>::success(result);
}

} // namespace rgsml::tests::m17_verifier
