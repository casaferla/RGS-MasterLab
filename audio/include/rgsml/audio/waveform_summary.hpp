#pragma once

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>
#include <stop_token>
#include <string_view>
#include <vector>

namespace rgsml::audio {

class WavReader;

inline constexpr std::string_view kWaveformSummaryAlgorithmId =
    "rgsml.waveform.summary.minmax-pyramid";
inline constexpr std::string_view kWaveformSummaryAlgorithmVersion = "1.0.0";

class WaveformSummary final {
    struct LevelData;

public:
    struct PeakRange final {
        double minimum;
        double maximum;

        [[nodiscard]] bool operator==(const PeakRange&) const = default;
    };

    class LevelView final {
    public:
        [[nodiscard]] core::FrameCount frames_per_bucket() const noexcept;
        [[nodiscard]] core::FrameCount bucket_count() const noexcept;
        [[nodiscard]] std::size_t channel_count() const noexcept;
        [[nodiscard]] core::Result<PeakRange> peak(
            std::size_t channelIndex,
            core::FrameIndex bucketIndex) const;

    private:
        friend class WaveformSummary;
        LevelView(const LevelData* data, std::size_t channelCount) noexcept;

        const LevelData* data_;
        std::size_t channelCount_;
    };

    static constexpr std::size_t kMaximumBaseBucketsPerChannel = 65'536U;
    static constexpr std::size_t kMaximumUiRangesPerChannel = 4'096U;
    static constexpr std::size_t kMaximumDecodeBlockFrames = 4'096U;
    static constexpr std::size_t kMaximumTotalBucketsPerChannel = 131'071U;

    WaveformSummary(WaveformSummary&&) noexcept = default;
    WaveformSummary& operator=(WaveformSummary&&) noexcept = default;
    WaveformSummary(const WaveformSummary&) = delete;
    WaveformSummary& operator=(const WaveformSummary&) = delete;
    ~WaveformSummary() = default;

    [[nodiscard]] core::SampleRate sample_rate() const noexcept;
    [[nodiscard]] std::size_t channel_count() const noexcept;
    [[nodiscard]] core::FrameCount source_frame_count() const noexcept;
    [[nodiscard]] std::size_t level_count() const noexcept;
    [[nodiscard]] core::Result<LevelView> level(std::size_t levelIndex) const;
    [[nodiscard]] std::size_t payload_bytes() const noexcept;

private:
    friend core::Result<WaveformSummary> build_waveform_summary(
        WavReader& reader,
        std::stop_token stopToken);
    friend core::Result<WaveformSummary> build_waveform_summary_with_block_limit(
        WavReader& reader,
        std::size_t blockFrameLimit,
        std::stop_token stopToken);

    struct LevelData final {
        core::FrameCount framesPerBucket;
        core::FrameCount bucketCount;
        std::vector<PeakRange> peaks;
    };

    WaveformSummary(
        core::SampleRate sampleRate,
        std::size_t channelCount,
        core::FrameCount sourceFrameCount,
        std::vector<LevelData> levels,
        std::size_t payloadBytes) noexcept;

    core::SampleRate sampleRate_;
    std::size_t channelCount_;
    core::FrameCount sourceFrameCount_;
    std::vector<LevelData> levels_;
    std::size_t payloadBytes_;
};

[[nodiscard]] core::Result<WaveformSummary> build_waveform_summary(
    WavReader& reader,
    std::stop_token stopToken = {});

}  // namespace rgsml::audio
