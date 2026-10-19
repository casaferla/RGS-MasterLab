#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
    bool enable_unqualified_src)
{
    using Candidate = TruePeakLimiterOwnedSourceCandidate;
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
    auto output=audio::materialize_full_resource_output_rate_candidate(
        view,output_rate,*boundedBlock.value());
    if(!output)return core::Result<Candidate>::failure(*output.error());
    auto candidate=render_true_peak_limiter_bound_terminal_candidate(
        output.value()->pcm.view(),terminal_chain,bindings,registry,block);
    if(!candidate)
        return core::Result<Candidate>::failure(*candidate.error());
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
    core::FrameCount block)
{
    return render_true_peak_limiter_owned_candidate_impl(
        std::move(manual),manual_chain,full_frames,manual_revision,
        output_rate,terminal_chain,bindings,registry,block,false);
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
    core::FrameCount block)
{
    return render_true_peak_limiter_owned_candidate_impl(
        std::move(manual),manual_chain,full_frames,manual_revision,
        output_rate,terminal_chain,bindings,registry,block,true);
}
} // namespace rgsml::render
