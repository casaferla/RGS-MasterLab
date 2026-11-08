#pragma once
// L1-M22: genuine read-only same-rate Preview crop of the exact current
// FULL Master already computed by the canonical manual DSP -> M17
// pipeline. No second renderer, limiter, SRC, dither or file publisher.
// The existing M18 Product epoch, current SOURCE/chain/MASTER rate,
// accepted M17 final M16 check and unrevoked staged owner are mandatory.
#include "m18_product_complete_binding.hpp"
#include "../../../render/src/internal/m22_verified_full_output_window_candidate.hpp"
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

    // The source full-programme count and final Master output count are
    // different after an authentic nonidentity Q0 conversion. The existing
    // check full.frame_count()==rendered.programme_frames was correct only
    // for identity Fs; it silently disabled cross-rate Product Preview.
    // Reobserve real native manual/Q0/M17/M16 evidence before any subview.
    const auto native=m22_private::observe_verified_full_output_window_candidate(
        rendered,exactOutputFrameWindow);
    if(!native)
        return refuse("M22_PREVIEW_REQUIRES_NATIVE_VERIFIED_OUTPUT_RATE_WINDOW");
    const auto& witnessed=native.value()->full_master_witness;
    const auto& currentSource=product.upstream.source;
    if(witnessed.manual_chain_revision
            !=product.upstream.chain.chain().revision()
       ||witnessed.terminal_chain_revision
            !=product.upstream.terminal.chain().revision()
       ||witnessed.complete_source_frames!=currentSource.frame_count()
       ||witnessed.manual_source_hz!=currentSource.sample_rate_hz()
       ||witnessed.channels
            !=static_cast<std::size_t>(currentSource.channel_count())
       ||witnessed.final_master_hz
            !=product.upstream.rate.master_rate_hz())
        return refuse("M22_PREVIEW_REAL_SOURCE_CHAIN_OR_OUTPUT_RATE_STALE");

    // This view is borrowed from the SAME staged, immutable verified M17
    // PCM. M22ProductPreviewAdmission::Capture retains that shared owner
    // and rechecks the exact Product epoch before queued use.
    return R::success(native.value()->exact_output_crop);
}

} // namespace rgsml::app::internal
