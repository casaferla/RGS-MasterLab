#include "true_peak_live_input_binding.hpp"

#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::app {

core::Result<BoundLiveTruePeakInput> bind_processed_live_input(
    std::shared_ptr<const render::RenderResult> accepted,
    core::RealizationId expectedRealization,
    const TruePeakExpectedSource& expectedSource,
    const core::PlaybackSnapshot& playback,
    const platform::windows::WindowsConsumedOutputPrefix& consumed)
{
    const auto fail = [](const char* why) {
        return core::Result<BoundLiveTruePeakInput>::failure(
            core::Error{core::ErrorCode::InvalidState, why});
    };
    // In C2D6 a structural prefix, even a complete one, NEVER claims that
    // the authoritative product target has been independently verified.
    if (consumed.certifiedForLiveTruePeak || !accepted
        || expectedRealization.value == 0
        || !consumed.partitionRepresented || consumed.count == 0U
        || consumed.count > consumed.kMaxSlices
        || consumed.processedWatermarkExclusive <= 0
        || (consumed.reason
            != platform::windows::WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER
            && consumed.reason
            != platform::windows::WindowsConsumedGapReason::UNPROVEN_LOOP
            && consumed.reason
                != platform::windows::WindowsConsumedGapReason::UNPROVEN_HANDOFF)) {
        return fail("C2D6 requires exactly one complete source-candidate prefix and a retained owner.");
    }
    if (playback.state != core::PlaybackState::PLAYING
        || playback.audibleRealization.phase != core::AudibleHandoffPhase::NEW
        || playback.audibleRealization.realizationId != std::optional{expectedRealization}) {
        return fail("C2D6 heard source is not independently PROCESSED NEW.");
    }
    const auto range = core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{expectedSource.frameCount.value()});
    if (!range || expectedSource.frameCount.value() <= 0) {
        return fail("C2D6 invalid expected source range.");
    }
    const auto view = accepted->view();
    if (accepted->render_window() != *range.value()
        || accepted->buffer().view().absolute_range() != *range.value()
        || view.absolute_range() != *range.value()
        || accepted->frame_domain_id() != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.timebase().frame_domain_id() != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.frame_count() != expectedSource.frameCount
        || view.format().sample_rate() != expectedSource.sampleRate
        || view.format().channel_layout() != expectedSource.layout) {
        return fail("C2D6 PROCESSED owner lacks whole canonical source-domain coverage.");
    }
    // A loop may have left a real GAP in the past. After the user turns
    // Loop Region OFF, only the NEW, fully consumed, directly source-mapped
    // tail may be measured. Every earlier GAP remains explicitly represented
    // and NO value is transferred across it. In particular, a current loop
    // cannot generate a new exact source run from the playback backend.
    std::int64_t cursor = 0;
    bool earlierGap = false;
    bool earlierMatchingGap = false;
    bool anyProvenLoopTraversal = false;
    for (std::size_t i = 0; i < consumed.count; ++i) {
        const auto& part = consumed.slices[i];
        if (part.generation != consumed.generation
            || part.outputBegin != cursor
            || part.outputEnd <= part.outputBegin
            || part.outputEnd > consumed.processedWatermarkExclusive)
            return fail("C2D10 consumed loop chronology has a missing or malformed interval.");
        if (i + 1U < consumed.count
            && part.kind != platform::windows::WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED) {
            earlierGap = true;
            // Never use an unrelated loop gap to justify a later handoff,
            // or an unrelated handoff gap to justify a loop recovery.
            if (part.kind == platform::windows::WindowsConsumedSliceKind::GAP_UNPROVEN
                && part.reason == consumed.reason)
                earlierMatchingGap = true;
        }
        // Loop proof is a block-generation property, not a guess from the
        // current checkbox state. Keep wrap identity for the consumed suffix.
        anyProvenLoopTraversal |= part.kind
                == platform::windows::WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED
            && part.loopTraversalProven;
        cursor = part.outputEnd;
    }
    const bool loopProofPresent =
        consumed.reason == platform::windows::WindowsConsumedGapReason::UNPROVEN_LOOP
        && anyProvenLoopTraversal;
    if (cursor != consumed.processedWatermarkExclusive
        || (consumed.reason == platform::windows::WindowsConsumedGapReason::UNPROVEN_HANDOFF
            && (!earlierGap || !earlierMatchingGap))
        || (consumed.reason == platform::windows::WindowsConsumedGapReason::UNPROVEN_LOOP
            && !loopProofPresent && (!earlierGap || !earlierMatchingGap)))
        return fail("C2D12 requires a fresh proven suffix after a loop or handoff gap.");

    const auto& slice = consumed.slices[consumed.count - 1U];
    if (slice.kind != platform::windows::WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED
        || slice.generation != consumed.generation
        || slice.outputBegin < 0
        || slice.outputEnd != consumed.processedWatermarkExclusive
        || !slice.sourceBegin || !slice.sourceEnd
        || *slice.sourceBegin < 0
        || *slice.sourceEnd <= *slice.sourceBegin
        || *slice.sourceEnd > expectedSource.frameCount.value()
        || *slice.sourceEnd - *slice.sourceBegin
            != slice.outputEnd - slice.outputBegin
        || slice.realizationId != std::optional{expectedRealization}
        || (loopProofPresent && playback.loop
            && !slice.loopTraversalProven)
        || (loopProofPresent
            && slice.loopWrapCount != playback.loopWrapCount)) {
        return fail("C2D6 source mapping is discontinuous, SRC, stale or mismatched.");
    }
    const auto revision = accepted->chain_revision();
    return core::Result<BoundLiveTruePeakInput>::success(
        BoundLiveTruePeakInput{std::move(accepted), expectedRealization,
            revision, consumed.generation, *slice.sourceBegin,
            *slice.sourceEnd, consumed.processedWatermarkExclusive,
            expectedSource.layout,
            expectedSource.sampleRate});
}

} // namespace rgsml::app
