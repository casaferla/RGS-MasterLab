#include <rgsml/render/compressor_telemetry_history.hpp>

#include <algorithm>
#include <limits>

namespace rgsml::render {

CompressorTelemetryHistory::CompressorTelemetryHistory(
    std::size_t max_buckets_per_lane,
    std::size_t max_memory_bytes) noexcept
    : max_buckets_per_lane_(std::max<std::size_t>(1U, max_buckets_per_lane))
    , max_memory_bytes_(std::min(max_memory_bytes, kAbsoluteMaxMemoryBytes))
{
}

void CompressorTelemetryHistory::clear() noexcept
{
    valid_ = true;
    status_ = CompressorTelemetryStatus::OK;
    instance_id_ = std::nullopt;
    chain_revision_ = 0;
    realization_id_ = std::nullopt;
    lanes_.clear();
}

std::size_t CompressorTelemetryHistory::bucket_count(std::size_t lane) const noexcept
{
    if (lane >= lanes_.size()) {
        return 0U;
    }
    return lanes_[lane].size();
}

const std::vector<CompressorTelemetryBucket>&
CompressorTelemetryHistory::lane_buckets(std::size_t lane) const noexcept
{
    static const std::vector<CompressorTelemetryBucket> kEmptyLane;
    if (lane >= lanes_.size()) {
        return kEmptyLane;
    }
    return lanes_[lane];
}

void CompressorTelemetryHistory::push_sidecar(const CompressorTelemetrySidecar& sidecar)
{
    if (!sidecar.valid
        || sidecar.status != CompressorTelemetryStatus::OK
        || !sidecar.realization_id.has_value()) {
        valid_ = false;
        status_ = CompressorTelemetryStatus::UNAVAILABLE;
        return;
    }

    // Check realization / chain revision / instance alignment:
    // If instance ID, chain revision, or realization ID changes, clear history for the new identity sequence
    if (instance_id_ != sidecar.module_instance_id ||
        chain_revision_ != sidecar.chain_revision ||
        realization_id_ != sidecar.realization_id) {
        lanes_.clear();
        instance_id_ = sidecar.module_instance_id;
        chain_revision_ = sidecar.chain_revision;
        realization_id_ = sidecar.realization_id;
        valid_ = true;
        status_ = CompressorTelemetryStatus::OK;
    }

    if (sidecar.channel_lanes.empty()) {
        return;
    }

    if (lanes_.size() < sidecar.channel_lanes.size()) {
        lanes_.resize(sidecar.channel_lanes.size());
    }

    for (std::size_t lane = 0; lane < sidecar.channel_lanes.size(); ++lane) {
        const auto& in_buckets = sidecar.channel_lanes[lane].buckets;
        auto& target_lane = lanes_[lane];

        for (const auto& bucket : in_buckets) {
            if (!bucket.valid
                || !bucket.realization_id.has_value()
                || bucket.realization_id != sidecar.realization_id) {
                valid_ = false;
                status_ = CompressorTelemetryStatus::UNAVAILABLE;
                return;
            }

            target_lane.push_back(bucket);

            // Bounded FIFO eviction
            if (target_lane.size() > max_buckets_per_lane_) {
                target_lane.erase(target_lane.begin());
            }
        }
    }

    // Memory budget validation
    std::size_t total_buckets = 0;
    for (const auto& lane_vec : lanes_) {
        total_buckets += lane_vec.size();
    }

    const std::size_t estimated_bytes = total_buckets * sizeof(CompressorTelemetryBucket);
    if (estimated_bytes > max_memory_bytes_) {
        valid_ = false;
        status_ = CompressorTelemetryStatus::UNAVAILABLE;
        lanes_.clear();
    }
}

void CompressorTelemetryHistory::push_bucket(std::size_t lane, const CompressorTelemetryBucket& bucket)
{
    if (!bucket.valid || !bucket.realization_id.has_value()) {
        valid_ = false;
        status_ = CompressorTelemetryStatus::UNAVAILABLE;
        return;
    }

    if (instance_id_ != bucket.module_instance_id || realization_id_ != bucket.realization_id) {
        lanes_.clear();
        instance_id_ = bucket.module_instance_id;
        realization_id_ = bucket.realization_id;
        valid_ = true;
        status_ = CompressorTelemetryStatus::OK;
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
        status_ = CompressorTelemetryStatus::UNAVAILABLE;
        lanes_.clear();
    }
}

}  // namespace rgsml::render
