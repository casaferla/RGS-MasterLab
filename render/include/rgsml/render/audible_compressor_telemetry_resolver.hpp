#pragma once

#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/realization_identity.hpp>
#include <rgsml/render/compressor_telemetry_history.hpp>
#include <rgsml/render/render_result.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace rgsml::render {

enum class AuditionTarget : std::uint8_t {
    PROCESSED,
    PREPARED,
    GOLD,
};

enum class AudibleTelemetryStatus : std::uint8_t {
    ACTIVE_WET,
    ACTIVE_DRY_ONLY,
    BYPASS,
    NOT_AUDITIONED,
    TRANSITION,
    PAUSED,
    STOPPED,
    UNAVAILABLE,
};

// Aliases for backwards compatibility
constexpr AudibleTelemetryStatus kAudibleStatusOk = AudibleTelemetryStatus::ACTIVE_WET;

struct RegisteredSidecarEntry final {
    CompressorTelemetrySidecar sidecar;
    bool bypass{false};
    double mix_percent{100.0};
};

struct ResolverUpdateContext final {
    rgsml::core::PlaybackSnapshot playback_snapshot;
    AuditionTarget audition_target{AuditionTarget::PROCESSED};
};

/**
 * @brief Dedicated Stage 3B2 Bounded Audible History Buffer.
 *
 * Maintains bounded recent listening history (~5s / ~1000 buckets per lane) that preserves
 * cross-realization audible history (OLD -> gap during TRANSITION -> NEW) without clearing
 * when RealizationId changes.
 */
class AudibleTelemetryHistory final {
public:
    static constexpr std::size_t kDefaultMaxBucketsPerLane = 1000U;
    static constexpr std::size_t kAbsoluteMaxMemoryBytes = 128U * 1024U * 1024U;

    explicit AudibleTelemetryHistory(
        std::size_t max_buckets_per_lane = kDefaultMaxBucketsPerLane,
        std::size_t max_memory_bytes = kAbsoluteMaxMemoryBytes) noexcept
        : max_buckets_per_lane_(std::max<std::size_t>(1U, max_buckets_per_lane))
        , max_memory_bytes_(std::min(max_memory_bytes, kAbsoluteMaxMemoryBytes))
    {
    }

    void push_bucket(std::size_t lane, const CompressorTelemetryBucket& bucket) {
        if (!valid_) {
            return;
        }
        if (!bucket.valid || !bucket.realization_id.has_value()) {
            valid_ = false;
            status_ = AudibleTelemetryStatus::UNAVAILABLE;
            lanes_.clear();
            return;
        }
        if (lanes_.size() <= lane) {
            lanes_.resize(lane + 1U);
        }
        auto& target_lane = lanes_[lane];
        target_lane.push_back(bucket);

        if (target_lane.size() > max_buckets_per_lane_) {
            target_lane.erase(target_lane.begin());
        }

        std::size_t total_buckets = 0;
        for (const auto& lane_vec : lanes_) {
            total_buckets += lane_vec.size();
        }
        if (total_buckets * sizeof(CompressorTelemetryBucket) > max_memory_bytes_) {
            valid_ = false;
            status_ = AudibleTelemetryStatus::UNAVAILABLE;
            lanes_.clear();
        }
    }

    void clear() noexcept {
        valid_ = true;
        status_ = AudibleTelemetryStatus::ACTIVE_WET;
        lanes_.clear();
    }

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] AudibleTelemetryStatus status() const noexcept { return status_; }
    [[nodiscard]] std::size_t num_lanes() const noexcept { return lanes_.size(); }
    [[nodiscard]] std::size_t bucket_count(std::size_t lane = 0) const noexcept {
        if (lane >= lanes_.size()) {
            return 0U;
        }
        return lanes_[lane].size();
    }
    [[nodiscard]] const std::vector<CompressorTelemetryBucket>& lane_buckets(std::size_t lane) const noexcept {
        static const std::vector<CompressorTelemetryBucket> kEmpty;
        if (lane >= lanes_.size()) {
            return kEmpty;
        }
        return lanes_[lane];
    }

private:
    bool valid_{true};
    AudibleTelemetryStatus status_{AudibleTelemetryStatus::ACTIVE_WET};
    std::size_t max_buckets_per_lane_{kDefaultMaxBucketsPerLane};
    std::size_t max_memory_bytes_{kAbsoluteMaxMemoryBytes};
    std::vector<std::vector<CompressorTelemetryBucket>> lanes_;
};

