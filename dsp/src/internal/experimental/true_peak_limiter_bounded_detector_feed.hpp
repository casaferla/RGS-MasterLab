#pragma once

// PO GO 2026-10-22 - isolated M17 streaming detector experiment.
// NOT registered; never included from the frozen production M17 module.
// Owns only retained active signed detector events. Every source frame is
// consumed once by the genuine frozen signed FIR detector.
#include "internal/true_peak_limiter_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::dsp::internal::experimental {

class M17BoundedDetectorFeed final {
public:
    using Event=LimiterDetectorEvent;
    static constexpr std::size_t kMaxRetained=32768U;

    [[nodiscard]] static std::optional<M17BoundedDetectorFeed>
    create(std::int64_t sample_rate,double ceiling_db_tp,
        std::uint32_t channels)
    {
        if(channels!=1U && channels!=2U)return std::nullopt;
        const auto controller=TruePeakLimiterControllerState::create(
            sample_rate,ceiling_db_tp);
        if(!controller)return std::nullopt;
        const double linear=std::pow(10.0,ceiling_db_tp/20.0);
        if(!(linear>0.0)||!std::isfinite(linear))return std::nullopt;
        return M17BoundedDetectorFeed(channels,linear);
    }

    // Failed input (nonfinite PCM, over-capacity event retention, or bad
    // detector output) leaves both detector and retained-event cache intact.
    [[nodiscard]] bool push(double left,double right)
    {
        if(drained_ || !std::isfinite(left)
            || (detector_.channels()==2U && !std::isfinite(right)))
            return false;
        auto candidate=detector_;
        auto quarters=candidate.push_programme_frame(left,right);
        if(!quarters)return false;
        std::array<Event,8> newEvents{};
        std::size_t count=0;
        std::int64_t covered=coverage_tick_;
        for(const auto& q:*quarters.value()) {
            if(!collect(q.preceding_anchor,newEvents,count,covered)
               ||!collect(q.midpoint,newEvents,count,covered))
                return false;
        }
        if(newEvents.size()<count || retained_.size()>kMaxRetained-count)
            return false;
        for(std::size_t i=0;i<count;++i)retained_.push_back(newEvents[i]);
        detector_=std::move(candidate);
        coverage_tick_=covered;
        ++frames_;
        total_active_+=count;
        peak_retained_=std::max(peak_retained_,retained_.size());
        return true;
    }

    [[nodiscard]] bool drain()
    {
        if(drained_ || frames_==0U)return false;
        auto candidate=detector_;
        auto quarters=candidate.finalize_detector();
        if(!quarters)return false;
        std::array<Event,118> newEvents{};
        std::size_t count=0;
        std::int64_t covered=coverage_tick_;
        for(const auto& q:*quarters.value()) {
            if(!collect(q.preceding_anchor,newEvents,count,covered)
               ||!collect(q.midpoint,newEvents,count,covered))
                return false;
        }
        if(newEvents.size()<count || retained_.size()>kMaxRetained-count)
            return false;
        for(std::size_t i=0;i<count;++i)retained_.push_back(newEvents[i]);
        detector_=std::move(candidate);
        coverage_tick_=covered;
        drained_=true;
        total_active_+=count;
        peak_retained_=std::max(peak_retained_,retained_.size());
        return true;
    }

    // Only drop events strictly before the earliest tick of the next
    // certified controller batch, never the inclusive current event.
    void discard_before(std::int64_t tick)
    {
        const auto stop=std::lower_bound(retained_.begin(),retained_.end(),tick,
            [](const Event& e,std::int64_t t){return e.tick<t;});
        retained_.erase(retained_.begin(),stop);
    }

    [[nodiscard]] std::span<const Event> batch(
        std::int64_t first_m,std::int64_t last_m,
        std::int64_t horizon) const
    {
        if(last_m<first_m || horizon<1
           || last_m>std::numeric_limits<std::int64_t>::max()-horizon)
            return {};
        const auto begin=std::lower_bound(
            retained_.begin(),retained_.end(),first_m,
            [](const Event& e,std::int64_t tick) {return e.tick<tick;});
        const auto end=std::upper_bound(
            begin,retained_.end(),last_m+horizon,
            [](std::int64_t tick,const Event& e) {return tick<e.tick;});
        return begin==end ? std::span<const Event>{}
            :std::span<const Event>{
                &*begin,static_cast<std::size_t>(end-begin)};
    }

    [[nodiscard]] double peak_at(std::int64_t tick) const
    {
        const auto it=std::lower_bound(retained_.begin(),retained_.end(),tick,
            [](const Event& e,std::int64_t t){return e.tick<t;});
        return it!=retained_.end() && it->tick==tick ? it->peak : +0.0;
    }

    [[nodiscard]] std::int64_t coverage_tick() const noexcept {
        return coverage_tick_;
    }
    [[nodiscard]] bool drained() const noexcept {return drained_;}
    [[nodiscard]] std::size_t retained_count() const noexcept {
        return retained_.size();
    }
    [[nodiscard]] std::size_t high_water_events() const noexcept {
        return peak_retained_;
    }
    [[nodiscard]] std::uint64_t frames() const noexcept {return frames_;}
    [[nodiscard]] std::size_t total_active_events() const noexcept {
        return total_active_;
    }

private:
    M17BoundedDetectorFeed(std::uint32_t channels,double ceiling)
        :detector_(channels),ceiling_(ceiling) {
        retained_.reserve(kMaxRetained);
    }

    template<std::size_t N>
    [[nodiscard]] bool collect(const Event& event,
        std::array<Event,N>& newEvents,std::size_t& count,
        std::int64_t& covered) const
    {
        if(!event.valid)return true;
        if(event.tick<=covered || !std::isfinite(event.peak)
            || event.peak<0.0)return false;
        covered=event.tick;
        if(event.peak>ceiling_) {
            if(count>=N)return false;
            newEvents[count++]=event;
        }
        return true;
    }

    TruePeakLimiterDetectorState detector_;
    double ceiling_{1.0};
    std::vector<Event> retained_;
    std::int64_t coverage_tick_{-63};
    std::uint64_t frames_{0};
    std::size_t total_active_{0};
    std::size_t peak_retained_{0};
    bool drained_{false};
};

} // namespace rgsml::dsp::internal::experimental
