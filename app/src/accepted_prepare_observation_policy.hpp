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
// The actual active chain revisions, not just a matching Ceiling. On an
// A -> B -> A edit the monotonic revision must change, so old ACT timings
// cannot reappear as if the current DSP owner were unchanged.
struct AcceptedPrepareRevisionKey {
    std::uint64_t manual{0};
    std::uint64_t terminal{0};
    std::uint64_t parameters{0};
};
// Exact scalar timings of an already accepted full Prepare. Derived only
// when separately measured, nonoverlapping stages fit the render wall time.
// "Other" is explicitly unattributed; it is NOT a new performance forecast.
struct AcceptedPrepareStageBreakdown {
    std::int64_t src_us;
    std::int64_t other_render_us;
    bool converted;
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
    [[nodiscard]] static std::optional<AcceptedPrepareStageBreakdown>
    stage_breakdown(const AcceptedPrepareObservation& x) noexcept {
        if(!eligible(x))return std::nullopt;
        auto remaining=x.wall_us;
        if(x.m17_us>remaining)return std::nullopt;
        remaining-=x.m17_us;
        if(x.m16_us>remaining)return std::nullopt;
        remaining-=x.m16_us;
        const auto measured_src=std::max<std::int64_t>(0,x.src_us);
        if(measured_src>remaining)return std::nullopt;
        remaining-=measured_src;
        return AcceptedPrepareStageBreakdown{
            measured_src,remaining,x.source_hz!=x.master_hz};
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
        std::int64_t master_hz,
        std::optional<AcceptedPrepareRevisionKey> activeRevisions =
            std::nullopt) const noexcept
    {
        const auto current=[&](const AcceptedPrepareObservation& x) noexcept {
            return !activeRevisions
                ||(x.manual_revision==activeRevisions->manual
                   &&x.terminal_revision==activeRevisions->terminal
                   &&x.terminal_parameter_revision==activeRevisions->parameters);
        };
        const AcceptedPrepareObservation* last=nullptr;
        for(const auto& x:history_)
            if(eligible(x) && current(x) && x.source_hz==source_hz
               && x.source_frames==source_frames && x.channels==channels
               && x.master_hz==master_hz
               && (!last||x.invocation>last->invocation))last=&x;
        if(!last)return std::nullopt;
        std::size_t count=0;
        auto low=std::numeric_limits<std::int64_t>::max();
        std::int64_t high=0;
        for(const auto& x:history_) {
            if(!eligible(x)||!current(x)||x.source_hz!=last->source_hz
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

    
// PROCESS-LOCAL HOST PERFORMANCE LEARNING, using only completed authentic
// M17 full-Prepare timing scalars. It never stores PCM, file/source identity,
// OS identifiers, window/device rate, or user selections. A profile requires
// >=3 genuine accepted whole-Prepare observations of the SAME SOURCE/MASTER
// rate pair, channel count, Ceiling and frozen M17 revisions. It is NOT
// hardware FQ, a guarantee, SRC admission or a persistent host benchmark.
struct AcceptedPrepareHostTotalRange {
    std::int64_t low_us{0},high_us{0};
    std::size_t matched_observations{0};
};
class AcceptedPrepareHostTimingHistory {
public:
    [[nodiscard]] bool observe(const AcceptedPrepareObservation& x) {
        if(x.invocation<=lastSeen_)return false;
        lastSeen_=x.invocation;
        if(!m17_final_src_candidate_admissible(x.source_hz,x.master_hz)
           ||!AcceptedPrepareObservationHistory::stage_breakdown(x))
            return false;
        try {
            if(observed_.size()==48)observed_.erase(observed_.begin());
            observed_.push_back(x);
        } catch(const std::bad_alloc&) {return false;}
        return true;
    }
    [[nodiscard]] std::optional<AcceptedPrepareHostTotalRange> predict(
        std::int64_t source_hz,std::int64_t source_frames,int channels,
        std::int64_t master_hz,double ceiling,
        std::uint64_t terminal_revision,
        std::uint64_t parameter_revision) const noexcept
    {
        if(source_frames<=0 || (channels!=1&&channels!=2)
           ||!std::isfinite(ceiling)
           ||!m17_final_src_candidate_admissible(source_hz,master_hz))
            return std::nullopt;
        long double low=std::numeric_limits<long double>::infinity();
        long double high=0;
        std::size_t count=0;
        for(const auto& x:observed_) {
            if(x.source_hz!=source_hz ||x.master_hz!=master_hz
               ||x.channels!=channels ||x.ceiling!=ceiling
               ||x.terminal_revision!=terminal_revision
               ||x.terminal_parameter_revision!=parameter_revision
               ||!AcceptedPrepareObservationHistory::stage_breakdown(x))
                continue;
            // A huge duration extrapolation is not a trustworthy profile.
            const auto scale=static_cast<long double>(source_frames)
                /static_cast<long double>(x.source_frames);
            if(scale<0.25L || scale>4.0L)continue;
            const auto projected=static_cast<long double>(
                x.queue_us+x.wall_us)*scale;
            if(!std::isfinite(projected)||projected<=0)continue;
            low=std::min(low,projected);
            high=std::max(high,projected);
            ++count;
        }
        if(count<3)return std::nullopt;
        // Expanded diagnostic band, not a statistical confidence interval.
        // Queue and all measured/unknown render stages are INCLUDED because
        // the inputs are complete accepted whole-Prepare wall observations.
        low=std::max(1.0L,std::floor(low/2.0L));
        high=std::ceil(high*2.0L);
        constexpr auto maximum=static_cast<long double>(
            std::numeric_limits<std::int64_t>::max());
        if(!std::isfinite(low)||!std::isfinite(high)
           ||high>=maximum||low>high)return std::nullopt;
        return AcceptedPrepareHostTotalRange{
            static_cast<std::int64_t>(low),
            static_cast<std::int64_t>(high),count};
    }
    // Every genuine ENGINE/algorithm update constructs a new process-local
    // object. A new SOURCE does NOT erase anonymous completed timings.
    void clear() noexcept {observed_.clear();}
private:
    std::uint64_t lastSeen_{0};
    std::vector<AcceptedPrepareObservation> observed_;
};

} // namespace rgsml::app
