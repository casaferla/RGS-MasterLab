#include "terminal_consumed_output_binding.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace rgsml::app {
namespace {
using Evidence = TerminalConsumedOutputEvidence;
using Input = TerminalPrivateProductSession::InstalledOutputIdentity;
using Prefix = platform::windows::WindowsConsumedOutputPrefix;
using SliceKind = platform::windows::WindowsConsumedSliceKind;
using GapReason = platform::windows::WindowsConsumedGapReason;

[[nodiscard]] core::Result<Evidence> refuse(const char* why)
{
    return core::Result<Evidence>::failure(
        core::Error{core::ErrorCode::InvalidState, why});
}

[[nodiscard]] bool stable_identity(const Input& before, const Input& after)
{
    const auto& a = before.playback;
    const auto& b = after.playback;
    return before.owner && after.owner
        && before.owner == after.owner
        && before.realizationId.value != 0
        && before.realizationId == after.realizationId
        // The Windows worker supports both PLAYING and PAUSED sink clocks.
        // PAUSED may expose ONLY already independently consumed frames,
        // not future queued PCM. Do not bridge PLAYING -> PAUSED during the
        // observation: the two serialized reads must fence a stable state.
        && (a.state == core::PlaybackState::PLAYING
            || a.state == core::PlaybackState::PAUSED)
        && b.state == a.state
        && a.audibleRealization == b.audibleRealization
        && a.audibleRealization.phase == core::AudibleHandoffPhase::NEW
        && a.audibleRealization.realizationId
            == std::optional{before.realizationId}
        && a.duration == b.duration
        && a.loop == b.loop
        && a.seekSerial == b.seekSerial
        && a.traversalSerial == b.traversalSerial
        && a.loopWrapCount == b.loopWrapCount
        && before.source.duration == after.source.duration
        && before.source.loop == after.source.loop;
}
} // namespace

