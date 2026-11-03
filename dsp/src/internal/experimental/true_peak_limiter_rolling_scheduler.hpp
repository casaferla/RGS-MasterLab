#pragma once

// RGS MasterLab M17: PO-authorized EXPERIMENT, rolling *batch* coordinator.
// Does NOT alter production DSP/factory/limiter controller/checkpoint.
// Source-event completeness and end-of-stream proof remain caller-owned;
// the caller MUST pass complete detector events, never a guessed horizon.
#include "internal/experimental/true_peak_limiter_selector.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace rgsml::dsp::internal::experimental {

class M17RollingBatchSelector final {
public:
    using Event = LimiterDetectorEvent;
    using Choice = M17LookaheadSelector::Choice;
    static constexpr std::size_t kMaxBatchEvents =
        M17LookaheadSelector::kMaxBatchEvents;

    [[nodiscard]] static std::optional<M17RollingBatchSelector>
    create(std::int64_t sample_rate, double ceiling_db_tp)
    {
        const auto controller =
            TruePeakLimiterControllerState::create(sample_rate,ceiling_db_tp);
        auto selector=M17LookaheadSelector::create(sample_rate,ceiling_db_tp);
        if(!controller || !selector)return std::nullopt;
        return M17RollingBatchSelector(std::move(*selector),
            controller.value()->lookahead_ticks());
    }

    // Batch includes COMPLETE active detector events in [first_m,last_m+L].
    // coverage_tick is the last *physical* signed-detector event certified
    // by the calling detector, including events below Ceiling. The final
    // drained flag is valid only after its finalizer succeeded. Neither
    // argument can be fabricated by this isolated experimental scheduler.
    // Returns false without modifying prior accepted batch on bad input.
    [[nodiscard]] bool load(std::span<const Event> complete_events,
        std::int64_t first_m, std::int64_t last_m,
        std::int64_t coverage_tick, bool detector_drained)
    {
        if(first_m < -62 || last_m < first_m
            || last_m>std::numeric_limits<std::int64_t>::max()-horizon_
            || complete_events.size()>kMaxBatchEvents
            || (accepted_ && first_m!=next_tick_)
            || (!accepted_ && first_m!=-62)
            || (!detector_drained && coverage_tick<last_m+horizon_))
            return false;

        const auto cutoff=last_m+horizon_;
        std::int64_t prior=first_m-1;
        for(const auto& event:complete_events) {
            if(!event.valid || !std::isfinite(event.peak)
                || event.peak<0.0 || event.tick<=prior
                || event.tick>cutoff)
                return false;
            prior=event.tick;
        }

        // selector.prepare validates the entire event span before it
        // builds replacements and commits only completed temporary caches.
        // A second deep copy of the OLD accepted cache is unnecessary
        // and can dominate a short 64-frame offline batch.
        if(!selector_.prepare(complete_events))return false;
        accepted_=true;
        batch_last_tick_=last_m;
        batch_coverage_tick_=coverage_tick;
        detector_drained_=detector_drained;
        batches_+=1;
        peak_events_=std::max(peak_events_,selector_.cached_event_count());
        return true;
    }

    [[nodiscard]] std::optional<Choice>
    choose(std::int64_t tick,double current_peak)
    {
        if(!accepted_ || tick!=next_tick_ || tick>batch_last_tick_
            || (!detector_drained_
                && batch_coverage_tick_<tick+horizon_))
            return std::nullopt;
        auto result=selector_.choose_monotone(tick,current_peak);
        if(!result)return std::nullopt;
        ++next_tick_;
        return result;
    }

    // Test-only comparator: same complete certified events, same
    // controller, original independent binary upper_bounds each tick.
    [[nodiscard]] std::optional<Choice>
    choose_binary_search_for_test(
        std::int64_t tick,double current_peak)
    {
        if(!accepted_ || tick!=next_tick_ || tick>batch_last_tick_
            || (!detector_drained_
                && batch_coverage_tick_<tick+horizon_))
            return std::nullopt;
        auto result=selector_.choose(tick,current_peak);
        if(!result)return std::nullopt;
        ++next_tick_;
        return result;
    }

    [[nodiscard]] std::size_t cached_events() const noexcept {
        return selector_.cached_event_count();
    }
    [[nodiscard]] std::size_t high_water_events() const noexcept {
        return peak_events_;
    }
    [[nodiscard]] std::size_t weight_build_count() const noexcept {
        return selector_.weight_build_count();
    }
    [[nodiscard]] std::size_t batches() const noexcept {return batches_;}
    [[nodiscard]] std::int64_t next_tick() const noexcept {return next_tick_;}

private:
    M17RollingBatchSelector(M17LookaheadSelector selector,
        std::int64_t horizon) noexcept
        : selector_(std::move(selector)),horizon_(horizon) {}

    M17LookaheadSelector selector_;
    std::int64_t horizon_{0};
    std::int64_t next_tick_{-62};
    std::int64_t batch_last_tick_{-63};
    std::int64_t batch_coverage_tick_{-63};
    std::size_t peak_events_{0};
    std::size_t batches_{0};
    bool detector_drained_{false};
    bool accepted_{false};
};

} // namespace rgsml::dsp::internal::experimental
