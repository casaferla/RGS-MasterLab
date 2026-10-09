#include <rgsml/dsp/stereo_ms_module.hpp>

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/dsp/stereo_ms_crossover_runtime.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::dsp {
namespace {

constexpr auto kTypeId = "rgsml.dsp.stereo-ms";
constexpr auto kAlgorithm = "1.0.0";
constexpr auto kSchema = "rgsml.dsp.stereo-ms.parameters/1.0.0";
// Correctly rounded binary64 1/sqrt(2); no float32 or hidden normalizer.
constexpr double kOrthonormalScale = 0x1.6a09e667f3bcdp-1;

[[nodiscard]] rgsml::core::Error ms_error(
    rgsml::core::ErrorCode code, std::string category, std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] bool valid_domain(rgsml::audio::FrameDomainId id) noexcept
{
    return id == rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || id == rgsml::audio::FrameDomainId::OUTPUT_RATE;
}

[[nodiscard]] rgsml::core::Status validate_spec(const DspProcessSpec& spec)
{
    if (!valid_domain(spec.frame_domain_id)
        || spec.maximum_block_frames.value() <= 0
        || (spec.audio_format.channel_layout() != rgsml::audio::ChannelLayout::MONO_C
            && spec.audio_format.channel_layout() != rgsml::audio::ChannelLayout::STEREO_LR)) {
        return rgsml::core::Status::failure(ms_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_SPEC",
            "Stereo/M-S requires a valid binary64 mono or stereo specification."));
    }
    return rgsml::core::Status::success();
}

template <typename Left, typename Right>
[[nodiscard]] bool spans_overlap(Left left, Right right) noexcept
{
    if (left.empty() || right.empty()) {
        return false;
    }
    const auto a = reinterpret_cast<std::uintptr_t>(left.data());
    const auto b = reinterpret_cast<std::uintptr_t>(right.data());
    return a < b + right.size_bytes() && b < a + left.size_bytes();
}

[[nodiscard]] bool spatial_broadband_only(
    const StereoMsParameters& p, rgsml::audio::ChannelLayout layout) noexcept
{
    return layout == rgsml::audio::ChannelLayout::MONO_C
        || p.side_muted()
        || p.mono_bass_mode() == MonoBassMode::OFF;
}

struct StereoFrame final {
    double left;
    double right;
};

[[nodiscard]] StereoFrame process_stereo_frame(
    double left,
    double right,
    bool side_muted,
    double mid_gain,
    double side_gain) noexcept
{
    if (side_muted) {
        // Exact rank-one Side projection. Mono Bass and Side gain must have
        // no influence on this path, including any hidden all-pass phase.
        const double center = (0.5 * left + 0.5 * right) * mid_gain;
        return {center, center};
    }

    const double mid = (left + right) * kOrthonormalScale;
    const double side = (left - right) * kOrthonormalScale;
    const double boosted_mid = mid * mid_gain;
    const double boosted_side = side * side_gain;
    return {
        (boosted_mid + boosted_side) * kOrthonormalScale,
        (boosted_mid - boosted_side) * kOrthonormalScale};
}

}  // namespace

struct StereoMsModule::Impl final {
    const ModuleDescriptor* descriptor;
    StereoMsParameters parameters;
    double mid_gain;
    double side_gain;
    std::optional<DspProcessSpec> prepared_spec;
    std::optional<StereoMsCrossoverRuntime> crossover;
    std::vector<double> scratch_left;
    std::vector<double> scratch_right;
    std::optional<rgsml::core::FrameIndex> next_frame;
    bool stream_bound{false};
    bool stream_ended{false};
};

rgsml::core::Result<std::unique_ptr<StereoMsModule>> StereoMsModule::create(
    const ModuleDescriptor& descriptor, StereoMsParameters parameters)
{
    const auto version = descriptor.algorithm_version();
    const auto schema = descriptor.parameter_schema_id();
    if (descriptor.type_id() != kTypeId || !version || *version != kAlgorithm
        || !schema || *schema != kSchema) {
        return rgsml::core::Result<std::unique_ptr<StereoMsModule>>::failure(
            ms_error(rgsml::core::ErrorCode::UnsupportedOperation,
                     "MODULE_IMPLEMENTATION_UNAVAILABLE",
                     "Descriptor is not the frozen Stereo/M-S v1 identity."));
    }
    try {
        const double mid_gain = parameters.mid_gain_db() == 0.0
            ? 1.0 : std::pow(10.0, parameters.mid_gain_db() / 20.0);
        const double side_gain = parameters.side_gain_db() == 0.0
            ? 1.0 : std::pow(10.0, parameters.side_gain_db() / 20.0);
        if (!std::isfinite(mid_gain) || !std::isfinite(side_gain)
            || mid_gain <= 0.0 || side_gain <= 0.0) {
            return rgsml::core::Result<std::unique_ptr<StereoMsModule>>::failure(
                ms_error(rgsml::core::ErrorCode::InvalidState,
                         "MODULE_IMPLEMENTATION_UNAVAILABLE",
                         "Broadband coefficients must be finite positive gains."));
        }
        auto impl = std::make_unique<Impl>(
            Impl{&descriptor, parameters, mid_gain, side_gain, std::nullopt,
                 std::nullopt, {}, {}, std::nullopt, false, false});
        return rgsml::core::Result<std::unique_ptr<StereoMsModule>>::success(
            std::unique_ptr<StereoMsModule>{new StereoMsModule{std::move(impl)}});
    } catch (...) {
        return rgsml::core::Result<std::unique_ptr<StereoMsModule>>::failure(
            ms_error(rgsml::core::ErrorCode::InvalidState,
                     "MODULE_IMPLEMENTATION_UNAVAILABLE",
                     "Stereo/M-S construction failed."));
    }
}

