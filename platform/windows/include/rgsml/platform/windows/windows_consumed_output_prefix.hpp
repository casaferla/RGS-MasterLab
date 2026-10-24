#pragma once

#include <rgsml/core/realization_identity.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace rgsml::platform::windows {

// C2D6: Windows-specific, bounded, detached, read-only worker observation.
// These slices are OUTPUT_CONSUMED evidence, not a public TruePeak result.
// Source-candidate intervals are NOT approved for LIVE presentation until an
// external immutable PROCESSED RenderResult + chainRevision is verified.
enum class WindowsConsumedSliceKind {
    SOURCE_CANDIDATE_NOT_QUALIFIED,
    GAP_UNPROVEN,
    GAP_LOST_PROVENANCE,
};
enum class WindowsConsumedGapReason {
    NOT_MONITORED,
    STALE_GENERATION,
    GENERATION_EXHAUSTED,
    CLOCK_BEFORE_BASELINE,
    CLOCK_AHEAD_OF_GENERATED,
    CLOCK_AHEAD_OF_ACCEPTED,
    ACCEPTED_LEDGER_OVERFLOW,
    UNPROVEN_HANDOFF,
    UNPROVEN_SRC,
    UNPROVEN_LOOP,
    MISSING_CHRONOLOGICAL_LEDGER,
};

struct WindowsConsumedOutputSlice final {
    std::uint64_t generation{0};
    std::int64_t outputBegin{0};
    std::int64_t outputEnd{0};
    WindowsConsumedSliceKind kind{WindowsConsumedSliceKind::GAP_UNPROVEN};
    std::optional<std::int64_t> sourceBegin;
    std::optional<std::int64_t> sourceEnd;
    std::optional<core::RealizationId> realizationId;
    WindowsConsumedGapReason reason{WindowsConsumedGapReason::NOT_MONITORED};
    bool loopTraversalProven{false};
    std::uint64_t loopWrapCount{0};
    bool handoffPureNewProven{false};
};

struct WindowsConsumedOutputPrefix final {
    static constexpr std::size_t kMaxSlices = 129U;
    std::uint64_t generation{0};
    std::int64_t processedWatermarkExclusive{0};
    // First actually retained OUTPUT frame. Everything earlier was already
    // consumed before being pruned; it is NOT claimed as certified history.
    std::int64_t retainedOutputBegin{0};
    std::array<WindowsConsumedOutputSlice, kMaxSlices> slices{};
    std::size_t count{0};
    bool partitionRepresented{false};
    // Authenticated OBSERVATIONAL pre-SRC SOURCE prefix retired by the
    // actual consumed OUTPUT timeline. The app must independently bind
    // this owner to immutable PROCESSED before any LIVE True Peak work.
    // Never derive it from the OUTPUT-to-SOURCE UI cursor.
    std::optional<std::int64_t> srcRetiredSourceEnd;
    // Explicit SOURCE origin of the same sink-consumed SRC epoch. A seek
    // cannot borrow pre-seek samples or invent complete temporal buckets.
    std::optional<std::int64_t> srcRetiredSourceBegin;
    // Fresh device-consumed loop traversal, retained separately from GAPs.
    std::int64_t srcConsumedOutputBegin{0};
    std::uint64_t srcLoopWrapCount{0};
    bool srcLoopTraversalProven{false};
    std::optional<core::RealizationId> srcRealizationId;
    std::int64_t srcInputRateHz{0};
    std::int64_t srcOutputRateHz{0};
    bool certifiedForLiveTruePeak{false};  // NEVER set by C2D6
    WindowsConsumedGapReason reason{WindowsConsumedGapReason::NOT_MONITORED};
    [[nodiscard]] std::span<const WindowsConsumedOutputSlice>
    view() const noexcept {
        return {slices.data(), count};
    }
};

} // namespace rgsml::platform::windows
