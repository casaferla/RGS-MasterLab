#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/compressor_telemetry_collector.hpp>
#include <rgsml/render/stereo_ms_execution_signature.hpp>

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/checked_integer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/compressor_module.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::render {
namespace {

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";
constexpr auto kCompressorTypeId = "rgsml.dsp.compressor";
constexpr auto kStereoMsTypeId = "rgsml.dsp.stereo-ms";

struct PreparedModule final {
    std::unique_ptr<rgsml::dsp::IModule> instance;
    rgsml::dsp::ModuleInstanceId instance_id;
    std::string type_id;
    std::int64_t algorithmic_latency_frames{0};
    std::int64_t look_ahead_frames{0};
    std::int64_t effective_tail_frames{0};
};

[[nodiscard]] rgsml::core::Error render_error(
    rgsml::core::ErrorCode code,
    std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] const rgsml::dsp::ModuleExecutionBinding* find_binding(
    const RenderRequest& request,
    const rgsml::dsp::ModuleInstanceId& id) noexcept
{
    const auto bindings = request.bindings();
    const auto iterator = std::find_if(
        bindings.begin(),
        bindings.end(),
        [&id](const rgsml::dsp::ModuleExecutionBinding& binding) {
            return binding.instance_id == id;
        });
    return iterator == bindings.end() ? nullptr : std::addressof(*iterator);
}

// Direct, integer Grid100 evaluation avoids cumulative rounding drift at
// non-10-divisible sample rates (e.g. 44105 Hz hop 4410, 4411, 4411, 4410).
[[nodiscard]] std::optional<std::int64_t> correlation_grid_offset(
    std::uint64_t index,
    std::uint64_t sample_rate) noexcept
{
    if (sample_rate == 0 ||
        index > std::numeric_limits<std::uint64_t>::max() / sample_rate) {
        return std::nullopt;
    }
    const auto numerator = index * sample_rate;
    auto rounded = numerator / 10U;
    const auto remainder = numerator % 10U;
    if (remainder > 5U || (remainder == 5U && (rounded % 2U) == 1U)) {
        ++rounded;
    }
    if (rounded > static_cast<std::uint64_t>(
                      std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(rounded);
}

[[nodiscard]] StereoMsCorrelationWindow measure_centered_correlation(
    std::int64_t begin,
    std::int64_t end,
    std::span<const double> left,
    std::span<const double> right) noexcept
{
    StereoMsCorrelationWindow result{begin, end};
    const auto n = left.size();
    if (n == 0 || n != right.size()) {
        return result;
    }
    // Both passes use the same exact complete window; removing the local
    // means is mandatory, unlike raw Side-energy or RMS measurement.
    long double mean_left = 0.0L;
    long double mean_right = 0.0L;
    for (std::size_t i = 0; i < n; ++i) {
        mean_left += static_cast<long double>(left[i]);
        mean_right += static_cast<long double>(right[i]);
    }
    mean_left /= static_cast<long double>(n);
    mean_right /= static_cast<long double>(n);
    long double energy_l = 0.0L;
    long double energy_r = 0.0L;
    long double covariance = 0.0L;
    for (std::size_t i = 0; i < n; ++i) {
        const auto l = static_cast<long double>(left[i]) - mean_left;
        const auto r = static_cast<long double>(right[i]) - mean_right;
        energy_l += l * l;
        energy_r += r * r;
        covariance += l * r;
    }
    const auto denominator = std::sqrt(energy_l) * std::sqrt(energy_r);
    if (energy_l / static_cast<long double>(n) < 1e-10L
        || energy_r / static_cast<long double>(n) < 1e-10L
        || denominator < 1e-20L
        || !std::isfinite(denominator)) {
        return result;
    }
    long double value = covariance / denominator;
    if (std::isfinite(value) && (value < -1.0L || value > 1.0L)) {
        // At the exact +/-1 endpoints sqrt(E_L)*sqrt(E_R) can round
        // below |C| on either toolchain. Re-evaluate using the equivalent
        // nonnegative squared-distance identity, not a numeric clamp:
        // rho = sign(C) * (1 - sum((L' - sign(C)*sqrt(E_L/E_R)*R')^2)/(2*E_L)).
        const long double sign = covariance < 0.0L ? -1.0L : 1.0L;
        const long double scale = std::sqrt(energy_l / energy_r);
        long double squared_distance = 0.0L;
        if (std::isfinite(scale)) {
            for (std::size_t i = 0; i < n; ++i) {
                const auto l = static_cast<long double>(left[i]) - mean_left;
                const auto r = static_cast<long double>(right[i]) - mean_right;
                const auto difference = l - sign * scale * r;
                squared_distance += difference * difference;
            }
            value = sign * (1.0L - squared_distance / (2.0L * energy_l));
        }
    }
    if (!std::isfinite(value) || value < -1.0L || value > 1.0L) {
        // Bad arithmetic remains invalid; never silently clamp a
        // genuinely out-of-range result to a plausible correlation.
        return result;
    }
    result.validity = StereoMsCorrelationWindowValidity::VALID;
    result.rho = static_cast<double>(value);
    return result;
}

void collect_stage_correlation(
    StereoMsStageOutputSidecar& capture,
    rgsml::audio::AudioBufferView output,
    std::int64_t source_origin,
    std::size_t max_telemetry_bytes)
{
    const auto fs = static_cast<std::uint64_t>(capture.sample_rate_hz);
    const auto requested_begin = capture.requested_begin_frame;
    const auto requested_end = capture.requested_end_frame;
    if (fs == 0 || source_origin > requested_begin
        || requested_begin >= requested_end || !output.channel(0)
        || !output.channel(1)) {
        return;
    }
    constexpr std::size_t kMaxCorrelationBytes = 64U * 1024U;
    const auto used_capture_bytes =
        capture.output_lr_frames.size() * sizeof(std::array<double, 2>);
    const auto remaining_budget = max_telemetry_bytes > used_capture_bytes
        ? max_telemetry_bytes - used_capture_bytes : 0U;
    const auto capacity = std::min(kMaxCorrelationBytes, remaining_budget)
        / sizeof(StereoMsCorrelationWindow);
    if (capacity == 0) {
        return;
    }
    const auto count = static_cast<std::int64_t>((2U * fs + 2U) / 5U);
    if (count < 1 || requested_end - requested_begin < count) {
        return;
    }

    const auto left = *output.channel(0).value();
    const auto right = *output.channel(1).value();
    const auto origin = output.absolute_start_frame().value();
    const auto upper = output.absolute_end_frame().value();
    const auto delta = static_cast<std::uint64_t>(requested_begin - source_origin);
    // Start near the first eligible grid point without iterating the whole
    // track's history (a preview can begin far beyond the source start).
    const auto whole_seconds = delta / fs;
    if (whole_seconds > std::numeric_limits<std::uint64_t>::max() / 10U) {
        return;
    }
    auto j = whole_seconds * 10U;
    capture.correlation_windows.reserve(std::min<std::size_t>(capacity, 256U));
    bool omitted_windows = false;
    while (true) {
        const auto hop = correlation_grid_offset(j, fs);
        if (!hop) {
            break;
        }
        const auto absolute_start = rgsml::core::checked_add(
            source_origin, *hop);
        if (!absolute_start) {
            break;
        }
        const auto begin = *absolute_start.value();
        if (begin >= requested_end || requested_end - begin < count) {
            break;
        }
        if (begin >= requested_begin && begin >= origin
            && upper - begin >= count) {
            if (capture.correlation_windows.size() >= capacity) {
                omitted_windows = true;
                break;
            }
            const auto offset = static_cast<std::size_t>(begin - origin);
            const auto n = static_cast<std::size_t>(count);
            capture.correlation_windows.push_back(
                measure_centered_correlation(
                    begin, begin + count,
                    left.subspan(offset, n), right.subspan(offset, n)));
        }
        if (j == std::numeric_limits<std::uint64_t>::max()) {
            break;
        }
        ++j;
    }
    if (!capture.correlation_windows.empty()) {
        capture.correlation_status = omitted_windows
            ? StereoMsCorrelationStatus::PARTIAL
            : StereoMsCorrelationStatus::COMPLETE;
    }
}

[[nodiscard]] rgsml::core::Status copy_audio(
    rgsml::audio::AudioBufferView input,
    rgsml::audio::MutableAudioBufferView output)
{
    if (input.format() != output.format()
        || input.frame_count() != output.frame_count()) {
        return rgsml::core::Status::failure(render_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_RENDER_COPY",
            "Render Preview copy views do not match."));
    }
    for (std::size_t channel = 0; channel < input.format().channel_count(); ++channel) {
        const auto source = *input.channel(channel).value();
        auto destination = *output.channel(channel).value();
        std::copy(source.begin(), source.end(), destination.begin());
    }
    return rgsml::core::Status::success();
}

}  // namespace

rgsml::core::Result<RenderResult> render_preview(
    const RenderRequest& request,
    const rgsml::dsp::ModuleRegistry& registry)
{
    try {
        const auto source = request.source();
        const auto window = request.render_window();
        const auto range_length = window.length();
        if (!range_length) {
            return rgsml::core::Result<RenderResult>::failure(*range_length.error());
        }

        std::vector<PreparedModule> modules;
        std::vector<ModuleExecutionSignature> signatures;
        modules.reserve(request.chain_instances().size());
        signatures.reserve(request.bindings().size());

        const rgsml::dsp::DspProcessSpec process_spec{
            source.format(),
            source.timebase().frame_domain_id(),
            request.maximum_block_frames()};

        std::int64_t cumulative_latency = 0;

        for (const auto& instance : request.chain_instances()) {
            const auto descriptor = registry.find_descriptor(instance.module_type_id());
            if (!descriptor) {
                return rgsml::core::Result<RenderResult>::failure(*descriptor.error());
            }

            // M15-B2c: Stereo/M-S uses only its exact immutable typed binding.
            // Never call the generic default factory for this module.
            const bool is_gain = (instance.module_type_id() == kGainTypeId);
            const bool is_eq = (instance.module_type_id() == kEqTypeId);
            const bool is_compressor = (instance.module_type_id() == kCompressorTypeId);
            const bool is_stereo_ms = (instance.module_type_id() == kStereoMsTypeId);
            const bool is_parameterized = is_gain || is_eq || is_compressor || is_stereo_ms;

            if (!instance.active()) {
                if (is_parameterized) {
                    const auto* binding = find_binding(request, instance.instance_id());
                    if (binding == nullptr) {
                        return rgsml::core::Result<RenderResult>::failure(render_error(
                            rgsml::core::ErrorCode::InvalidArgument,
                            "MISSING_PARAMETER_BINDING",
                            "A bypassed parameterized module has no immutable binding."));
                    }
                    if (is_stereo_ms) {
                        const auto* parameters = std::get_if<rgsml::dsp::StereoMsParameters>(
                            &binding->parameters);
                        if (parameters == nullptr) {
                            return rgsml::core::Result<RenderResult>::failure(render_error(
                                rgsml::core::ErrorCode::InvalidArgument,
                                "MODULE_PARAMETER_PAYLOAD_MISMATCH",
                                "Stereo/M-S requires a typed StereoMsParameters binding."));
                        }
                        auto signature = make_stereo_ms_execution_signature(
                            descriptor.value()->get(), instance.instance_id(),
                            *parameters, source.format().channel_layout(), true);
                        if (!signature) {
                            return rgsml::core::Result<RenderResult>::failure(*signature.error());
                        }
                        signatures.push_back(std::move(*signature.value()));
                    } else if (is_gain) {
                        const auto* gain_params = std::get_if<rgsml::dsp::GainParameters>(&binding->parameters);
                        signatures.push_back(ModuleExecutionSignature{
                            instance.instance_id(),
                            std::string{kGainTypeId},
                            std::string{descriptor.value()->get().algorithm_version().value_or("1.0.0")},
                            std::string{descriptor.value()->get().parameter_schema_id().value_or("rgsml.dsp.gain.parameters/1.0.0")},
                            ModuleExecutionDisposition::BYPASS_IDENTITY,
                            GainExecutionSignaturePayload{gain_params->gain_db()}});
                    } else if (is_eq) {
                        const auto* eq_params = std::get_if<rgsml::dsp::ParametricEqParameters>(&binding->parameters);
                        std::vector<EqBandSignaturePayload> enabled_bands;
                        for (const auto& band : eq_params->bands()) {
                            if (band.enabled()) {
                                enabled_bands.push_back(EqBandSignaturePayload{
                                    band.filter_type(),
                                    band.routing(),
                                    band.payload()});
                            }
                        }
                        signatures.push_back(ModuleExecutionSignature{
                            instance.instance_id(),
                            std::string{kEqTypeId},
                            std::string{descriptor.value()->get().algorithm_version().value_or("1.0.0")},
                            std::string{descriptor.value()->get().parameter_schema_id().value_or("rgsml.dsp.parametric-eq.parameters/1.0.0")},
                            ModuleExecutionDisposition::BYPASS_IDENTITY,
                            ParametricEqExecutionSignaturePayload{std::move(enabled_bands)}});
                    } else if (is_compressor) {
                        const auto* comp_params = std::get_if<rgsml::dsp::CompressorParameters>(&binding->parameters);
                        const bool is_mono = (source.format().channel_layout() == rgsml::audio::ChannelLayout::MONO_C);
                        std::optional<rgsml::dsp::CompressorChannelLink> effective_link =
                            is_mono ? std::nullopt : std::optional<rgsml::dsp::CompressorChannelLink>{comp_params->channel_link()};
                        signatures.push_back(ModuleExecutionSignature{
                            instance.instance_id(),
                            std::string{kCompressorTypeId},
                            std::string{descriptor.value()->get().algorithm_version().value_or("1.0.0")},
                            std::string{descriptor.value()->get().parameter_schema_id().value_or("rgsml.dsp.compressor.parameters/1.0.0")},
                            ModuleExecutionDisposition::BYPASS_IDENTITY,
                            CompressorExecutionSignaturePayload{
                                comp_params->detector_mode(),
                                effective_link,
                                comp_params->threshold_dbfs(),
                                comp_params->ratio(),
                                comp_params->knee_db(),
                                comp_params->attack_ms(),
                                comp_params->release_ms(),
                                comp_params->rms_time_constant_ms(),
                                comp_params->look_ahead_ms(),
                                comp_params->mix_percent(),
                                comp_params->makeup_gain_db()}});
                    }
                }
                continue;
            }

            if (!registry.has_factory(instance.module_type_id())) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "MODULE_IMPLEMENTATION_UNAVAILABLE",
                    "An active chain node has no production implementation."));
            }

            const auto* binding = find_binding(request, instance.instance_id());
            if (is_parameterized && binding == nullptr) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "MODULE_IMPLEMENTATION_UNAVAILABLE",
                    "An active chain node has no parameter binding."));
            }

            auto module = (binding != nullptr)
                ? registry.create_module(instance.module_type_id(), binding->parameters)
                : registry.create_module(instance.module_type_id());
            if (!module) {
                return rgsml::core::Result<RenderResult>::failure(*module.error());
            }

            auto requirements = (*module.value())->runtime_requirements(process_spec);
            if (!requirements) {
                return rgsml::core::Result<RenderResult>::failure(*requirements.error());
            }
            const auto& required = *requirements.value();
            if (required.execution_model != rgsml::dsp::DspExecutionModel::STREAMING_CAUSAL
                || required.requires_prepass) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "UNSUPPORTED_RENDER_REQUIREMENTS",
                    "Render Preview accepts only streaming-causal modules without prepass."));
            }

            const auto module_latency = required.algorithmic_latency_frames.value();

            const auto next_cum_res = rgsml::core::checked_add(cumulative_latency, module_latency);
            if (!next_cum_res) {
                return rgsml::core::Result<RenderResult>::failure(*next_cum_res.error());
            }
            cumulative_latency = *next_cum_res.value();

            auto prepared = (*module.value())->prepare(process_spec);
            if (!prepared) {
                return rgsml::core::Result<RenderResult>::failure(*prepared.error());
            }
            (*module.value())->reset();
            modules.push_back(PreparedModule{
                std::move(*module.value()),
                instance.instance_id(),
                std::string{instance.module_type_id()},
                required.algorithmic_latency_frames.value(),
                required.look_ahead_frames.value(),
                required.effective_tail_frames.value()});

            if (is_stereo_ms) {
                const auto* parameters = std::get_if<rgsml::dsp::StereoMsParameters>(
                    &binding->parameters);
                if (parameters == nullptr) {
                    return rgsml::core::Result<RenderResult>::failure(render_error(
                        rgsml::core::ErrorCode::InvalidArgument,
                        "MODULE_PARAMETER_PAYLOAD_MISMATCH",
                        "Stereo/M-S requires a typed StereoMsParameters binding."));
                }
                auto signature = make_stereo_ms_execution_signature(
                    descriptor.value()->get(), instance.instance_id(),
                    *parameters, source.format().channel_layout(), false);
                if (!signature) {
                    return rgsml::core::Result<RenderResult>::failure(*signature.error());
                }
                signatures.push_back(std::move(*signature.value()));
            } else if (is_gain) {
                const auto* gain_params = std::get_if<rgsml::dsp::GainParameters>(&binding->parameters);
                signatures.push_back(ModuleExecutionSignature{
                    instance.instance_id(),
                    std::string{kGainTypeId},
                    std::string{descriptor.value()->get().algorithm_version().value_or("1.0.0")},
                    std::string{descriptor.value()->get().parameter_schema_id().value_or("rgsml.dsp.gain.parameters/1.0.0")},
                    ModuleExecutionDisposition::PROCESSED,
                    GainExecutionSignaturePayload{gain_params->gain_db()}});
            } else if (is_eq) {
                const auto* eq_params = std::get_if<rgsml::dsp::ParametricEqParameters>(&binding->parameters);
                std::vector<EqBandSignaturePayload> enabled_bands;
                for (const auto& band : eq_params->bands()) {
                    if (band.enabled()) {
                        enabled_bands.push_back(EqBandSignaturePayload{
                            band.filter_type(),
                            band.routing(),
                            band.payload()});
                    }
                }
                signatures.push_back(ModuleExecutionSignature{
                    instance.instance_id(),
                    std::string{kEqTypeId},
                    std::string{descriptor.value()->get().algorithm_version().value_or("1.0.0")},
                    std::string{descriptor.value()->get().parameter_schema_id().value_or("rgsml.dsp.parametric-eq.parameters/1.0.0")},
                    ModuleExecutionDisposition::PROCESSED,
                    ParametricEqExecutionSignaturePayload{std::move(enabled_bands)}});
            } else if (is_compressor) {
                const auto* comp_params = std::get_if<rgsml::dsp::CompressorParameters>(&binding->parameters);
                const bool is_mono = (source.format().channel_layout() == rgsml::audio::ChannelLayout::MONO_C);
                std::optional<rgsml::dsp::CompressorChannelLink> effective_link =
                    is_mono ? std::nullopt : std::optional<rgsml::dsp::CompressorChannelLink>{comp_params->channel_link()};
                signatures.push_back(ModuleExecutionSignature{
                    instance.instance_id(),
                    std::string{kCompressorTypeId},
                    std::string{descriptor.value()->get().algorithm_version().value_or("1.0.0")},
                    std::string{descriptor.value()->get().parameter_schema_id().value_or("rgsml.dsp.compressor.parameters/1.0.0")},
                    ModuleExecutionDisposition::PROCESSED,
                    CompressorExecutionSignaturePayload{
                        comp_params->detector_mode(),
                        effective_link,
                        comp_params->threshold_dbfs(),
                        comp_params->ratio(),
                        comp_params->knee_db(),
                        comp_params->attack_ms(),
                        comp_params->release_ms(),
                        comp_params->rms_time_constant_ms(),
                        comp_params->look_ahead_ms(),
                        comp_params->mix_percent(),
                        comp_params->makeup_gain_db()}});
            }
        }

        auto result_buffer = rgsml::audio::AudioBuffer::create(
            source.format(),
            source.timebase().frame_domain_id(),
            window.begin(),
            *range_length.value());
        if (!result_buffer) {
            return rgsml::core::Result<RenderResult>::failure(*result_buffer.error());
        }

        const auto max_block_size = request.maximum_block_frames().value();
        const auto source_start = source.absolute_start_frame().value();
        const auto source_end = source.absolute_end_frame().value();
        const auto total_latency_val = cumulative_latency;

        const auto target_raw_end_res = rgsml::core::checked_add(window.end().value(), total_latency_val);
        if (!target_raw_end_res) {
            return rgsml::core::Result<RenderResult>::failure(*target_raw_end_res.error());
        }
        const auto target_raw_end = *target_raw_end_res.value();
        const auto needed_source_end = std::min(source_end, target_raw_end);
        const auto source_read_count = needed_source_end - source_start;

        std::optional<CompressorTelemetrySidecar> compressor_sidecar;
        std::vector<StereoMsStageOutputSidecar> stereo_ms_stage_captures;

        if (modules.empty()) {
            auto source_chunk = source.subview(
                window.begin(),
                *range_length.value());
            if (!source_chunk) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::InvalidFrameRange,
                    "INVALID_RENDER_CHUNK",
                    "Render Preview could not materialize a validated chunk range."));
            }
            auto copied = copy_audio(*source_chunk.value(), result_buffer.value()->mutable_view());
            if (!copied) {
                return rgsml::core::Result<RenderResult>::failure(*copied.error());
            }
        } else {
            // Stage 0 source initial buffer
            const auto initial_count = *rgsml::core::FrameCount::create(source_read_count).value();
            auto current_buffer = rgsml::audio::AudioBuffer::create(
                source.format(),
                source.timebase().frame_domain_id(),
                source.absolute_start_frame(),
                initial_count);
            if (!current_buffer) {
                return rgsml::core::Result<RenderResult>::failure(*current_buffer.error());
            }

            if (source_read_count > 0) {
                auto source_chunk = source.subview(
                    source.absolute_start_frame(),
                    initial_count);
                if (!source_chunk) {
                    return rgsml::core::Result<RenderResult>::failure(render_error(
                        rgsml::core::ErrorCode::InvalidFrameRange,
                        "INVALID_RENDER_CHUNK",
                        "Render Preview could not materialize a validated source chunk."));
                }
                auto copied = copy_audio(*source_chunk.value(), current_buffer.value()->mutable_view());
                if (!copied) {
                    return rgsml::core::Result<RenderResult>::failure(*copied.error());
                }
            }

            std::int64_t stage_start = source_start;
            std::int64_t stage_end = needed_source_end;
            bool stream_eos_reached = (needed_source_end == source_end);
            std::int64_t stage_accumulated_latency = 0;

            for (std::size_t m = 0; m < modules.size(); ++m) {
                // This module's stage samples live in the accumulated
                // latency domain, not necessarily the final playback domain.
                stage_accumulated_latency += modules[m].algorithmic_latency_frames;
                const auto in_count = stage_end - stage_start;
                const bool is_latency_or_lookahead_bearing =
                    (modules[m].algorithmic_latency_frames > 0 || modules[m].look_ahead_frames > 0);
                const bool will_finalize =
                    stream_eos_reached && is_latency_or_lookahead_bearing && (modules[m].effective_tail_frames > 0);
                const auto drain_length = will_finalize ? modules[m].effective_tail_frames : 0;

                const auto out_count_res = rgsml::core::checked_add(in_count, drain_length);
                if (!out_count_res) {
                    return rgsml::core::Result<RenderResult>::failure(*out_count_res.error());
                }

                const auto out_frame_count = *rgsml::core::FrameCount::create(*out_count_res.value()).value();

                std::unique_ptr<CompressorTelemetryCollector> collector;
                auto* comp_mod = dynamic_cast<rgsml::dsp::CompressorModule*>(modules[m].instance.get());
                if (comp_mod != nullptr) {
                    const auto* binding = find_binding(request, modules[m].instance_id);
                    if (binding != nullptr) {
                        if (const auto* comp_params = std::get_if<rgsml::dsp::CompressorParameters>(&binding->parameters)) {
                            const std::int64_t total_mod_frames = out_frame_count.value();
                            const auto sample_rate_val = static_cast<std::uint32_t>(source.format().sample_rate().value());
                            collector = std::make_unique<CompressorTelemetryCollector>(
                                stage_start,
                                total_mod_frames,
                                sample_rate_val,
                                source.format().channel_layout(),
                                comp_params->channel_link(),
                                modules[m].instance_id,
                                request.chain_revision(),
                                request.max_telemetry_bytes().value_or(128U * 1024U * 1024U),
                                request.realization_id());
                            comp_mod->set_telemetry_sink(collector.get());
                        }
                    }
                }

                auto next_buffer = rgsml::audio::AudioBuffer::create(
                    source.format(),
                    source.timebase().frame_domain_id(),
                    rgsml::core::FrameIndex{stage_start},
                    out_frame_count);
                if (!next_buffer) {
                    return rgsml::core::Result<RenderResult>::failure(*next_buffer.error());
                }

                // Step A: Process input frames
                std::int64_t cursor = stage_start;
                while (cursor < stage_end) {
                    const auto chunk_size = std::min(stage_end - cursor, max_block_size);
                    const auto chunk_count = *rgsml::core::FrameCount::create(chunk_size).value();
                    const rgsml::core::FrameIndex chunk_start{cursor};
                    const rgsml::core::FrameIndex chunk_end{cursor + chunk_size};
                    const auto chunk_range = *rgsml::core::FrameRange::create(
                        chunk_start, chunk_end).value();

                    auto in_chunk = current_buffer.value()->view().subview(chunk_start, chunk_count);
                    auto out_chunk = next_buffer.value()->mutable_view().subview(chunk_start, chunk_count);
                    if (!in_chunk || !out_chunk) {
                        return rgsml::core::Result<RenderResult>::failure(render_error(
                            rgsml::core::ErrorCode::InvalidFrameRange,
                            "INVALID_RENDER_CHUNK",
                            "Render Preview could not materialize a validated process chunk range."));
                    }

                    const bool is_begins = (cursor == source_start);
                    const bool is_ends = (cursor + chunk_size == stage_end && stream_eos_reached && !will_finalize);
                    const rgsml::dsp::DspProcessContext context{
                        chunk_range,
                        is_begins,
                        is_ends};

                    auto processed = modules[m].instance->process(*in_chunk.value(), *out_chunk.value(), context);
                    if (!processed) {
                        return rgsml::core::Result<RenderResult>::failure(*processed.error());
                    }
                    cursor += chunk_size;
                }

                // Step B: Finalize tail emission if at EOS and module is latency/lookahead bearing
                if (will_finalize) {
                    std::int64_t fin_cursor = stage_end;
                    const auto fin_end_res = rgsml::core::checked_add(stage_end, drain_length);
                    if (!fin_end_res) {
                        return rgsml::core::Result<RenderResult>::failure(*fin_end_res.error());
                    }
                    const std::int64_t fin_end = *fin_end_res.value();
                    while (fin_cursor < fin_end) {
                        const auto chunk_size = std::min(fin_end - fin_cursor, max_block_size);
                        const auto chunk_count = *rgsml::core::FrameCount::create(chunk_size).value();
                        const rgsml::core::FrameIndex chunk_start{fin_cursor};
                        const rgsml::core::FrameIndex chunk_end{fin_cursor + chunk_size};
                        const auto chunk_range = *rgsml::core::FrameRange::create(
                            chunk_start, chunk_end).value();

                        auto out_chunk = next_buffer.value()->mutable_view().subview(chunk_start, chunk_count);
                        if (!out_chunk) {
                            return rgsml::core::Result<RenderResult>::failure(render_error(
                                rgsml::core::ErrorCode::InvalidFrameRange,
                                "INVALID_RENDER_CHUNK",
                                "Render Preview could not materialize a validated finalize chunk range."));
                        }

                        const bool is_begins = (fin_cursor == stage_end && cursor == stage_start);
                        const bool is_ends = (fin_cursor + chunk_size == fin_end);
                        const rgsml::dsp::DspProcessContext context{
                            chunk_range,
                            is_begins,
                            is_ends};

                        auto finalized = modules[m].instance->finalize(*out_chunk.value(), context);
                        if (!finalized) {
                            return rgsml::core::Result<RenderResult>::failure(*finalized.error());
                        }
                        fin_cursor += chunk_size;
                    }
                }

                if (comp_mod != nullptr && collector != nullptr) {
                    comp_mod->set_telemetry_sink(nullptr);
                    compressor_sidecar = collector->build_sidecar();
                }

                if (modules[m].type_id == kStereoMsTypeId
                    && source.format().channel_layout()
                        == rgsml::audio::ChannelLayout::STEREO_LR) {
                    StereoMsStageOutputSidecar capture{modules[m].instance_id};
                    capture.chain_revision = request.chain_revision();
                    capture.realization_id = request.realization_id();
                    capture.channel_layout = source.format().channel_layout();
                    capture.sample_rate_hz =
                        static_cast<std::uint32_t>(source.format().sample_rate().value());
                    capture.frame_domain_id = source.timebase().frame_domain_id();

                    const auto requested_start = rgsml::core::checked_add(
                        window.begin().value(), stage_accumulated_latency);
                    const auto requested_end = rgsml::core::checked_add(
                        window.end().value(), stage_accumulated_latency);
                    if (requested_start && requested_end) {
                        capture.requested_begin_frame = *requested_start.value();
                        capture.requested_end_frame = *requested_end.value();
                        capture.captured_begin_frame = capture.requested_begin_frame;

                        // Limit this exact PCM excerpt to 64 KiB; 400 ms
                        // correlation and live density are separate B4b/B4c
                        // streaming measurements, NEVER inferred from a
                        // truncated prefix as if it were full evidence.
                        constexpr std::size_t kMaxCaptureBytes = 64U * 1024U;
                        constexpr std::size_t kMaxCaptureFrames = 4096U;
                        const auto cap_bytes = std::min(
                            kMaxCaptureBytes,
                            request.max_telemetry_bytes().value_or(kMaxCaptureBytes));
                        const auto available = next_buffer.value()->view();
                        const auto capture_start = capture.requested_begin_frame;
                        const auto capture_end = capture.requested_end_frame;
                        const auto first = available.absolute_start_frame().value();
                        const auto last = available.absolute_end_frame().value();
                        if (capture_start >= first && capture_start < last
                            && capture_end > capture_start) {
                            const auto remaining = static_cast<std::size_t>(
                                std::min(capture_end, last) - capture_start);
                            const auto take = std::min({
                                remaining,
                                kMaxCaptureFrames,
                                cap_bytes / sizeof(std::array<double, 2>)});
                            if (take > 0) {
                                try {
                                    const auto left = *available.channel(0).value();
                                    const auto right = *available.channel(1).value();
                                    const auto offset = static_cast<std::size_t>(
                                        capture_start - first);
                                    capture.output_lr_frames.reserve(take);
                                    bool finite = true;
                                    for (std::size_t i = 0; i < take; ++i) {
                                        const double l = left[offset + i];
                                        const double r = right[offset + i];
                                        if (!std::isfinite(l) || !std::isfinite(r)) {
                                            finite = false;
                                            break;
                                        }
                                        capture.output_lr_frames.push_back({l, r});
                                    }
                                    if (finite) {
                                        const bool complete =
                                            capture_start == capture.requested_begin_frame
                                            && take == static_cast<std::size_t>(
                                                capture_end - capture_start);
                                        capture.status = complete
                                            ? StereoMsStageCaptureStatus::COMPLETE
                                            : StereoMsStageCaptureStatus::PARTIAL;
                                    } else {
                                        capture.output_lr_frames.clear();
                                    }
                                } catch (...) {
                                    // Telemetry allocation failures must NOT
                                    // turn successful audible PCM into a failure.
                                    capture.output_lr_frames.clear();
                                }
                            }
                        }
                    }
                    // Compute canonical complete correlation windows from
                    // the FULL real stage output, never B4a's capped excerpt.
                    // A failed or incomplete window remains UNAVAILABLE.
                    try {
                        const auto stage_grid_origin = rgsml::core::checked_add(
                            source_start, stage_accumulated_latency);
                        if (stage_grid_origin) {
                            collect_stage_correlation(
                                capture, next_buffer.value()->view(),
                                *stage_grid_origin.value(),
                                request.max_telemetry_bytes().value_or(
                                    128U * 1024U * 1024U));
                        }
                    } catch (...) {
                        capture.correlation_windows.clear();
                        capture.correlation_status = StereoMsCorrelationStatus::UNAVAILABLE;
                    }
                    stereo_ms_stage_captures.push_back(std::move(capture));
                }

                current_buffer = std::move(next_buffer);
                stage_end = *rgsml::core::checked_add(stage_end, drain_length).value();
            }

            // Slice target raw range [window_begin + total_latency, window_end + total_latency]
            const auto target_raw_begin_res = rgsml::core::checked_add(window.begin().value(), total_latency_val);
            if (!target_raw_begin_res) {
                return rgsml::core::Result<RenderResult>::failure(*target_raw_begin_res.error());
            }
            const auto target_raw_begin = *target_raw_begin_res.value();
            const rgsml::core::FrameIndex slice_start{target_raw_begin};
            auto final_subview = current_buffer.value()->view().subview(
                slice_start,
                *range_length.value());
            if (!final_subview) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::InvalidFrameRange,
                    "INVALID_RENDER_WINDOW",
                    "Render Preview could not slice the compensated output range."));
            }
            auto copied = copy_audio(*final_subview.value(), result_buffer.value()->mutable_view());
            if (!copied) {
                return rgsml::core::Result<RenderResult>::failure(*copied.error());
            }
        }

        return rgsml::core::Result<RenderResult>::success(RenderResult{
            std::move(*result_buffer.value()),
            window,
            source.timebase().frame_domain_id(),
            request.chain_revision(),
            std::move(signatures),
            std::move(compressor_sidecar),
            std::move(stereo_ms_stage_captures)});
    } catch (...) {
        return rgsml::core::Result<RenderResult>::failure(render_error(
            rgsml::core::ErrorCode::InvalidState,
            "RENDER_PREVIEW_FAILURE",
            "Render Preview contained an implementation failure at its public boundary."));
    }
}

}  // namespace rgsml::render
