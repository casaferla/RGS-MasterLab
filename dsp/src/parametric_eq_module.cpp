#include <rgsml/dsp/parametric_eq_module.hpp>

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_descriptor.hpp>

#include "internal/parametric_eq_coefficients.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";
constexpr auto kEqAlgorithmVersion = "1.0.0";
constexpr auto kEqParameterSchema = "rgsml.dsp.parametric-eq.parameters/1.0.0";

[[nodiscard]] rgsml::core::Error eq_error(
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

[[nodiscard]] rgsml::core::Status validate_spec(
    const DspProcessSpec& spec,
    const ParametricEqParameters& parameters)
{
    if (!valid_domain(spec.frame_domain_id)
        || spec.maximum_block_frames.value() <= 0
        || !std::isfinite(spec.audio_format.sample_rate_hz())
        || spec.audio_format.sample_rate_hz() <= 0.0) {
        return rgsml::core::Status::failure(eq_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_SPEC",
            "Parametric EQ requires a valid binary64 process specification."));
    }

    const auto layout = spec.audio_format.channel_layout();
    if (layout != rgsml::audio::ChannelLayout::MONO_C
        && layout != rgsml::audio::ChannelLayout::STEREO_LR) {
        return rgsml::core::Status::failure(eq_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_SPEC",
            "Parametric EQ requires MONO_C or STEREO_LR channel layout."));
    }

    if (layout == rgsml::audio::ChannelLayout::MONO_C) {
        for (const auto& band : parameters.bands()) {
            if (band.enabled() && band.routing() != EqRouting::STEREO) {
                return rgsml::core::Status::failure(eq_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INVALID_ROUTING_FOR_MONO",
                    "Mono channel layout prohibits non-STEREO routing."));
            }
        }
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

struct BiquadState final {
    double s1{0.0};
    double s2{0.0};

    void reset() noexcept
    {
        s1 = 0.0;
        s2 = 0.0;
    }
};

struct SectionRuntimeState final {
    internal::BiquadCoefficients coeffs;
    BiquadState state_ch0; // L or C or M
    BiquadState state_ch1; // R or S
};

struct BandRuntimeState final {
    rgsml::core::Uuid band_id;
    bool enabled{true};
    EqFilterType filter_type{EqFilterType::BELL};
    EqRouting routing{EqRouting::STEREO};
    std::vector<SectionRuntimeState> sections;

    void reset() noexcept
    {
        for (auto& sec : sections) {
            sec.state_ch0.reset();
            sec.state_ch1.reset();
        }
    }
};

}  // namespace

struct ParametricEqModule::Impl final {
    const ModuleDescriptor* descriptor;
    ParametricEqParameters parameters;
    std::optional<DspProcessSpec> prepared_spec;
    std::vector<BandRuntimeState> band_states;
    std::vector<BandRuntimeState> backup_band_states;
    std::int64_t total_settling_frames{0};
    std::vector<double> scratch_ch0;
    std::vector<double> scratch_ch1;
};

rgsml::core::Result<std::unique_ptr<ParametricEqModule>> ParametricEqModule::create(
    const ModuleDescriptor& descriptor,
    ParametricEqParameters parameters)
{
    const auto algorithm = descriptor.algorithm_version();
    const auto schema = descriptor.parameter_schema_id();
    if (descriptor.type_id() != kEqTypeId
        || !algorithm || *algorithm != kEqAlgorithmVersion
        || !schema || *schema != kEqParameterSchema) {
        return rgsml::core::Result<std::unique_ptr<ParametricEqModule>>::failure(eq_error(
            rgsml::core::ErrorCode::UnsupportedOperation,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "The descriptor does not identify the frozen Parametric EQ-v1 implementation."));
    }

    try {
        auto impl = std::make_unique<Impl>(Impl{
            &descriptor, std::move(parameters), std::nullopt, {}, {}, 0, {}, {}});
        return rgsml::core::Result<std::unique_ptr<ParametricEqModule>>::success(
            std::unique_ptr<ParametricEqModule>{new ParametricEqModule{std::move(impl)}});
    } catch (...) {
        return rgsml::core::Result<std::unique_ptr<ParametricEqModule>>::failure(eq_error(
            rgsml::core::ErrorCode::InvalidState,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "Parametric EQ-v1 construction failed without crossing the public boundary."));
    }
}

