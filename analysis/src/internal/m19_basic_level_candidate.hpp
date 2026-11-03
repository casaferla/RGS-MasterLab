#pragma once
// M19 PREPRODUCTION ONLY. Not a public Phase-6 FeatureDefinitionRegistry,
// not a production M16 adapter, not a qualified loudness implementation.
// Frozen scope: integrated LUFS, full-scope RMS, sample peak, M16 TP, sample-peak crest.
// This isolated prototype computes four metrics; TP stays unavailable until
// the actual qualified M16 surface is bound. No extra live/windowed metrics.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
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

[[nodiscard]] inline std::optional<GatedBlockCandidateEvidence>
gate_complete_400ms_block_powers(std::span<const double> powers)
{
    GatedBlockCandidateEvidence evidence;
    const double absolutePower = std::pow(10.0, (-70.0 + 0.691) / 10.0);
    double sumA = 0.0;
    for (double q : powers) {
        if (!std::isfinite(q) || q < 0.0) {
            return std::nullopt;
        }
        if (q > absolutePower) {
            sumA += q;
            ++evidence.absolute_count;
        }
    }
    if (evidence.absolute_count == 0U) {
        return evidence;
    }
    if (!std::isfinite(sumA)) {
        return std::nullopt;
    }
    // ITU one-pass relative gate = -10 LU relative to the original
    // absolute-gated mean POWER, never mean of dB nor iterative gates.
    const double relativePower = (sumA /
        static_cast<double>(evidence.absolute_count)) / 10.0;
    double sumR = 0.0;
    for (double q : powers) {
        if (q > absolutePower && q > relativePower) {
            sumR += q;
            ++evidence.final_count;
        }
    }
    if (evidence.final_count == 0U || !std::isfinite(sumR)) {
        return std::nullopt;
    }
    const double loudness = -0.691 + 10.0 *
        std::log10(sumR / static_cast<double>(evidence.final_count));
    if (!std::isfinite(loudness)) {
        return std::nullopt;
    }
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
                    const double ratio = peak_magnitude_ / magnitude;
                    rms_normalized_squares_ *= ratio * ratio;
                    peak_magnitude_ = magnitude;
                }
                if (peak_magnitude_ > 0.0) {
                    const double scaled = magnitude / peak_magnitude_;
                    rms_normalized_squares_ += scaled * scaled;
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
            const double relativePower = rms_normalized_squares_ / totalSamples;
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
    std::array<KFilter, 2> filters_{};
    std::array<double, 2> quarter_energy_sum_{};
    std::array<std::array<double, 4>, 2> quarter_mean_squares_{};
    std::vector<double> block_powers_{};
};

} // namespace rgsml::analysis::internal::m19
