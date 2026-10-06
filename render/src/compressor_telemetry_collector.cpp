#include <rgsml/render/compressor_telemetry_collector.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace rgsml::render {

namespace {

[[nodiscard]] inline bool safe_add_int64(std::int64_t a, std::int64_t b, std::int64_t& result) noexcept {
    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
        (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b)) {
        return false;
    }
    result = a + b;
    return true;
}

[[nodiscard]] inline bool safe_mul_int64(std::int64_t a, std::int64_t b, std::int64_t& result) noexcept {
    if (a == 0 || b == 0) {
        result = 0;
        return true;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > std::numeric_limits<std::int64_t>::max() / b) return false;
        } else {
            if (b < std::numeric_limits<std::int64_t>::min() / a) return false;
        }
    } else {
        if (b > 0) {
            if (a < std::numeric_limits<std::int64_t>::min() / b) return false;
        } else {
            if (a != 0 && b < std::numeric_limits<std::int64_t>::max() / a) return false;
        }
    }
    result = a * b;
    return true;
}

}  // namespace

CompressorTelemetryCollector::CompressorTelemetryCollector(
    std::int64_t start_frame,
    std::int64_t total_frames,
    std::uint32_t sample_rate_hz,
    rgsml::audio::ChannelLayout channel_layout,
    rgsml::dsp::CompressorChannelLink channel_link,
    rgsml::dsp::ModuleInstanceId instance_id,
    std::uint64_t chain_revision,
    std::size_t max_memory_bytes)
    : start_frame_(start_frame)
    , total_frames_(total_frames)
    , sample_rate_hz_(sample_rate_hz)
    , channel_layout_(channel_layout)
    , channel_link_(channel_link)
    , instance_id_(instance_id)
    , chain_revision_(chain_revision)
    , expected_next_frame_(start_frame)
    , pushed_frame_count_(0)
{
    const bool is_mono = (channel_layout_ == rgsml::audio::ChannelLayout::MONO_C);
    const bool is_dual_mono = (!is_mono && channel_link_ == rgsml::dsp::CompressorChannelLink::DUAL_MONO);
    num_lanes_ = is_dual_mono ? 2U : 1U;

    if (total_frames_ <= 0 || sample_rate_hz_ == 0) {
        telemetry_failed_ = true;
        return;
    }

    std::int64_t end_frame = 0;
    if (!safe_add_int64(start_frame_, total_frames_, end_frame)) {
        telemetry_failed_ = true;
        return;
    }

    std::int64_t start_mul = 0, start_add = 0;
    if (!safe_mul_int64(start_frame_, 200, start_mul) || !safe_add_int64(start_mul, 199, start_add)) {
        telemetry_failed_ = true;
        return;
    }
    const std::int64_t start_bucket = start_add / static_cast<std::int64_t>(sample_rate_hz_);

    std::int64_t end_minus_1 = 0, end_mul = 0, end_add = 0;
    if (!safe_add_int64(end_frame, -1, end_minus_1) ||
        !safe_mul_int64(end_minus_1, 200, end_mul) ||
        !safe_add_int64(end_mul, 199, end_add)) {
        telemetry_failed_ = true;
        return;
    }
    const std::int64_t end_bucket = end_add / static_cast<std::int64_t>(sample_rate_hz_);

    if (end_bucket < start_bucket) {
        telemetry_failed_ = true;
        return;
    }
    const std::size_t num_buckets = static_cast<std::size_t>(end_bucket - start_bucket + 1);

    // Memory budget check against max_memory_bytes
    const std::size_t size_per_bucket = sizeof(CompressorTelemetryBucket) + sizeof(double) + sizeof(std::uint32_t);
    if (num_buckets > std::numeric_limits<std::size_t>::max() / (num_lanes_ * size_per_bucket)) {
        telemetry_failed_ = true;
        return;
    }
    const std::size_t estimated_bytes = num_lanes_ * num_buckets * size_per_bucket;

    if (estimated_bytes > max_memory_bytes) {
        telemetry_failed_ = true;
        return;
    }

    try {
        lanes_.resize(num_lanes_);
        sums_.resize(num_lanes_, std::vector<double>(num_buckets, 0.0));
        counts_.resize(num_lanes_, std::vector<std::uint32_t>(num_buckets, 0U));

        for (std::size_t lane = 0; lane < num_lanes_; ++lane) {
            lanes_[lane].buckets.reserve(num_buckets);
            for (std::size_t b = 0; b < num_buckets; ++b) {
                const std::int64_t b_idx = start_bucket + static_cast<std::int64_t>(b);
                std::int64_t b_mul = 0, b_plus_1 = 0, b_plus_1_mul = 0;
                if (!safe_mul_int64(b_idx, static_cast<std::int64_t>(sample_rate_hz_), b_mul) ||
                    !safe_add_int64(b_idx, 1, b_plus_1) ||
                    !safe_mul_int64(b_plus_1, static_cast<std::int64_t>(sample_rate_hz_), b_plus_1_mul)) {
                    telemetry_failed_ = true;
                    return;
                }
                const std::int64_t canonical_b_start = b_mul / 200;
                const std::int64_t canonical_b_end = b_plus_1_mul / 200;

                const std::int64_t b_start = std::max(canonical_b_start, start_frame_);
                const std::int64_t b_end = std::min(canonical_b_end, end_frame);

                CompressorTelemetryBucket bucket{instance_id_};
                bucket.begin_frame = b_start;
                bucket.end_frame = b_end;
                bucket.frame_count = static_cast<std::uint32_t>(std::max<std::int64_t>(0, b_end - b_start));
                bucket.valid = true;
                lanes_[lane].buckets.push_back(bucket);
            }
        }
    } catch (...) {
        telemetry_failed_ = true;
        lanes_.clear();
        sums_.clear();
        counts_.clear();
    }
}

