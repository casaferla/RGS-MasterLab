#include "terminal_candidate_gate.hpp"

#include <rgsml/core/error.hpp>

#include <limits>
#include <utility>

namespace rgsml::app {
namespace {
[[nodiscard]] core::Error reject(const char* category, const char* reason)
{
    return core::Error{core::ErrorCode::InvalidState,
        reason,{{"category",category}}};
}

[[nodiscard]] bool complete_manual_pair(
    const render::RenderResult& prepared,
    const render::RenderResult& manual) noexcept
{
    const auto p=prepared.view();
    const auto m=manual.view();
    return prepared.frame_domain_id()==audio::FrameDomainId::SOURCE_PROCESSING_RATE
        && manual.frame_domain_id()==audio::FrameDomainId::SOURCE_PROCESSING_RATE
        && p.absolute_start_frame().value()==0
        && m.absolute_start_frame().value()==0
        && prepared.render_window()==p.absolute_range()
        && manual.render_window()==m.absolute_range()
        && p.frame_count().value()>0
        && p.frame_count()==m.frame_count()
        && p.format()==m.format();
}
} // namespace

core::Result<TerminalCandidateGate::Capture>
TerminalCandidateGate::capture(
    std::shared_ptr<const render::RenderResult> prepared,
    std::shared_ptr<const render::RenderResult> manual,
    std::optional<core::RealizationId> manual_id,
    const dsp::ProcessingChain& manual_chain,
    const TerminalLimiterState& terminal,
    core::SampleRate rate)
{
    if(!prepared||!manual||!manual_id||manual_id->value==0
       ||manual_chain.context().stage!=dsp::ProcessingStage::MASTER
       ||manual_chain.context().segment!=dsp::ChainSegment::MANUAL
       ||manual_chain.revision()!=manual->chain_revision()
       ||!complete_manual_pair(*prepared,*manual)
       ||terminal.chain().context().stage!=dsp::ProcessingStage::MASTER
       ||terminal.chain().context().segment!=dsp::ChainSegment::TERMINAL
       ||terminal.chain().instances().size()!=1
       ||terminal.chain().instances()[0].instance_id()!=terminal.instance_id()
       ||terminal.chain().instances()[0].module_type_id()!="rgsml.dsp.true-peak-limiter") {
        // Even a failed replacement attempt must withdraw an older staged
        // output, never leave seemingly-current PCM available for promotion.
        invalidate();
        return core::Result<Capture>::failure(reject(
            "M17_CAPTURE_INVALID_AUTHORITY",
            "M17 cannot capture an incomplete or unowned manual/terminal realization."));
    }
    if(ticket_>=std::numeric_limits<std::uint64_t>::max()-1) {
        staged_.reset();
        return core::Result<Capture>::failure(reject(
            "M17_CAPTURE_TICKET_EXHAUSTED",
            "Terminal candidate admission sequence exhausted."));
    }
    // Evaluate non-owning metadata BEFORE moving the owner pointers.
    // C++ aggregate initializer evaluation is left-to-right: using
    // manual->view() after std::move(manual) would dereference null.
    const auto originalFrameCount=manual->view().frame_count();
    ++ticket_;
    staged_.reset();
    return core::Result<Capture>::success(Capture{
        std::move(prepared),std::move(manual),*manual_id,ticket_,
        manual_chain.revision(),terminal.chain().revision(),
        terminal.parameter_revision(),terminal.instance_id(),
        terminal.bypassed(),terminal.parameters().ceiling_db_tp(),
        originalFrameCount,rate
    });
}

core::Status TerminalCandidateGate::accept(
    const Capture& job,
    render::TruePeakLimiterOwnedSourceCandidate candidate,
    std::shared_ptr<const render::RenderResult> prepared,
    std::shared_ptr<const render::RenderResult> manual,
    std::optional<core::RealizationId> manual_id,
    const dsp::ProcessingChain& manual_chain,
    const TerminalLimiterState& terminal)
{
    if(job.ticket!=ticket_)
        return core::Status::failure(reject(
            "M17_STALE_TICKET","Terminal candidate has been superseded."));
    const auto& check=candidate.terminal.checked;
    const auto& pcm=check.terminal.compensated_pcm;
    const auto pcmView=pcm.view();
    const bool isStale =
        !prepared||!manual||!manual_id
        ||prepared!=job.prepared_owner
        ||manual!=job.manual_owner
        ||*manual_id!=job.manual_realization_id
        ||manual_chain.context().stage!=dsp::ProcessingStage::MASTER
        ||manual_chain.context().segment!=dsp::ChainSegment::MANUAL
        ||manual_chain.revision()!=job.manual_chain_revision
        ||manual->chain_revision()!=job.manual_chain_revision
        ||!complete_manual_pair(*prepared,*manual)
        ||terminal.chain().revision()!=job.terminal_chain_revision
        ||terminal.parameter_revision()!=job.terminal_parameter_revision
        ||terminal.instance_id()!=job.terminal_instance_id
        ||terminal.parameters().ceiling_db_tp()!=job.ceiling_db_tp
        ||terminal.bypassed()!=job.terminal_bypassed
        ||candidate.manual_owner!=manual
        ||candidate.manual_chain_revision!=job.manual_chain_revision
        ||candidate.terminal.terminal_chain_revision!=job.terminal_chain_revision
        ||candidate.terminal.limiter_instance_id!=job.terminal_instance_id
        ||candidate.programme_frames!=job.full_source_frames
        ||candidate.output_rate!=job.output_rate
        ||candidate.used_candidate_src
            !=(manual->view().format().sample_rate()!=job.output_rate)
        ||(candidate.used_candidate_src
            &&candidate.src_kernel_sha256.empty())
        ||check.terminal.bypassed!=job.terminal_bypassed
        ||pcmView.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
        ||pcmView.format().sample_rate()!=job.output_rate
        ||pcmView.absolute_start_frame().value()!=0
        ||pcmView.frame_count().value()<=0
        ||(job.terminal_bypassed
            ? (check.final_check.has_value()
               ||!check.terminal.applied_gain_frames.empty())
            : (!check.final_check.has_value()
               ||check.terminal.applied_gain_frames.size()
                    !=static_cast<std::size_t>(pcmView.frame_count().value())));
    if(isStale) {
        invalidate();
        return core::Status::failure(reject(
            "M17_STALE_OR_INVALID_TERMINAL",
            "M17 terminal candidate no longer matches the accepted source and DSP revisions."));
    }
    staged_=std::make_shared<const StagedCandidate>(StagedCandidate{
        std::move(candidate),job.manual_realization_id,job.prepared_owner});
    // A late duplicate completion with the same ticket must never replace
    // the accepted candidate or make its provenance appear new.
    if(ticket_<std::numeric_limits<std::uint64_t>::max())++ticket_;
    return core::Status::success();
}

void TerminalCandidateGate::invalidate() noexcept
{
    staged_.reset();
    if(ticket_<std::numeric_limits<std::uint64_t>::max())++ticket_;
}
} // namespace rgsml::app
