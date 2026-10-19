#include "terminal_source_control_bridge.hpp"

#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::app {
namespace {
[[nodiscard]] core::Error denied(const char* category, const char* reason)
{
    return core::Error{core::ErrorCode::InvalidState,
        reason,{{"category",category}}};
}
} // namespace

core::Result<TerminalSourceControlBridge>
TerminalSourceControlBridge::create(
    TerminalCandidateGate& gate,
    core::IAudioPlaybackService& playback,
    core::RealizationId terminalId)
{
    auto owner=gate.staged();
    if(!owner || terminalId.value==0
       ||terminalId==owner->original_manual_realization_id)
        return core::Result<TerminalSourceControlBridge>::failure(denied(
            "M17_SOURCE_CONTROL_UNPUBLISHED",
            "A distinct, nonzero, independently published terminal playback ID is required."));
    auto map=TerminalPlaybackFrameMap::create(*owner);
    if(!map)return core::Result<TerminalSourceControlBridge>::failure(*map.error());
    TerminalSourceControlBridge result{
        &gate,&playback,std::move(owner),std::move(*map.value()),terminalId};
    auto current=result.checked_output_snapshot();
    if(!current)return core::Result<TerminalSourceControlBridge>::failure(
        *current.error());
    return core::Result<TerminalSourceControlBridge>::success(std::move(result));
}

core::Result<core::PlaybackSnapshot>
TerminalSourceControlBridge::checked_output_snapshot() const
{
    if(!active_ || !gate_ || !playback_ || !accepted_
       ||gate_->staged()!=accepted_)
        return core::Result<core::PlaybackSnapshot>::failure(denied(
            "M17_SOURCE_CONTROL_STALE_OWNER",
            "Terminal source-control candidate was invalidated or superseded."));
    auto state=playback_->snapshot();
    if(!state)return state;
    if(state.value()->state==core::PlaybackState::NO_SOURCE
       ||!state.value()->duration
       ||*state.value()->duration!=map_.output_frames()
       ||state.value()->position.value()<0
       ||state.value()->position.value()>map_.output_frames().value()
       ||state.value()->audibleRealization.phase
          !=core::AudibleHandoffPhase::NEW
       ||state.value()->audibleRealization.realizationId
          !=std::optional{terminalId_})
        return core::Result<core::PlaybackSnapshot>::failure(denied(
            "M17_SOURCE_CONTROL_WRONG_AUDIBLE_ID",
            "The output device is not serving the exact new terminal realization."));
    return state;
}

core::Status TerminalSourceControlBridge::seek_source(core::FrameIndex source)
{
    auto current=checked_output_snapshot();
    if(!current)return core::Status::failure(*current.error());
    auto mapped=map_.source_to_output(source);
    if(!mapped)return core::Status::failure(*mapped.error());
    auto result=playback_->seek(*mapped.value());
    if(!result)return result;
    auto after=checked_output_snapshot();
    return after ? core::Status::success() : core::Status::failure(*after.error());
}

core::Status TerminalSourceControlBridge::set_source_loop(
    std::optional<core::FrameRange> sourceLoop)
{
    auto current=checked_output_snapshot();
    if(!current)return core::Status::failure(*current.error());
    std::optional<core::FrameRange> mapped;
    if(sourceLoop) {
        auto next=map_.source_loop_to_output(*sourceLoop);
        if(!next)return core::Status::failure(*next.error());
        mapped=*next.value();
    }
    auto result=playback_->set_loop(mapped);
    if(!result)return result;
    auto after=checked_output_snapshot();
    if(!after)return core::Status::failure(*after.error());
    if(after.value()->loop!=mapped)
        return core::Status::failure(denied(
            "M17_SOURCE_CONTROL_LOOP_MISMATCH",
            "Output device did not retain the mapped SOURCE Loop Region."));
    sourceLoop_=std::move(sourceLoop);
    return core::Status::success();
}

core::Result<TerminalSourceControlBridge::SourceSnapshot>
TerminalSourceControlBridge::source_snapshot() const
{
    using S=SourceSnapshot;
    auto state=checked_output_snapshot();
    if(!state)return core::Result<S>::failure(*state.error());
    std::optional<core::FrameRange> expectedLoop;
    if(sourceLoop_) {
        auto mapped=map_.source_loop_to_output(*sourceLoop_);
        if(!mapped)return core::Result<S>::failure(*mapped.error());
        expectedLoop=*mapped.value();
    }
    if(state.value()->loop!=expectedLoop)
        return core::Result<S>::failure(denied(
            "M17_SOURCE_CONTROL_LOOP_MISMATCH",
            "The device Loop Region no longer matches its SOURCE authority."));
    auto source=map_.output_to_source_display(state.value()->position);
    if(!source)return core::Result<S>::failure(*source.error());
    return core::Result<S>::success(S{
        state.value()->state,*source.value(),map_.source_frames(),sourceLoop_});
}
} // namespace rgsml::app
