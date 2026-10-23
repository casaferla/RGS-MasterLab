#pragma once

// RGS MasterLab M17 - PO-authorized EXPERIMENT, 2026-10-22.
// Isolated selector prototype. NOT a registered module, factory, renderer,
// installed DSP backend, sonic-law revision, or qualification evidence.
// The frozen TruePeakLimiterControllerState remains the only source of
// density, hold, release, reduction, and gain. This selector returns at most
// ONE unmodified detector event per certified lookahead horizon.

#include "internal/true_peak_limiter_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::dsp::internal::experimental {

class M17LookaheadSelector final {
public:
    using Event = LimiterDetectorEvent;

    struct Choice final {
        std::optional<Event> future_event{};
        double attack_floor_db{+0.0};
        std::size_t visited{0};
        std::size_t skipped{0};
        std::size_t skipped_superblocks{0};
        bool bounded{false};
    };

    // Bounded per-batch cache: <=32768 event records plus L+2 weights.
    // This does not establish a streaming resource-wide memory bound.
    static constexpr std::size_t kMaxBatchEvents = 32768U;
    static constexpr std::size_t kGroup = 32U;
    static constexpr std::size_t kDense = 64U;
    static constexpr std::size_t kSuperblock = 16U*kGroup;

    [[nodiscard]] static std::optional<M17LookaheadSelector>
    create(std::int64_t sample_rate, double ceiling_db_tp)
    {
        const auto reference =
            TruePeakLimiterControllerState::create(sample_rate, ceiling_db_tp);
        if (!reference) return std::nullopt;
        const auto linear = std::pow(10.0, ceiling_db_tp/20.0);
        if (!(linear > 0.0) || !std::isfinite(linear)) return std::nullopt;
        return M17LookaheadSelector(reference.value()->lookahead_ticks(), linear);
    }

    [[nodiscard]] bool prepare(std::span<const Event> batch)
    {
        if (batch.size() > kMaxBatchEvents) return false;
        std::int64_t previous = std::numeric_limits<std::int64_t>::min();
        for (const auto& event : batch) {
            if (!event.valid || event.tick <= previous ||
                !std::isfinite(event.peak) || event.peak < 0.0) return false;
            previous = event.tick;
        }

        // Construct temporaries and commit all at once. No partial state.
        std::vector<Event> events(batch.begin(), batch.end());
        std::vector<double> demands;
        std::vector<double> weights;
        std::vector<double> suffix;
        std::vector<double> group;
        std::vector<double> superblock;
        if (events.size() >= kDense) {
            demands.reserve(events.size());
            for (const auto& event : events)
                demands.push_back(demand_db(event.peak));
            // Hann and suffix weights depend ONLY on frozen lookahead
            // H and binary64 arithmetic: retain them across 64-frame
            // batches. Construct them once using the SAME expression,
            // including identical evaluation order, then publish only
            // after the incoming detector events validate successfully.
            if(weights_.empty()) {
                weights.resize(static_cast<std::size_t>(horizon_+1));
                for (std::int64_t lag=1;lag<=horizon_;++lag) {
                    const double distance = static_cast<double>(lag);
                    weights[static_cast<std::size_t>(lag)] =
                        0.5*(1.0+std::cos(
                            std::numbers::pi_v<double>*distance/
                            static_cast<double>(horizon_)));
                }
                suffix.resize(static_cast<std::size_t>(horizon_+2), +0.0);
                for (std::int64_t lag=horizon_;lag>=1;--lag) {
                    const auto offset=static_cast<std::size_t>(lag);
                    suffix[offset]=std::max(suffix[offset+1],weights[offset]);
                }
            }
            group.resize((events.size()+kGroup-1U)/kGroup,+0.0);
            superblock.resize((events.size()+kSuperblock-1U)/kSuperblock,+0.0);
            for (std::size_t i=0;i<events.size();++i) {
                group[i/kGroup]=std::max(group[i/kGroup],demands[i]);
                superblock[i/kSuperblock]=std::max(
                    superblock[i/kSuperblock],demands[i]);
            }
        }

        events_=std::move(events);
        demands_=std::move(demands);
        if(!weights.empty()) {
            weights_=std::move(weights);
            suffix_=std::move(suffix);
            ++weight_build_count_;
        }
        group_=std::move(group);
        superblock_=std::move(superblock);
        return true;
    }

