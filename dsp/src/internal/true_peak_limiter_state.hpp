#pragma once
#include "true_peak_limiter_itu4x_frozen_coefficients.hpp"
#include "true_peak_limiter_midpoint_fir16.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

// M17 internal, non-public, signed-phase detector state. Not a True Peak
// Meter; only M16 measure_true_peak_full_resource is final/public authority.
// Events are returned in PHYSICAL 8x tick order. A midpoint v[j] is at
// 2*j-62, but anchor a[j] is at 2*j-47: delaying the anchor by eight 4x
// quarters gives the adjacent ordered event pair a[j-8], v[j].
// Upstream controller must advance all EMPTY tick positions with q=0.
namespace rgsml::dsp::internal {

struct LimiterDetectorEvent final {
    std::int64_t tick{0};
    double peak{0.0}; // magnitude linked across active mono/stereo channels
    bool valid{false};
};

struct LimiterDetectorQuarter final {
    LimiterDetectorEvent preceding_anchor{}; // tick = 2*j-63 if j>=8
    LimiterDetectorEvent midpoint{}; // tick = 2*j-62, always present
};

class TruePeakLimiterDetectorState final {
public:
    explicit TruePeakLimiterDetectorState(std::uint32_t channels) noexcept
        : channels_(channels)
    {
    }

    [[nodiscard]] std::uint32_t channels() const noexcept { return channels_; }
    [[nodiscard]] std::uint64_t programme_frames() const noexcept
    {
        return programme_frames_;
    }
    [[nodiscard]] std::int64_t next_quarter_index() const noexcept
    {
        return next_quarter_;
    }
    [[nodiscard]] bool finalized() const noexcept { return finalized_; }

    void reset() noexcept
    {
        history_ = {};
        anchors_ = {};
        programme_frames_ = 0;
        next_quarter_ = 0;
        finalized_ = false;
    }

    [[nodiscard]] rgsml::core::Result<std::array<LimiterDetectorQuarter, 4>>
    push_programme_frame(double left, double right)
    {
        using rgsml::core::Result;
        using rgsml::core::Error;
        using rgsml::core::ErrorCode;
        if (channels_ != 1 && channels_ != 2) {
            return Result<std::array<LimiterDetectorQuarter, 4>>::failure(
                Error{ErrorCode::InvalidArgument,
                    "M17 detector requires mono or stereo."});
        }
        if (finalized_ || programme_frames_ == std::numeric_limits<std::uint64_t>::max()
            || next_quarter_ > (std::numeric_limits<std::int64_t>::max()-128)/2) {
            return Result<std::array<LimiterDetectorQuarter, 4>>::failure(
                Error{ErrorCode::InvalidState,
                    "M17 detector cannot receive more programme frames."});
        }
        if (!std::isfinite(left) || (channels_ == 2 && !std::isfinite(right))) {
            return Result<std::array<LimiterDetectorQuarter, 4>>::failure(
                Error{ErrorCode::InvalidAudioSample,
                    "M17 detector requires finite PCM."});
        }
        auto candidate = *this; // entire four-phase input frame is transactional
        std::array<LimiterDetectorQuarter, 4> group{};
        for (int phase = 0; phase != 4; ++phase) {
            if (phase == 0) candidate.accept_source_frame(left, right);
            auto result = candidate.evaluate_quarter(phase);
            if (!result) {
                return Result<std::array<LimiterDetectorQuarter, 4>>::failure(
                    *result.error());
            }
            group[static_cast<std::size_t>(phase)] = *result.value();
        }
        ++candidate.programme_frames_;
        *this = candidate;
        return Result<std::array<LimiterDetectorQuarter, 4>>::success(group);
    }

