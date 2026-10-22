#pragma once

// M17 PO-authorized experiment, NOT part of the frozen production DSP.
// Alternative scalar controller, initially for exact reference comparison.
// Reuse the frozen equations *verbatim*; only avoid a release exponential
// when the release branch is not selected, and reuse an already-calculated
// gain for an identical adjacent reduction. Never alter the production
// controller, checkpoint version, registry, FIR, or controller clock.

#include "internal/true_peak_limiter_state.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>

namespace rgsml::dsp::internal::experimental {

class M17LazyController final {
public:
    using Event=LimiterDetectorEvent;
    using Step=LimiterControlStep;
    using Snapshot=LimiterControllerSnapshot;

    struct Counters {
        std::uint64_t ticks{0};
        std::uint64_t release_exp_evaluated{0};
        std::uint64_t release_exp_skipped{0};
        std::uint64_t gain_pow_evaluated{0};
        std::uint64_t gain_pow_reused{0};
    };

    [[nodiscard]] static std::optional<M17LazyController>
    create(std::int64_t sample_rate,double ceiling_db_tp)
    {
        const auto frozen=TruePeakLimiterControllerState::create(
            sample_rate,ceiling_db_tp);
        if(!frozen)return std::nullopt;
        const auto half_frame=sample_rate/200;
        const auto remainder=sample_rate%200;
        const std::int64_t h=half_frame
            +(remainder>100 || (remainder==100 && half_frame%2!=0)?1:0);
        const double linear=std::pow(10.0,ceiling_db_tp/20.0);
        const auto fcontrol=8*sample_rate;
        const double density_coeff=std::exp(
            -1.0/(0.100*static_cast<double>(fcontrol)));
        return M17LazyController{sample_rate,linear,h,density_coeff};
    }

    [[nodiscard]] Snapshot snapshot() const noexcept {
        return {next_tick_,density_,r_previous_,hold_};
    }

    [[nodiscard]] bool restore(const Snapshot& s) noexcept
    {
        if(s.next_tick<-62
            || s.next_tick>=std::numeric_limits<std::int64_t>::max()-lookahead_ticks()
            || !std::isfinite(s.density) || s.density<0.0 || s.density>1.0
            || !std::isfinite(s.previous_reduction_db)
            || s.previous_reduction_db<0.0 || s.hold_remaining<0
            || s.hold_remaining>hold_ticks())return false;
        next_tick_=s.next_tick;
        density_=s.density;
        r_previous_=s.previous_reduction_db;
        hold_=s.hold_remaining;
        cached_gain_.reset();
        return true;
    }

    [[nodiscard]] std::int64_t next_tick() const noexcept {return next_tick_;}
    [[nodiscard]] std::int64_t lookahead_ticks() const noexcept {return 8*h_;}
    [[nodiscard]] std::int64_t hold_ticks() const noexcept {return sample_rate_/25;}
    [[nodiscard]] Counters counters() const noexcept {return counters_;}