core::Result<TerminalPrivateGrSeries>
project_private_terminal_gr(const TerminalConsumedOutputEvidence& proof)
{
    using Result=core::Result<TerminalPrivateGrSeries>;
    auto reject=[](const char* why)->Result {
        return Result::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if (!proof.owner || proof.realizationId.value==0
        || proof.realizationId==proof.owner->original_manual_realization_id
        || proof.playbackGeneration==0
        || proof.consumedOutputWatermarkExclusive<=0)
        return reject("M17 GR lacks independent consumed output ownership.");
    const auto& output=proof.owner->rendered;
    const auto& terminal=output.terminal.checked;
    const auto pcm=terminal.terminal.compensated_pcm.view();
    const auto n=pcm.frame_count().value();
    const auto& gains=terminal.terminal.applied_gain_frames;
    const auto delay=terminal.terminal.algorithmic_latency_frames;
    if(terminal.terminal.bypassed || !terminal.final_check
       ||pcm.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||pcm.absolute_start_frame().value()!=0
       ||pcm.format().sample_rate()!=output.output_rate
       ||delay<=0 || n<=0
       ||gains.size()!=static_cast<std::size_t>(n)
       ||proof.firstTerminalFrame<0
       ||proof.lastTerminalFrameExclusive>n
       ||proof.firstTerminalFrame>=proof.lastTerminalFrameExclusive)
        return reject("M17 active gain witness is missing or out of bounds.");
    // GR is painted on the GUI control plane on frequent transport ticks.
    // Scanning the entire consumed programme on each tick is quadratic in
    // playback duration. Display only the latest one second of CONTIGUOUS
    // independently consumed gain; the first earlier gap and every owner
    // guard above remain authoritative. No gain or PCM is recalculated.
    const auto end=proof.lastTerminalFrameExclusive;
    const auto maximumHistoryFrames=output.output_rate.value();
    const auto begin=std::max(
        proof.firstTerminalFrame,end-maximumHistoryFrames);
    const auto length=end-begin;
    constexpr std::int64_t kMaxBuckets=256;
    const auto width=1+(length-1)/kMaxBuckets;
    TerminalPrivateGrSeries series{
        proof.realizationId,proof.playbackGeneration,begin,end,{},
        proof.consumedOutputWatermarkExclusive,proof.loopWrapCount,
        proof.seekSerial,proof.traversalSerial};
    series.buckets.reserve(static_cast<std::size_t>(1+(length-1)/width));
    for(auto start=begin;start<end;) {
        const auto stop=start+std::min(width,end-start);
        double peak=0.0;
        for(auto frame=start;frame<stop;++frame) {
            const auto& observed=gains[static_cast<std::size_t>(frame)];
            if(frame>std::numeric_limits<std::int64_t>::max()-delay
                || observed.programme_frame!=frame
                ||observed.raw_output_frame!=frame+delay
                ||!std::isfinite(observed.reduction_db)
                ||observed.reduction_db<0.0
                ||!std::isfinite(observed.gain)
                ||observed.gain<=0.0 ||observed.gain>1.0)
                return reject("M17 actual gain witness is invalid.");
            if(std::abs(observed.reduction_db
                +20.0*std::log10(observed.gain))>1e-5)
                return reject("M17 actual gain and dB witness disagree.");
            peak=std::max(peak,observed.reduction_db);
        }
        series.buckets.push_back({start,stop,peak});
        start=stop;
    }
    if(series.buckets.empty() ||series.buckets.size()>256)
        return reject("M17 gain history must remain bounded.");
    return Result::success(std::move(series));
}

core::Result<TerminalPrivateGrSeries>
observe_private_terminal_gr(
    const TerminalPrivateProductSession& product,
    const TerminalConsumedPrefixRead& readPrefix)
{
    auto evidence=observe_private_terminal_consumed_output(product,readPrefix);
    if(!evidence)
        return core::Result<TerminalPrivateGrSeries>::failure(*evidence.error());
    return project_private_terminal_gr(*evidence.value());
}

core::Result<TerminalConsumedOutputEvidence>
observe_private_terminal_consumed_output(
    const TerminalPrivateProductSession& product,
    const TerminalConsumedPrefixRead& readPrefix)
{
    if (!readPrefix)
        return refuse("M17 worker consumed-prefix reader is unavailable.");
    auto before = product.private_installed_output_identity();
    if (!before)
        return core::Result<Evidence>::failure(*before.error());
    if (before.value()->playback.state != core::PlaybackState::PLAYING
        && before.value()->playback.state != core::PlaybackState::PAUSED)
        return refuse("M17 sink consumption requires PLAYING or PAUSED terminal ownership.");

    auto first = readPrefix(std::nullopt);
    if (!first)
        return core::Result<Evidence>::failure(*first.error());
    if (first.value()->generation == 0)
        return refuse("M17 initial consumed worker generation is not valid.");

    // A second serialized worker read fences the generation, proving that
    // neither observer call crossed an unrecognized playback stream reset.
    auto second = readPrefix(first.value()->generation);
    if (!second)
        return core::Result<Evidence>::failure(*second.error());
    if (second.value()->generation != first.value()->generation
        || second.value()->processedWatermarkExclusive
            < first.value()->processedWatermarkExclusive)
        return refuse("M17 playback worker generation or clock changed during observation.");

    auto after = product.private_installed_output_identity();
    if (!after)
        return core::Result<Evidence>::failure(*after.error());
    return bind_private_terminal_consumed_output(
        *before.value(), *second.value(), *after.value());
}

core::Result<TerminalConsumedOutputEvidence>
bind_private_terminal_consumed_output(
    const Input& before, const Prefix& consumed, const Input& after)
{
    if (!stable_identity(before, after))
        return refuse("M17 consumed evidence changed owner, seek or loop during observation.");

    const auto& accepted = before.owner;
    const auto& checked = accepted->rendered.terminal.checked;
    const auto pcm = checked.terminal.compensated_pcm.view();
    const auto n = pcm.frame_count().value();
    if (!accepted->rendered.manual_owner
        || accepted->original_manual_realization_id.value == 0
        || before.realizationId == accepted->original_manual_realization_id
        || pcm.timebase().frame_domain_id() != audio::FrameDomainId::OUTPUT_RATE
        || pcm.absolute_start_frame().value() != 0 || n <= 0
        || pcm.format().sample_rate() != accepted->rendered.output_rate
        || before.playback.duration != std::optional{pcm.frame_count()}
        || before.source.duration
            != accepted->rendered.manual_owner->view().frame_count())
        return refuse("M17 consumed witness lacks independently owned exact OUTPUT_RATE PCM.");

    // A Qt playback output candidate remains unqualified even when its
    // owner-stamped actual consumed chronology can be checked.
    // Preserve the EXACT established fail-closed predicate. Partition its
    // refusal reasons so a real Windows witness can distinguish missing or
    // torn worker chronology from an actual device SRC mismatch. No extra
    // source is admitted, no worker samples are synthesized, and no
    // authorization is implied by a diagnostic string.
    if (consumed.srcRealizationId
        || consumed.srcRetiredSourceBegin || consumed.srcRetiredSourceEnd
        || consumed.srcInputRateHz != 0 || consumed.srcOutputRateHz != 0
        || consumed.reason == GapReason::UNPROVEN_SRC)
        return refuse("M17 sink prefix is SRC-derived; terminal playback requires an exact-rate output.");
    if (!consumed.partitionRepresented
        || consumed.count == 0 || consumed.count > Prefix::kMaxSlices
        || consumed.generation == 0
        || consumed.retainedOutputBegin < 0
        || consumed.processedWatermarkExclusive <= consumed.retainedOutputBegin)
        return refuse("M17 sink prefix has no complete consumed OUTPUT partition.");
    if (consumed.certifiedForLiveTruePeak)
        return refuse("M17 sink prefix has unexpected LIVE certification.");
    if (consumed.reason == GapReason::NOT_MONITORED
        || consumed.reason == GapReason::STALE_GENERATION
        || consumed.reason == GapReason::GENERATION_EXHAUSTED
        || consumed.reason == GapReason::CLOCK_BEFORE_BASELINE
        || consumed.reason == GapReason::CLOCK_AHEAD_OF_GENERATED
        || consumed.reason == GapReason::CLOCK_AHEAD_OF_ACCEPTED
        || consumed.reason == GapReason::ACCEPTED_LEDGER_OVERFLOW)
        return refuse("M17 sink prefix has unverified worker clock or ledger epoch.");

    // An output generation may retain PREVIOUS loop traversals or an older
    // GAP in its bounded C2 ledger. Those bytes are real history, but MUST
    // NEVER be replayed as part of the CURRENT meter/FIR traversal.
    // First verify the complete observed OUTPUT partition structurally;
    // no lost or overlapping ledger interval may masquerade as a suffix.
    std::int64_t outputCursor = consumed.retainedOutputBegin;
    for(const auto& part : consumed.view()) {
        if(part.generation != consumed.generation
           ||part.outputBegin != outputCursor
           ||part.outputEnd <= part.outputBegin
           ||part.outputEnd > consumed.processedWatermarkExclusive)
            return refuse("M17 Windows consumed ledger has torn/gapped OUTPUT chronology.");
        outputCursor=part.outputEnd;
    }
    if(outputCursor != consumed.processedWatermarkExclusive)
        return refuse("M17 sink chronology has an unrepresented consumed tail.");

    // Take only the latest adjacent, exact-owner, same-wrap, one-to-one
    // OUTPUT_RATE suffix. The first older GAP, previous loop epoch, seek or
    // discontinuity is a hard boundary: no FIR/HOLD context crosses it.
    const auto parts=consumed.view();
    std::int64_t expectedOutputEnd=consumed.processedWatermarkExclusive;
    std::int64_t expectedSourceEnd=-1;
    std::int64_t firstTerminalFrame=-1;
    std::int64_t lastTerminalFrameExclusive=-1;
    for(std::size_t index=parts.size();index>0;--index) {
        const auto& part=parts[index-1U];
        const bool valid=
            part.kind==SliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED
            &&part.generation==consumed.generation
            &&part.outputEnd==expectedOutputEnd
            &&part.outputBegin<part.outputEnd
            &&part.realizationId==std::optional{before.realizationId}
            &&part.sourceBegin &&part.sourceEnd
            &&*part.sourceBegin>=0 &&*part.sourceEnd<=n
            &&*part.sourceEnd>*part.sourceBegin
            &&*part.sourceEnd-*part.sourceBegin
                ==part.outputEnd-part.outputBegin
            &&(expectedSourceEnd<0 || *part.sourceEnd==expectedSourceEnd)
            &&part.loopWrapCount==before.playback.loopWrapCount
            &&part.handoffPureNewProven
            &&(before.playback.loop
               ? (part.loopTraversalProven
                  &&*part.sourceBegin
                    >=before.playback.loop->begin().value()
                  &&*part.sourceEnd
                    <=before.playback.loop->end().value())
               : (!part.loopTraversalProven &&part.loopWrapCount==0));
        if(!valid)break;
        if(lastTerminalFrameExclusive<0)
            lastTerminalFrameExclusive=*part.sourceEnd;
        firstTerminalFrame=*part.sourceBegin;
        expectedOutputEnd=part.outputBegin;
        expectedSourceEnd=*part.sourceBegin;
    }
    if(firstTerminalFrame<0
       ||lastTerminalFrameExclusive<=firstTerminalFrame)
        return refuse("M17 current consumed traversal has no proven terminal suffix.");

    return core::Result<Evidence>::success(Evidence{
        accepted, before.realizationId, consumed.generation,
        consumed.processedWatermarkExclusive, firstTerminalFrame,
        lastTerminalFrameExclusive, before.playback.loopWrapCount,
        before.playback.seekSerial, before.playback.traversalSerial});
}
} // namespace rgsml::app