void CompressorTelemetryCollector::push_frame_telemetry(
    std::int64_t absolute_frame,
    const rgsml::dsp::CompressorFrameTelemetry& frame) noexcept
{
    if (telemetry_failed_) {
        return;
    }

    // Strict contiguity and coverage checks:
    if (absolute_frame != expected_next_frame_) {
        telemetry_failed_ = true;
        return;
    }
    expected_next_frame_++;
    pushed_frame_count_++;

    std::int64_t f_mul = 0, f_add = 0, s_mul = 0, s_add = 0;
    if (!safe_mul_int64(absolute_frame, 200, f_mul) ||
        !safe_add_int64(f_mul, 199, f_add) ||
        !safe_mul_int64(start_frame_, 200, s_mul) ||
        !safe_add_int64(s_mul, 199, s_add)) {
        telemetry_failed_ = true;
        return;
    }

    const std::int64_t b_idx = f_add / static_cast<std::int64_t>(sample_rate_hz_);
    const std::int64_t start_bucket = s_add / static_cast<std::int64_t>(sample_rate_hz_);
    const std::int64_t rel_b = b_idx - start_bucket;

    if (rel_b < 0 || rel_b >= static_cast<std::int64_t>(lanes_[0].buckets.size())) {
        telemetry_failed_ = true;
        return;
    }

    const auto b_u = static_cast<std::size_t>(rel_b);

    auto& target_bucket = lanes_[0].buckets[b_u];
    if (absolute_frame < target_bucket.begin_frame || absolute_frame >= target_bucket.end_frame) {
        telemetry_failed_ = true;
        return;
    }

    // Verify L/R equivalence for LINKED stereo layout
    if (num_lanes_ == 1 && channel_layout_ == rgsml::audio::ChannelLayout::STEREO_LR) {
        if (frame.applied_reduction_db_ch0 != frame.applied_reduction_db_ch1 ||
            frame.linear_gain_ch0 != frame.linear_gain_ch1) {
            telemetry_failed_ = true;
            return;
        }
    }

    for (std::size_t lane = 0; lane < num_lanes_; ++lane) {
        const double red_db = (lane == 0) ? frame.applied_reduction_db_ch0 : frame.applied_reduction_db_ch1;
        const double gain_lin = (lane == 0) ? frame.linear_gain_ch0 : frame.linear_gain_ch1;

        auto& bucket = lanes_[lane].buckets[b_u];

        // Domain validation
        if (!std::isfinite(red_db) || red_db < 0.0 || !std::isfinite(gain_lin) || gain_lin <= 0.0 || gain_lin > 1.0) {
            bucket.valid = false;
            telemetry_failed_ = true;
            return;
        }

        // ULP-scale double-precision consistency check: gain_lin vs 10^(-red_db / 20)
        const double expected_gain = std::pow(10.0, -red_db / 20.0);
        const double diff = std::abs(gain_lin - expected_gain);
        const double tol = 128.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, expected_gain);
        if (diff > tol) {
            bucket.valid = false;
            telemetry_failed_ = true;
            return;
        }

        // Contradiction checks
        if (red_db == 0.0 && gain_lin < 1.0) {
            bucket.valid = false;
            telemetry_failed_ = true;
            return;
        }
        if (red_db > 0.0 && gain_lin == 1.0) {
            bucket.valid = false;
            telemetry_failed_ = true;
            return;
        }

        const auto offset = static_cast<std::uint32_t>(absolute_frame - bucket.begin_frame);

        if (counts_[lane][b_u] == 0U) {
            bucket.peak_reduction_db = red_db;
            bucket.peak_offset_frames = offset;
        } else if (red_db > bucket.peak_reduction_db) {
            bucket.peak_reduction_db = red_db;
            bucket.peak_offset_frames = offset;
        }

        sums_[lane][b_u] += red_db;
        counts_[lane][b_u]++;
        bucket.end_reduction_db = red_db;

        // Activity: linear_gain < 1.0 strictly!
        if (gain_lin < 1.0) {
            bucket.attenuated_frame_count++;
        }
    }
}

CompressorTelemetrySidecar CompressorTelemetryCollector::build_sidecar()
{
    CompressorTelemetrySidecar sidecar{instance_id_};

    if (telemetry_failed_ || pushed_frame_count_ != total_frames_) {
        sidecar.valid = false;
        sidecar.status = CompressorTelemetryStatus::UNAVAILABLE;
        sidecar.channel_layout = channel_layout_;
        sidecar.sample_rate_hz = sample_rate_hz_;
        sidecar.chain_revision = chain_revision_;

        for (auto& lane : lanes_) {
            for (auto& bucket : lane.buckets) {
                bucket.valid = false;
            }
        }
        sidecar.channel_lanes = std::move(lanes_);
        return sidecar;
    }

    sidecar.valid = true;
    sidecar.status = CompressorTelemetryStatus::OK;
    sidecar.channel_layout = channel_layout_;
    sidecar.sample_rate_hz = sample_rate_hz_;
    sidecar.chain_revision = chain_revision_;

    for (std::size_t lane = 0; lane < num_lanes_; ++lane) {
        for (std::size_t b = 0; b < lanes_[lane].buckets.size(); ++b) {
            auto& bucket = lanes_[lane].buckets[b];
            const std::uint32_t cnt = counts_[lane][b];
            if (cnt > 0) {
                bucket.mean_reduction_db = sums_[lane][b] / static_cast<double>(cnt);
            }
        }
    }

    sidecar.channel_lanes = std::move(lanes_);
    return sidecar;
}

}  // namespace rgsml::render
