#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/audio_playback_service.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::analysis {
class LiveSpectrumAnalyzer;
}

namespace rgsml::platform::windows::internal {

enum class DeviceSampleFormat {
    IEEE_F32,
    PCM_S16,
};

struct DeviceFormat final {
    int sampleRateHz;
    std::size_t channelCount;
    DeviceSampleFormat sampleFormat;
    bool srcApplied;

    [[nodiscard]] bool operator==(const DeviceFormat&) const = default;
};

[[nodiscard]] core::Result<DeviceFormat> select_device_format(
    const audio::AudioFormat& sourceFormat,
    bool exactFloat32Supported,
    bool exactPcm16Supported,
    bool pairedFloat32Supported,
    bool pairedPcm16Supported);

[[nodiscard]] core::Result<std::vector<std::byte>> encode_device_block(
    audio::AudioBufferView source,
    DeviceSampleFormat destinationFormat);

enum class OutputState {
    STOPPED,
    ACTIVE,
    SUSPENDED,
    IDLE,
    ERROR,
};

// Private playback-only seam. Implementations own a bounded byte queue; the
// real-time backend callback may only drain that queue.
class IPlaybackOutput {
public:
    virtual ~IPlaybackOutput() noexcept = default;

    [[nodiscard]] virtual std::size_t writable_bytes() const noexcept = 0;
    [[nodiscard]] virtual std::size_t queued_bytes() const noexcept = 0;
    [[nodiscard]] virtual core::Result<std::size_t> enqueue(
        std::span<const std::byte> bytes) = 0;
    virtual void clear_queue() noexcept = 0;
    [[nodiscard]] virtual core::Status start() = 0;
    [[nodiscard]] virtual core::Status suspend() = 0;
    [[nodiscard]] virtual core::Status resume() = 0;
    [[nodiscard]] virtual core::Status stop() = 0;
    [[nodiscard]] virtual std::int64_t processed_frames() const noexcept = 0;
    [[nodiscard]] virtual OutputState state() const noexcept = 0;
    [[nodiscard]] virtual std::optional<core::Error> error() const = 0;
    virtual void attach_analyzer(
        analysis::LiveSpectrumAnalyzer* /*analyzer*/) {}
};

class IPlaybackSource;

// C2D1 conservative observation seam. This is NOT a coverage ledger, public ABI,
// or authority to report recent True Peak: until chronological ownership and
// exact source support are proven, every result remains incomplete.
enum class C2CoverageProbeReason {
    NOT_MONITORED,
    STALE_GENERATION,
    GENERATION_EXHAUSTED,
    CLOCK_BEFORE_BASELINE,
    CLOCK_AHEAD_OF_GENERATED,
    UNPROVEN_HANDOFF,
    UNPROVEN_SRC,
    UNPROVEN_LOOP,
    MISSING_CHRONOLOGICAL_LEDGER,
};

struct C2CoverageProbe final {
    std::uint64_t generation{0};
    std::int64_t processedWatermarkExclusive{0};
    std::int64_t generatedOutputFrames{0};
    bool completeThroughWatermark{false};
    C2CoverageProbeReason reason{C2CoverageProbeReason::NOT_MONITORED};
};

class PlaybackEngine final {
public:
    static constexpr std::int64_t kDecodeBlockFrames = 1024;
    static constexpr std::size_t kMaximumPumpIterations = 16U;

    PlaybackEngine();
    ~PlaybackEngine() noexcept;

    PlaybackEngine(const PlaybackEngine&) = delete;
    PlaybackEngine& operator=(const PlaybackEngine&) = delete;

    [[nodiscard]] core::Status install_candidate(
        std::unique_ptr<audio::WavReader> reader,
        std::unique_ptr<IPlaybackOutput> output,
        DeviceSampleFormat sampleFormat,
        std::optional<audio::PlaybackSampleRateAdapter> rateAdapter =
            std::nullopt);
    [[nodiscard]] core::Status install_pcm_candidate(
        audio::AudioBufferView source,
        std::unique_ptr<IPlaybackOutput> output,
        DeviceSampleFormat sampleFormat,
        std::optional<audio::PlaybackSampleRateAdapter> rateAdapter =
            std::nullopt,
        std::shared_ptr<const void> lifetime = nullptr,
        std::optional<core::RealizationId> realizationId = std::nullopt);
    [[nodiscard]] core::Status handoff_pcm(
        audio::AudioBufferView source,
        std::shared_ptr<const void> lifetime = nullptr,
        std::optional<core::RealizationId> realizationId = std::nullopt);
    [[nodiscard]] core::Status clear();
    [[nodiscard]] core::Status play();
    [[nodiscard]] core::Status pause();
    [[nodiscard]] core::Status stop();
    [[nodiscard]] core::Status seek(core::FrameIndex position);
    [[nodiscard]] core::Status set_loop(std::optional<core::FrameRange> loop);
    [[nodiscard]] core::Result<core::PlaybackSnapshot> snapshot() const;
    // Serialized owner-thread diagnostic only; never called by the audio callback.
    // Incomplete is intentional: no programme interval is yet certified VALID.
    [[nodiscard]] C2CoverageProbe c2_processed_coverage_probe(
        std::optional<std::uint64_t> expectedGeneration = std::nullopt) const noexcept;

