#include <rgsml/render/stereo_ms_execution_signature.hpp>

#include <rgsml/core/error.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::render {
namespace {

constexpr std::string_view kTypeId = "rgsml.dsp.stereo-ms";
constexpr std::string_view kAlgorithmVersion = "1.0.0";
constexpr std::string_view kParameterSchemaId =
    "rgsml.dsp.stereo-ms.parameters/1.0.0";

[[nodiscard]] rgsml::core::Error signature_error(std::string message)
{
    return rgsml::core::Error{
        rgsml::core::ErrorCode::InvalidArgument,
        std::move(message),
        {{"category", "INVALID_STEREO_MS_SIGNATURE_INPUT"}}};
}

}  // namespace

rgsml::core::Result<ModuleExecutionSignature>
make_stereo_ms_execution_signature(
    const rgsml::dsp::ModuleDescriptor& descriptor,
    rgsml::dsp::ModuleInstanceId instance_id,
    const rgsml::dsp::StereoMsParameters& p,
    rgsml::audio::ChannelLayout source_channel_layout,
    bool user_bypassed)
{
    using rgsml::core::Result;
    if (descriptor.type_id() != kTypeId
        || descriptor.algorithm_version()
            != std::optional<std::string_view>{kAlgorithmVersion}
        || descriptor.parameter_schema_id()
            != std::optional<std::string_view>{kParameterSchemaId}) {
        return Result<ModuleExecutionSignature>::failure(signature_error(
            "Stereo/M-S execution signature requires the canonical frozen descriptor."));
    }

    if (source_channel_layout != rgsml::audio::ChannelLayout::MONO_C
        && source_channel_layout != rgsml::audio::ChannelLayout::STEREO_LR) {
        return Result<ModuleExecutionSignature>::failure(signature_error(
            "Stereo/M-S execution signature accepts only canonical mono or stereo layout."));
    }

    StereoMsExecutionSignaturePayload payload{};

    // Bypassed module: all six sonic fields are non-effective.
    // Canonical mono: input is bit-identical and all six spatial fields
    // are non-effective even if gain is non-neutral or crossover is active.
    if (!user_bypassed
        && source_channel_layout == rgsml::audio::ChannelLayout::STEREO_LR) {
        payload.mid_gain_db = p.mid_gain_db();
        payload.side_muted = p.side_muted();

        if (!p.side_muted()) {
            payload.side_gain_db = p.side_gain_db();
            payload.mono_bass_mode = p.mono_bass_mode();
            if (p.mono_bass_mode() != rgsml::dsp::MonoBassMode::OFF) {
                payload.mono_bass_cutoff_hz = p.mono_bass_cutoff_hz();
                payload.low_band_width_percent = p.low_band_width_percent();
            }
        }
    }

    return Result<ModuleExecutionSignature>::success(
        ModuleExecutionSignature{
            std::move(instance_id),
            std::string{kTypeId},
            std::string{kAlgorithmVersion},
            std::string{kParameterSchemaId},
            user_bypassed ? ModuleExecutionDisposition::BYPASS_IDENTITY
                          : ModuleExecutionDisposition::PROCESSED,
            std::move(payload)});
}

}  // namespace rgsml::render
