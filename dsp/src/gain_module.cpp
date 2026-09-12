#include <rgsml/dsp/gain_module.hpp>

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_descriptor.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kGainAlgorithmVersion = "1.0.0";
constexpr auto kGainParameterSchema = "rgsml.dsp.gain.parameters/1.0.0";

[[nodiscard]] rgsml::core::Error gain_error(
    rgsml::core::ErrorCode code,
    std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] bool valid_domain(rgsml::audio::FrameDomainId domain) noexcept
{
    return domain == rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || domain == rgsml::audio::FrameDomainId::OUTPUT_RATE;
}

[[nodiscard]] rgsml::core::Status validate_spec(const DspProcessSpec& spec)
{
    if (!valid_domain(spec.frame_domain_id)
        || spec.maximum_block_frames.value() <= 0
        || (spec.audio_format.channel_layout() != rgsml::audio::ChannelLayout::MONO_C
            && spec.audio_format.channel_layout()
                != rgsml::audio::ChannelLayout::STEREO_LR)) {
        return rgsml::core::Status::failure(gain_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_SPEC",
            "Gain requires a valid binary64 planar mono/stereo process specification."));
    }
    return rgsml::core::Status::success();
}

template <typename Left, typename Right>
[[nodiscard]] bool spans_overlap(Left left, Right right) noexcept
{
    if (left.empty() || right.empty()) {
        return false;
    }
    const auto left_begin = reinterpret_cast<std::uintptr_t>(left.data());
    const auto right_begin = reinterpret_cast<std::uintptr_t>(right.data());
    const auto left_end = left_begin + left.size_bytes();
    const auto right_end = right_begin + right.size_bytes();
    return left_begin < right_end && right_begin < left_end;
}

}  // namespace

struct GainModule::Impl final {
    const ModuleDescriptor* descriptor;
    GainParameters parameters;
    double linear_gain;
    std::optional<DspProcessSpec> prepared_spec;
};

rgsml::core::Result<std::unique_ptr<GainModule>> GainModule::create(
    const ModuleDescriptor& descriptor,
    GainParameters parameters)
{
    const auto algorithm = descriptor.algorithm_version();
    const auto schema = descriptor.parameter_schema_id();
    if (descriptor.type_id() != kGainTypeId
        || !algorithm || *algorithm != kGainAlgorithmVersion
        || !schema || *schema != kGainParameterSchema) {
        return rgsml::core::Result<std::unique_ptr<GainModule>>::failure(gain_error(
            rgsml::core::ErrorCode::UnsupportedOperation,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "The descriptor does not identify the frozen Gain-v1 implementation."));
    }

    try {
        const auto linear_gain = parameters.gain_db() == 0.0
            ? 1.0
            : std::pow(10.0, parameters.gain_db() / 20.0);
        if (!std::isfinite(linear_gain) || linear_gain <= 0.0) {
            return rgsml::core::Result<std::unique_ptr<GainModule>>::failure(gain_error(
                rgsml::core::ErrorCode::InvalidState,
                "MODULE_IMPLEMENTATION_UNAVAILABLE",
                "Gain-v1 could not materialize a finite positive factor."));
        }
        auto impl = std::make_unique<Impl>(Impl{
            &descriptor, parameters, linear_gain, std::nullopt});
        return rgsml::core::Result<std::unique_ptr<GainModule>>::success(
            std::unique_ptr<GainModule>{new GainModule{std::move(impl)}});
    } catch (...) {
        return rgsml::core::Result<std::unique_ptr<GainModule>>::failure(gain_error(
            rgsml::core::ErrorCode::InvalidState,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "Gain-v1 construction failed without crossing the public boundary."));
    }
}

