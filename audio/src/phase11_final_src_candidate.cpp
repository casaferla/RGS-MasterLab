#include <rgsml/audio/phase11_final_src_candidate.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_geometry.hpp>
#include <rgsml/audio/internal/phase11_rgsmsr1_candidate_asset.hpp>
#include <rgsml/audio/internal/phase11_strict_scalar_reference.hpp>
#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>

namespace rgsml::audio {
namespace {
[[nodiscard]] core::Result<FullResourceOutputRateCandidate>
refuse(const char* reason,core::ErrorCode code=core::ErrorCode::InvalidArgument)
{
    return core::Result<FullResourceOutputRateCandidate>::failure(
        core::Error{code,reason,
                    {{"category","PHASE11_PINNED_FINAL_SRC_CANDIDATE_NOT_RELEASED"}}});
}
[[nodiscard]] bool supported(std::int64_t hz) noexcept
{
    for(const auto r:{44100,48000,88200,96000,176400,192000})
        if(hz==r)return true;
    return false;
}
} // namespace

core::Result<FullResourceOutputRateCandidate>
materialize_phase11_pinned_final_src_candidate(
    AudioBufferView input,core::SampleRate outputRate,core::FrameCount block,
    std::span<const std::uint8_t> asset,std::string_view sha,
    const FullResourceOutputRateCancel& cancelled)
{
    const auto stopped=[&] {return cancelled&&cancelled();};
    if(stopped())return refuse("Phase11 candidate conversion cancelled",
                               core::ErrorCode::InvalidState);
    const auto fin=input.format().sample_rate().value();
    const auto fout=outputRate.value();
    const auto n=input.frame_count().value();
    if(input.timebase().frame_domain_id()!=FrameDomainId::SOURCE_PROCESSING_RATE
       ||input.absolute_start_frame()!=core::FrameIndex{0}
       ||n<=0 ||block.value()<=0||block.value()>4096
       ||(input.format().channel_layout()!=ChannelLayout::MONO_C
          &&input.format().channel_layout()!=ChannelLayout::STEREO_LR)
       ||!supported(fin)||!supported(fout))
        return refuse("Phase11 candidate requires complete SOURCE PCM, six-rate mono/stereo and bounded block");
    if(fin==fout) {
        if(!asset.empty()||!sha.empty())
            return refuse("Phase11 identity refuses external FIR and hash");
        // Reuse the existing exact-bit identity path; no second DSP engine.
        return materialize_full_resource_output_rate_candidate(
            input,outputRate,block,cancelled);
    }
    const auto g=std::gcd(fin,fout);
    const auto L=static_cast<std::uint32_t>(fout/g);
    const auto M=static_cast<std::uint32_t>(fin/g);
    const auto shape=internal::phase11_frozen_shape(L,M);
    if(!shape)
        return refuse("Phase11 selected ratio has no frozen Q0 geometry",
                      core::ErrorCode::UnsupportedOperation);
    auto parsed=internal::parse_phase11_rgsmsr1_pinned_candidate_bits(
        asset,L,M,shape->T,sha);
    if(!parsed)return core::Result<FullResourceOutputRateCandidate>::failure(
        *parsed.error());
    // Preserve the full original source as the reflection context; no
    // arbitrary crop/source-window interpretation is allowed.
    const auto whole=n/fin;
    const auto remainder=n%fin;
    const auto tail=(remainder*fout+fin-1)/fin;
    if(whole>(std::numeric_limits<std::int64_t>::max()-tail)/fout)
        return refuse("Phase11 output sample count overflow",
                      core::ErrorCode::IntegerOverflow);
    const auto total=whole*fout+tail;
    auto frames=core::FrameCount::create(total);
    if(!frames)return core::Result<FullResourceOutputRateCandidate>::failure(
        *frames.error());
    auto format=AudioFormat::create(outputRate,input.format().channel_layout());
    if(!format)return core::Result<FullResourceOutputRateCandidate>::failure(
        *format.error());
    // Validate source finiteness before ANY output publication.
    for(std::size_t ch=0;ch<input.format().channel_count();++ch) {
        const auto src=input.channel(ch);
        if(!src)return core::Result<FullResourceOutputRateCandidate>::failure(
            *src.error());
        for(const auto value:*src.value())
            if(!std::isfinite(value))
                return refuse("Phase11 source PCM contains nonfinite sample",
                              core::ErrorCode::InvalidAudioSample);
    }
    auto output=AudioBuffer::create(*format.value(),FrameDomainId::OUTPUT_RATE,
                                    core::FrameIndex{0},*frames.value());
    if(!output)return core::Result<FullResourceOutputRateCandidate>::failure(
        *output.error());
    for(std::size_t ch=0;ch<input.format().channel_count();++ch) {
        if(stopped())return refuse("Phase11 SRC cancelled before channel",
                                   core::ErrorCode::InvalidState);
        const auto src=input.channel(ch);
        const auto dst=output.value()->mutable_view().channel(ch);
        if(!src||!dst)return refuse("Phase11 channel allocation/read failed",
                                   core::ErrorCode::InvalidState);
        const auto status=internal::render_phase11_strict_scalar_reference(
            *src.value(),*dst.value(),0,L,M,*parsed.value(),cancelled);
        if(!status)return core::Result<FullResourceOutputRateCandidate>::failure(
            *status.error());
    }
    if(stopped())return refuse("Phase11 SRC cancelled after conversion",
                               core::ErrorCode::InvalidState);
    // This candidate carries NO release hash or production admission.
    // Hash verification above proves input-byte identity only.
    return core::Result<FullResourceOutputRateCandidate>::success({
        std::move(*output.value()),input.format().sample_rate(),outputRate,
        input.frame_count(),true,{}});
}
} // namespace rgsml::audio