StereoMsModule::StereoMsModule(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

StereoMsModule::~StereoMsModule() = default;

const StereoMsParameters& StereoMsModule::parameters() const noexcept
{
    return impl_->parameters;
}

const ModuleDescriptor& StereoMsModule::descriptor() const noexcept
{
    return *impl_->descriptor;
}

rgsml::core::Result<DspRuntimeRequirements>
StereoMsModule::runtime_requirements(const DspProcessSpec& spec) const
{
    const auto valid = validate_spec(spec);
    if (!valid) {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(
            *valid.error());
    }
    auto settling = *rgsml::core::FrameCount::create(0).value();
    if (!spatial_broadband_only(impl_->parameters,
                                spec.audio_format.channel_layout())) {
        const auto d = design_stereo_ms_crossover(
            impl_->parameters.mono_bass_mode(),
            impl_->parameters.mono_bass_cutoff_hz(),
            static_cast<double>(spec.audio_format.sample_rate().value()));
        if (!d) {
            return rgsml::core::Result<DspRuntimeRequirements>::failure(*d.error());
        }
        const auto count = rgsml::core::FrameCount::create(d.value()->settling_frames);
        if (!count) {
            return rgsml::core::Result<DspRuntimeRequirements>::failure(*count.error());
        }
        settling = *count.value();
    }
    const auto zero = *rgsml::core::FrameCount::create(0).value();
    return rgsml::core::Result<DspRuntimeRequirements>::success(
        DspRuntimeRequirements{
            DspExecutionModel::STREAMING_CAUSAL,
            zero, zero, settling, settling, settling, false});
}

rgsml::core::Status StereoMsModule::prepare(const DspProcessSpec& spec)
{
    // Fail closed on re-prepare: an old prepared DSP realization never leaks
    // through an unsuccessful new specification or memory allocation.
    impl_->prepared_spec.reset();
    impl_->crossover.reset();
    impl_->scratch_left.clear();
    impl_->scratch_right.clear();
    impl_->next_frame.reset();
    impl_->stream_bound = false;
    impl_->stream_ended = false;

    const auto req = runtime_requirements(spec);
    if (!req) {
        return rgsml::core::Status::failure(*req.error());
    }
    try {
        if (!spatial_broadband_only(impl_->parameters,
                                    spec.audio_format.channel_layout())) {
            const auto d = design_stereo_ms_crossover(
                impl_->parameters.mono_bass_mode(),
                impl_->parameters.mono_bass_cutoff_hz(),
                static_cast<double>(spec.audio_format.sample_rate().value()));
            if (!d) {
                return rgsml::core::Status::failure(*d.error());
            }
            impl_->crossover.emplace(*d.value());
            const auto maximum = static_cast<std::size_t>(
                spec.maximum_block_frames.value());
            impl_->scratch_left.assign(maximum, 0.0);
            impl_->scratch_right.assign(maximum, 0.0);
        }
        impl_->prepared_spec = spec;
        return rgsml::core::Status::success();
    } catch (...) {
        impl_->crossover.reset();
        impl_->scratch_left.clear();
        impl_->scratch_right.clear();
        return rgsml::core::Status::failure(ms_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_PREPARE_FAILURE",
            "Stereo/M-S could not allocate its bounded crossover scratch."));
    }
}

void StereoMsModule::reset() noexcept
{
    if (impl_->crossover) {
        impl_->crossover->reset();
    }
    impl_->next_frame.reset();
    impl_->stream_bound = false;
    impl_->stream_ended = false;
}