GainModule::GainModule(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

GainModule::~GainModule() = default;

const GainParameters& GainModule::parameters() const noexcept
{
    return impl_->parameters;
}

const ModuleDescriptor& GainModule::descriptor() const noexcept
{
    return *impl_->descriptor;
}

rgsml::core::Result<DspRuntimeRequirements> GainModule::runtime_requirements(
    const DspProcessSpec& spec) const
{
    const auto valid = validate_spec(spec);
    if (!valid) {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(*valid.error());
    }
    const auto zero = *rgsml::core::FrameCount::create(0).value();
    return rgsml::core::Result<DspRuntimeRequirements>::success(
        DspRuntimeRequirements{
            DspExecutionModel::STREAMING_CAUSAL,
            zero,
            zero,
            zero,
            zero,
            zero,
            false});
}

rgsml::core::Status GainModule::prepare(const DspProcessSpec& spec)
{
    const auto valid = validate_spec(spec);
    if (!valid) {
        return valid;
    }
    impl_->prepared_spec = spec;
    return rgsml::core::Status::success();
}

void GainModule::reset() noexcept
{
    // Gain-v1 is stateless. The prepared specification remains authoritative.
}

rgsml::core::Status GainModule::process(
    rgsml::audio::AudioBufferView input,
    rgsml::audio::MutableAudioBufferView output,
    const DspProcessContext& context)
{
    try {
        if (!impl_->prepared_spec) {
            return rgsml::core::Status::failure(gain_error(
                rgsml::core::ErrorCode::InvalidState,
                "DSP_MODULE_NOT_PREPARED",
                "Gain-v1 must be prepared before processing."));
        }
        const auto& spec = *impl_->prepared_spec;
        if (input.format() != spec.audio_format
            || output.format() != spec.audio_format
            || input.timebase().frame_domain_id() != spec.frame_domain_id
            || output.timebase().frame_domain_id() != spec.frame_domain_id
            || input.absolute_range() != context.output_frame_range
            || output.absolute_range() != context.output_frame_range
            || input.frame_count() != output.frame_count()
            || input.frame_count().value() > spec.maximum_block_frames.value()) {
            return rgsml::core::Status::failure(gain_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Gain-v1 input, output, context, or prepared specification do not match."));
        }

        const auto channel_count = input.format().channel_count();
        for (std::size_t input_channel = 0; input_channel < channel_count; ++input_channel) {
            const auto input_plane = *input.channel(input_channel).value();
            for (std::size_t output_channel = 0; output_channel < channel_count; ++output_channel) {
                const auto output_plane = *output.channel(output_channel).value();
                if (spans_overlap(input_plane, output_plane)) {
                    return rgsml::core::Status::failure(gain_error(
                        rgsml::core::ErrorCode::InvalidArgument,
                        "OVERLAPPING_AUDIO_VIEWS",
                        "Gain-v1 requires disjoint input and output storage."));
                }
            }
        }

        for (std::size_t channel = 0; channel < channel_count; ++channel) {
            const auto input_plane = *input.channel(channel).value();
            for (const auto sample : input_plane) {
                if (!std::isfinite(sample)) {
                    return rgsml::core::Status::failure(gain_error(
                        rgsml::core::ErrorCode::InvalidAudioSample,
                        "NONFINITE_INPUT_SAMPLE",
                        "Gain-v1 rejected a non-finite input sample."));
                }
            }
        }

        for (std::size_t channel = 0; channel < channel_count; ++channel) {
            const auto input_plane = *input.channel(channel).value();
            auto output_plane = *output.channel(channel).value();
            if (impl_->parameters.gain_db() == 0.0) {
                std::copy(input_plane.begin(), input_plane.end(), output_plane.begin());
            } else {
                for (std::size_t frame = 0; frame < input_plane.size(); ++frame) {
                    const auto processed = input_plane[frame] * impl_->linear_gain;
                    if (!std::isfinite(processed)) {
                        return rgsml::core::Status::failure(gain_error(
                            rgsml::core::ErrorCode::InvalidAudioSample,
                            "NONFINITE_OUTPUT_SAMPLE",
                            "Gain-v1 rejected a non-finite derived output sample."));
                    }
                    output_plane[frame] = processed;
                }
            }
        }
        return rgsml::core::Status::success();
    } catch (...) {
        return rgsml::core::Status::failure(gain_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_PROCESS_FAILURE",
            "Gain-v1 contained an implementation failure at its public boundary."));
    }
}

}  // namespace rgsml::dsp
