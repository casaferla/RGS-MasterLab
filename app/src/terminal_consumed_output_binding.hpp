#pragma once

#include "terminal_private_product_session.hpp"
#include <rgsml/platform/windows/windows_consumed_output_prefix.hpp>

#include <functional>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>

namespace rgsml::app {

// PRIVATE, unqualified chronology witness: validates that a single terminal
// OUTPUT_RATE PCM owner has a complete consumed-sink suffix. No M16 TRUE PEAK,
// limiter GR, rendering availability, DAC clock or FQ is inferred.
struct TerminalConsumedOutputEvidence final {
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> owner;
    core::RealizationId realizationId;
    std::uint64_t playbackGeneration;
    std::int64_t consumedOutputWatermarkExclusive;
    std::int64_t firstTerminalFrame;
    std::int64_t lastTerminalFrameExclusive;
    std::uint64_t loopWrapCount;
};

// Bounded, PRIVATE GR projection from genuine applied-gain sidecar and
// independently validated consumed OUTPUT_RATE evidence. Verification trim
// is intentionally excluded; this is not public/live product telemetry.
struct TerminalPrivateGrBucket final {
    std::int64_t frameBegin{0};
    std::int64_t frameEndExclusive{0};
    double peakReductionDb{0.0};
};
struct TerminalPrivateGrSeries final {
    core::RealizationId realizationId;
    std::uint64_t playbackGeneration{0};
    std::int64_t consumedFrameBegin{0};
    std::int64_t consumedFrameEndExclusive{0};
    std::vector<TerminalPrivateGrBucket> buckets;
};
[[nodiscard]] core::Result<TerminalPrivateGrSeries>
project_private_terminal_gr(const TerminalConsumedOutputEvidence& proof);

// PRIVATE worker callback. Both reads use Windows' serialized worker prefix.
// The second query is generation-fenced against the first, while product
// identity observations enclose the entire probe. May fail without samples,
// and never publishes meter/GR or a DAC-clock claim.
using TerminalConsumedPrefixRead = std::function<
    core::Result<platform::windows::WindowsConsumedOutputPrefix>(
        std::optional<std::uint64_t> expectedGeneration)>;

[[nodiscard]] core::Result<TerminalPrivateGrSeries>
observe_private_terminal_gr(
    const TerminalPrivateProductSession& product,
    const TerminalConsumedPrefixRead& readPrefix);

[[nodiscard]] core::Result<TerminalConsumedOutputEvidence>
observe_private_terminal_consumed_output(
    const TerminalPrivateProductSession& product,
    const TerminalConsumedPrefixRead& readPrefix);

// Call with independent before/after product identity snapshots enclosing the
// device consumed-prefix read. A seek, loop edit, source swap, identity handoff
// or ownership transition during observation MUST fail closed.
[[nodiscard]] core::Result<TerminalConsumedOutputEvidence>
bind_private_terminal_consumed_output(
    const TerminalPrivateProductSession::InstalledOutputIdentity& before,
    const platform::windows::WindowsConsumedOutputPrefix& consumed,
    const TerminalPrivateProductSession::InstalledOutputIdentity& after);

} // namespace rgsml::app