/**
 * @brief Production Stage 3B2 Audible Telemetry Resolver.
 *
 * Resolves Stage 3B1 Compressor telemetry against Stage 3A Audible Realization State
 * and authoritative PlaybackSnapshot control-plane truth (traversalSerial, seekSerial, loopWrapCount).
 */
class AudibleCompressorTelemetryResolver final {
public:
    explicit AudibleCompressorTelemetryResolver(
        std::size_t max_buckets_per_lane = AudibleTelemetryHistory::kDefaultMaxBucketsPerLane,
        std::size_t max_memory_bytes = AudibleTelemetryHistory::kAbsoluteMaxMemoryBytes) noexcept;

    ~AudibleCompressorTelemetryResolver() = default;

    AudibleCompressorTelemetryResolver(AudibleCompressorTelemetryResolver&&) noexcept = default;
    AudibleCompressorTelemetryResolver& operator=(AudibleCompressorTelemetryResolver&&) noexcept = default;
    AudibleCompressorTelemetryResolver(const AudibleCompressorTelemetryResolver&) = delete;
    AudibleCompressorTelemetryResolver& operator=(const AudibleCompressorTelemetryResolver&) = delete;

    /// Register or update a rendered telemetry sidecar for a given realization identity.
    void register_sidecar(
        CompressorTelemetrySidecar sidecar,
        bool bypass = false,
        double mix_percent = 100.0);

    /// Remove a sidecar by realization identity manually.
    void unregister_sidecar(rgsml::core::RealizationId realization_id);

    /// Process an audible update tick according to Stage 3A realization state & playback snapshot.
    void update(const ResolverUpdateContext& context);

    /// Reset all history, registered sidecars, and resolver state.
    void reset() noexcept;

    // Queries / Seams for ViewModel / UI
    [[nodiscard]] bool valid() const noexcept { return status_ != AudibleTelemetryStatus::UNAVAILABLE; }
    [[nodiscard]] AudibleTelemetryStatus status() const noexcept { return status_; }
    [[nodiscard]] bool is_dry_only() const noexcept { return is_dry_only_; }
    [[nodiscard]] bool is_bypassed() const noexcept { return status_ == AudibleTelemetryStatus::BYPASS; }
    [[nodiscard]] std::optional<rgsml::core::RealizationId> active_realization_id() const noexcept { return active_realization_id_; }
    [[nodiscard]] const AudibleTelemetryHistory& history() const noexcept { return history_; }
    [[nodiscard]] std::uint64_t traversal_serial() const noexcept { return traversal_serial_; }
    [[nodiscard]] std::uint64_t seek_serial() const noexcept { return seek_serial_; }
    [[nodiscard]] std::uint64_t loop_wrap_count() const noexcept { return loop_wrap_count_; }
    [[nodiscard]] std::size_t registered_sidecar_count() const noexcept { return registered_sidecars_.size(); }
    [[nodiscard]] std::optional<std::int64_t> last_consumed_frame() const noexcept { return last_consumed_frame_; }
    [[nodiscard]] std::optional<std::size_t> last_consumed_bucket_index() const noexcept { return last_consumed_bucket_index_; }
    [[nodiscard]] std::optional<std::int64_t> eligible_start_frame() const noexcept { return eligible_start_frame_; }

private:
    void prune_sidecars(const rgsml::core::AudibleRealizationState& state, AuditionTarget target);

    AudibleTelemetryHistory history_;
    AudibleTelemetryStatus status_{AudibleTelemetryStatus::UNAVAILABLE};
    bool is_dry_only_{false};
    std::optional<rgsml::core::RealizationId> active_realization_id_{std::nullopt};

    std::map<std::uint64_t, RegisteredSidecarEntry> registered_sidecars_;

    std::optional<rgsml::core::RealizationId> current_consuming_realization_{std::nullopt};
    std::size_t next_bucket_index_{0};
    std::optional<std::int64_t> last_consumed_frame_{std::nullopt};
    std::optional<std::size_t> last_consumed_bucket_index_{std::nullopt};

    // Authoritative Frame Anchor
    std::optional<std::int64_t> eligible_start_frame_{std::nullopt};

    std::uint64_t traversal_serial_{0};
    std::uint64_t seek_serial_{0};
    std::uint64_t loop_wrap_count_{0};
    AuditionTarget previous_audition_target_{AuditionTarget::PROCESSED};
};

}  // namespace rgsml::render
