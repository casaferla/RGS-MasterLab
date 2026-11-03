#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/result.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::analysis::internal {

// B1c INTERNAL numeric evidence ONLY, deliberately not TruePeakMeasurement:
// no product result-schema promise, global-channel tie policy, qualified rate
// label, regional TP, live telemetry or rendering/limiter integration.
struct UnqualifiedTruePeakResourceEvidence final {
    std::int64_t sample_rate_hz{0};
    std::size_t channel_count{0};
    std::array<double, 2> per_channel_linear_peak{};
    std::array<std::optional<double>, 2> per_channel_db_tp{};
    double global_linear_peak{0.0};
    std::optional<double> global_db_tp{};
    bool is_exact_silence{true};
};

// Accept an OWNING complete AudioBuffer, never a subview/region. No mutation.
// Frozen FIR 4x / 11-zero EOS from B1a/B1b; six frozen supported rates.
// NOT qualified until the official independent M16 Gate G is completed.
[[nodiscard]] core::Result<UnqualifiedTruePeakResourceEvidence>
measure_unqualified_true_peak_full_resource(const audio::AudioBuffer& resource);

}  // namespace rgsml::analysis::internal
