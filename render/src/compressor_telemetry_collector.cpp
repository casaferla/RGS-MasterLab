#include <rgsml/render/compressor_telemetry_collector.hpp>

#include <algorithm>
#include <cmath>

namespace rgsml::render {

CompressorTelemetryCollector::CompressorTelemetryCollector(
    std::int64_t start_frame,
    std::int64_t total_frames,
    std::uint32_t sample_rate_hz,
    rgsml::audio::ChannelLayout channel_layout,
    rgsml::dsp::CompressorChannelLink channel_link,
    rgsml::dsp::ModuleInstanceId instance_id,
    std::uint64_t chain_revision)
    : start_frame_(start_frame)
    , total_frames_(total_frames)
    , sample_rate_hz_(sample_rate_hz)
    , channel_layout_(channel_layout)
    , channel_link_(channel_link)
    , instance_id_(instance_id)
    , chain_revision_(chain_revision)
{
    const bool is_mono = (channel_layout_ == rgsml::audio::ChannelLayout::MONO_C);
    const bool is_dual_mono = (!is_mono && channel_link_ == rgsml::dsp::CompressorChannelLink::DUAL_MONO);
    num_lanes_ = is_dual_mono ? 2U : 1U;

    if (total_frames_ <= 0 || sample_rate_hz_ == 0) {
        return;
    }

    const std::int64_t end_frame = start_frame_ + total_frames_;
    const std::int64_t start_bucket = (start_frame_ * 200) / sample_rate_hz_;
    const std::int64_t end_bucket = ((end_frame - 1) * 200) / sample_rate_hz_;
    const std::size_t num_buckets = static_cast<std::size_t>(std::max<std::int64_t>(1, end_bucket - start_bucket + 1));

    lanes_.resize(num_lanes_);
    sums_.resize(num_lanes_, std::vector<double>(num_buckets, 0.0));
    counts_.resize(num_lanes_, std::vector<std::uint32_t>(num_buckets, 0U));

    for (std::size_t lane = 0; lane < num_lanes_; ++lane) {
        lanes_[lane].buckets.reserve(num_buckets);
        for (std::size_t b = 0; b < num_buckets; ++b) {
            const std::int64_t b_idx = start_bucket + static_cast<std::int64_t>(b);
            const std::int64_t b_start = (b_idx * sample_rate_hz_) / 200;
            const std::int64_t b_end = ((b_idx + 1) * sample_rate_hz_) / 200;

            CompressorTelemetryBucket bucket{instance_id_};
            bucket.begin_frame = b_start;
            bucket.end_frame = b_end;
            bucket.frame_count = static_cast<std::uint32_t>(b_end - b_start);
            bucket.valid = true;
            lanes_[lane].buckets.push_back(bucket);
        }
    }
}

void CompressorTelemetryCollector::push_frame_telemetry(
    std::int64_t absolute_frame,
    const rgsml::dsp::CompressorFrameTelemetry& frame) noexcept
{
    if (sample_rate_hz_ == 0 || lanes_.empty() || lanes_[0].buckets.empty()) return;
    const std::int64_t b_idx = (absolute_frame * 200) / sample_rate_hz_;
    const std::int64_t start_bucket = (start_frame_ * 200) / sample_rate_hz_;
    const std::int64_t rel_b = b_idx - start_bucket;

    if (rel_b < 0 || rel_b >= static_cast<std::int64_t>(lanes_[0].buckets.size())) {
        return;
    }

    const auto b_u = static_cast<std::size_t>(rel_b);

    // Verify L/R applied gain equality for linked stereo realizations
    if (num_lanes_ == 1 && channel_layout_ == rgsml::audio::ChannelLayout::STEREO_LR) {
        if (std::abs(frame.applied_reduction_db_ch0 - frame.applied_reduction_db_ch1) > 1e-5) {
            has_invalid_sample_ = true;
        }
    }

    for (std::size_t lane = 0; lane < num_lanes_; ++lane) {
        const double red_db = (lane == 0) ? frame.applied_reduction_db_ch0 : frame.applied_reduction_db_ch1;
        const double gain_lin = (lane == 0) ? frame.linear_gain_ch0 : frame.linear_gain_ch1;

        auto& bucket = lanes_[lane].buckets[b_u];

        if (!std::isfinite(red_db) || red_db < 0.0 || !std::isfinite(gain_lin)) {
            bucket.valid = false;
            has_invalid_sample_ = true;
            continue;
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

        if (red_db > 1e-6 || gain_lin < 1.0 - 1e-6) {
            bucket.attenuated_frame_count++;
        }
    }
}

CompressorTelemetrySidecar CompressorTelemetryCollector::build_sidecar()
{
    CompressorTelemetrySidecar sidecar{instance_id_};
    sidecar.valid = !has_invalid_sample_;
    sidecar.status = has_invalid_sample_ ? CompressorTelemetryStatus::UNAVAILABLE : CompressorTelemetryStatus::OK;
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
