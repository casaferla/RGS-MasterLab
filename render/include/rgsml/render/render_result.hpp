#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/realization_identity.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rgsml::dsp {
class ModuleRegistry;
}

namespace rgsml::render {

class RenderRequest;
class RenderResult;

struct CompressorTelemetryBucket final {
    double end_reduction_db{0.0};
    double mean_reduction_db{0.0};
    double peak_reduction_db{0.0};
    std::uint32_t peak_offset_frames{0};
    std::uint32_t attenuated_frame_count{0};
    std::int64_t begin_frame{0};
    std::int64_t end_frame{0};
    std::uint32_t frame_count{0};
    bool valid{true};
    rgsml::dsp::ModuleInstanceId module_instance_id;
    std::optional<rgsml::core::RealizationId> realization_id{std::nullopt};

    explicit CompressorTelemetryBucket(
        rgsml::dsp::ModuleInstanceId id,
        std::optional<rgsml::core::RealizationId> real_id = std::nullopt) noexcept
        : module_instance_id(id), realization_id(real_id) {}

    friend bool operator==(
        const CompressorTelemetryBucket&,
        const CompressorTelemetryBucket&) = default;
};

struct CompressorTelemetryLane final {
    std::vector<CompressorTelemetryBucket> buckets;

    friend bool operator==(
        const CompressorTelemetryLane&,
        const CompressorTelemetryLane&) = default;
};

enum class CompressorTelemetryStatus : std::uint8_t {
    OK,
    BYPASS,
    NOT_AUDITIONED,
    UNAVAILABLE,
};

struct CompressorTelemetrySidecar final {
    bool valid{true};
    CompressorTelemetryStatus status{CompressorTelemetryStatus::OK};
    rgsml::audio::ChannelLayout channel_layout{rgsml::audio::ChannelLayout::STEREO_LR};
    std::uint32_t sample_rate_hz{44100};
    rgsml::dsp::ModuleInstanceId module_instance_id;
    std::uint64_t chain_revision{0};
    std::optional<rgsml::core::RealizationId> realization_id{std::nullopt};
    std::vector<CompressorTelemetryLane> channel_lanes;

    explicit CompressorTelemetrySidecar(
        rgsml::dsp::ModuleInstanceId id,
        std::optional<rgsml::core::RealizationId> real_id = std::nullopt) noexcept
        : module_instance_id(id), realization_id(real_id) {}

    friend bool operator==(
        const CompressorTelemetrySidecar&,
        const CompressorTelemetrySidecar&) = default;
};

// B4a is a bounded, exact PCM excerpt from the output of the named M/S
// stage, BEFORE later DSP nodes. A partial excerpt is never full-window
// correlation/density evidence, and is not yet proof of current audition.
enum class StereoMsStageCaptureStatus : std::uint8_t {
    COMPLETE,
    PARTIAL,
    UNAVAILABLE,
};

struct StereoMsStageOutputSidecar final {
    rgsml::dsp::ModuleInstanceId module_instance_id;
    std::uint64_t chain_revision{0};
    std::optional<rgsml::core::RealizationId> realization_id{std::nullopt};
    rgsml::audio::ChannelLayout channel_layout{rgsml::audio::ChannelLayout::STEREO_LR};
    std::uint32_t sample_rate_hz{0};
    rgsml::audio::FrameDomainId frame_domain_id{rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    StereoMsStageCaptureStatus status{StereoMsStageCaptureStatus::UNAVAILABLE};
    std::int64_t requested_begin_frame{0};
    std::int64_t requested_end_frame{0};
    std::int64_t captured_begin_frame{0};
    std::vector<std::array<double, 2>> output_lr_frames;

    explicit StereoMsStageOutputSidecar(
        rgsml::dsp::ModuleInstanceId instance_id) noexcept
        : module_instance_id(instance_id) {}
};

enum class ModuleExecutionDisposition : std::uint8_t {
    PROCESSED,
    BYPASS_IDENTITY,
};

struct GainExecutionSignaturePayload final {
    double gain_db{0.0};

    friend bool operator==(
        const GainExecutionSignaturePayload&,
        const GainExecutionSignaturePayload&) = default;
};

struct EqBandSignaturePayload final {
    rgsml::dsp::EqFilterType filter_type;
    rgsml::dsp::EqRouting routing;
    rgsml::dsp::EqBandPayload payload;

    friend bool operator==(
        const EqBandSignaturePayload&,
        const EqBandSignaturePayload&) = default;
};

struct ParametricEqExecutionSignaturePayload final {
    std::vector<EqBandSignaturePayload> enabled_bands;

