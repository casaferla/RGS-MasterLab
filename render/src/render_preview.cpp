#include <rgsml/render/render_preview.hpp>

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::render {
namespace {

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";

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
        || input.absolute_range() != output.absolute_range()
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

        std::vector<std::unique_ptr<rgsml::dsp::IModule>> modules;
        std::vector<ModuleExecutionSignature> signatures;
        modules.reserve(request.chain_instances().size());
        signatures.reserve(request.bindings().size());

        const rgsml::dsp::DspProcessSpec process_spec{
            source.format(),
            source.timebase().frame_domain_id(),
            request.maximum_block_frames()};

        for (const auto& instance : request.chain_instances()) {
            const auto descriptor = registry.find_descriptor(instance.module_type_id());
            if (!descriptor) {
                return rgsml::core::Result<RenderResult>::failure(*descriptor.error());
            }

            const bool is_gain = (instance.module_type_id() == kGainTypeId);
            const bool is_eq = (instance.module_type_id() == kEqTypeId);

            if (!instance.active()) {
                if (is_gain || is_eq) {
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
                    } else {
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
            if (binding == nullptr) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "MODULE_IMPLEMENTATION_UNAVAILABLE",
                    "An active chain node has no parameter binding."));
            }

            auto module = registry.create_module(
                instance.module_type_id(), binding->parameters);
            if (!module) {
                return rgsml::core::Result<RenderResult>::failure(*module.error());
            }

            auto requirements = (*module.value())->runtime_requirements(process_spec);
            if (!requirements) {
                return rgsml::core::Result<RenderResult>::failure(*requirements.error());
            }
            const auto& required = *requirements.value();
            if (required.execution_model != rgsml::dsp::DspExecutionModel::STREAMING_CAUSAL
                || required.algorithmic_latency_frames.value() != 0
                || required.look_ahead_frames.value() != 0
                || required.requires_prepass) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "UNSUPPORTED_RENDER_REQUIREMENTS",
                    "Render Preview accepts only streaming-causal modules with zero latency and zero lookahead."));
            }

            auto prepared = (*module.value())->prepare(process_spec);
            if (!prepared) {
                return rgsml::core::Result<RenderResult>::failure(*prepared.error());
            }
            (*module.value())->reset();
            modules.push_back(std::move(*module.value()));

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
        const auto window_begin = window.begin().value();
        const auto window_end = window.end().value();

        // Phase 1: Causal Preroll [source_start, window_begin)
        std::int64_t preroll_cursor = source_start;
        while (preroll_cursor < window_begin) {
            const auto remaining = window_begin - preroll_cursor;
            const auto chunk_size = std::min(remaining, max_block_size);
            const auto chunk_count = *rgsml::core::FrameCount::create(chunk_size).value();
            const rgsml::core::FrameIndex chunk_start{preroll_cursor};
            const rgsml::core::FrameIndex chunk_end{preroll_cursor + chunk_size};
            const auto chunk_range = *rgsml::core::FrameRange::create(
                chunk_start, chunk_end).value();

            auto source_chunk = source.subview(chunk_start, chunk_count);
            if (!source_chunk) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::InvalidFrameRange,
                    "INVALID_RENDER_CHUNK",
                    "Render Preview could not materialize a validated preroll chunk range."));
            }

            if (!modules.empty()) {
                auto first_block = rgsml::audio::AudioBuffer::create(
                    source.format(),
                    source.timebase().frame_domain_id(),
                    chunk_start,
                    chunk_count);
                auto second_block = rgsml::audio::AudioBuffer::create(
                    source.format(),
                    source.timebase().frame_domain_id(),
                    chunk_start,
                    chunk_count);
                if (!first_block || !second_block) {
                    const auto* error = first_block ? second_block.error() : first_block.error();
                    return rgsml::core::Result<RenderResult>::failure(*error);
                }

                auto current = *source_chunk.value();
                for (std::size_t index = 0; index < modules.size(); ++index) {
                    auto destination = (index % 2U == 0U)
                        ? first_block.value()->mutable_view()
                        : second_block.value()->mutable_view();
                    const rgsml::dsp::DspProcessContext context{
                        chunk_range,
                        preroll_cursor == source_start,
                        preroll_cursor + chunk_size == source_end};
                    auto processed = modules[index]->process(current, destination, context);
                    if (!processed) {
                        return rgsml::core::Result<RenderResult>::failure(*processed.error());
                    }
                    current = destination.as_const();
                }
            }
            preroll_cursor += chunk_size;
        }

        // Phase 2: Preview Window Render [window_begin, window_end)
        std::int64_t window_cursor = window_begin;
        while (window_cursor < window_end) {
            const auto remaining = window_end - window_cursor;
            const auto chunk_size = std::min(remaining, max_block_size);
            const auto chunk_count = *rgsml::core::FrameCount::create(chunk_size).value();
            const rgsml::core::FrameIndex chunk_start{window_cursor};
            const rgsml::core::FrameIndex chunk_end{window_cursor + chunk_size};
            const auto chunk_range = *rgsml::core::FrameRange::create(
                chunk_start, chunk_end).value();

            auto source_chunk = source.subview(chunk_start, chunk_count);
            auto result_chunk = result_buffer.value()->mutable_view().subview(
                chunk_start, chunk_count);
            if (!source_chunk || !result_chunk) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::InvalidFrameRange,
                    "INVALID_RENDER_CHUNK",
                    "Render Preview could not materialize a validated chunk range."));
            }

            if (modules.empty()) {
                auto copied = copy_audio(*source_chunk.value(), *result_chunk.value());
                if (!copied) {
                    return rgsml::core::Result<RenderResult>::failure(*copied.error());
                }
            } else {
                auto first_block = rgsml::audio::AudioBuffer::create(
                    source.format(),
                    source.timebase().frame_domain_id(),
                    chunk_start,
                    chunk_count);
                auto second_block = rgsml::audio::AudioBuffer::create(
                    source.format(),
                    source.timebase().frame_domain_id(),
                    chunk_start,
                    chunk_count);
                if (!first_block || !second_block) {
                    const auto* error = first_block ? second_block.error() : first_block.error();
                    return rgsml::core::Result<RenderResult>::failure(*error);
                }

                auto current = *source_chunk.value();
                for (std::size_t index = 0; index < modules.size(); ++index) {
                    auto destination = (index % 2U == 0U)
                        ? first_block.value()->mutable_view()
                        : second_block.value()->mutable_view();
                    const rgsml::dsp::DspProcessContext context{
                        chunk_range,
                        window_cursor == source_start,
                        window_cursor + chunk_size == source_end};
                    auto processed = modules[index]->process(current, destination, context);
                    if (!processed) {
                        return rgsml::core::Result<RenderResult>::failure(*processed.error());
                    }
                    current = destination.as_const();
                }
                auto copied = copy_audio(current, *result_chunk.value());
                if (!copied) {
                    return rgsml::core::Result<RenderResult>::failure(*copied.error());
                }
            }
            window_cursor += chunk_size;
        }

        return rgsml::core::Result<RenderResult>::success(RenderResult{
            std::move(*result_buffer.value()),
            window,
            source.timebase().frame_domain_id(),
            request.chain_revision(),
            std::move(signatures)});
    } catch (...) {
        return rgsml::core::Result<RenderResult>::failure(render_error(
            rgsml::core::ErrorCode::InvalidState,
            "RENDER_PREVIEW_FAILURE",
            "Render Preview contained an implementation failure at its public boundary."));
    }
}

}  // namespace rgsml::render
