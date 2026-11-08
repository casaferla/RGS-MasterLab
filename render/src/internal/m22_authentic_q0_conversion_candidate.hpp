// M22 isolated REAL Phase-11 Q0 FIR source-conversion candidate.
//
// This joins existing authentic Source PCM and frozen-original Q0 convolution
// WITHOUT calling M17, exporting WAV, or certifying a complete Master.
// The Product's verified full-M17 owner, Q1-Q6 and release remain separate.
#pragma once
#include <rgsml/audio/phase11_final_src_candidate.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_geometry.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>

namespace rgsml::m22_private {
struct AuthenticQ0SourceConversionCandidate final {
    rgsml::audio::FullResourceOutputRateCandidate pre_terminal_pcm;
    // Returned by authenticated original Q0 algorithm, not caller input.
    std::string_view frozen_q0_sha256;
    bool used_frozen_q0{};
    // Source-rate conversion precedes manual Master and checked M17.
    [[nodiscard]] bool authorizes_release() const noexcept {return false;}
};

[[nodiscard]] inline rgsml::core::Result<AuthenticQ0SourceConversionCandidate>
materialize_authentic_q0_source_candidate(
    rgsml::audio::AudioBufferView complete_source,
    rgsml::core::SampleRate target_output_rate,
    rgsml::core::FrameCount maximum_output_block,
    std::span<const std::uint8_t> exact_frozen_original_fir_bytes,
    const rgsml::audio::FullResourceOutputRateCancel& cancelled={},
    const rgsml::audio::FullResourceOutputRateProgress& progress={})
{
    using R=rgsml::core::Result<AuthenticQ0SourceConversionCandidate>;
    const auto refuse=[](const char* message) {
        return R::failure(rgsml::core::Error{
            rgsml::core::ErrorCode::InvalidState,message});
    };
    const auto sourceRate=complete_source.format().sample_rate().value();
    const auto targetRate=target_output_rate.value();
    const auto g=std::gcd(sourceRate,targetRate);
    if(sourceRate<=0||targetRate<=0||g<=0
       ||complete_source.timebase().frame_domain_id()
            !=rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE
       ||complete_source.absolute_start_frame().value()!=0
       ||complete_source.frame_count().value()<=0)
        return refuse("M22_Q0_REQUIRES_COMPLETE_ZERO_ORIGIN_SOURCE_PCM");

    const bool identity=sourceRate==targetRate;
    const rgsml::audio::internal::Phase11FrozenOriginalFirPin* pin=nullptr;
    if(identity) {
        if(!exact_frozen_original_fir_bytes.empty())
            return refuse("M22_Q0_IDENTITY_MUST_NOT_CONSUME_FIR_ASSET");
    }else{
        const auto numerator=static_cast<std::uint32_t>(targetRate/g);
        const auto denominator=static_cast<std::uint32_t>(sourceRate/g);
        pin=rgsml::audio::internal::phase11_frozen_original_fir_pin(
            numerator,denominator);
        if(!pin
           ||!rgsml::audio::internal::phase11_frozen_shape(
                 numerator,denominator)
           ||exact_frozen_original_fir_bytes.empty())
            return refuse("M22_Q0_MISSING_REAL_FROZEN_SOURCE_ASSET");
    }
    // Existing Phase-11 parser authenticates the binary asset against
    // the COMPILED SHA pin; no manifest-supplied or self-declared hash.
    auto produced=
        rgsml::audio::materialize_phase11_scoped_q0_pinned_offline_candidate(
            complete_source,target_output_rate,maximum_output_block,
            exact_frozen_original_fir_bytes,cancelled,progress);
    if(!produced)return R::failure(*produced.error());

    const auto full=produced.value()->pcm.view();
    if(full.timebase().frame_domain_id()
            !=rgsml::audio::FrameDomainId::OUTPUT_RATE
       ||full.absolute_start_frame().value()!=0
       ||full.frame_count().value()<=0
       ||full.format().sample_rate()!=target_output_rate
       ||full.format().channel_layout()
            !=complete_source.format().channel_layout()
       ||produced.value()->source_frames!=complete_source.frame_count()
       ||produced.value()->used_src!=(!identity)
       ||(identity
          ? !produced.value()->kernel_sha256.empty()
          : produced.value()->kernel_sha256!=pin->sha256))
        return refuse("M22_Q0_ACTUAL_FULL_OUTPUT_OR_PIN_MISMATCH");

    return R::success(AuthenticQ0SourceConversionCandidate{
        std::move(*produced.value()),
        identity ? std::string_view{} : std::string_view{pin->sha256},
        !identity});
}
} // namespace rgsml::m22_private