    friend bool operator==(
        const ParametricEqExecutionSignaturePayload&,
        const ParametricEqExecutionSignaturePayload&) = default;
};

struct CompressorExecutionSignaturePayload final {
    rgsml::dsp::CompressorDetectorMode detector_mode;
    std::optional<rgsml::dsp::CompressorChannelLink> channel_link;
    double threshold_dbfs;
    double ratio;
    double knee_db;
    double attack_ms;
    double release_ms;
    double rms_time_constant_ms;
    double look_ahead_ms;
    double mix_percent;
    double makeup_gain_db;

    friend bool operator==(
        const CompressorExecutionSignaturePayload&,
        const CompressorExecutionSignaturePayload&) = default;
};

// Only audio-effective Stereo/M-S parameters participate in sonic
// execution identity. Non-effective fields must be absent (nullopt), not
// zeroed: mono input excludes all spatial fields; Side mute dominates
// Side gain and Mono Bass; OFF excludes crossover controls.
struct StereoMsExecutionSignaturePayload final {
    std::optional<double> mid_gain_db;
    std::optional<double> side_gain_db;
    std::optional<bool> side_muted;
    std::optional<rgsml::dsp::MonoBassMode> mono_bass_mode;
    std::optional<double> mono_bass_cutoff_hz;
    std::optional<double> low_band_width_percent;

    friend bool operator==(const StereoMsExecutionSignaturePayload&,
                           const StereoMsExecutionSignaturePayload&) = default;
};

using ModuleExecutionSignaturePayload = std::variant<
    GainExecutionSignaturePayload,
    ParametricEqExecutionSignaturePayload,
    CompressorExecutionSignaturePayload,
    StereoMsExecutionSignaturePayload>;

struct ModuleExecutionSignature final {
    rgsml::dsp::ModuleInstanceId instance_id;
    std::string type_id;
    std::string algorithm_version;
    std::string parameter_schema_id;
    ModuleExecutionDisposition disposition;
    ModuleExecutionSignaturePayload payload;

    friend bool operator==(
        const ModuleExecutionSignature&,
        const ModuleExecutionSignature&) = default;
};

class RenderResult final {
public:
    RenderResult(RenderResult&&) noexcept = default;
    RenderResult& operator=(RenderResult&&) noexcept = default;
    RenderResult(const RenderResult&) = delete;
    RenderResult& operator=(const RenderResult&) = delete;
    ~RenderResult() = default;

    [[nodiscard]] const rgsml::audio::AudioBuffer& buffer() const noexcept;
    [[nodiscard]] rgsml::audio::AudioBufferView view() const noexcept;
    [[nodiscard]] rgsml::core::FrameRange render_window() const noexcept;
    [[nodiscard]] rgsml::audio::FrameDomainId frame_domain_id() const noexcept;
    [[nodiscard]] std::uint64_t chain_revision() const noexcept;
    [[nodiscard]] const std::vector<ModuleExecutionSignature>&
    signatures() const noexcept;
    [[nodiscard]] const std::optional<CompressorTelemetrySidecar>&
    compressor_telemetry_sidecar() const noexcept;
    [[nodiscard]] const std::vector<StereoMsStageOutputSidecar>&
    stereo_ms_stage_output_sidecars() const noexcept;
    [[nodiscard]] rgsml::core::Status bind_stereo_ms_stage_output_realization_id(
        rgsml::core::RealizationId realization_id) noexcept;
    [[nodiscard]] rgsml::core::Status bind_compressor_telemetry_realization_id(
        rgsml::core::RealizationId realization_id) noexcept;

private:
    friend rgsml::core::Result<RenderResult> render_preview(
        const RenderRequest& request,
        const rgsml::dsp::ModuleRegistry& registry);

    RenderResult(
        rgsml::audio::AudioBuffer buffer,
        rgsml::core::FrameRange render_window,
        rgsml::audio::FrameDomainId frame_domain_id,
        std::uint64_t chain_revision,
        std::vector<ModuleExecutionSignature> signatures,
        std::optional<CompressorTelemetrySidecar> compressor_telemetry_sidecar = std::nullopt,
        std::vector<StereoMsStageOutputSidecar> stereo_ms_stage_output_sidecars = {}) noexcept;

    rgsml::audio::AudioBuffer buffer_;
    rgsml::core::FrameRange render_window_;
    rgsml::audio::FrameDomainId frame_domain_id_;
    std::uint64_t chain_revision_;
    std::vector<ModuleExecutionSignature> signatures_;
    std::optional<CompressorTelemetrySidecar> compressor_telemetry_sidecar_;
    std::vector<StereoMsStageOutputSidecar> stereo_ms_stage_output_sidecars_;
};

}  // namespace rgsml::render
