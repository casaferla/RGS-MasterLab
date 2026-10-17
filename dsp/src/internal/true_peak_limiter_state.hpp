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
#include <numbers>
#include <span>
#include <vector>

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


/*
 * Core-first M17 frozen control-law state. This is a per-8x-tick primitive,
 * NOT a complete IModule and NOT a source of public True Peak measurements.
 * The caller must establish a complete, forward-proven [m,m+L] detector
 * lookahead before advancing one tick, including empty ticks as peak +0.0.
 * A subsequent streaming scheduler and checkpoint must preserve this proof.
 */
struct LimiterControlStep final {
    std::int64_t tick{0};
    double required_db{0.0};
    double attack_floor_db{0.0};
    double reduction_db{0.0};
    double gain{1.0};
    double density{0.0};
    double release_ms{20.0};
    std::int64_t hold_remaining{0};
};

class TruePeakLimiterControllerState final {
public:
    [[nodiscard]] static rgsml::core::Result<TruePeakLimiterControllerState>
    create(std::int64_t sample_rate, double ceiling_db_tp)
    {
        using rgsml::core::Result;
        using rgsml::core::Error;
        using rgsml::core::ErrorCode;
        const bool supported = sample_rate == 44'100 || sample_rate == 48'000
            || sample_rate == 88'200 || sample_rate == 96'000
            || sample_rate == 176'400 || sample_rate == 192'000;
        if (!supported || !std::isfinite(ceiling_db_tp)
            || ceiling_db_tp < -12.0 || ceiling_db_tp > 0.0) {
            return Result<TruePeakLimiterControllerState>::failure(
                Error{ErrorCode::InvalidArgument,
                    "M17 control requires a supported sample rate and valid Ceiling."});
        }
        const auto half_frame = sample_rate/200;
        const auto remainder = sample_rate%200;
        const std::int64_t h = half_frame
            + (remainder > 100 || (remainder == 100 && (half_frame%2 != 0)) ? 1 : 0);
        const auto ceiling_linear = std::pow(10.0, ceiling_db_tp/20.0);
        const auto fcontrol = 8*sample_rate;
        const auto density_coeff = std::exp(-1.0/(0.100*static_cast<double>(fcontrol)));
        if (!(ceiling_linear > 0.0) || !std::isfinite(ceiling_linear)
            || !(density_coeff > 0.0) || !(density_coeff < 1.0)) {
            return Result<TruePeakLimiterControllerState>::failure(
                Error{ErrorCode::InvalidState, "M17 control coefficients are invalid."});
        }
        return Result<TruePeakLimiterControllerState>::success(
            TruePeakLimiterControllerState{sample_rate, ceiling_db_tp,
                ceiling_linear, h, density_coeff});
    }

    [[nodiscard]] std::int64_t next_tick() const noexcept { return next_tick_; }
    [[nodiscard]] std::int64_t lookahead_frames() const noexcept { return h_; }
    [[nodiscard]] std::int64_t delay_frames() const noexcept { return h_+8; }
    [[nodiscard]] std::int64_t lookahead_ticks() const noexcept { return 8*h_; }
    [[nodiscard]] std::int64_t hold_ticks() const noexcept { return sample_rate_/25; }
    [[nodiscard]] double density() const noexcept { return density_; }
    [[nodiscard]] double previous_reduction_db() const noexcept { return r_previous_; }
    [[nodiscard]] std::int64_t hold_remaining() const noexcept { return hold_; }

    void reset() noexcept
    {
        next_tick_ = -62; // PO ACCEPTED_FROZEN. No PCM before programme n=0.
        density_ = +0.0;
        r_previous_ = +0.0;
        hold_ = 0;
    }

