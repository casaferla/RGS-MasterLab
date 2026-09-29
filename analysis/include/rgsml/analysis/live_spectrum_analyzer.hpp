#ifndef RGSML_ANALYSIS_LIVE_SPECTRUM_ANALYZER_HPP
#define RGSML_ANALYSIS_LIVE_SPECTRUM_ANALYZER_HPP

#include <rgsml/analysis/fft.hpp>
#include <rgsml/analysis/hann.hpp>
#include <rgsml/analysis/log_binning.hpp>
#include <rgsml/analysis/smoothing.hpp>
#include <rgsml/analysis/spectrum_config.hpp>
#include <rgsml/analysis/spectrum_snapshot.hpp>
#include <rgsml/analysis/spsc_ring.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace rgsml::analysis {

class LiveSpectrumAnalyzer final {
public:
    LiveSpectrumAnalyzer();
    ~LiveSpectrumAnalyzer();

    void start();
    void stop();

    // Lock-free push from audio callback. Automatically applies analyzer's current generation & epoch.
    void push_audio_bytes(
        const void* pcmData,
        std::size_t byteCount,
        std::uint32_t sampleRateHz,
        std::uint8_t channelCount,
        SampleEncoding encoding);

    void invalidate_and_clear();
    void set_stream_generation(std::uint64_t generation);

    [[nodiscard]] SpectrumSnapshot latest_snapshot() const;
    [[nodiscard]] std::uint64_t current_generation() const noexcept { return current_generation_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t current_epoch() const noexcept { return current_epoch_.load(std::memory_order_relaxed); }

private:
    void worker_loop();
    void reconfigure_if_needed(std::uint32_t sampleRateHz, std::size_t channelCount);
    void process_window();

    std::atomic<bool> running_{false};
    std::thread worker_thread_;

    SpscFrameRing ring_{32768};

    std::atomic<std::uint64_t> current_generation_{1};
    std::atomic<std::uint64_t> current_epoch_{1};

    SpectrumConfig config_;
    HannWindow hann_;
    std::unique_ptr<Radix2Fft> fft_;
    LogGrid log_grid_;
    TemporalSmoother temporal_smoother_;

    // Small bounded consumer batch buffer (4096 frames = 98 KiB)
    std::vector<AnalysisFrame> ingress_frame_buffer_;
    std::vector<double> channel0_samples_;
    std::vector<double> channel1_samples_;
    std::vector<std::complex<double>> channel0_fft_;
    std::vector<std::complex<double>> channel1_fft_;
    std::vector<double> bin_power_a2_;
    std::vector<double> node_power_a2_;
    std::vector<double> smoothed_frequency_a2_;
    std::vector<double> smoothed_temporal_a2_;
    std::vector<double> dbfs_output_;

    std::vector<float> sliding_window_interleaved_;
    std::size_t sliding_window_frames_{0};
    std::size_t hop_accumulator_{0};

    std::uint64_t active_window_generation_{0};
    std::uint64_t active_window_epoch_{0};

    mutable std::mutex snapshot_mutex_;
    SpectrumSnapshot latest_snapshot_;
    std::uint64_t sequence_counter_{0};

    bool first_window_processed_{false};
};

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_LIVE_SPECTRUM_ANALYZER_HPP
