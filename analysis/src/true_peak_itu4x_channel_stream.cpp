#include "internal/true_peak_itu4x_channel_stream.hpp"
#include "internal/true_peak_itu4x_coefficients.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace rgsml::analysis::internal {

core::Result<TruePeakItu4xPhaseGroup>
TruePeakItu4xChannelStream::evaluate(double pcm)
{
    if (!std::isfinite(pcm)) {
        return core::Result<TruePeakItu4xPhaseGroup>::failure(
            core::Error{core::ErrorCode::InvalidAudioSample,
                        "Non-finite PCM in True Peak FIR."});
    }

    std::array<double, 12> taps{};
    taps[0] = pcm;
    std::copy(history_.begin(), history_.end(), taps.begin() + 1);

    TruePeakItu4xPhaseGroup output{};
    double proposedPeak = peak_;
    for (std::size_t phase = 0; phase < kTruePeakItu4xPhaseCount; ++phase) {
        double sum = 0.0;
        for (std::size_t lag = 0; lag < kTruePeakItu4xTapsPerPhase; ++lag) {
            const double product =
                kTruePeakItu4xPhaseMajor[12 * phase + lag] * taps[lag];
            if (!std::isfinite(product)) {
                return core::Result<TruePeakItu4xPhaseGroup>::failure(
                    core::Error{core::ErrorCode::InvalidAudioSample,
                                "Non-finite True Peak FIR intermediate."});
            }
            sum += product;
            if (!std::isfinite(sum)) {
                return core::Result<TruePeakItu4xPhaseGroup>::failure(
                    core::Error{core::ErrorCode::InvalidAudioSample,
                                "Non-finite True Peak FIR accumulation."});
            }
        }
        output.signed_phases[phase] = sum;
        proposedPeak = std::max(proposedPeak, std::abs(sum));
    }
    // Commit only after ALL phase calculations passed: a late non-finite
    // operation must not corrupt the accepted FIR history or peak.
    for (std::size_t lag = history_.size() - 1; lag > 0; --lag)
        history_[lag] = history_[lag - 1];
    history_[0] = pcm;
    peak_ = proposedPeak;
    return core::Result<TruePeakItu4xPhaseGroup>::success(output);
}

core::Result<TruePeakItu4xPhaseGroup>
TruePeakItu4xChannelStream::push(double pcm)
{
    if (finalized_) {
        return core::Result<TruePeakItu4xPhaseGroup>::failure(
            core::Error{core::ErrorCode::InvalidState,
                        "True Peak FIR stream already finalized."});
    }
    if (programme_frames_ == std::numeric_limits<std::uint64_t>::max()) {
        return core::Result<TruePeakItu4xPhaseGroup>::failure(
            core::Error{core::ErrorCode::IntegerOverflow,
                        "True Peak FIR programme-frame counter exhausted."});
    }
    auto output = evaluate(pcm);
    if (output) ++programme_frames_;
    return output;
}

core::Status TruePeakItu4xChannelStream::push_chunk(
    std::span<const double> pcm)
{
    if (finalized_) {
        return core::Status::failure(
            core::Error{core::ErrorCode::InvalidState,
                        "True Peak FIR stream already finalized."});
    }
    // Never materialize a second PCM copy. Only the small fixed FIR history
    // and accepted scalar counters are copied for transactional commit.
    auto candidate = *this;
    for (const double sample : pcm) {
        auto step = candidate.push(sample);
        if (!step)
            return core::Status::failure(*step.error());
    }
    *this = candidate;
    return core::Status::success();
}

core::Result<double> TruePeakItu4xChannelStream::finalize()
{
    if (finalized_ || programme_frames_ == 0) {
        return core::Result<double>::failure(
            core::Error{core::ErrorCode::InvalidState,
                        "True Peak FIR finalization requires unfinished nonempty resource."});
    }
    // Transactional EOS: no partial finalization on an arithmetic error.
    auto candidate = *this;
    for (int tail = 0; tail < 11; ++tail) {
        auto result = candidate.evaluate(+0.0);
        if (!result)
            return core::Result<double>::failure(*result.error());
    }
    candidate.finalized_ = true;
    *this = candidate;
    return core::Result<double>::success(peak_);
}

void TruePeakItu4xChannelStream::reset() noexcept
{
    history_.fill(+0.0);
    peak_ = 0.0;
    programme_frames_ = 0;
    finalized_ = false;
}

}  // namespace rgsml::analysis::internal
