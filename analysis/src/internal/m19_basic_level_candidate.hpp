#pragma once
// M19 PREPRODUCTION ONLY. Not a public Phase-6 FeatureDefinitionRegistry,
// not a production M16 adapter, not a qualified loudness implementation.
// Frozen scope: integrated LUFS, full-scope RMS, sample peak, M16 TP, sample-peak crest.
// This isolated prototype computes four metrics; TP stays unavailable until
// the actual qualified M16 surface is bound. No extra live/windowed metrics.
#include "m19_k_weighting_frozen_banks.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rgsml::analysis::internal::m19 {

enum class LoudnessPreproductionState {
    not_finalized,
    available,
    insufficient_duration,
    no_absolute_gated_blocks,
    unsupported_k_weighting_rate
};

struct BasicLevelPreproductionSnapshot {
    std::uint64_t scope_begin_frame{0};
    std::uint64_t scope_frames{0};
    std::uint32_t sample_rate_hz{0};
    std::uint32_t channel_count{0};
    std::optional<double> integrated_lufs{};
    std::optional<double> full_scope_rms_dbfs{};
    std::optional<double> sample_peak_dbfs{};
    // NO M19 oversampler or surrogate. Bind only the canonical M16 result later.
    std::optional<double> true_peak_db_tp{};
    std::optional<double> sample_peak_crest_db{};
    // PO-frozen asset provenance only, not a product-qualification assertion.
    std::string_view k_weighting_bank_member_id{};
    std::string_view k_weighting_bank_sha256{};
    LoudnessPreproductionState loudness_state{
        LoudnessPreproductionState::not_finalized};
};

// Private gate evidence for independent numerical adversaries. All comparisons
// are STRICT in power domain; this avoids near-equality reclassification from
// the rounded -0.691 + log10(q) -> subtraction path.
struct GatedBlockCandidateEvidence {
    std::optional<double> integrated_lufs{};
    std::size_t absolute_count{0};
    std::size_t final_count{0};
};

// Strict positive block-energy mean with a compensated direct path and a
// separately recomputed peak-scaled path ONLY when the sum would overflow.
// The mean of finite nonnegative q cannot exceed max(q), even when sum(q)
// does not fit in binary64. Do not label an otherwise representable LUFS as
// numerically undefined just because a naive accumulator overflowed.
//
// The fallback never changes the comparator direction, introduces an epsilon,
// or changes the frozen power-domain -70/-10 one-pass gates. A fully audited
// QTP/range policy is still required before production integration.
struct PositivePowerMeanCandidate final {
    double mean{0.0};
    std::size_t count{0};
};

template <typename Selected>
[[nodiscard]] inline std::optional<PositivePowerMeanCandidate>
positive_block_power_mean(std::span<const double> powers, Selected&& selected)
{
    PositivePowerMeanCandidate result;
    double sum = 0.0;
    double correction = 0.0;
    double peak = 0.0;
    bool needs_scaled_fallback = false;
    for (const double q : powers) {
        if (!selected(q)) continue;
        ++result.count;
        if (q > peak) peak = q;
        if (needs_scaled_fallback) continue;
        const double next = sum + q;
        if (!std::isfinite(next)) {
            needs_scaled_fallback = true;
            continue;
        }
        // Neumaier adds small block powers without depending on input
        // permutation as strongly as an ordinary sequential sum.
        if (sum >= q) {
            correction += (sum - next) + q;
        } else {
            correction += (q - next) + sum;
        }
        sum = next;
        if (!std::isfinite(correction)) needs_scaled_fallback = true;
    }
    if (result.count == 0U) return result;
    if (!(peak > 0.0) || !std::isfinite(peak)) return std::nullopt;

    if (!needs_scaled_fallback && std::isfinite(sum + correction)) {
        result.mean = (sum + correction) /
            static_cast<double>(result.count);
    } else {
        // Every summand is now <=1, so the sum is bounded by count,
        // independent of the original power magnitudes. Re-evaluate from
        // complete input, rather than mixing pre-overflow partial state.
        double scaledSum = 0.0;
        double scaledCorrection = 0.0;
        for (const double q : powers) {
            if (!selected(q)) continue;
            const double term = q / peak;
            const double next = scaledSum + term;
            if (scaledSum >= term) {
                scaledCorrection += (scaledSum - next) + term;
            } else {
                scaledCorrection += (term - next) + scaledSum;
            }
            scaledSum = next;
        }
        const double normalizedMean =
            (scaledSum + scaledCorrection) /
            static_cast<double>(result.count);
        result.mean = peak * normalizedMean;
    }
    if (!(result.mean > 0.0) || !std::isfinite(result.mean)) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] inline std::optional<GatedBlockCandidateEvidence>
