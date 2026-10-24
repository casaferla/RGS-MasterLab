#pragma once

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/core/audio_playback_service.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
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

// Future monitoring-only route: derive a device PCM rate from an observed
// *preferred* Windows format, never change the M17/M18 MASTER rate. This is
// capability selection, NOT the PCM resampler, playback install or proof of
// a physical DAC clock. The observed preference is not a measured clock.
[[nodiscard]] core::Result<DeviceFormat> select_monitoring_device_format(
    const audio::AudioFormat& immutableMasterFormat,
    int observedPreferredRateHz,
    bool preferredFloat32Supported,
    bool preferredPcm16Supported);

// Private MONITOR route only: a prepared device-rate PCM may start/resume
// only on the same opaque Windows device id, with the same preferred rate
// and still-supported exact format. Not a physical DAC clock measurement.
[[nodiscard]] inline bool monitoring_endpoint_is_current(
    std::string_view preparedId,int preparedRate,
    std::string_view currentId,int currentRate,
    bool exactFormatStillSupported) noexcept
{
    return !preparedId.empty() && !currentId.empty()
        && preparedId==currentId && preparedRate>0
        && preparedRate==currentRate && exactFormatStillSupported;
}

// The Windows MONITOR installer must bind the produced device PCM to
// the precise endpoint observed at its last preflight; device sample rate
// equality alone is insufficient (two endpoints may both advertise 48 kHz).
// Portable internal PlaybackEngine smoke tests intentionally have no token.
[[nodiscard]] inline bool monitoring_candidate_matches_endpoint(
    const audio::MonitoringPcmCandidate& candidate,
    std::string_view currentDeviceId,
    int currentPreferredRate,
    bool exactFormatStillSupported) noexcept
{
    return monitoring_endpoint_is_current(
        candidate.windows_prepared_endpoint_id,
        static_cast<int>(candidate.device_rate.value()),
        currentDeviceId,currentPreferredRate,exactFormatStillSupported);
}

// Terminal PCM is already at its output rate. Exact-format selection
// cannot silently fall back to the 44.1/48 kHz playback SRC.
[[nodiscard]] core::Result<DeviceFormat> select_terminal_output_device_format(
    const audio::AudioFormat& terminalFormat,
    bool exactFloat32Supported,
    bool exactPcm16Supported);

// Private M17 rate probe: only six documented DSP rates are allowed.
// Preferred is device-advertised and exact-format supported; it is NOT proof
// of the physical Windows shared-mixer clock or qualification of playback.
[[nodiscard]] core::Result<core::SampleRate> select_preferred_terminal_output_rate(
    int preferredRateHz, audio::ChannelLayout layout,
    bool exactFloat32Supported, bool exactPcm16Supported);

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
    CLOCK_AHEAD_OF_ACCEPTED,
    ACCEPTED_LEDGER_OVERFLOW,
    UNPROVEN_HANDOFF,
    UNPROVEN_SRC,
    UNPROVEN_LOOP,
    MISSING_CHRONOLOGICAL_LEDGER,
};

// Bounded owner-stamped ENQUEUE observations, NOT proof that frames were
// consumed or that the source is the canonical PROCESSED monitoring target.
struct C2AcceptedOutputRun final {
    std::uint64_t generation{0};
    std::int64_t outputBegin{0};
    std::int64_t outputEnd{0};
    std::optional<std::int64_t> sourceBegin;
    std::optional<std::int64_t> sourceEnd;
    // Generation-stamped SOURCE-independent absolute SRC OUTPUT interval.
    // Never interpreted as 1:1 source coverage or a live True Peak value.
    std::optional<std::int64_t> srcOutputBegin;
    std::optional<std::int64_t> srcOutputEnd;
    std::optional<core::RealizationId> realizationId;
    bool exactOneToOne{false};
    bool loopTraversalProven{false};
    std::uint64_t loopWrapCount{0};
    bool handoffPureNewProven{false};
    C2CoverageProbeReason gapReason{C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER};
};

struct C2CoverageProbe final {
    std::uint64_t generation{0};
    std::int64_t processedWatermarkExclusive{0};
    // Old already-consumed ledger entries may be safely pruned.
    std::int64_t retainedOutputBegin{0};
    std::int64_t generatedOutputFrames{0};
    std::int64_t acceptedOutputFrames{0};
    std::size_t acceptedRunCount{0};
    bool acceptedLedgerOverflow{false};
    bool completeThroughWatermark{false};
    C2CoverageProbeReason reason{C2CoverageProbeReason::NOT_MONITORED};
};

// Diagnostic ONLY: after one uninterrupted SRC OUTPUT_RATE traversal
// beginning at an independently retained absolute OUTPUT seek origin,
// identifies a canonical SOURCE interval excluded from ALL future FIR windows. This does not certify that those samples were heard,
// a PROCESSED target, a 100ms True Peak window or any LIVE value.
struct C2SrcRetirementObservation final {
    std::uint64_t generation{0};
    std::int64_t consumedOutputExclusive{0};
    std::optional<std::int64_t> noFutureSourcePrefixExclusive;
    // Absolute SOURCE origin of the NEW consumed seek epoch, never inferred
    // from the current GUI cursor or sink watermark.
    std::optional<std::int64_t> firstSourceFrame;
    // New source traversal begins at this actually consumed OUTPUT frame;
    // explicit loop identity never derives from a GUI loop checkbox alone.
    std::int64_t consumedOutputBegin{0};
    std::uint64_t loopWrapCount{0};
    bool loopTraversalProven{false};
    std::optional<core::RealizationId> realizationId;
    std::int64_t sourceRateHz{0};
    std::int64_t outputRateHz{0};
    C2CoverageProbeReason reason{C2CoverageProbeReason::NOT_MONITORED};
};