    // Called by a bounded owner-thread timer, never by the audio callback.
    void tick();

private:
    // Maps an uninterrupted device-frame timeline to Source frames. A Loop
    // hot-toggle changes only future decoded PCM; already scheduled PCM keeps
    // its original mapping until the sink actually processes it.
    struct LoopEpoch final {
        std::int64_t firstGeneratedFrame{0};
        std::int64_t firstOutputFrame{0};
        std::optional<core::FrameRange> loop;
        bool eligible{false};
        std::uint64_t wrapsBefore{0};
        // Restore this cursor if repeated toggles replace the policy before
        // any additional PCM is generated (ON/OFF must be a true no-op).
        std::int64_t cursorBeforeUncommittedSwitch{0};
    };

    struct PendingHandoff final {
        std::optional<core::RealizationId> oldRealizationId;
        std::optional<core::RealizationId> newRealizationId;
        std::int64_t startProcessedFrame{0};
        std::int64_t endProcessedFrame{0};
        std::optional<std::int64_t> newNumericStartSourceFrame{std::nullopt};
        bool awaitingReplay{false};
    };

    [[nodiscard]] core::Status prefill();
    [[nodiscard]] core::Status pump_once();
    [[nodiscard]] std::int64_t source_to_output_frame(
        std::int64_t sourceFrame) const noexcept;
    [[nodiscard]] std::int64_t output_to_source_frame(
        std::int64_t outputFrame) const noexcept;
    [[nodiscard]] std::int64_t output_boundary() const noexcept;
    [[nodiscard]] std::int64_t current_output_frame() const noexcept;
    [[nodiscard]] std::uint64_t current_loop_wrap_count() const noexcept;
    [[nodiscard]] LoopEpoch audible_loop_epoch(std::int64_t processedFrame) const noexcept;
    [[nodiscard]] std::uint64_t wraps_in_loop_epoch(
        const LoopEpoch& epoch, std::int64_t elapsedFrames) const noexcept;
    [[nodiscard]] core::AudibleRealizationState audible_realization_state() const noexcept;
    [[nodiscard]] core::Status play_internal(bool isNewTraversal);
    [[nodiscard]] bool has_source() const noexcept;
    [[nodiscard]] const audio::AudioFormat& source_format() const noexcept;
    [[nodiscard]] core::Result<core::FrameCount> read_source_frames(
        core::FrameIndex absoluteStart,
        audio::MutableAudioBufferView destination);
    void update_position() noexcept;
    void record_runtime_error(core::Error error) noexcept;
    void reset_queue_state(std::int64_t frame) noexcept;

    std::unique_ptr<IPlaybackSource> source_;
    std::unique_ptr<IPlaybackOutput> output_;
    std::optional<DeviceSampleFormat> sampleFormat_;
    std::optional<audio::PlaybackSampleRateAdapter> rateAdapter_;
    core::PlaybackState state_{core::PlaybackState::NO_SOURCE};
    core::FrameIndex position_{0};
    core::FrameIndex sourceBegin_{0};
    std::optional<core::FrameCount> duration_;
    std::optional<core::FrameCount> outputDuration_;
    std::optional<core::FrameRange> loop_;
    bool loopTraversalEligible_{false};
    std::int64_t scheduledOutputFrame_{0};
    std::int64_t playbackStartOutputFrame_{0};
    std::int64_t generatedOutputFrames_{0};
    LoopEpoch initialLoopEpoch_;
    std::vector<LoopEpoch> scheduledLoopEpochs_;
    std::vector<std::byte> pendingBytes_;
    std::size_t pendingOffset_{0U};
    bool eofScheduled_{false};
    std::optional<core::Error> runtimeError_;
    std::int64_t processedFrameBaseline_{0};
    std::optional<core::RealizationId> activeRealizationId_;
    std::optional<PendingHandoff> pendingHandoff_;
    std::uint64_t traversalSerial_{0};
    std::uint64_t seekSerial_{0};
    std::uint64_t baseLoopWrapCount_{0};
    std::uint64_t c2CoverageGeneration_{0};
    bool c2CoverageGenerationExhausted_{false};
};

}  // namespace rgsml::platform::windows::internal