    [[nodiscard]] rgsml::core::Result<LimiterControlStep>
    advance(double current_peak, std::span<const LimiterDetectorEvent> future_events)
    {
        using rgsml::core::Result;
        using rgsml::core::Error;
        using rgsml::core::ErrorCode;
        if (!std::isfinite(current_peak) || current_peak < 0.0
            || next_tick_ >= std::numeric_limits<std::int64_t>::max()-lookahead_ticks()) {
            return Result<LimiterControlStep>::failure(
                Error{ErrorCode::InvalidAudioSample,
                    "M17 control current peak or clock is invalid."});
        }
        const auto required = demand_db(current_peak);
        if (!std::isfinite(required)) {
            return Result<LimiterControlStep>::failure(
                Error{ErrorCode::InvalidAudioSample,
                    "M17 control demand is nonfinite."});
        }
        // Input is explicitly scoped to (m,m+L], sorted and valid. Reject
        // incomplete/provenance-free scheduling at the owning module boundary.
        double attack_floor = required; // the inclusive deadline at k=m
        std::int64_t previous_future_tick = next_tick_;
        for (const auto& ev : future_events) {
            if (!ev.valid || !std::isfinite(ev.peak) || ev.peak < 0.0
                || ev.tick <= previous_future_tick
                || ev.tick > next_tick_ + lookahead_ticks()) {
                return Result<LimiterControlStep>::failure(
                    Error{ErrorCode::InvalidArgument,
                        "M17 lookahead demands must be sorted, bounded and finite."});
            }
            previous_future_tick = ev.tick;
            const double demand = demand_db(ev.peak);
            if (!std::isfinite(demand)) {
                return Result<LimiterControlStep>::failure(
                    Error{ErrorCode::InvalidAudioSample,
                        "M17 future demand is nonfinite."});
            }
            if (demand > 0.0) {
                const double d = static_cast<double>(ev.tick-next_tick_);
                const double w = 0.5 * (1.0 + std::cos(
                    std::numbers::pi_v<double> * d /
                    static_cast<double>(lookahead_ticks())));
                attack_floor = std::max(attack_floor, demand*w);
            }
        }
        const double q = required > 0.0 ? 1.0 : 0.0;
        const double new_density = density_coeff_*density_
            + (1.0-density_coeff_)*q;
        const double remaining = 1.0-new_density;
        const double s = 1.0-remaining*remaining*remaining*remaining;
        const double tau_ms = 20.0+980.0*s;
        const double release_coeff = std::exp(
            -1.0 / ((tau_ms/1000.0)*static_cast<double>(8*sample_rate_)));

        double applied = +0.0;
        auto next_hold = hold_;
        if (attack_floor >= r_previous_) {
            applied = attack_floor;
            next_hold = hold_ticks();
        } else if (next_hold > 0) {
            applied = std::max(attack_floor, r_previous_);
            --next_hold;
        } else {
            const double candidate = release_coeff*r_previous_
                + (1.0-release_coeff)*attack_floor;
            applied = std::max(attack_floor, candidate);
        }
        const double gain = applied == 0.0 ? 1.0
            : std::pow(10.0,-applied/20.0);
        if (!std::isfinite(new_density) || new_density < 0.0
            || new_density > 1.0 || !std::isfinite(tau_ms)
            || !std::isfinite(applied) || applied < 0.0
            || !std::isfinite(gain) || gain <= 0.0 || gain > 1.0) {
            return Result<LimiterControlStep>::failure(
                Error{ErrorCode::InvalidState,
                    "M17 control recurrence generated invalid numeric state."});
        }
        LimiterControlStep output{next_tick_,required,attack_floor,
            applied,gain,new_density,tau_ms,next_hold};
        density_ = new_density;
        r_previous_ = applied;
        hold_ = next_hold;
        ++next_tick_;
        return Result<LimiterControlStep>::success(output);
    }

private:
    TruePeakLimiterControllerState(std::int64_t sample_rate,
        double ceiling_db_tp, double ceiling_linear, std::int64_t h,
        double density_coeff) noexcept
        : sample_rate_(sample_rate), ceiling_db_tp_(ceiling_db_tp),
          ceiling_linear_(ceiling_linear), h_(h),
          density_coeff_(density_coeff)
    {
    }
    [[nodiscard]] double demand_db(double peak) const noexcept
    {
        return peak > ceiling_linear_
            ? 20.0*std::log10(peak/ceiling_linear_) : +0.0;
    }

    std::int64_t sample_rate_{0};
    double ceiling_db_tp_{-1.0};
    double ceiling_linear_{1.0};
    std::int64_t h_{0};
    double density_coeff_{0.0};
    std::int64_t next_tick_{-62};
    double density_{+0.0};
    double r_previous_{+0.0};
    std::int64_t hold_{0};
};

} // namespace rgsml::dsp::internal
