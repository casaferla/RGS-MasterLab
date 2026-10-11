#pragma once

#include <rgsml/core/result.hpp>

#include <array>
#include <cstdint>

namespace rgsml::analysis::internal {

// B1b INTERNAL one-channel FIR evaluator only. Not TruePeakMeter,
// not a public result DTO, not an ITU/EBU qualification claim.
// All four signed phase values are observable for test/oracle comparison.
struct TruePeakItu4xPhaseGroup final {
    std::array<double, 4> signed_phases{};
};

class TruePeakItu4xChannelStream final {
public:
    [[nodiscard]] core::Result<TruePeakItu4xPhaseGroup> push(double pcm);
    // Complete FULL-resource zero-extended support with exactly eleven
    // analyzer-only zero input frames; never append them to programme PCM.
    [[nodiscard]] core::Result<double> finalize();
    void reset() noexcept;
    [[nodiscard]] double linear_peak_so_far() const noexcept { return peak_; }
    [[nodiscard]] std::uint64_t programme_frames() const noexcept { return programme_frames_; }
    [[nodiscard]] bool finalized() const noexcept { return finalized_; }

private:
    [[nodiscard]] core::Result<TruePeakItu4xPhaseGroup> evaluate(double pcm);
    // newest sample first: history_[0] = x[i-1] at next evaluation.
    std::array<double, 11> history_{};
    double peak_{0.0};
    std::uint64_t programme_frames_{0};
    bool finalized_{false};
};

}  // namespace rgsml::analysis::internal
