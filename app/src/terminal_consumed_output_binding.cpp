#include "terminal_consumed_output_binding.hpp"

#include <rgsml/core/error.hpp>

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
        && a.state == core::PlaybackState::PLAYING
        && b.state == core::PlaybackState::PLAYING
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
    if (before.value()->playback.state != core::PlaybackState::PLAYING)
        return refuse("M17 must be playing before observing sink consumption.");

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
    if (consumed.certifiedForLiveTruePeak
        || !consumed.partitionRepresented
        || consumed.count == 0 || consumed.count > Prefix::kMaxSlices
        || consumed.generation == 0
        || consumed.retainedOutputBegin < 0
        || consumed.processedWatermarkExclusive <= consumed.retainedOutputBegin
        || consumed.srcRealizationId
        || consumed.srcRetiredSourceBegin || consumed.srcRetiredSourceEnd
        || consumed.srcInputRateHz != 0 || consumed.srcOutputRateHz != 0
        || consumed.reason == GapReason::UNPROVEN_SRC
        || consumed.reason == GapReason::NOT_MONITORED
        || consumed.reason == GapReason::STALE_GENERATION
        || consumed.reason == GapReason::GENERATION_EXHAUSTED
        || consumed.reason == GapReason::CLOCK_BEFORE_BASELINE
        || consumed.reason == GapReason::CLOCK_AHEAD_OF_GENERATED
        || consumed.reason == GapReason::CLOCK_AHEAD_OF_ACCEPTED
        || consumed.reason == GapReason::ACCEPTED_LEDGER_OVERFLOW)
        return refuse("M17 sink prefix is missing, discontinuous or SRC-derived.");

    // Conservative first gate: a SINGLE continuous consumed OUTPUT_RATE
    // traversal, with no silent historical GAP, from this exact terminal ID.
    // Later qualifications may safely add independently proven split epochs.
    std::int64_t outputCursor = consumed.retainedOutputBegin;
    std::int64_t terminalCursor = -1;
    std::int64_t firstTerminalFrame = -1;
    for (const auto& part : consumed.view()) {
        if (part.kind != SliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED
            || part.generation != consumed.generation
            || part.outputBegin != outputCursor
            || part.outputEnd <= part.outputBegin
            || part.outputEnd > consumed.processedWatermarkExclusive
            || part.realizationId != std::optional{before.realizationId}
            || !part.sourceBegin || !part.sourceEnd
            || *part.sourceBegin < 0 || *part.sourceEnd > n
            || *part.sourceEnd <= *part.sourceBegin
            || *part.sourceEnd - *part.sourceBegin
                != part.outputEnd - part.outputBegin
            || (terminalCursor >= 0 && *part.sourceBegin != terminalCursor)
            || part.loopWrapCount != before.playback.loopWrapCount
            || !part.handoffPureNewProven)
            return refuse("M17 consumed chronology has a gap, foreign owner or wrong traversal.");

        if (before.playback.loop) {
            // The device, not the UI Loop checkbox, must prove the current
            // wrap. Reject pre-wrap fragments and seeks outside the loop.
            if (!part.loopTraversalProven
                || *part.sourceBegin < before.playback.loop->begin().value()
                || *part.sourceEnd > before.playback.loop->end().value())
                return refuse("M17 active Loop Region lacks exact consumed traversal proof.");
        } else if (part.loopTraversalProven || part.loopWrapCount != 0) {
            return refuse("M17 stale loop traversal cannot certify a non-loop interval.");
        }
        if (firstTerminalFrame < 0)
            firstTerminalFrame = *part.sourceBegin;
        terminalCursor = *part.sourceEnd;
        outputCursor = part.outputEnd;
    }
    if (outputCursor != consumed.processedWatermarkExclusive
        || terminalCursor <= firstTerminalFrame)
        return refuse("M17 sink chronology has an unrepresented consumed tail.");

    return core::Result<Evidence>::success(Evidence{
        accepted, before.realizationId, consumed.generation,
        consumed.processedWatermarkExclusive, firstTerminalFrame,
        terminalCursor, before.playback.loopWrapCount});
}
} // namespace rgsml::app
