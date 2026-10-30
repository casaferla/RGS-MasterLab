#pragma once
#include "final_src_candidate_policy.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <vector>

namespace rgsml::app {
// Already-accepted whole-Prepare observations ONLY. No audio, filenames,
// machine identity, DSP certification or predictions are stored.
struct AcceptedPrepareObservation {
    std::uint64_t invocation{0};
    std::int64_t source_hz{0},master_hz{0},source_frames{0};
    int channels{0};
    double ceiling{0.0};
    std::uint64_t manual_revision{0},terminal_revision{0},
        terminal_parameter_revision{0};
    std::int64_t queue_us{-1},wall_us{-1},src_us{-1},
        m17_us{-1},m16_us{-1};
};
struct AcceptedPrepareHistorySummary {
    AcceptedPrepareObservation last;
    std::size_t exact_revision_count{0};
    std::int64_t minimum_total_us{0},maximum_total_us{0};
};
class AcceptedPrepareObservationHistory {
public:
    [[nodiscard]] static bool eligible(const AcceptedPrepareObservation& x) noexcept {
        return x.invocation>0 && m17_six_rate(x.source_hz)
            && m17_six_rate(x.master_hz) && x.source_frames>0
            && (x.channels==1||x.channels==2) && std::isfinite(x.ceiling)
            && x.ceiling>=-12.0 && x.ceiling<=0.0
            && x.queue_us>=0 && x.wall_us>0
            && x.wall_us<=std::numeric_limits<std::int64_t>::max()-x.queue_us
            && x.m17_us>=0 && x.m16_us>=0
            && (x.source_hz==x.master_hz || x.src_us>=0);
    }
    [[nodiscard]] bool observe(const AcceptedPrepareObservation& x) {
        if(x.invocation<=last_seen_)return false;
        last_seen_=x.invocation;
        if(!eligible(x))return false;
        // Optional diagnostics must never break an already accepted M17
        // owner if the host is low on memory.
        try {
            if(history_.size()>=24)history_.erase(history_.begin());
            history_.push_back(x);
        } catch(const std::bad_alloc&) {
            return false;
        }
        return true;
    }
    // SOURCE changed (including a different file with the same Fs/frames).
    // Keep last-seen serial to reject a late duplicate from an older owner.
    void source_changed() noexcept {history_.clear();}
    [[nodiscard]] std::optional<AcceptedPrepareHistorySummary> latest(
        std::int64_t source_hz,std::int64_t source_frames,int channels,
        std::int64_t master_hz) const noexcept
    {
        const AcceptedPrepareObservation* last=nullptr;
        for(const auto& x:history_)
            if(eligible(x) && x.source_hz==source_hz
               && x.source_frames==source_frames && x.channels==channels
               && x.master_hz==master_hz
               && (!last||x.invocation>last->invocation))last=&x;
        if(!last)return std::nullopt;
        std::size_t count=0;
        auto low=std::numeric_limits<std::int64_t>::max();
        std::int64_t high=0;
        for(const auto& x:history_) {
            if(!eligible(x)||x.source_hz!=last->source_hz
               ||x.master_hz!=last->master_hz
               ||x.source_frames!=last->source_frames
               ||x.channels!=last->channels || x.ceiling!=last->ceiling
               ||x.manual_revision!=last->manual_revision
               ||x.terminal_revision!=last->terminal_revision
               ||x.terminal_parameter_revision!=last->terminal_parameter_revision)
                continue;
            const auto total=x.queue_us+x.wall_us;
            low=std::min(low,total);high=std::max(high,total);++count;
        }
        if(!count)return std::nullopt;
        return AcceptedPrepareHistorySummary{*last,count,low,high};
    }
private:
    std::uint64_t last_seen_{0};
    std::vector<AcceptedPrepareObservation> history_;
};
} // namespace rgsml::app