    [[nodiscard]] std::optional<Choice>
    choose(std::int64_t m, double current_peak) const
    {
        if (!std::isfinite(current_peak) || current_peak<0.0
            || m>std::numeric_limits<std::int64_t>::max()-horizon_)
            return std::nullopt;

        const auto first=std::upper_bound(events_.begin(),events_.end(),m,
            [](std::int64_t tick,const Event& event) {
                return tick<event.tick;
            });
        const auto last=std::upper_bound(first,events_.end(),m+horizon_,
            [](std::int64_t tick,const Event& event) {
                return tick<event.tick;
            });

        Choice choice{};
        choice.attack_floor_db=demand_db(current_peak);
        const auto begin=static_cast<std::size_t>(first-events_.begin());
        const auto stop=static_cast<std::size_t>(last-events_.begin());
        choice.bounded=stop-begin>=kDense && !demands_.empty();

        if (choice.bounded) {
            std::size_t index=begin;
            while(index<stop) {
                // Entire aligned superblocks may be skipped. All of their
                // detector ticks are future and inside the certified horizon;
                // peak demand is bounded by the stored maximum, and every
                // later Hann weight by the suffix maximum at the FIRST lag.
                // Strict '<' leaves all equality/tie ordering untouched.
                if(index%kSuperblock==0 && stop-index>=kSuperblock) {
                    const auto lag=static_cast<std::size_t>(
                        events_[index].tick-m);
                    const double upper=superblock_[index/kSuperblock]*
                        suffix_[lag];
                    if(upper<choice.attack_floor_db) {
                        choice.skipped+=kSuperblock;
                        ++choice.skipped_superblocks;
                        index+=kSuperblock;
                        continue;
                    }
                }
                const auto groupIndex=index/kGroup;
                const auto end=std::min(stop,(groupIndex+1)*kGroup);
                const auto lag=static_cast<std::size_t>(events_[index].tick-m);
                const double upper=group_[groupIndex]*suffix_[lag];
                if(upper<choice.attack_floor_db) {
                    choice.skipped+=end-index;
                    index=end;
                    continue;
                }
                for(;index<end;++index) {
                    ++choice.visited;
                    const auto d=demands_[index];
                    if(d>0.0) {
                        const auto delta=static_cast<std::size_t>(
                            events_[index].tick-m);
                        const double weighted=d*weights_[delta];
                        if(weighted>choice.attack_floor_db) {
                            choice.attack_floor_db=weighted;
                            choice.future_event=events_[index];
                        }
                    }
                }
            }
        } else {
            for(std::size_t index=begin;index<stop;++index) {
                ++choice.visited;
                const auto d=demand_db(events_[index].peak);
                if(d>0.0) {
                    const double delta=static_cast<double>(events_[index].tick-m);
                    const double weight=0.5*(1.0+std::cos(
                        std::numbers::pi_v<double>*delta/
                        static_cast<double>(horizon_)));
                    const double weighted=d*weight;
                    if(weighted>choice.attack_floor_db) {
                        choice.attack_floor_db=weighted;
                        choice.future_event=events_[index];
                    }
                }
            }
        }
        return choice;
    }

    [[nodiscard]] std::size_t cached_event_count() const noexcept {
        return events_.size();
    }
    [[nodiscard]] std::size_t weight_build_count() const noexcept {
        return weight_build_count_;
    }

private:
    M17LookaheadSelector(std::int64_t horizon, double ceiling) noexcept
        : horizon_(horizon), ceiling_(ceiling) {}

    [[nodiscard]] double demand_db(double peak) const noexcept {
        return peak>ceiling_ ? 20.0*std::log10(peak/ceiling_) : +0.0;
    }

    std::int64_t horizon_{0};
    double ceiling_{1.0};
    std::vector<Event> events_;
    std::vector<double> demands_;
    std::vector<double> weights_;
    std::vector<double> suffix_;
    std::vector<double> group_;
    std::vector<double> superblock_;
    std::size_t weight_build_count_{0};
};

} // namespace rgsml::dsp::internal::experimental
