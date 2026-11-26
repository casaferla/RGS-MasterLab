#pragma once
// Render-local, opt-in observation of the REAL C00 engine's already computed
// g_attack_db / g_sustain_db. This is NOT an estimator or a second DSP.
// The executing DSP must call push() after successful sample computation.
// This class never touches audio samples or grants playback authority.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace rgsml::render::linked_activity {

struct AppliedGainBucket final {
    std::uint64_t frame_begin{};
    std::uint64_t frame_end_exclusive{};
    double attack_min_db{};
    double attack_max_db{};
    double attack_last_db{};
    double sustain_min_db{};
    double sustain_max_db{};
    double sustain_last_db{};
};

enum class ObservationStatus : std::uint8_t {
    OK, INVALID_CONFIGURATION, INVALID_SAMPLE, FRAME_DISCONTINUITY,
    CAPACITY_EXCEEDED, ALREADY_FINISHED
};

class TransientActivityCollector final {
public:
    // 50 buckets/second (20 ms), with exact min/max and last actual gain.
    // Default cap: 20 minutes at all supported rates, about 3.84 MiB.
    // Allocation happens only during construction, outside DSP process().
    static constexpr std::size_t kMaxBuckets = 60000U;
    explicit TransientActivityCollector(std::uint32_t sample_rate_hz,
                                        std::uint64_t first_absolute_frame = 0,
                                        std::size_t max_buckets = kMaxBuckets)
        : sample_rate_hz_(sample_rate_hz),
          bucket_frames_(sample_rate_hz / 50U),
          next_frame_(first_absolute_frame),
          capacity_(max_buckets) {
        if ((sample_rate_hz != 44100U && sample_rate_hz != 48000U
             && sample_rate_hz != 88200U && sample_rate_hz != 96000U
             && sample_rate_hz != 176400U && sample_rate_hz != 192000U)
            || capacity_ == 0 || capacity_ > kMaxBuckets
            || bucket_frames_ == 0U) {
            status_ = ObservationStatus::INVALID_CONFIGURATION;
            return;
        }
        completed_.reserve(capacity_);
    }

    // Called only from the actual processor's observed gain values; no
    // allocations, no callbacks, no clocks, no second envelope calculation.
    // Observation errors fail the sidecar closed, NEVER the audio processor.
    [[nodiscard]] bool push(std::uint64_t frame, double attack_db,
                            double sustain_db) noexcept {
        if (status_ != ObservationStatus::OK) return false;
        if (finished_) return fail(ObservationStatus::ALREADY_FINISHED);
        if (frame != next_frame_ || frame == std::numeric_limits<std::uint64_t>::max())
            return fail(ObservationStatus::FRAME_DISCONTINUITY);
        if (!std::isfinite(attack_db) || !std::isfinite(sustain_db)
            || std::abs(attack_db) > 12.000001
            || std::abs(sustain_db) > 12.000001)
            return fail(ObservationStatus::INVALID_SAMPLE);
        if (partial_count_ == 0U) {
            if (completed_.size() == capacity_)
                return fail(ObservationStatus::CAPACITY_EXCEEDED);
            partial_ = {frame, frame + 1, attack_db, attack_db, attack_db,
                        sustain_db, sustain_db, sustain_db};
        } else {
            partial_.frame_end_exclusive = frame + 1;
            partial_.attack_min_db = std::min(partial_.attack_min_db, attack_db);
            partial_.attack_max_db = std::max(partial_.attack_max_db, attack_db);
            partial_.attack_last_db = attack_db;
            partial_.sustain_min_db = std::min(partial_.sustain_min_db, sustain_db);
            partial_.sustain_max_db = std::max(partial_.sustain_max_db, sustain_db);
            partial_.sustain_last_db = sustain_db;
        }
        ++partial_count_;
        ++next_frame_;
        if (partial_count_ == bucket_frames_) {
            completed_.push_back(partial_); // no allocation: reserved upfront
            partial_count_ = 0U;
        }
        return true;
    }

    [[nodiscard]] bool finish() noexcept {
        if (status_ != ObservationStatus::OK) return false;
        if (finished_) return fail(ObservationStatus::ALREADY_FINISHED);
        if (partial_count_ != 0U) {
            if (completed_.size() == capacity_)
                return fail(ObservationStatus::CAPACITY_EXCEEDED);
            completed_.push_back(partial_);
            partial_count_ = 0U;
        }
        finished_ = true;
        return true;
    }

    // Called ONLY outside the processing path. A seek may request any past
    // window retained in the full-track immutable sidecar. Never reveal a
    // bucket whose end lies after the audible playhead frame boundary.
    [[nodiscard]] std::vector<AppliedGainBucket> visible_window(
        std::uint64_t audible_frame, std::uint64_t window_frames) const {
        std::vector<AppliedGainBucket> result;
        if (status_ != ObservationStatus::OK || !finished_ || window_frames == 0U)
            return result;
        const auto start = audible_frame > window_frames
            ? audible_frame - window_frames : 0U;
        const auto first=std::upper_bound(completed_.begin(),completed_.end(),start,
            [](std::uint64_t frame,const AppliedGainBucket& b) {
                return frame<b.frame_end_exclusive;
            });
        for(auto it=first;it!=completed_.end()&&it->frame_end_exclusive<=audible_frame;++it)
            result.push_back(*it);
        return result;
    }

    [[nodiscard]] ObservationStatus status() const noexcept { return status_; }
    [[nodiscard]] std::uint32_t bucket_frames() const noexcept { return bucket_frames_; }
    [[nodiscard]] std::size_t completed_buckets() const noexcept { return completed_.size(); }
    [[nodiscard]] std::uint64_t next_frame() const noexcept { return next_frame_; }
    [[nodiscard]] bool finished() const noexcept { return finished_; }
    [[nodiscard]] std::size_t reserved_capacity() const noexcept { return completed_.capacity(); }

private:
    [[nodiscard]] bool fail(ObservationStatus reason) noexcept {
        status_ = reason;
        return false;
    }
    std::uint32_t sample_rate_hz_{};
    std::uint32_t bucket_frames_{};
    std::uint64_t next_frame_{};
    std::size_t capacity_{};
    std::size_t partial_count_{};
    bool finished_{false};
    ObservationStatus status_{ObservationStatus::OK};
    AppliedGainBucket partial_{};
    std::vector<AppliedGainBucket> completed_;
};

} // namespace rgsml::render::linked_activity