ParametricEqModule::ParametricEqModule(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

ParametricEqModule::~ParametricEqModule() = default;

const ParametricEqParameters& ParametricEqModule::parameters() const noexcept
{
    return impl_->parameters;
}

const ModuleDescriptor& ParametricEqModule::descriptor() const noexcept
{
    return *impl_->descriptor;
}

rgsml::core::Result<DspRuntimeRequirements> ParametricEqModule::runtime_requirements(
    const DspProcessSpec& spec) const
{
    const auto valid = validate_spec(spec, impl_->parameters);
    if (!valid) {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(*valid.error());
    }

    auto coeffs = internal::compute_parametric_eq_coefficients(
        impl_->parameters, spec.audio_format.sample_rate_hz());
    if (!coeffs) {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(*coeffs.error());
    }

    const auto settling = *rgsml::core::FrameCount::create(coeffs.value()->total_settling_frames).value();
    const auto zero = *rgsml::core::FrameCount::create(0).value();

    return rgsml::core::Result<DspRuntimeRequirements>::success(
        DspRuntimeRequirements{
            DspExecutionModel::STREAMING_CAUSAL,
            zero,
            zero,
            settling,
            settling,
            settling,
            false});
}

rgsml::core::Status ParametricEqModule::prepare(const DspProcessSpec& spec)
{
    const auto valid = validate_spec(spec, impl_->parameters);
    if (!valid) {
        return valid;
    }

    auto coeffs = internal::compute_parametric_eq_coefficients(
        impl_->parameters, spec.audio_format.sample_rate_hz());
    if (!coeffs) {
        return rgsml::core::Status::failure(*coeffs.error());
    }

    impl_->band_states.clear();
    impl_->band_states.reserve(coeffs.value()->bands.size());

    for (const auto& band_coeff : coeffs.value()->bands) {
        BandRuntimeState band_state;
        band_state.band_id = band_coeff.band_id;
        band_state.enabled = band_coeff.enabled;
        band_state.filter_type = band_coeff.filter_type;
        band_state.routing = band_coeff.routing;
        band_state.sections.reserve(band_coeff.sections.size());

        for (const auto& sec_coeff : band_coeff.sections) {
            SectionRuntimeState sec_state;
            sec_state.coeffs = sec_coeff.coeffs;
            sec_state.state_ch0.reset();
            sec_state.state_ch1.reset();
            band_state.sections.push_back(sec_state);
        }
        impl_->band_states.push_back(std::move(band_state));
    }

    impl_->backup_band_states = impl_->band_states;

    impl_->total_settling_frames = coeffs.value()->total_settling_frames;
    const std::size_t max_frames = static_cast<std::size_t>(spec.maximum_block_frames.value());
    impl_->scratch_ch0.assign(max_frames, 0.0);
    if (spec.audio_format.channel_layout() == rgsml::audio::ChannelLayout::STEREO_LR) {
        impl_->scratch_ch1.assign(max_frames, 0.0);
    } else {
        impl_->scratch_ch1.clear();
    }

    impl_->prepared_spec = spec;
    return rgsml::core::Status::success();
}

void ParametricEqModule::reset() noexcept
{
    for (auto& band : impl_->band_states) {
        band.reset();
    }
    for (auto& band : impl_->backup_band_states) {
        band.reset();
    }
}

rgsml::core::Status ParametricEqModule::process(
    rgsml::audio::AudioBufferView input,
    rgsml::audio::MutableAudioBufferView output,
    const DspProcessContext& context)
{
    try {
        if (!impl_->prepared_spec) {
            return rgsml::core::Status::failure(eq_error(
                rgsml::core::ErrorCode::InvalidState,
                "DSP_MODULE_NOT_PREPARED",
                "Parametric EQ-v1 must be prepared before processing."));
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
            return rgsml::core::Status::failure(eq_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Parametric EQ-v1 input, output, context, or prepared specification do not match."));
        }

        const auto channel_count = input.format().channel_count();
        for (std::size_t input_channel = 0; input_channel < channel_count; ++input_channel) {
            const auto input_plane = *input.channel(input_channel).value();
            for (std::size_t output_channel = 0; output_channel < channel_count; ++output_channel) {
                const auto output_plane = *output.channel(output_channel).value();
                if (spans_overlap(input_plane, output_plane)) {
                    return rgsml::core::Status::failure(eq_error(
                        rgsml::core::ErrorCode::InvalidArgument,
                        "OVERLAPPING_AUDIO_VIEWS",
                        "Parametric EQ-v1 requires disjoint input and output storage."));
                }
            }
        }

        const auto num_frames = static_cast<std::size_t>(input.frame_count().value());
        for (std::size_t channel = 0; channel < channel_count; ++channel) {
            const auto input_plane = *input.channel(channel).value();
            for (const auto sample : input_plane) {
                if (!std::isfinite(sample)) {
                    return rgsml::core::Status::failure(eq_error(
                        rgsml::core::ErrorCode::InvalidAudioSample,
                        "NONFINITE_INPUT_SAMPLE",
                        "Parametric EQ-v1 rejected a non-finite input sample."));
                }
            }
        }

        // Copy input to scratch
        const auto in_ch0 = *input.channel(0).value();
        std::copy(in_ch0.begin(), in_ch0.end(), impl_->scratch_ch0.begin());

        if (channel_count == 2) {
            const auto in_ch1 = *input.channel(1).value();
            std::copy(in_ch1.begin(), in_ch1.end(), impl_->scratch_ch1.begin());
        }

        // Backup band states in case output contains non-finite samples (reuses pre-allocated capacity)
        impl_->backup_band_states = impl_->band_states;

        const double kSqrt2 = std::numbers::sqrt2;

        // Process bands sequentially
        for (auto& band : impl_->band_states) {
            if (!band.enabled) {
                continue;
            }

            if (channel_count == 1) {
                // Mono: STEREO routing only
                for (auto& sec : band.sections) {
                    const auto& c = sec.coeffs;
                    auto& st = sec.state_ch0;
                    for (std::size_t f = 0; f < num_frames; ++f) {
                        const double x = impl_->scratch_ch0[f];
                        const double y = c.b0 * x + st.s1;
                        st.s1 = c.b1 * x - c.a1 * y + st.s2;
                        st.s2 = c.b2 * x - c.a2 * y;
                        impl_->scratch_ch0[f] = y;
                    }
                }
            } else {
                // Stereo
                switch (band.routing) {
                case EqRouting::STEREO: {
                    for (auto& sec : band.sections) {
                        const auto& c = sec.coeffs;
                        auto& st0 = sec.state_ch0;
                        auto& st1 = sec.state_ch1;
                        for (std::size_t f = 0; f < num_frames; ++f) {
                            const double x0 = impl_->scratch_ch0[f];
                            const double y0 = c.b0 * x0 + st0.s1;
                            st0.s1 = c.b1 * x0 - c.a1 * y0 + st0.s2;
                            st0.s2 = c.b2 * x0 - c.a2 * y0;
                            impl_->scratch_ch0[f] = y0;

                            const double x1 = impl_->scratch_ch1[f];
                            const double y1 = c.b0 * x1 + st1.s1;
                            st1.s1 = c.b1 * x1 - c.a1 * y1 + st1.s2;
                            st1.s2 = c.b2 * x1 - c.a2 * y1;
                            impl_->scratch_ch1[f] = y1;
                        }
                    }
                    break;
                }
                case EqRouting::LEFT: {
                    for (auto& sec : band.sections) {
                        const auto& c = sec.coeffs;
                        auto& st0 = sec.state_ch0;
                        for (std::size_t f = 0; f < num_frames; ++f) {
                            const double x0 = impl_->scratch_ch0[f];
                            const double y0 = c.b0 * x0 + st0.s1;
                            st0.s1 = c.b1 * x0 - c.a1 * y0 + st0.s2;
                            st0.s2 = c.b2 * x0 - c.a2 * y0;
                            impl_->scratch_ch0[f] = y0;
                        }
                    }
                    break;
                }
                case EqRouting::RIGHT: {
                    for (auto& sec : band.sections) {
                        const auto& c = sec.coeffs;
                        auto& st1 = sec.state_ch1;
                        for (std::size_t f = 0; f < num_frames; ++f) {
                            const double x1 = impl_->scratch_ch1[f];
                            const double y1 = c.b0 * x1 + st1.s1;
                            st1.s1 = c.b1 * x1 - c.a1 * y1 + st1.s2;
                            st1.s2 = c.b2 * x1 - c.a2 * y1;
                            impl_->scratch_ch1[f] = y1;
                        }
                    }
                    break;
                }
                case EqRouting::MID: {
                    for (std::size_t f = 0; f < num_frames; ++f) {
                        const double l = impl_->scratch_ch0[f];
                        const double r = impl_->scratch_ch1[f];
                        double m = (l + r) / kSqrt2;
                        const double s = (l - r) / kSqrt2;

                        for (auto& sec : band.sections) {
                            const auto& c = sec.coeffs;
                            auto& st0 = sec.state_ch0;
                            const double y = c.b0 * m + st0.s1;
                            st0.s1 = c.b1 * m - c.a1 * y + st0.s2;
                            st0.s2 = c.b2 * m - c.a2 * y;
                            m = y;
                        }

                        impl_->scratch_ch0[f] = (m + s) / kSqrt2;
                        impl_->scratch_ch1[f] = (m - s) / kSqrt2;
                    }
                    break;
                }
                case EqRouting::SIDE: {
                    for (std::size_t f = 0; f < num_frames; ++f) {
                        const double l = impl_->scratch_ch0[f];
                        const double r = impl_->scratch_ch1[f];
                        const double m = (l + r) / kSqrt2;
                        double s = (l - r) / kSqrt2;

                        for (auto& sec : band.sections) {
                            const auto& c = sec.coeffs;
                            auto& st1 = sec.state_ch1;
                            const double y = c.b0 * s + st1.s1;
                            st1.s1 = c.b1 * s - c.a1 * y + st1.s2;
                            st1.s2 = c.b2 * s - c.a2 * y;
                            s = y;
                        }

                        impl_->scratch_ch0[f] = (m + s) / kSqrt2;
                        impl_->scratch_ch1[f] = (m - s) / kSqrt2;
                    }
                    break;
                }
                }
            }
        }

        // Verify non-finite samples in scratch
        for (std::size_t f = 0; f < num_frames; ++f) {
            if (!std::isfinite(impl_->scratch_ch0[f])
                || (channel_count == 2 && !std::isfinite(impl_->scratch_ch1[f]))) {
                impl_->band_states = impl_->backup_band_states;
                return rgsml::core::Status::failure(eq_error(
                    rgsml::core::ErrorCode::InvalidAudioSample,
                    "NONFINITE_OUTPUT_SAMPLE",
                    "Parametric EQ-v1 rejected a non-finite output sample."));
            }
        }

        // Copy scratch to output
        auto out_ch0 = *output.channel(0).value();
        std::copy_n(impl_->scratch_ch0.begin(), num_frames, out_ch0.begin());

        if (channel_count == 2) {
            auto out_ch1 = *output.channel(1).value();
            std::copy_n(impl_->scratch_ch1.begin(), num_frames, out_ch1.begin());
        }

        return rgsml::core::Status::success();
    } catch (...) {
        return rgsml::core::Status::failure(eq_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_PROCESS_FAILURE",
            "Parametric EQ-v1 contained an implementation failure at its public boundary."));
    }
}

}  // namespace rgsml::dsp
