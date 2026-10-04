#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

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

using ModuleExecutionSignaturePayload = std::variant<
    GainExecutionSignaturePayload,
    ParametricEqExecutionSignaturePayload,
    CompressorExecutionSignaturePayload>;

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

private:
    friend rgsml::core::Result<RenderResult> render_preview(
        const RenderRequest& request,
        const rgsml::dsp::ModuleRegistry& registry);

    RenderResult(
        rgsml::audio::AudioBuffer buffer,
        rgsml::core::FrameRange render_window,
        rgsml::audio::FrameDomainId frame_domain_id,
        std::uint64_t chain_revision,
        std::vector<ModuleExecutionSignature> signatures) noexcept;

    rgsml::audio::AudioBuffer buffer_;
    rgsml::core::FrameRange render_window_;
    rgsml::audio::FrameDomainId frame_domain_id_;
    std::uint64_t chain_revision_;
    std::vector<ModuleExecutionSignature> signatures_;
};

}  // namespace rgsml::render