gate_complete_400ms_block_powers(std::span<const double> powers)
{
    GatedBlockCandidateEvidence evidence;
    const double absolutePower = std::pow(10.0, (-70.0 + 0.691) / 10.0);
    for (const double q : powers) {
        if (!std::isfinite(q) || q < 0.0) return std::nullopt;
    }

    const auto absolute = positive_block_power_mean(powers,
        [absolutePower](double q) { return q > absolutePower; });
    if (!absolute) return std::nullopt;
    evidence.absolute_count = absolute->count;
    if (absolute->count == 0U) return evidence;

    // The original absolute-gated mean is the ONLY input to the one-pass
    // relative threshold. Strict comparisons use linear power, not a
    // rounded LUFS display, a floating epsilon, or an iterative gate.
    const double relativePower = absolute->mean / 10.0;
    const auto retained = positive_block_power_mean(powers,
        [absolutePower,relativePower](double q) {
            return q > absolutePower && q > relativePower;
        });
    if (!retained || retained->count == 0U) return std::nullopt;
    evidence.final_count = retained->count;
    const double loudness =
        -0.691 + 10.0 * std::log10(retained->mean);
    if (!std::isfinite(loudness)) return std::nullopt;
    evidence.integrated_lufs = loudness;
    return evidence;
}

class BasicLevelPreproduction final {
public:
    BasicLevelPreproduction(std::uint32_t sampleRateHz,
                            std::uint32_t channelCount,
                            std::uint64_t absoluteScopeBegin,
                            std::uint64_t scopeFrames) noexcept
        : sample_rate_hz_(sampleRateHz),
          channel_count_(channelCount),
          scope_begin_(absoluteScopeBegin),
          scope_frames_(scopeFrames)
    {
        valid_ = (channelCount == 1U || channelCount == 2U)
            && (sampleRateHz == 44100U || sampleRateHz == 48000U
                || sampleRateHz == 88200U || sampleRateHz == 96000U
                || sampleRateHz == 176400U || sampleRateHz == 192000U)
            && scopeFrames > 0U
            && absoluteScopeBegin <=
                (std::numeric_limits<std::uint64_t>::max)() - scopeFrames;
    }

    // Input must be the actual chronological PCM inside exactly one logical
    // AnalysisScope. Chunk boundaries never reset the K filter or energy grid.
    // Whole-chunk validation prevents any partial state mutation on invalid PCM.
    [[nodiscard]] bool push(std::span<const double> channel0,
                            std::span<const double> channel1 = {})
    {
        if (!valid_ || finished_ || channel0.empty()
            || (channel_count_ == 1U && !channel1.empty())
            || (channel_count_ == 2U && channel1.size() != channel0.size())
            || channel0.size() > scope_frames_ - consumed_) {
            return false;
        }
        for (std::size_t i = 0; i < channel0.size(); ++i) {
            if (!std::isfinite(channel0[i])
                || (channel_count_ == 2U && !std::isfinite(channel1[i]))) {
                return false;
            }
        }

        // Exact frozen 48-kHz coefficients only. At the other five clocks the
        // physical K-weighting bank is not yet authority-reconciled: no LUFS.
        for (std::size_t i = 0; i < channel0.size(); ++i) {
            for (std::uint32_t channel = 0; channel < channel_count_; ++channel) {
                const double x = channel == 0U ? channel0[i] : channel1[i];
                const double magnitude = std::abs(x);
                if (magnitude > peak_magnitude_) {
                    // Scale *both* parts of the compensated, nonnegative
                    // square sum. Direct x*x would overflow above 2^512
                    // or underflow for tiny but nonzero dyadic samples.
                    const double ratio = peak_magnitude_ / magnitude;
                    const double factor = ratio * ratio;
                    rms_normalized_squares_ *= factor;
                    rms_normalized_compensation_ *= factor;
                    peak_magnitude_ = magnitude;
                }
                if (peak_magnitude_ > 0.0) {
                    const double scaled = magnitude / peak_magnitude_;
                    const double add = scaled * scaled;
                    // Neumaier compensated addition, in the mathematical
                    // *sample order*; neither chunk boundaries nor channel
                    // buffer partitioning regroup the reduction. Needed for
                    // F07: 1 followed by 4096 * 2^-30 contributes 2^-48
                    // to the mean-square numerator, despite each addition
                    // being lost by naive sequential binary64 addition.
                    const double next = rms_normalized_squares_ + add;
                    if (rms_normalized_squares_ >= add) {
                        rms_normalized_compensation_ +=
                            (rms_normalized_squares_ - next) + add;
                    } else {
                        rms_normalized_compensation_ +=
                            (add - next) + rms_normalized_squares_;
                    }
                    rms_normalized_squares_ = next;
                }
                if (sample_rate_hz_ == 48000U) {
                    const double shelf = filters_[channel].shelf.push(x);
                    const double y = filters_[channel].highpass.push(shelf);
                    if (!std::isfinite(y) || !std::isfinite(y * y)) {
                        valid_ = false;
                        return false;
                    }
                    quarter_energy_sum_[channel] += y * y;
                    if (!std::isfinite(quarter_energy_sum_[channel])) {
                        valid_ = false;
                        return false;
                    }
                }
            }
            ++consumed_;
            if (sample_rate_hz_ == 48000U && consumed_ % kQuarterFrames == 0U) {
                const std::size_t bucket = static_cast<std::size_t>(
                    quarters_completed_ % 4U);
                for (std::uint32_t channel = 0; channel < channel_count_; ++channel) {
                    quarter_mean_squares_[channel][bucket] =
                        quarter_energy_sum_[channel] / static_cast<double>(kQuarterFrames);
                    quarter_energy_sum_[channel] = 0.0;
                }
                ++quarters_completed_;
                if (quarters_completed_ >= 4U) {
                    double q = 0.0;
                    for (std::uint32_t channel = 0; channel < channel_count_; ++channel) {
                        for (double meanPower : quarter_mean_squares_[channel]) {
                            q += meanPower / 4.0;
                        }
                    }
                    if (!std::isfinite(q) || q < 0.0) {
                        valid_ = false;
                        return false;
                    }
                    block_powers_.push_back(q);
                }
            }
        }
        return true;
    }

