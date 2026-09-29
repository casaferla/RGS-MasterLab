#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include <rgsml/analysis/psd.hpp>

#include <algorithm>
#include <chrono>

namespace rgsml::analysis {

LiveSpectrumAnalyzer::LiveSpectrumAnalyzer()
{
    reconfigure_if_needed(44100, 2);
}

LiveSpectrumAnalyzer::~LiveSpectrumAnalyzer()
{
    stop();
}

void LiveSpectrumAnalyzer::start()
{
    if (running_.exchange(true)) {
        return;
    }
    worker_thread_ = std::thread(&LiveSpectrumAnalyzer::worker_loop, this);
}

void LiveSpectrumAnalyzer::stop()
{
    if (!running_.exchange(false)) {
        return;
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void LiveSpectrumAnalyzer::push_audio_bytes(
    const void* pcmData,
    std::size_t byteCount,
    std::uint32_t sampleRateHz,
    std::uint8_t channelCount,
    SampleEncoding encoding,
    std::uint64_t streamGeneration,
    std::uint64_t analysisEpoch)
{
    ring_.push_pcm_bytes(
        pcmData,
        byteCount,
        sampleRateHz,
        channelCount,
        encoding,
        streamGeneration,
        analysisEpoch);
}

void LiveSpectrumAnalyzer::invalidate_and_clear()
{
    clear_requested_.store(true, std::memory_order_release);
    current_generation_.store(0, std::memory_order_release);
    current_epoch_.fetch_add(1, std::memory_order_release);

    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    latest_snapshot_.valid = false;
    latest_snapshot_.dbfs_powers.clear();
}

void LiveSpectrumAnalyzer::set_stream_generation(std::uint64_t generation)
{
    current_generation_.store(generation, std::memory_order_release);
}

SpectrumSnapshot LiveSpectrumAnalyzer::latest_snapshot() const
{
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return latest_snapshot_;
}

void LiveSpectrumAnalyzer::reconfigure_if_needed(std::uint32_t sampleRateHz, std::size_t channelCount)
{
    if (config_.sample_rate_hz == sampleRateHz && config_.channel_count == channelCount && fft_ != nullptr) {
        return;
    }

    config_ = SpectrumConfig::compute(sampleRateHz, channelCount);
    hann_ = HannWindow::create(config_.window_size);
    fft_ = std::make_unique<Radix2Fft>(config_.fft_size);
    log_grid_ = LogGrid::create(config_.sample_rate_hz, 512);
    temporal_smoother_.configure(static_cast<double>(config_.hop_size), static_cast<double>(config_.sample_rate_hz), log_grid_.point_count);

    const std::size_t halfBins = config_.fft_size / 2 + 1;
    channel0_samples_.resize(config_.window_size, 0.0);
    channel1_samples_.resize(config_.window_size, 0.0);
    channel0_fft_.resize(config_.fft_size);
    channel1_fft_.resize(config_.fft_size);
    bin_power_a2_.resize(halfBins, 0.0);
    node_power_a2_.resize(log_grid_.point_count, 0.0);
    smoothed_frequency_a2_.resize(log_grid_.point_count, 0.0);
    smoothed_temporal_a2_.resize(log_grid_.point_count, 0.0);
    dbfs_output_.resize(log_grid_.point_count, -90.0);

    ingress_float_buffer_.resize(32768 * config_.channel_count);
    sliding_window_interleaved_.resize(config_.window_size * config_.channel_count, 0.0f);
    sliding_window_frames_ = 0;
    hop_accumulator_ = 0;
    first_window_processed_ = false;
}

void LiveSpectrumAnalyzer::worker_loop()
{
    while (running_.load(std::memory_order_relaxed)) {
        if (clear_requested_.exchange(false, std::memory_order_acq_rel)) {
            ring_.clear();
            sliding_window_frames_ = 0;
            hop_accumulator_ = 0;
            first_window_processed_ = false;
        }

        const std::size_t avail = ring_.available_frames();
        if (avail == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        SpscFrameRing::IngressMeta meta;
        const std::size_t popped = ring_.pop_frames_to_float(
            ingress_float_buffer_.size() / std::max<std::size_t>(1, config_.channel_count),
            ingress_float_buffer_.data(),
            meta);

        if (popped == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        reconfigure_if_needed(meta.sample_rate_hz, meta.channel_count);

        const std::size_t channels = config_.channel_count;
        const std::size_t windowCap = config_.window_size;

        const std::size_t overflowThreshold = config_.window_size + 2 * config_.hop_size;
        if (ring_.available_frames() > overflowThreshold) {
            ring_.clear();
            sliding_window_frames_ = 0;
            hop_accumulator_ = 0;
            first_window_processed_ = false;
        }

        std::size_t processedInBatch = 0;
        while (processedInBatch < popped) {
            if (sliding_window_frames_ < windowCap) {
                const std::size_t needed = windowCap - sliding_window_frames_;
                const std::size_t toCopy = std::min(needed, popped - processedInBatch);
                std::copy(
                    ingress_float_buffer_.begin() + processedInBatch * channels,
                    ingress_float_buffer_.begin() + (processedInBatch + toCopy) * channels,
                    sliding_window_interleaved_.begin() + sliding_window_frames_ * channels);
                sliding_window_frames_ += toCopy;
                processedInBatch += toCopy;

                if (sliding_window_frames_ == windowCap) {
                    process_window();
                    hop_accumulator_ = 0;
                }
            } else {
                const std::size_t hopSize = config_.hop_size;
                const std::size_t neededForHop = hopSize - hop_accumulator_;
                const std::size_t toCopy = std::min(neededForHop, popped - processedInBatch);

                // Shift left by toCopy frames
                std::move(
                    sliding_window_interleaved_.begin() + toCopy * channels,
                    sliding_window_interleaved_.end(),
                    sliding_window_interleaved_.begin());

                // Append new frames
                std::copy(
                    ingress_float_buffer_.begin() + processedInBatch * channels,
                    ingress_float_buffer_.begin() + (processedInBatch + toCopy) * channels,
                    sliding_window_interleaved_.end() - toCopy * channels);

                sliding_window_frames_ = windowCap;
                hop_accumulator_ += toCopy;
                processedInBatch += toCopy;

                if (hop_accumulator_ >= hopSize) {
                    process_window();
                    hop_accumulator_ = 0;
                }
            }
        }
    }
}

void LiveSpectrumAnalyzer::process_window()
{
    const std::size_t channels = config_.channel_count;
    const std::size_t windowSize = config_.window_size;

    for (std::size_t i = 0; i < windowSize; ++i) {
        channel0_samples_[i] = static_cast<double>(sliding_window_interleaved_[i * channels + 0]) * hann_.window[i];
        if (channels > 1) {
            channel1_samples_[i] = static_cast<double>(sliding_window_interleaved_[i * channels + 1]) * hann_.window[i];
        }
    }

    fft_->forward(channel0_samples_.data(), windowSize, channel0_fft_.data());
    if (channels > 1) {
        fft_->forward(channel1_samples_.data(), windowSize, channel1_fft_.data());
    } else {
        channel1_fft_.clear();
    }

    compute_live_display_power(
        channel0_fft_,
        channels > 1 ? channel1_fft_ : std::span<const std::complex<double>>{},
        hann_.sum_w,
        bin_power_a2_);

    reduce_to_log_grid(bin_power_a2_, static_cast<double>(config_.sample_rate_hz), log_grid_, node_power_a2_);
    smooth_frequency_triangular(node_power_a2_, smoothed_frequency_a2_);

    const bool freshRestart = !first_window_processed_;
    temporal_smoother_.process(smoothed_frequency_a2_, smoothed_temporal_a2_, freshRestart);
    first_window_processed_ = true;

    power_to_dbfs(smoothed_temporal_a2_, dbfs_output_);

    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        latest_snapshot_.valid = true;
        latest_snapshot_.stream_generation = current_generation_.load(std::memory_order_relaxed);
        latest_snapshot_.analysis_epoch = current_epoch_.load(std::memory_order_relaxed);
        latest_snapshot_.sequence_number = ++sequence_counter_;
        latest_snapshot_.sample_rate_hz = config_.sample_rate_hz;
        latest_snapshot_.point_count = log_grid_.point_count;
        latest_snapshot_.frequencies_hz = log_grid_.node_centers;
        latest_snapshot_.dbfs_powers = dbfs_output_;
    }
}

}  // namespace rgsml::analysis