// C2D4: fixed-capacity, owner-thread observation of the *consumed* prefix.
// SOURCE_CANDIDATE is NEVER a certified PROCESSED target or True-Peak value:
// owner key/chain revision, target and complete FIR support remain unbound.
// The result is a detached snapshot; no span into mutable queue bookkeeping.
enum class C2ConsumedSliceKind {
    SOURCE_CANDIDATE_NOT_QUALIFIED,
    GAP_UNPROVEN,
    GAP_LOST_PROVENANCE,
};

struct C2ConsumedSlice final {
    std::uint64_t generation{0};
    std::int64_t outputBegin{0};
    std::int64_t outputEnd{0};
    C2ConsumedSliceKind kind{C2ConsumedSliceKind::GAP_UNPROVEN};
    std::optional<std::int64_t> sourceBegin;
    std::optional<std::int64_t> sourceEnd;
    std::optional<core::RealizationId> realizationId;
    C2CoverageProbeReason reason{C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER};
    bool loopTraversalProven{false};
    std::uint64_t loopWrapCount{0};
    bool handoffPureNewProven{false};
};

struct C2ConsumedPrefix final {
    static constexpr std::size_t kMaximumSlices = 129U;
    std::uint64_t generation{0};
    std::int64_t processedWatermarkExclusive{0};
    // Bounded represented SUFFIX, not false evidence for discarded history.
    std::int64_t retainedOutputBegin{0};
    std::array<C2ConsumedSlice, kMaximumSlices> slices{};
    std::size_t size{0};
    bool partitionRepresented{false};
    bool certifiedForLiveTruePeak{false}; // MUST remain false in C2D4
    C2CoverageProbeReason reason{C2CoverageProbeReason::NOT_MONITORED};
    [[nodiscard]] std::span<const C2ConsumedSlice> view() const noexcept {
        return {slices.data(), size};
    }
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
    // Private M17 candidate-only input: no implicit SRC or SOURCE alias.
    [[nodiscard]] core::Status install_terminal_output_pcm_candidate(
        audio::AudioBufferView source,
        std::unique_ptr<IPlaybackOutput> output,
        DeviceSampleFormat sampleFormat,
        std::shared_ptr<const void> lifetime,
        core::RealizationId identity,
        core::SampleRate actualDeviceRate);
    // PROTOTYPE monitor-only OUTPUT_RATE PCM, distinct from terminal M17
    // PCM and source-domain playback. The caller supplies a separately
    // allocated device-rate buffer with a lifetime owner; there is NO M17
    // realization identity and NO certified LIVE true-peak coverage.
    // Transport coordinates are device PCM frames until a verified
    // master-to-device frame map exists. Not exposed to UI.
    [[nodiscard]] core::Status install_monitoring_output_pcm_candidate(
        audio::AudioBufferView devicePcm,
        std::unique_ptr<IPlaybackOutput> output,
        DeviceSampleFormat sampleFormat,
        std::shared_ptr<const void> lifetime,
        core::SampleRate actualDeviceRate);
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
    // Diagnostics on the serialized owner thread; span invalid after next tick/reset.
    // Runs represent only *accepted* whole device frames, never true LIVE TP.
    [[nodiscard]] std::span<const C2AcceptedOutputRun>
    c2_accepted_output_runs() const noexcept { return c2AcceptedRuns_; }
    // Read-only owner-thread sink-consumed SRC retirement observation.
    // Fails closed on seek/loop/handoff, clock gaps, owner changes or
    // pruned history. Never promotes an SRC GAP to an M16 LIVE candidate.
    [[nodiscard]] C2SrcRetirementObservation c2_src_retirement_observation(
        std::optional<std::uint64_t> expectedGeneration = std::nullopt) const noexcept;
    // No allocations, no callback/DSP work, bounded to 128 accepted events
    // and one explicit missing-provenance tail. Does not certify target owner.
    [[nodiscard]] C2ConsumedPrefix c2_consumed_prefix(
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

    [[nodiscard]] core::Status install_pcm_candidate_accepted(
        audio::AudioBufferView source,
        std::unique_ptr<IPlaybackOutput> output,
        DeviceSampleFormat sampleFormat,
        std::optional<audio::PlaybackSampleRateAdapter> rateAdapter,
        std::shared_ptr<const void> lifetime,
        std::optional<core::RealizationId> realizationId,
        bool terminalOutputDomain);
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
    void c2_record_accepted_frames(std::size_t oldComplete,
        std::size_t newComplete) noexcept;

    std::unique_ptr<IPlaybackSource> source_;
    std::unique_ptr<IPlaybackOutput> output_;
    std::optional<DeviceSampleFormat> sampleFormat_;
    std::optional<audio::PlaybackSampleRateAdapter> rateAdapter_;
    bool terminalOutputDomain_{false}; // no historical M16-LIVE source proof
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
    static constexpr std::size_t kC2MaximumAcceptedRuns = 128U;
    std::vector<C2AcceptedOutputRun> c2AcceptedRuns_;
    std::int64_t c2AcceptedOutputFrames_{0};
    std::int64_t c2RetainedOutputBegin_{0};
    bool c2AcceptedLedgerOverflow_{false};
    bool c2AcceptedContainsUnprovenHandoff_{false};
    std::int64_t c2PendingSourceBegin_{0};
    std::optional<core::RealizationId> c2PendingOwner_;
    bool c2PendingExactMapping_{false};
    bool c2PendingLoopTraversalProven_{false};
    bool c2PendingHandoffPureNew_{false};
    C2CoverageProbeReason c2PendingGapReason_{C2CoverageProbeReason::MISSING_CHRONOLOGICAL_LEDGER};
    std::uint64_t c2PendingLoopWrapCount_{0};
};

}  // namespace rgsml::platform::windows::internal