rgsml::core::Status StereoMsModule::process(
    rgsml::audio::AudioBufferView input,
    rgsml::audio::MutableAudioBufferView output,
    const DspProcessContext& context)
{
    if (!impl_->prepared_spec) {
        return rgsml::core::Status::failure(ms_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_MODULE_NOT_PREPARED",
            "Stereo/M-S must be prepared before processing."));
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
        return rgsml::core::Status::failure(ms_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_CONTEXT",
            "Stereo/M-S input/output or frame range mismatch."));
    }

    // Stream binding is transactional and contiguous, as for the Compressor.
    // Reject invalid calls before any output store or recursive state mutation.
    if (impl_->stream_ended
        || (!impl_->stream_bound && !context.begins_stream)
        || (impl_->stream_bound && (context.begins_stream
            || !impl_->next_frame
            || context.output_frame_range.begin() != *impl_->next_frame))) {
        return rgsml::core::Status::failure(ms_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_CONTEXT",
            "Stereo/M-S requires a contiguous stream and one begin/end."));
    }

    const auto channels = input.format().channel_count();
    for (std::size_t i = 0; i < channels; ++i) {
        const auto in = *input.channel(i).value();
        for (std::size_t j = 0; j < channels; ++j) {
            if (spans_overlap(in, *output.channel(j).value())) {
                return rgsml::core::Status::failure(ms_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "OVERLAPPING_AUDIO_VIEWS",
                    "Stereo/M-S requires disjoint source and destination planes."));
            }
        }
        for (double sample : in) {
            if (!std::isfinite(sample)) {
                return rgsml::core::Status::failure(ms_error(
                    rgsml::core::ErrorCode::InvalidAudioSample,
                    "NONFINITE_INPUT_SAMPLE",
                    "Stereo/M-S rejects non-finite PCM."));
            }
        }
    }

    const auto in0 = *input.channel(0).value();
    auto out0 = *output.channel(0).value();
    if (input.format().channel_layout() == rgsml::audio::ChannelLayout::MONO_C) {
        // Strict bit identity: no floating-point spatial arithmetic.
        std::copy(in0.begin(), in0.end(), out0.begin());
        impl_->next_frame = context.output_frame_range.end();
        impl_->stream_bound = true;
        impl_->stream_ended = context.ends_stream;
        return rgsml::core::Status::success();
    }
    const auto in1 = *input.channel(1).value();
    auto out1 = *output.channel(1).value();

    if (impl_->crossover) {
        // A tentative copy contains ALL four Mid/Side x Low/High recursive
        // histories. No mutation of the live realization is allowed until
        // the complete frame block has produced only finite output/states.
        auto candidate = *impl_->crossover;
        const double beta = impl_->parameters.low_band_width_percent() / 100.0;
        for (std::size_t frame = 0; frame < in0.size(); ++frame) {
            const double mid = (in0[frame] + in1[frame])
                * kOrthonormalScale * impl_->mid_gain;
            const double side = (in0[frame] - in1[frame])
                * kOrthonormalScale * impl_->side_gain;
            const auto filtered = candidate.process(mid, side, beta);
            const double left = (filtered.mid + filtered.side) * kOrthonormalScale;
            const double right = (filtered.mid - filtered.side) * kOrthonormalScale;
            if (!std::isfinite(left) || !std::isfinite(right)
                || !candidate.finite()) {
                return rgsml::core::Status::failure(ms_error(
                    rgsml::core::ErrorCode::InvalidAudioSample,
                    "NONFINITE_OUTPUT_SAMPLE",
                    "Stereo/M-S recursive crossover rejected nonfinite output or state."));
            }
            impl_->scratch_left[frame] = left;
            impl_->scratch_right[frame] = right;
        }

        // Commit output, recursive state and frame cursor as one successful
        // operation. The preallocated scratch allows allocation-free process.
        std::copy_n(impl_->scratch_left.begin(), in0.size(), out0.begin());
        std::copy_n(impl_->scratch_right.begin(), in0.size(), out1.begin());
        *impl_->crossover = candidate;
        impl_->next_frame = context.output_frame_range.end();
        impl_->stream_bound = true;
        impl_->stream_ended = context.ends_stream;
        return rgsml::core::Status::success();
    }

    // Fail before the first output store if any derived stereo sample is
    // non-finite. The second pass is allocation-free and stateless.
    for (std::size_t frame = 0; frame < in0.size(); ++frame) {
        const auto sample = process_stereo_frame(
            in0[frame], in1[frame], impl_->parameters.side_muted(),
            impl_->mid_gain, impl_->side_gain);
        if (!std::isfinite(sample.left) || !std::isfinite(sample.right)) {
            return rgsml::core::Status::failure(ms_error(
                rgsml::core::ErrorCode::InvalidAudioSample,
                "NONFINITE_OUTPUT_SAMPLE",
                "Stereo/M-S output overflowed binary64."));
        }
    }
    for (std::size_t frame = 0; frame < in0.size(); ++frame) {
        const auto sample = process_stereo_frame(
            in0[frame], in1[frame], impl_->parameters.side_muted(),
            impl_->mid_gain, impl_->side_gain);
        out0[frame] = sample.left;
        out1[frame] = sample.right;
    }
    impl_->next_frame = context.output_frame_range.end();
    impl_->stream_bound = true;
    impl_->stream_ended = context.ends_stream;
    return rgsml::core::Status::success();
}

}  // namespace rgsml::dsp
