#pragma once
// Research-only full-track C01 Activity sidecar: exact executed gain sample
// extrema, no Qt/FFT, no extra envelope or DSP. The observed C01 process
// block calls this success-only sink with caller-preallocated scratch.
#include "../../dsp/src/internal/dynamic_eq_c01_engine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace rgsml::research::dynamic_ui {
using rgsml::dsp::internal::C01FrameActivitySample;
using rgsml::dsp::internal::IC01ActivitySink;
using rgsml::dsp::EqRouting;

struct C01ActivityBandBucket final {
    rgsml::core::Uuid band_id{};
    EqRouting routing{EqRouting::STEREO};
    double gain_min_db{};
    double gain_max_db{};
    double gain_last_db{};
    double effective_gain_last_db{};
    double signed_target_last_db{};
    double detector_power_max{};
    double detector_power_last{};
    bool detector_level_defined_last{false};
    double detector_level_last_dbfs{};
};
struct C01ActivityBucket final {
    std::uint64_t frame_begin{};
    std::uint64_t frame_end_exclusive{};
    std::size_t band_count{};
    std::array<C01ActivityBandBucket,6> bands{};
};
enum class C01HistoryStatus : std::uint8_t {
    OK,INVALID_CONFIGURATION,EMPTY_OR_INVALID_BANDS,NONFINITE_OBSERVATION,
    FRAME_DISCONTINUITY,BAND_IDENTITY_CHANGED,CAPACITY_EXCEEDED,
    ALREADY_FINISHED
};

