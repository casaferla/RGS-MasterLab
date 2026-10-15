#pragma once

#include "playback_support.hpp"
#include <rgsml/platform/windows/windows_consumed_output_prefix.hpp>

namespace rgsml::platform::windows::internal {

// Every gap kind and reason must cross worker boundary without collapsing
// UNPROVEN into VALID or dropped bytes into digital silence.
[[nodiscard]] inline WindowsConsumedGapReason project_c2_reason(
    C2CoverageProbeReason reason) noexcept
{
    switch (reason) {
    case C2CoverageProbeReason::NOT_MONITORED:
        return WindowsConsumedGapReason::NOT_MONITORED;
    case C2CoverageProbeReason::STALE_GENERATION:
        return WindowsConsumedGapReason::STALE_GENERATION;
    case C2CoverageProbeReason::GENERATION_EXHAUSTED:
        return WindowsConsumedGapReason::GENERATION_EXHAUSTED;
    case C2CoverageProbeReason::CLOCK_BEFORE_BASELINE:
        return WindowsConsumedGapReason::CLOCK_BEFORE_BASELINE;
    case C2CoverageProbeReason::CLOCK_AHEAD_OF_GENERATED:
        return WindowsConsumedGapReason::CLOCK_AHEAD_OF_GENERATED;
    case C2CoverageProbeReason::CLOCK_AHEAD_OF_ACCEPTED:
        return WindowsConsumedGapReason::CLOCK_AHEAD_OF_ACCEPTED;
    case C2CoverageProbeReason::ACCEPTED_LEDGER_OVERFLOW:
        return WindowsConsumedGapReason::ACCEPTED_LEDGER_OVERFLOW;
    case C2CoverageProbeReason::UNPROVEN_HANDOFF:
        return WindowsConsumedGapReason::UNPROVEN_HANDOFF;
    case C2CoverageProbeReason::UNPROVEN_SRC:
        return WindowsConsumedGapReason::UNPROVEN_SRC;
    case C2CoverageProbeReason::UNPROVEN_LOOP:
        return WindowsConsumedGapReason::UNPROVEN_LOOP;
    case C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER:
        return WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
    }
    return WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
}

[[nodiscard]] inline WindowsConsumedOutputPrefix project_c2_prefix(
    const C2ConsumedPrefix& internal) noexcept
{
    WindowsConsumedOutputPrefix out{};
    out.generation = internal.generation;
    out.processedWatermarkExclusive = internal.processedWatermarkExclusive;
    out.reason = project_c2_reason(internal.reason);
    if (internal.size > out.kMaxSlices) {
        out.reason = WindowsConsumedGapReason::ACCEPTED_LEDGER_OVERFLOW;
        return out;
    }
    for (const auto& original : internal.view()) {
        auto& target = out.slices[out.count++];
        target.generation = original.generation;
        target.outputBegin = original.outputBegin;
        target.outputEnd = original.outputEnd;
        target.sourceBegin = original.sourceBegin;
        target.sourceEnd = original.sourceEnd;
        target.realizationId = original.realizationId;
        target.reason = project_c2_reason(original.reason);
        switch (original.kind) {
        case C2ConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED:
            target.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
            break;
        case C2ConsumedSliceKind::GAP_UNPROVEN:
            target.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
            break;
        case C2ConsumedSliceKind::GAP_LOST_PROVENANCE:
            target.kind = WindowsConsumedSliceKind::GAP_LOST_PROVENANCE;
            break;
        }
        if (target.kind != WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED) {
            // A GAP must never leak a stale source frame or realization ID.
            target.sourceBegin.reset();
            target.sourceEnd.reset();
            target.realizationId.reset();
        }
    }
    out.partitionRepresented = internal.partitionRepresented;
    // A worker-origin SOURCE_CANDIDATE is not a full PROCESSED binding.
    out.certifiedForLiveTruePeak = false;
    return out;
}

} // namespace rgsml::platform::windows::internal
