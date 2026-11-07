#pragma once
// L1-M22: genuine read-only same-rate Preview crop of the exact current
// FULL Master already computed by the canonical manual DSP -> M17
// pipeline. No second renderer, limiter, SRC, dither or file publisher.
// The existing M18 Product epoch, current SOURCE/chain/MASTER rate,
// accepted M17 final M16 check and unrevoked staged owner are mandatory.
#include "m18_product_complete_binding.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <memory>

namespace rgsml::app::internal {

[[nodiscard]] inline core::Result<audio::AudioBufferView>
m22_verified_master_preview_crop(
    const M18ProductCompleteBinding::Controllers& product,
    const M18ProductCompleteBinding::Fanout& fanout,
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
        expectedVerifiedFullMaster,
    core::FrameRange exactOutputFrameWindow)
{
    using R=core::Result<audio::AudioBufferView>;
    const auto refuse=[](const char* reason) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,reason});
    };
    // Reuses EXACT M18 Product consent eligibility but does not actually
    // mint an export ticket. A parameter-only edit, Project/SOURCE
    // replacement, terminal change or held worker commit must fail.
    if(!M18ProductCompleteBinding::can_offer_verified_master(
           product,fanout)
       ||!expectedVerifiedFullMaster
       ||product.candidates.staged()!=expectedVerifiedFullMaster)
        return refuse("M22_PREVIEW_REQUIRES_CURRENT_VERIFIED_FULL_M17_OWNER");
    const auto& rendered=expectedVerifiedFullMaster->rendered;
    const auto& checked=rendered.terminal.checked;
    if(!checked.final_check ||checked.terminal.bypassed
       ||rendered.manual_chain_revision
            !=product.upstream.chain.chain().revision()
       ||rendered.output_rate.value()
            !=product.upstream.rate.master_rate_hz())
        return refuse("M22_PREVIEW_M17_CHAIN_RATE_OR_CHECK_NOT_CURRENT");

    const auto full=checked.terminal.compensated_pcm.view();
    // Full M17 must have already been rendered from canonical origin
    // to actual EOS. Never run M17 on only the selected interval:
    // that would reset compressor history and omit global M16 trim.
    if(full.timebase().frame_domain_id()
            !=audio::FrameDomainId::OUTPUT_RATE
       ||full.absolute_start_frame().value()!=0
       ||full.format().sample_rate()!=rendered.output_rate
       ||full.frame_count()!=rendered.programme_frames
       ||exactOutputFrameWindow.begin().value()<0
       ||exactOutputFrameWindow.begin()
            >=exactOutputFrameWindow.end()
       ||exactOutputFrameWindow.end().value()
            >full.frame_count().value())
        return refuse("M22_PREVIEW_REQUIRES_EXACT_FULL_OUTPUT_RATE_TIMELINE");
    const auto frames=core::FrameCount::create(
        exactOutputFrameWindow.end().value()-
        exactOutputFrameWindow.begin().value());
    if(!frames)return R::failure(*frames.error());
    // AudioBufferView.subview is a true zero-copy crop preserving actual
    // binary64 samples and the M17's complete prior state history.
    // PCM24 Philox must still use ABSOLUTE output frame addressing at
    // subsequent M18 encoding: there is no local reseed here.
    return full.subview(exactOutputFrameWindow.begin(),*frames.value());
}

} // namespace rgsml::app::internal
