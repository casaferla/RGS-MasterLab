// M22 private genuine full-M17 OUTPUT_RATE preview window, not a release.
// Reuse the real retained whole-manual / frozen Q0 / checked M17+M16 PCM:
// no resampling, no local M17 run, no new floating-point DSP, no dither.
#pragma once
#include "m22_full_chain_witness_candidate.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <cstdint>
#include <utility>

namespace rgsml::m22_private {

struct VerifiedFullOutputWindowCandidate final {
    FullChainWitness full_master_witness;
    rgsml::audio::AudioBufferView exact_output_crop;
    [[nodiscard]] bool authorizes_release() const noexcept {return false;}
};

// The caller must RETAIN the exact immutable TruePeakLimiterOwnedSourceCandidate
// (M18/M22 Product owns it as a staged shared owner). The borrowed view alone
// is never an independent playback/export ticket. Full-chain provenance is
// observed from actual native M17 PCM and original SHA-pinned Q0, not a
// predicted Fs ratio or the SOURCE programme_frames length.
[[nodiscard]] inline rgsml::core::Result<VerifiedFullOutputWindowCandidate>
observe_verified_full_output_window_candidate(
    const rgsml::render::TruePeakLimiterOwnedSourceCandidate& full,
    rgsml::core::FrameRange absolute_output_window)
{
    using R=rgsml::core::Result<VerifiedFullOutputWindowCandidate>;
    const auto refuse=[](const char* reason) {
        return R::failure(rgsml::core::Error{
            rgsml::core::ErrorCode::InvalidState,reason});
    };
    FullChainWitnessFailure witness_failure{};
    auto witnessed=observe_real_complete_chain_candidate(full,witness_failure);
    if(!witnessed)
        return refuse("M22_PREVIEW_REQUIRES_REAL_FULL_MANUAL_Q0_M17_M16");

    const auto first=absolute_output_window.begin().value();
    const auto end=absolute_output_window.end().value();
    // The absolute region is measured in OUTPUT_RATE frames. In a 44.1->48
    // render, SOURCE frame_count is NOT the audible Master's frame_count.
    if(first<0 || end<=first || end>witnessed->actual_master_frames)
        return refuse("M22_PREVIEW_REQUIRES_EXACT_OUTPUT_FRAME_WINDOW");

    const auto count=rgsml::core::FrameCount::create(end-first);
    if(!count)return R::failure(*count.error());
    const auto full_pcm=full.terminal.checked.terminal.compensated_pcm.view();
    const auto selected=full_pcm.subview(absolute_output_window.begin(),
                                        *count.value());
    if(!selected)return R::failure(*selected.error());
    if(selected.value()->absolute_start_frame()!=absolute_output_window.begin()
       ||selected.value()->absolute_end_frame()!=absolute_output_window.end()
       ||selected.value()->timebase().frame_domain_id()
            !=rgsml::audio::FrameDomainId::OUTPUT_RATE
       ||selected.value()->format().sample_rate().value()
            !=witnessed->final_master_hz)
        return refuse("M22_PREVIEW_OUTPUT_CROP_IDENTITY_MISMATCH");

    return R::success(VerifiedFullOutputWindowCandidate{
        std::move(*witnessed),*selected.value()});
}
} // namespace rgsml::m22_private
