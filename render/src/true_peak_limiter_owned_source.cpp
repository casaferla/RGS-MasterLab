#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <numeric>
#include <string_view>
#include <utility>

namespace rgsml::render {
namespace {
[[nodiscard]] core::Error source_error(
    core::ErrorCode code, const char* category, const char* reason)
{
    return core::Error{code,reason,{{"category",category}}};
}

[[nodiscard]] bool m16_supported_rate(std::int64_t hz) noexcept
{
    return hz==44100 || hz==48000 || hz==88200
        || hz==96000 || hz==176400 || hz==192000;
}
} // namespace

core::Result<TruePeakLimiterOwnedSourceCandidate>
render_true_peak_limiter_owned_candidate_impl(
    std::shared_ptr<const RenderResult> manual,
    const dsp::ProcessingChain& manual_chain,
    core::FrameCount full_frames,
    std::uint64_t manual_revision,
    core::SampleRate output_rate,
    const dsp::ProcessingChain& terminal_chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount block,
    bool enable_unqualified_src,
    const TerminalRenderCancel& cancelled,
    TerminalRenderStageTimings* stage_timings,
    bool use_experimental_offline_accelerator)
{
    using Candidate = TruePeakLimiterOwnedSourceCandidate;
    // Whole-programme stage diagnostics are captured on the worker thread.
    // No per-frame clocks, logger calls or floating-point/audio mutations.
    if(stage_timings) {
        // Reset measured durations without losing the caller's observer.
        auto observer=std::move(stage_timings->progress);
        *stage_timings = {};
        stage_timings->progress=std::move(observer);
    }
    const auto report=[&](TerminalRenderProgressPhase phase,
                          std::int64_t done=0,std::int64_t total=0) {
        if(stage_timings && stage_timings->progress)
            stage_timings->progress({phase,done,total});
    };
    const auto cancelledResult=[] {
        return core::Result<Candidate>::failure(source_error(
            core::ErrorCode::InvalidState,"M17_RENDER_CANCELLED",
            "Stale terminal candidate was retired without publication."));
    };
    if (cancelled && cancelled()) return cancelledResult();
    if (!manual) {
        return core::Result<Candidate>::failure(source_error(
            core::ErrorCode::InvalidState,"M17_MANUAL_OWNER_MISSING",
            "The complete immutable manual realization owner is required."));
    }
    const auto view=manual->view();
    // A previewed loop/window is never the whole programme.
    if (manual->frame_domain_id()!=audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.timebase().frame_domain_id()
            !=audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.absolute_start_frame().value()!=0
        || manual->render_window()!=view.absolute_range()
        || full_frames.value()<=0
        || view.frame_count()!=full_frames) {
        return core::Result<Candidate>::failure(source_error(
            core::ErrorCode::InvalidArgument,"M17_MANUAL_FULL_RESOURCE_REQUIRED",
            "Terminal limiter may consume only a complete zero-origin manual realization."));
    }
    if (manual_chain.context().stage!=dsp::ProcessingStage::MASTER
        || manual_chain.context().segment!=dsp::ChainSegment::MANUAL
        || manual_chain.revision()!=manual_revision
        || manual->chain_revision()!=manual_revision) {
        return core::Result<Candidate>::failure(source_error(
            core::ErrorCode::InvalidState,"M17_MANUAL_REVISION_STALE",
            "Manual chain revision changed since the candidate was captured."));
    }
    // A different module order/identity cannot be substituted merely
    // because two chain histories happen to have the same revision.
    const auto instances=manual_chain.instances();
    const auto& signatures=manual->signatures();
    if(instances.size()!=signatures.size()) {
        return core::Result<Candidate>::failure(source_error(
            core::ErrorCode::InvalidState,"M17_MANUAL_SIGNATURE_STALE",
            "The manual render does not match the expected DSP chain."));
    }
    for(std::size_t i=0;i<instances.size();++i) {
        if(instances[i].instance_id()!=signatures[i].instance_id
           || instances[i].module_type_id()!=std::string_view{signatures[i].type_id}) {
            return core::Result<Candidate>::failure(source_error(
                core::ErrorCode::InvalidState,"M17_MANUAL_SIGNATURE_STALE",
                "The manual render module identity/order does not match."));
        }
    }
    const auto in_hz=view.format().sample_rate().value();
    if (!m16_supported_rate(in_hz)
        || !m16_supported_rate(output_rate.value())) {
        return core::Result<Candidate>::failure(source_error(
            core::ErrorCode::UnsupportedOperation,"M17_SOURCE_RATE_UNSUPPORTED",
            "Source/output rate is unsupported for the protected M16 verification."));
    }
    if (!enable_unqualified_src && in_hz!=output_rate.value()) {
        return core::Result<Candidate>::failure(source_error(
            core::ErrorCode::UnsupportedOperation,"M17_OUTPUT_RATE_SRC_REQUIRED",
            "The strict same-rate handoff cannot silently perform SRC."));
    }
    if (block.value()<=0) return core::Result<Candidate>::failure(source_error(
        core::ErrorCode::InvalidArgument,"M17_INVALID_BLOCK",
        "Terminal renderer needs a positive block size."));
    // Dedicated private full-resource candidate conversion owns the
    // entire OUTPUT_RATE buffer before M17/M16 verification. No dynamic
    // resampling takes place in the playback callback.
    auto boundedBlock=core::FrameCount::create(std::min(
        block.value(),audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames));
    if(!boundedBlock)return core::Result<Candidate>::failure(*boundedBlock.error());
    report(TerminalRenderProgressPhase::SRC);
    const auto src_started=std::chrono::steady_clock::now();
    auto output=audio::materialize_full_resource_output_rate_candidate(
        view,output_rate,*boundedBlock.value());
    if(stage_timings)
        stage_timings->src_us=std::chrono::duration_cast<
            std::chrono::microseconds>(
                std::chrono::steady_clock::now()-src_started).count();
    if(!output)return core::Result<Candidate>::failure(*output.error());
    if(cancelled && cancelled()) return cancelledResult();
    report(TerminalRenderProgressPhase::M17_CORE,0,
           output.value()->pcm.view().frame_count().value());
    auto candidate=render_true_peak_limiter_bound_terminal_candidate(
        output.value()->pcm.view(),terminal_chain,bindings,registry,block,
        cancelled,stage_timings,use_experimental_offline_accelerator);
    if(!candidate)
        return core::Result<Candidate>::failure(*candidate.error());
    if(cancelled && cancelled()) return cancelledResult();
    report(TerminalRenderProgressPhase::FINISHING);
    // All intermediate PCM and final verification must succeed BEFORE
    // providing a candidate; the source owner remains pinned.
    return core::Result<Candidate>::success({
        std::move(manual),std::move(*candidate.value()),
        output_rate,full_frames,manual_revision,
        output.value()->used_src,output.value()->kernel_sha256});
}

core::Result<TruePeakLimiterOwnedSourceCandidate>
render_true_peak_limiter_owned_same_rate_candidate(
    std::shared_ptr<const RenderResult> manual,
    const dsp::ProcessingChain& manual_chain,
    core::FrameCount full_frames,
    std::uint64_t manual_revision,
    core::SampleRate output_rate,
    const dsp::ProcessingChain& terminal_chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount block,
    const TerminalRenderCancel& cancelled,
    TerminalRenderStageTimings* stage_timings,
    bool use_experimental_offline_accelerator)
{
    return render_true_peak_limiter_owned_candidate_impl(
        std::move(manual),manual_chain,full_frames,manual_revision,
        output_rate,terminal_chain,bindings,registry,block,false,cancelled,
        stage_timings,use_experimental_offline_accelerator);
}

core::Result<TruePeakLimiterOwnedSourceCandidate>
render_true_peak_limiter_owned_output_rate_candidate(
    std::shared_ptr<const RenderResult> manual,
    const dsp::ProcessingChain& manual_chain,
    core::FrameCount full_frames,
    std::uint64_t manual_revision,
    core::SampleRate output_rate,
    const dsp::ProcessingChain& terminal_chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount block,
    const TerminalRenderCancel& cancelled,
    TerminalRenderStageTimings* stage_timings,
    bool use_experimental_offline_accelerator)
{
    return render_true_peak_limiter_owned_candidate_impl(
        std::move(manual),manual_chain,full_frames,manual_revision,
        output_rate,terminal_chain,bindings,registry,block,true,cancelled,
        stage_timings,use_experimental_offline_accelerator);
}

core::Result<FinalMasterOutputRatePlan>
plan_final_master_output_rate(
    core::SampleRate source_rate,
    std::optional<core::SampleRate> requested_target_rate)
{
    const auto target_rate=requested_target_rate.value_or(source_rate);
    const auto fin=source_rate.value();
    const auto fout=target_rate.value();
    if(!m16_supported_rate(fin)||!m16_supported_rate(fout))
        return core::Result<FinalMasterOutputRatePlan>::failure(source_error(
            core::ErrorCode::UnsupportedOperation,
            "M17_FINAL_MASTER_TARGET_RATE_UNSUPPORTED",
            "Final master SOURCE and TARGET must use one of the six accepted RGS rates."));
    const auto common=std::gcd(fin,fout);
    if(common<=0)
        return core::Result<FinalMasterOutputRatePlan>::failure(source_error(
            core::ErrorCode::InvalidArgument,
            "M17_FINAL_MASTER_RATE_PLAN_INVALID",
            "Final master rate ratio cannot be represented."));
    return core::Result<FinalMasterOutputRatePlan>::success(
        FinalMasterOutputRatePlan{
            source_rate,target_rate,
            static_cast<std::uint32_t>(fout/common),
            static_cast<std::uint32_t>(fin/common),fin!=fout});
}

core::Result<TruePeakLimiterFinalMasterCandidate>
render_true_peak_limiter_owned_final_master_candidate(
    std::shared_ptr<const RenderResult> manual,
    const dsp::ProcessingChain& manual_chain,
    core::FrameCount full_frames,
    std::uint64_t manual_revision,
    std::optional<core::SampleRate> requested_master_rate,
    const dsp::ProcessingChain& terminal_chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleRegistry& registry,
    core::FrameCount block)
{
    using Final = TruePeakLimiterFinalMasterCandidate;
    if(!manual)
        return core::Result<Final>::failure(source_error(
            core::ErrorCode::InvalidState,"M17_FINAL_MASTER_NO_OWNER",
            "Final master rate requires a complete immutable manual owner."));
    // Final MASTER rate is explicitly requested or defaults to SOURCE.
    // Never probe or infer the Windows device rate at this boundary.
    auto plan=plan_final_master_output_rate(
        manual->view().format().sample_rate(),requested_master_rate);
    if(!plan)return core::Result<Final>::failure(*plan.error());
    const auto masterRate=plan.value()->target_rate;
    // Planning does not bypass the separate Phase-11 Final Output SRC
    // implementation/qualification gate: unavailable pairs fail closed.
    auto rendered=render_true_peak_limiter_owned_output_rate_candidate(
        std::move(manual),manual_chain,full_frames,manual_revision,
        masterRate,terminal_chain,bindings,registry,block);
    if(!rendered)
        return core::Result<Final>::failure(*rendered.error());
    Final result{std::move(*rendered.value()),masterRate};
    auto verified=validate_final_master_encoding_rate(result,masterRate);
    if(!verified)
        return core::Result<Final>::failure(*verified.error());
    return core::Result<Final>::success(std::move(result));
}

core::Status validate_final_master_encoding_rate(
    const TruePeakLimiterFinalMasterCandidate& candidate,
    core::SampleRate encoder_rate)
{
    const auto& output=candidate.output;
    const auto& terminal=output.terminal.checked;
    const auto pcm=terminal.terminal.compensated_pcm.view();
    if(!output.manual_owner
       ||encoder_rate!=candidate.master_sample_rate
       ||output.output_rate!=candidate.master_sample_rate
       ||pcm.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||pcm.format().sample_rate()!=candidate.master_sample_rate
       ||pcm.absolute_start_frame().value()!=0
       ||pcm.frame_count().value()<=0
       ||output.programme_frames!=output.manual_owner->view().frame_count()
       ||(!terminal.terminal.bypassed && !terminal.final_check))
        return core::Status::failure(source_error(
            core::ErrorCode::InvalidState,"M17_FINAL_MASTER_ENCODER_RATE_MISMATCH",
            "Final encoder must accept the exact M17-processed and verified master rate, without a later SRC."));
    return core::Status::success();
}

} // namespace rgsml::render