    [[nodiscard]] rgsml::core::Result<Step>
    advance(double current_peak,std::span<const Event> future_events)
    {
        using rgsml::core::Result;
        using rgsml::core::Error;
        using rgsml::core::ErrorCode;
        if(!std::isfinite(current_peak) || current_peak<0.0
            || next_tick_>=std::numeric_limits<std::int64_t>::max()
                -lookahead_ticks()) {
            return Result<Step>::failure(Error{ErrorCode::InvalidAudioSample,
                "M17 control current peak or clock is invalid."});
        }
        const auto required=demand_db(current_peak);
        if(!std::isfinite(required)) {
            return Result<Step>::failure(Error{ErrorCode::InvalidAudioSample,
                "M17 control demand is nonfinite."});
        }
        double attack_floor=required;
        std::int64_t previous_future_tick=next_tick_;
        for(const auto& ev:future_events) {
            if(!ev.valid || !std::isfinite(ev.peak) || ev.peak<0.0
                || ev.tick<=previous_future_tick
                || ev.tick>next_tick_+lookahead_ticks()) {
                return Result<Step>::failure(Error{ErrorCode::InvalidArgument,
                    "M17 lookahead demands must be sorted, bounded and finite."});
            }
            previous_future_tick=ev.tick;
            const double demand=demand_db(ev.peak);
            if(!std::isfinite(demand)) {
                return Result<Step>::failure(Error{ErrorCode::InvalidAudioSample,
                    "M17 future demand is nonfinite."});
            }
            if(demand>0.0) {
                const double d=static_cast<double>(ev.tick-next_tick_);
                const double w=0.5*(1.0+std::cos(
                    std::numbers::pi_v<double>*d/
                    static_cast<double>(lookahead_ticks())));
                attack_floor=std::max(attack_floor,demand*w);
            }
        }
        const double q=required>0.0?1.0:0.0;
        const double new_density=density_coeff_*density_
            +(1.0-density_coeff_)*q;
        const double remaining=1.0-new_density;
        const double s=1.0-remaining*remaining*remaining*remaining;
        const double tau_ms=20.0+980.0*s;

        double applied=+0.0;
        auto next_hold=hold_;
        bool evaluated_release=false;
        if(attack_floor>=r_previous_) {
            applied=attack_floor;
            next_hold=hold_ticks();
        } else if(next_hold>0) {
            applied=std::max(attack_floor,r_previous_);
            --next_hold;
        } else {
            // Frozen release expression, evaluated only when used.
            const double release_coeff=std::exp(
                -1.0/((tau_ms/1000.0)*static_cast<double>(8*sample_rate_)));
            const double candidate=release_coeff*r_previous_
                +(1.0-release_coeff)*attack_floor;
            applied=std::max(attack_floor,candidate);
            evaluated_release=true;
        }

        // A stored gain is valid only for the same binary64 reduction.
        // After restore, recompute instead of assuming checkpointed gain.
        const bool reuse_gain=cached_gain_.has_value()
            && std::bit_cast<std::uint64_t>(applied)
                ==std::bit_cast<std::uint64_t>(r_previous_);
        const double gain=reuse_gain?*cached_gain_:
            (applied==0.0?1.0:std::pow(10.0,-applied/20.0));
        if(!std::isfinite(new_density) || new_density<0.0
            || new_density>1.0 || !std::isfinite(tau_ms)
            || !std::isfinite(applied) || applied<0.0
            || !std::isfinite(gain) || gain<=0.0 || gain>1.0) {
            return Result<Step>::failure(Error{ErrorCode::InvalidState,
                "M17 control recurrence generated invalid numeric state."});
        }
        Step output{next_tick_,required,attack_floor,applied,gain,
            new_density,tau_ms,next_hold};
        density_=new_density;
        r_previous_=applied;
        hold_=next_hold;
        cached_gain_=gain;
        ++next_tick_;
        ++counters_.ticks;
        if(evaluated_release)++counters_.release_exp_evaluated;
        else ++counters_.release_exp_skipped;
        if(reuse_gain)++counters_.gain_pow_reused;
        else if(applied!=0.0)++counters_.gain_pow_evaluated;
        return Result<Step>::success(output);
    }

private:
    M17LazyController(std::int64_t sample_rate,double linear,
        std::int64_t h,double density_coeff) noexcept
        :sample_rate_(sample_rate),ceiling_linear_(linear),
         h_(h),density_coeff_(density_coeff) {}

    [[nodiscard]] double demand_db(double peak) const noexcept {
        return peak>ceiling_linear_
            ?20.0*std::log10(peak/ceiling_linear_):+0.0;
    }

    std::int64_t sample_rate_{0};
    double ceiling_linear_{1.0};
    std::int64_t h_{0};
    double density_coeff_{0.0};
    std::int64_t next_tick_{-62};
    double density_{+0.0};
    double r_previous_{+0.0};
    std::int64_t hold_{0};
    std::optional<double> cached_gain_;
    Counters counters_{};
};
} // namespace rgsml::dsp::internal::experimental
