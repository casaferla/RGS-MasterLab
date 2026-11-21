// M22 private genuine full-M17 OUTPUT_RATE preview window, not a release.
// Reuse the real retained whole-manual / frozen Q0 / checked M17+M16 PCM:
// no resampling, no local M17 run, no new floating-point DSP, no dither.
#pragma once
#include "m22_real_verified_output_plan_candidate.hpp"
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
    const auto first=absolute_output_window.begin().value();
    const auto end=absolute_output_window.end().value();
    if(first<0 ||end<=first)
        return refuse("M22_PREVIEW_REQUIRES_EXACT_OUTPUT_FRAME_WINDOW");
    RealFullOutputPlanFailure plan_failure{};
    auto planned=observe_real_full_output_plan_candidate(
        full,Mode::PREVIEW,
        Window{static_cast<std::uint64_t>(first),
               static_cast<std::uint64_t>(end)},plan_failure);
    if(!planned)
        return refuse("M22_PREVIEW_REQUIRES_REAL_CHECKED_OUTPUT_PLAN");
    // An absolute output-frame crop must be inside the NATIVE checked M17
    // programme, not a SOURCE-rate estimate or preview-local FIR run.
    if(planned->output.absolute_output_first
            !=static_cast<std::uint64_t>(first)
       ||planned->output.absolute_output_end
            !=static_cast<std::uint64_t>(end)
       ||planned->output.required_output_true_eos
            !=static_cast<std::uint64_t>(
                planned->native.actual_master_frames))
        return refuse("M22_PREVIEW_PLAN_MUST_END_AT_REAL_MASTER_EOS");
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
            !=planned->native.final_master_hz)
        return refuse("M22_PREVIEW_OUTPUT_CROP_IDENTITY_MISMATCH");

    return R::success(VerifiedFullOutputWindowCandidate{
        std::move(planned->native),*selected.value()});
}
} // namespace rgsml::m22_private
