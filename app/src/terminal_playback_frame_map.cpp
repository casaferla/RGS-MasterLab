#include "terminal_playback_frame_map.hpp"

#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::app {
namespace {
template<class T>
[[nodiscard]] core::Result<T> invalid(
    core::ErrorCode code, const char* category, const char* message)
{
    return core::Result<T>::failure(
        core::Error{code,message,{{"category",category}}});
}
} // namespace

core::Result<TerminalPlaybackFrameMap> TerminalPlaybackFrameMap::create(
    const TerminalCandidateGate::StagedCandidate& accepted)
{
    using Map=TerminalPlaybackFrameMap;
    const auto& output=accepted.rendered.terminal.checked.terminal
        .compensated_pcm.view();
    const auto& provenance=accepted.rendered;
    if(!accepted.prepared_owner || !provenance.manual_owner
       ||accepted.original_manual_realization_id.value==0
       ||provenance.manual_owner->frame_domain_id()
           !=audio::FrameDomainId::SOURCE_PROCESSING_RATE
       ||output.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||provenance.manual_owner->view().absolute_start_frame().value()!=0
       ||output.absolute_start_frame().value()!=0
       ||provenance.programme_frames.value()<=0
       ||provenance.programme_frames!=provenance.manual_owner->view().frame_count()
       ||provenance.output_rate!=output.format().sample_rate()
       ||provenance.manual_owner->view().format().channel_layout()
           !=output.format().channel_layout()) {
        return invalid<Map>(core::ErrorCode::InvalidState,
            "M17_FRAME_MAP_UNOWNED_SOURCE",
            "Terminal frame map requires an accepted complete origin with output-rate PCM.");
    }
    const auto sourceRate=provenance.manual_owner->view().format().sample_rate();
    const auto outputRate=provenance.output_rate;
    const auto sourceFrames=provenance.programme_frames;
    const auto outputFrames=output.frame_count();
    const bool converted=sourceRate!=outputRate;
    if(converted!=provenance.used_candidate_src)
        return invalid<Map>(core::ErrorCode::InvalidState,
            "M17_FRAME_MAP_SRC_MISMATCH",
            "Terminal source/output frequency provenance is inconsistent.");
    if(!converted) {
        if(sourceFrames!=outputFrames
           ||!provenance.src_kernel_sha256.empty())
            return invalid<Map>(core::ErrorCode::InvalidState,
                "M17_FRAME_MAP_IDENTITY_MISMATCH",
                "No-SRC terminal output must retain the exact SOURCE frame count.");
        return core::Result<Map>::success(Map{
            sourceRate,outputRate,sourceFrames,outputFrames,std::nullopt});
    }
    auto adapter=audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{sourceRate,outputRate,
            output.format().channel_layout(),sourceFrames});
    if(!adapter)
        return core::Result<Map>::failure(*adapter.error());
    if(adapter.value()->output_frame_count()!=outputFrames
       ||provenance.src_kernel_sha256!=adapter.value()->kernel_sha256()) {
        return invalid<Map>(core::ErrorCode::InvalidState,
            "M17_FRAME_MAP_KERNEL_OR_LENGTH_STALE",
            "Terminal PCM frame length or FIR kernel differs from accepted provenance.");
    }
    return core::Result<Map>::success(Map{
        sourceRate,outputRate,sourceFrames,outputFrames,
        std::move(*adapter.value())});
}

