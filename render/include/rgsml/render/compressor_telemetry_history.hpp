#pragma once

#include <rgsml/core/realization_identity.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/render/render_result.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::render {

/**
 * @brief Production telemetry history buffer maintaining bounded recent Compressor observations.
 *
 * Enforces a FIFO sliding window bounded to approximately 5 seconds of display history
 * (~1,000 buckets per lane at ~200 Hz bucket cadence), strictly aligned with Stage 3A
 * audible realization identity and bounded within the 128 MiB hard memory budget.
 */
class CompressorTelemetryHistory final {
public:
    static constexpr std::size_t kDefaultMaxBucketsPerLane = 1000U; // ~5 seconds @ 200 Hz
    static constexpr std::size_t kAbsoluteMaxMemoryBytes = 128U * 1024U * 1024U;

    explicit CompressorTelemetryHistory(
        std::size_t max_buckets_per_lane = kDefaultMaxBucketsPerLane,
        std::size_t max_memory_bytes = kAbsoluteMaxMemoryBytes) noexcept;

    ~CompressorTelemetryHistory() = default;

    CompressorTelemetryHistory(CompressorTelemetryHistory&&) noexcept = default;
    CompressorTelemetryHistory& operator=(CompressorTelemetryHistory&&) noexcept = default;
    CompressorTelemetryHistory(const CompressorTelemetryHistory&) = default;
    CompressorTelemetryHistory& operator=(const CompressorTelemetryHistory&) = default;

    void push_sidecar(const CompressorTelemetrySidecar& sidecar);

    void push_bucket(std::size_t lane, const CompressorTelemetryBucket& bucket);

    void clear() noexcept;

    [[nodiscard]] bool valid() const noexcept { return valid_ && status_ == CompressorTelemetryStatus::OK; }
    [[nodiscard]] CompressorTelemetryStatus status() const noexcept { return status_; }
    [[nodiscard]] std::size_t num_lanes() const noexcept { return lanes_.size(); }
    [[nodiscard]] std::size_t bucket_count(std::size_t lane = 0) const noexcept;
    [[nodiscard]] const std::vector<CompressorTelemetryBucket>& lane_buckets(std::size_t lane) const noexcept;
    [[nodiscard]] std::optional<rgsml::core::RealizationId> realization_id() const noexcept { return realization_id_; }
    [[nodiscard]] std::uint64_t chain_revision() const noexcept { return chain_revision_; }
    [[nodiscard]] std::optional<rgsml::dsp::ModuleInstanceId> module_instance_id() const noexcept { return instance_id_; }
    [[nodiscard]] std::size_t max_buckets_per_lane() const noexcept { return max_buckets_per_lane_; }
    [[nodiscard]] std::size_t max_memory_bytes() const noexcept { return max_memory_bytes_; }

private:
    std::size_t max_buckets_per_lane_{kDefaultMaxBucketsPerLane};
    std::size_t max_memory_bytes_{kAbsoluteMaxMemoryBytes};

    bool valid_{true};
    CompressorTelemetryStatus status_{CompressorTelemetryStatus::OK};
    std::optional<rgsml::dsp::ModuleInstanceId> instance_id_{std::nullopt};
    std::uint64_t chain_revision_{0};
    std::optional<rgsml::core::RealizationId> realization_id_{std::nullopt};

    std::vector<std::vector<CompressorTelemetryBucket>> lanes_;
};

}  // namespace rgsml::render