class C01ActivityHistoryCollector final:public IC01ActivitySink {
public:
    static constexpr std::size_t kMaxBuckets=60000U; // 20 min at 50 Hz.
    explicit C01ActivityHistoryCollector(
        std::uint32_t sample_rate_hz,
        std::uint64_t first_absolute_frame=0,
        std::size_t max_buckets=kMaxBuckets)
        :bucket_frames_(sample_rate_hz/50U),
         next_frame_(first_absolute_frame), capacity_(max_buckets) {
        if((sample_rate_hz!=44100 && sample_rate_hz!=48000 &&
            sample_rate_hz!=96000 && sample_rate_hz!=192000)
           || bucket_frames_==0 || capacity_==0 || capacity_>kMaxBuckets) {
            status_=C01HistoryStatus::INVALID_CONFIGURATION;
            return;
        }
        buckets_.reserve(capacity_); // outside the processor callback.
    }
    void on_successful_block(
        std::uint64_t absolute_first_frame,
        std::span<const C01FrameActivitySample> observed) noexcept override {
        if(status_!=C01HistoryStatus::OK || finished_)return;
        if(absolute_first_frame!=next_frame_) {
            status_=C01HistoryStatus::FRAME_DISCONTINUITY;return;
        }
        for(const auto& frame:observed) {
            if(!push(frame))return;
        }
    }
    [[nodiscard]] bool finish() noexcept {
        if(status_!=C01HistoryStatus::OK)return false;
        if(finished_)return fail(C01HistoryStatus::ALREADY_FINISHED);
        if(partial_count_>0) {
            if(buckets_.size()==capacity_)
                return fail(C01HistoryStatus::CAPACITY_EXCEEDED);
            buckets_.push_back(partial_);
            partial_count_=0;
        }
        finished_=true;
        return true;
    }
    // Called off the audio path. No sample after the audible frame boundary.
    [[nodiscard]] std::vector<C01ActivityBucket> visible_window(
        std::uint64_t audible_frame,std::uint64_t window_frames) const {
        std::vector<C01ActivityBucket> out;
        if(status_!=C01HistoryStatus::OK || !finished_ || window_frames==0)
            return out;
        const auto start=audible_frame>window_frames
            ?audible_frame-window_frames:0U;
        for(const auto& b:buckets_)
            if(b.frame_end_exclusive>start &&
               b.frame_end_exclusive<=audible_frame)
                out.push_back(b);
        return out;
    }
    [[nodiscard]] C01HistoryStatus status() const noexcept {return status_;}
    [[nodiscard]] bool finished() const noexcept {return finished_;}
    [[nodiscard]] std::size_t bucket_count() const noexcept {return buckets_.size();}
    [[nodiscard]] std::uint64_t next_frame() const noexcept {return next_frame_;}
    [[nodiscard]] std::uint32_t bucket_frames() const noexcept {return bucket_frames_;}

private:
    [[nodiscard]] bool fail(C01HistoryStatus s) noexcept {
        status_=s;return false;
    }
    [[nodiscard]] bool push(const C01FrameActivitySample& frame) noexcept {
        if(status_!=C01HistoryStatus::OK)return false;
        if(frame.band_count==0 || frame.band_count>6)
            return fail(C01HistoryStatus::EMPTY_OR_INVALID_BANDS);
        if(next_frame_==std::numeric_limits<std::uint64_t>::max())
            return fail(C01HistoryStatus::FRAME_DISCONTINUITY);
        if(partial_count_==0 && buckets_.size()==capacity_)
            return fail(C01HistoryStatus::CAPACITY_EXCEEDED);
        if(partial_count_==0) {
            partial_=C01ActivityBucket{};
            partial_.frame_begin=next_frame_;
            partial_.band_count=frame.band_count;
        }else if(partial_.band_count!=frame.band_count) {
            return fail(C01HistoryStatus::BAND_IDENTITY_CHANGED);
        }
        for(std::size_t j=0;j<frame.band_count;++j) {
            const auto& source=frame.bands[j];
            auto& dest=partial_.bands[j];
            if(source.band_id.is_nil() || !std::isfinite(source.linked_power)
               || source.linked_power<0.0
               || source.has_detector_level!=(source.linked_power>0.0)
               || !std::isfinite(source.detector_level_dbfs)
               || !std::isfinite(source.unsigned_target_db)
               || !std::isfinite(source.signed_target_dynamic_gain_db)
               || !std::isfinite(source.applied_dynamic_gain_db)
               || !std::isfinite(source.effective_gain_db)
               || source.unsigned_target_db<0.0
               || source.unsigned_target_db>18.000001
               || std::abs(source.applied_dynamic_gain_db)>18.000001
               || std::abs(source.effective_gain_db)>18.000001)
                return fail(C01HistoryStatus::NONFINITE_OBSERVATION);
            if(partial_count_==0) {
                dest.band_id=source.band_id;
                dest.routing=source.routing;
                dest.gain_min_db=source.applied_dynamic_gain_db;
                dest.gain_max_db=source.applied_dynamic_gain_db;
                dest.detector_power_max=source.linked_power;
            }else if(dest.band_id!=source.band_id
                     ||dest.routing!=source.routing) {
                return fail(C01HistoryStatus::BAND_IDENTITY_CHANGED);
            }else{
                dest.gain_min_db=std::min(
                    dest.gain_min_db,source.applied_dynamic_gain_db);
                dest.gain_max_db=std::max(
                    dest.gain_max_db,source.applied_dynamic_gain_db);
                dest.detector_power_max=std::max(
                    dest.detector_power_max,source.linked_power);
            }
            dest.gain_last_db=source.applied_dynamic_gain_db;
            dest.effective_gain_last_db=source.effective_gain_db;
            dest.signed_target_last_db=source.signed_target_dynamic_gain_db;
            dest.detector_power_last=source.linked_power;
            dest.detector_level_defined_last=source.has_detector_level;
            dest.detector_level_last_dbfs=source.detector_level_dbfs;
        }
        ++next_frame_;
        ++partial_count_;
        partial_.frame_end_exclusive=next_frame_;
        if(partial_count_==bucket_frames_) {
            buckets_.push_back(partial_); // pre-reserved; no heap allocation.
            partial_count_=0;
        }
        return true;
    }
    std::uint32_t bucket_frames_{};
    std::uint64_t next_frame_{};
    std::size_t capacity_{};
    std::size_t partial_count_{};
    bool finished_{false};
    C01HistoryStatus status_{C01HistoryStatus::OK};
    C01ActivityBucket partial_{};
    std::vector<C01ActivityBucket> buckets_;
};
} // namespace rgsml::research::dynamic_ui
