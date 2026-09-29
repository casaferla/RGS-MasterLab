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
    SampleEncoding encoding)
{
    const std::uint64_t gen = current_generation_.load(std::memory_order_relaxed);
    const std::uint64_t epoch = current_epoch_.load(std::memory_order_relaxed);
    bool overflow = false;

    ring_.push_pcm_bytes(
        pcmData,
        byteCount,
        sampleRateHz,
        channelCount,
        encoding,
        gen,
        epoch,
        overflow);

    if (overflow) {
        current_epoch_.fetch_add(1, std::memory_order_release);
    }
}

void LiveSpectrumAnalyzer::invalidate_and_clear()
{
    current_epoch_.fetch_add(1, std::memory_order_release);

    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    latest_snapshot_.valid = false;
    latest_snapshot_.dbfs_powers.clear();
}

void LiveSpectrumAnalyzer::set_stream_generation(std::uint64_t generation)
{
    current_generation_.store(generation, std::memory_order_release);
    current_epoch_.fetch_add(1, std::memory_order_release);
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

    ingress_frame_buffer_.resize(4096);
    sliding_window_interleaved_.resize(config_.window_size * config_.channel_count, 0.0f);
    sliding_window_frames_ = 0;
    hop_accumulator_ = 0;
    first_window_processed_ = false;
}

void LiveSpectrumAnalyzer::worker_loop()
{
    while (running_.load(std::memory_order_relaxed)) {
        const std::size_t avail = ring_.available_frames();
        if (avail == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        AnalysisFrame firstFrame;
        std::size_t totalPoppedInBatch = 0;

        if (ring_.pop_frames(1, &firstFrame) > 0) {
            reconfigure_if_needed(ring_.sample_rate_hz(), ring_.channel_count());
            ingress_frame_buffer_[0] = firstFrame;
            totalPoppedInBatch = 1;

            const std::size_t remainingAvail = ring_.available_frames();
            const std::size_t overflowThreshold = config_.window_size + 2 * config_.hop_size;

            if (remainingAvail + 1 > overflowThreshold) {
                const std::size_t targetRemaining = config_.window_size > 0 ? config_.window_size - 1 : 0;
                if (remainingAvail > targetRemaining) {
                    const std::size_t discardCount = remainingAvail - targetRemaining;
                    ring_.pop_frames(discardCount, nullptr);
                }
                sliding_window_frames_ = 0;
                hop_accumulator_ = 0;
                first_window_processed_ = false;
            }

            const std::size_t additionalPopped = ring_.pop_frames(
                ingress_frame_buffer_.size() - totalPoppedInBatch,
                ingress_frame_buffer_.data() + totalPoppedInBatch);
            totalPoppedInBatch += additionalPopped;
        }

        if (totalPoppedInBatch == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        for (std::size_t i = 0; i < totalPoppedInBatch; ++i) {
            const auto& frame = ingress_frame_buffer_[i];

            if (frame.stream_generation != active_window_generation_
                || frame.analysis_epoch != active_window_epoch_) {
                sliding_window_frames_ = 0;
                hop_accumulator_ = 0;
                first_window_processed_ = false;
                active_window_generation_ = frame.stream_generation;
                active_window_epoch_ = frame.analysis_epoch;
            }

            reconfigure_if_needed(ring_.sample_rate_hz(), ring_.channel_count());

            const std::size_t channels = config_.channel_count;
            const std::size_t windowCap = config_.window_size;

            sliding_window_interleaved_[sliding_window_frames_ * channels + 0] = frame.sample_l;
            if (channels > 1) {
                sliding_window_interleaved_[sliding_window_frames_ * channels + 1] = frame.sample_r;
            }
            ++sliding_window_frames_;

            if (sliding_window_frames_ == windowCap) {
                process_window();
                const std::size_t hopSize = config_.hop_size;
                if (windowCap > hopSize) {
                    std::move(
                        sliding_window_interleaved_.begin() + hopSize * channels,
                        sliding_window_interleaved_.end(),
                        sliding_window_interleaved_.begin());
                    sliding_window_frames_ = windowCap - hopSize;
                } else {
                    sliding_window_frames_ = 0;
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

    const std::uint64_t analyzedGen = active_window_generation_;
    const std::uint64_t analyzedEpoch = active_window_epoch_;

    if (analyzedGen != current_generation_.load(std::memory_order_relaxed)
        || analyzedEpoch != current_epoch_.load(std::memory_order_relaxed)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        latest_snapshot_.valid = true;
        latest_snapshot_.stream_generation = analyzedGen;
        latest_snapshot_.analysis_epoch = analyzedEpoch;
        latest_snapshot_.sequence_number = ++sequence_counter_;
        latest_snapshot_.sample_rate_hz = config_.sample_rate_hz;
        latest_snapshot_.point_count = log_grid_.point_count;
        latest_snapshot_.frequencies_hz = log_grid_.node_centers;
        latest_snapshot_.dbfs_powers = dbfs_output_;
    }
}

}  // namespace rgsml::analysis
