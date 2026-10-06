#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/compressor_telemetry_collector.hpp>

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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::render {
namespace {

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";
constexpr auto kCompressorTypeId = "rgsml.dsp.compressor";

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

            const bool is_gain = (instance.module_type_id() == kGainTypeId);
            const bool is_eq = (instance.module_type_id() == kEqTypeId);
            const bool is_compressor = (instance.module_type_id() == kCompressorTypeId);
            const bool is_parameterized = is_gain || is_eq || is_compressor;

            if (!instance.active()) {
                if (is_parameterized) {
                    const auto* binding = find_binding(request, instance.instance_id());
                    if (is_gain) {
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

            if (is_gain) {
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

            for (std::size_t m = 0; m < modules.size(); ++m) {
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
                                request.chain_revision());
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
            std::move(compressor_sidecar)});
    } catch (...) {
        return rgsml::core::Result<RenderResult>::failure(render_error(
            rgsml::core::ErrorCode::InvalidState,
            "RENDER_PREVIEW_FAILURE",
            "Render Preview contained an implementation failure at its public boundary."));
    }
}

}  // namespace rgsml::render