    // Exactly 11 analyzer-only source-zero frames (44 quarter anchors) and
    // another 15 zero-valued quarter anchors to drain the 16-tap midpoint.
    // Output tick of the last midpoint for N>0: 8*N+54.
    // Neither zero-extension nor delayed events are appended to programme PCM.
    [[nodiscard]] rgsml::core::Result<std::array<LimiterDetectorQuarter, 59>>
    finalize_detector()
    {
        using rgsml::core::Result;
        using rgsml::core::Error;
        using rgsml::core::ErrorCode;
        if (channels_ < 1 || channels_ > 2 || finalized_
            || programme_frames_ == 0
            || next_quarter_ > (std::numeric_limits<std::int64_t>::max()-128)/2) {
            return Result<std::array<LimiterDetectorQuarter, 59>>::failure(
                Error{ErrorCode::InvalidState,
                    "M17 detector finalize requires unfinished nonempty programme."});
        }
        auto candidate = *this;
        std::array<LimiterDetectorQuarter, 59> tail{};
        for (std::size_t phaseTick = 0; phaseTick != tail.size(); ++phaseTick) {
            const int phase = static_cast<int>(phaseTick % 4);
            if (phase == 0) candidate.accept_source_frame(+0.0, +0.0);
            auto result = candidate.evaluate_quarter(phase);
            if (!result) {
                return Result<std::array<LimiterDetectorQuarter, 59>>::failure(
                    *result.error());
            }
            tail[phaseTick] = *result.value();
        }
        candidate.finalized_ = true;
        *this = candidate;
        return Result<std::array<LimiterDetectorQuarter, 59>>::success(tail);
    }

private:
    void accept_source_frame(double left, double right) noexcept
    {
        const double pcm[2]{left, right};
        for (std::uint32_t ch = 0; ch != channels_; ++ch) {
            auto& history = history_[ch];
            for (std::size_t lag = history.size()-1; lag > 0; --lag) {
                history[lag] = history[lag-1];
            }
            history[0] = pcm[ch];
        }
    }

    [[nodiscard]] rgsml::core::Result<LimiterDetectorQuarter>
    evaluate_quarter(int phase)
    {
        using rgsml::core::Result;
        using rgsml::core::Error;
        using rgsml::core::ErrorCode;
        const auto j = next_quarter_;
        LimiterDetectorQuarter result{};
        if (j >= 8) {
            result.preceding_anchor.tick = 2*j-63;
            result.preceding_anchor.valid = true;
        }
        result.midpoint.tick = 2*j-62;
        result.midpoint.valid = true;
        for (std::uint32_t ch = 0; ch != channels_; ++ch) {
            double anchor = +0.0;
            for (std::size_t lag = 0; lag < 12; ++lag) {
                const double product =
                    kLimiterAnchorPhaseMajor[static_cast<std::size_t>(phase)*12+lag]
                    * history_[ch][lag];
                if (!std::isfinite(product)) {
                    return Result<LimiterDetectorQuarter>::failure(
                        Error{ErrorCode::InvalidAudioSample,
                            "M17 4x anchor product is nonfinite."});
                }
                anchor += product;
                if (!std::isfinite(anchor)) {
                    return Result<LimiterDetectorQuarter>::failure(
                        Error{ErrorCode::InvalidAudioSample,
                            "M17 4x anchor accumulation is nonfinite."});
                }
            }
            anchors_[ch][static_cast<std::size_t>(j % 16)] = anchor;
            if (j >= 8) {
                const double delayed =
                    anchors_[ch][static_cast<std::size_t>((j-8)%16)];
                result.preceding_anchor.peak = std::max(
                    result.preceding_anchor.peak, std::abs(delayed));
            }
            double midpoint = +0.0;
            for (std::int64_t lag = 0; lag < 16; ++lag) {
                // Negative pre-resource anchor indices are EXACT +0.0.
                const double sample = j >= lag
                    ? anchors_[ch][static_cast<std::size_t>((j-lag)%16)]
                    : +0.0;
                const double product =
                    kLimiterMidpointFir16[static_cast<std::size_t>(lag)] * sample;
                if (!std::isfinite(product)) {
                    return Result<LimiterDetectorQuarter>::failure(
                        Error{ErrorCode::InvalidAudioSample,
                            "M17 midpoint product is nonfinite."});
                }
                midpoint += product;
                if (!std::isfinite(midpoint)) {
                    return Result<LimiterDetectorQuarter>::failure(
                        Error{ErrorCode::InvalidAudioSample,
                            "M17 midpoint accumulation is nonfinite."});
                }
            }
            result.midpoint.peak =
                std::max(result.midpoint.peak, std::abs(midpoint));
        }
        ++next_quarter_;
        return Result<LimiterDetectorQuarter>::success(result);
    }

    std::uint32_t channels_{0};
    std::array<std::array<double, 12>, 2> history_{};
    std::array<std::array<double, 16>, 2> anchors_{};
    std::uint64_t programme_frames_{0};
    std::int64_t next_quarter_{0};
    bool finalized_{false};
};

} // namespace rgsml::dsp::internal
