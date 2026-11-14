#pragma once
// M18-only optional user-authorized delivery micro-trim. This scales
// bounded OUTPUT_RATE chunks in private scratch storage, never the immutable
// M17 Master, SOURCE, terminal DSP, or original candidate owner.
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cmath>
#include <cstddef>

namespace rgsml::platform::windows::internal {

// Fixed -0.010 dB, explicitly selected ONLY after a proved near-Ceiling
// decoded-WAV failure. No automatic gain changes in the normal export.
inline constexpr double kM18ExplicitMicroTrimDb = 0.010;
inline constexpr double kM18ExplicitMicroTrimGain =
    0.9988493699365052; // 10^(-0.010/20), binary64

[[nodiscard]] inline core::Result<audio::AudioBuffer>
m18_micro_trim_chunk(audio::AudioBufferView original)
{
    using R=core::Result<audio::AudioBuffer>;
    if(original.timebase().frame_domain_id()!=
           audio::FrameDomainId::OUTPUT_RATE)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "M18_MICRO_TRIM_REQUIRES_VERIFIED_OUTPUT_RATE_PCM"});
    auto scratch=audio::AudioBuffer::create(original.format(),
        audio::FrameDomainId::OUTPUT_RATE,
        original.absolute_start_frame(),original.frame_count());
    if(!scratch)return scratch;
    auto out=scratch.value()->mutable_view();
    for(std::size_t ch=0;ch<original.format().channel_count();++ch){
        const auto src=original.channel(ch);
        const auto dst=out.channel(ch);
        if(!src || !dst)
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "M18_MICRO_TRIM_CHANNEL_VIEW_INVALID"});
        for(std::size_t i=0;i<src.value()->size();++i){
            const double sample=(*src.value())[i];
            if(!std::isfinite(sample))
                return R::failure(core::Error{core::ErrorCode::InvalidAudioSample,
                    "M18_MICRO_TRIM_NONFINITE_SOURCE"});
            (*dst.value())[i]=sample*kM18ExplicitMicroTrimGain;
        }
    }
    return scratch;
}

} // namespace rgsml::platform::windows::internal