core::Result<TerminalPlaybackFrameMap> TerminalPlaybackFrameMap::create_measured(
    const TerminalCandidateGate::StagedMeasuredCandidate& accepted)
{
    using Map=TerminalPlaybackFrameMap;
    const auto& output=accepted.rendered.terminal.terminal
        .compensated_pcm.view();
    const auto& provenance=accepted.rendered;
    if(!accepted.prepared_owner || !provenance.manual_owner
       ||accepted.original_manual_realization_id.value==0
       ||provenance.manual_owner->frame_domain_id()
           !=audio::FrameDomainId::SOURCE_PROCESSING_RATE
       ||output.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||provenance.manual_owner->view().absolute_start_frame().value()!=0
       ||output.absolute_start_frame().value()!=0
       ||provenance.programme_frames.value()<=0
       ||provenance.programme_frames!=provenance.manual_owner->view().frame_count()
       ||provenance.output_rate!=output.format().sample_rate()
       ||provenance.manual_owner->view().format().channel_layout()
           !=output.format().channel_layout()) {
        return invalid<Map>(core::ErrorCode::InvalidState,
            "M17_FRAME_MAP_UNOWNED_SOURCE",
            "Terminal frame map requires an accepted complete origin with output-rate PCM.");
    }
    const auto sourceRate=provenance.manual_owner->view().format().sample_rate();
    const auto outputRate=provenance.output_rate;
    const auto sourceFrames=provenance.programme_frames;
    const auto outputFrames=output.frame_count();
    const bool converted=sourceRate!=outputRate;
    if(converted!=provenance.used_candidate_src)
        return invalid<Map>(core::ErrorCode::InvalidState,
            "M17_FRAME_MAP_SRC_MISMATCH",
            "Terminal source/output frequency provenance is inconsistent.");
    if(!converted) {
        if(sourceFrames!=outputFrames
           ||!provenance.src_kernel_sha256.empty())
            return invalid<Map>(core::ErrorCode::InvalidState,
                "M17_FRAME_MAP_IDENTITY_MISMATCH",
                "No-SRC terminal output must retain the exact SOURCE frame count.");
        return core::Result<Map>::success(Map{
            sourceRate,outputRate,sourceFrames,outputFrames,std::nullopt});
    }
    auto adapter=audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{sourceRate,outputRate,
            output.format().channel_layout(),sourceFrames});
    if(!adapter)
        return core::Result<Map>::failure(*adapter.error());
    if(adapter.value()->output_frame_count()!=outputFrames
       ||provenance.src_kernel_sha256!=adapter.value()->kernel_sha256()) {
        return invalid<Map>(core::ErrorCode::InvalidState,
            "M17_FRAME_MAP_KERNEL_OR_LENGTH_STALE",
            "Terminal PCM frame length or FIR kernel differs from accepted provenance.");
    }
    return core::Result<Map>::success(Map{
        sourceRate,outputRate,sourceFrames,outputFrames,
        std::move(*adapter.value())});
}

core::Result<core::FrameIndex>
TerminalPlaybackFrameMap::source_to_output(core::FrameIndex frame) const
{
    if(frame.value()<0 || frame.value()>sourceFrames_.value())
        return invalid<core::FrameIndex>(core::ErrorCode::OutOfRange,
            "M17_FRAME_MAP_SOURCE_OUT_OF_RANGE",
            "Source playhead is outside the full programme.");
    if(!adapter_)
        return core::Result<core::FrameIndex>::success(frame);
    return adapter_->map_input_frame_to_output(frame);
}

core::Result<core::FrameIndex>
TerminalPlaybackFrameMap::output_to_source_display(core::FrameIndex frame) const
{
    if(frame.value()<0 || frame.value()>outputFrames_.value())
        return invalid<core::FrameIndex>(core::ErrorCode::OutOfRange,
            "M17_FRAME_MAP_OUTPUT_OUT_OF_RANGE",
            "Output display cursor is outside the complete terminal PCM.");
    if(!adapter_)
        return core::Result<core::FrameIndex>::success(frame);
    return adapter_->map_output_frame_to_input_cursor(frame);
}

core::Result<core::FrameRange>
TerminalPlaybackFrameMap::source_loop_to_output(core::FrameRange range) const
{
    if(range.begin().value()<0
       ||range.end().value()>sourceFrames_.value()
       ||range.begin()>=range.end())
        return invalid<core::FrameRange>(core::ErrorCode::InvalidFrameRange,
            "M17_FRAME_MAP_INVALID_LOOP",
            "Source Loop Region is invalid or outside the programme.");
    auto first=source_to_output(range.begin());
    if(!first)return core::Result<core::FrameRange>::failure(*first.error());
    auto last=source_to_output(range.end());
    if(!last)return core::Result<core::FrameRange>::failure(*last.error());
    if(*first.value()>=*last.value())
        return invalid<core::FrameRange>(core::ErrorCode::InvalidFrameRange,
            "M17_FRAME_MAP_COLLAPSED_LOOP",
            "The source Loop Region collapses after output sample-rate conversion.");
    return core::FrameRange::create(*first.value(),*last.value());
}
} // namespace rgsml::app