    [[nodiscard]] std::optional<BasicLevelPreproductionSnapshot> finalize()
    {
        if (!valid_ || finished_ || consumed_ != scope_frames_) {
            return std::nullopt;
        }
        finished_ = true;
        BasicLevelPreproductionSnapshot out;
        out.scope_begin_frame = scope_begin_;
        out.scope_frames = scope_frames_;
        out.sample_rate_hz = sample_rate_hz_;
        out.channel_count = channel_count_;
        if (peak_magnitude_ > 0.0) {
            const double totalSamples = static_cast<double>(scope_frames_)
                * static_cast<double>(channel_count_);
            const double compensatedSquares =
                rms_normalized_squares_ + rms_normalized_compensation_;
            const double relativePower = compensatedSquares / totalSamples;
            const double peakDb = 20.0 * std::log10(peak_magnitude_);
            const double rmsDb = peakDb + 10.0 * std::log10(relativePower);
            const double crestDb = peakDb - rmsDb;
            if (!std::isfinite(peakDb) || !std::isfinite(rmsDb)
                || !std::isfinite(crestDb) || relativePower <= 0.0) {
                return std::nullopt;
            }
            out.sample_peak_dbfs = peakDb;
            out.full_scope_rms_dbfs = rmsDb;
            out.sample_peak_crest_db = crestDb;
        }
        if (sample_rate_hz_ != 48000U) {
            out.loudness_state =
                LoudnessPreproductionState::unsupported_k_weighting_rate;
        } else if (block_powers_.empty()) {
            out.loudness_state = LoudnessPreproductionState::insufficient_duration;
        } else {
            const auto gated = gate_complete_400ms_block_powers(block_powers_);
            if (!gated) {
                return std::nullopt;
            }
            if (gated->integrated_lufs) {
                out.integrated_lufs = gated->integrated_lufs;
                out.loudness_state = LoudnessPreproductionState::available;
            } else {
                out.loudness_state =
                    LoudnessPreproductionState::no_absolute_gated_blocks;
            }
        }
        return out;
    }

private:
    struct Biquad {
        double b0, b1, b2, a1, a2;
        double x1{0.0}, x2{0.0}, y1{0.0}, y2{0.0};
        [[nodiscard]] double push(double x) noexcept
        {
            const double y = b0 * x + b1 * x1 + b2 * x2
                - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };
    struct KFilter {
        Biquad shelf{1.53512485958697, -2.69169618940638,
                     1.19839281085285, -1.69065929318241,
                     0.73248077421585};
        Biquad highpass{1.0, -2.0, 1.0, -1.99004745483398,
                        0.99007225036621};
    };
    static constexpr std::uint64_t kQuarterFrames = 4800U;
    std::uint32_t sample_rate_hz_{};
    std::uint32_t channel_count_{};
    std::uint64_t scope_begin_{};
    std::uint64_t scope_frames_{};
    std::uint64_t consumed_{0};
    std::uint64_t quarters_completed_{0};
    bool valid_{false};
    bool finished_{false};
    double peak_magnitude_{0.0};
    double rms_normalized_squares_{0.0};
    double rms_normalized_compensation_{0.0};
    std::array<KFilter, 2> filters_{};
    std::array<double, 2> quarter_energy_sum_{};
    std::array<std::array<double, 4>, 2> quarter_mean_squares_{};
    std::vector<double> block_powers_{};
};

} // namespace rgsml::analysis::internal::m19
