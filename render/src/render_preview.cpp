#include <rgsml/render/render_preview.hpp>

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/gain_module.hpp>

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

[[nodiscard]] rgsml::core::Error render_error(
    rgsml::core::ErrorCode code,
    std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] const GainParameterBinding* find_binding(
    const RenderRequest& request,
    const rgsml::dsp::ModuleInstanceId& id) noexcept
{
    const auto bindings = request.gain_bindings();
    const auto iterator = std::find_if(
        bindings.begin(),
        bindings.end(),
        [&id](const GainParameterBinding& binding) {
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
        const auto range_length = request.render_window().length();
        if (!range_length) {
            return rgsml::core::Result<RenderResult>::failure(*range_length.error());
        }

        std::vector<std::unique_ptr<rgsml::dsp::GainModule>> modules;
        std::vector<GainExecutionSignature> signatures;
        modules.reserve(request.chain_instances().size());
        signatures.reserve(request.gain_bindings().size());

        const rgsml::dsp::DspProcessSpec process_spec{
            source.format(),
            source.timebase().frame_domain_id(),
            request.maximum_block_frames()};

        for (const auto& instance : request.chain_instances()) {
            const auto descriptor = registry.find_descriptor(instance.module_type_id());
            if (!descriptor) {
                return rgsml::core::Result<RenderResult>::failure(*descriptor.error());
            }
            if (!instance.active()) {
                if (instance.module_type_id() == kGainTypeId) {
                    const auto* binding = find_binding(request, instance.instance_id());
                    signatures.push_back(GainExecutionSignature{
                        instance.instance_id(),
                        std::string{kGainTypeId},
                        "1.0.0",
                        "rgsml.dsp.gain.parameters/1.0.0",
                        binding->parameters.gain_db(),
                        GainExecutionDisposition::BYPASS_IDENTITY});
                }
                continue;
            }
            if (instance.module_type_id() != kGainTypeId
                || !registry.has_factory(instance.module_type_id())) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "MODULE_IMPLEMENTATION_UNAVAILABLE",
                    "An active chain node has no production implementation."));
            }
            const auto* binding = find_binding(request, instance.instance_id());
            auto module = rgsml::dsp::GainModule::create(
                descriptor.value()->get(), binding->parameters);
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
                || required.pre_context_frames.value() != 0
                || required.post_context_frames.value() != 0
                || required.effective_tail_frames.value() != 0
                || required.requires_prepass) {
                return rgsml::core::Result<RenderResult>::failure(render_error(
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "UNSUPPORTED_RENDER_REQUIREMENTS",
                    "L1-M08 Preview accepts only zero-context streaming-causal Gain."));
            }
            auto prepared = (*module.value())->prepare(process_spec);
            if (!prepared) {
                return rgsml::core::Result<RenderResult>::failure(*prepared.error());
            }
            (*module.value())->reset();
            modules.push_back(std::move(*module.value()));
            signatures.push_back(GainExecutionSignature{
                instance.instance_id(),
                std::string{kGainTypeId},
                "1.0.0",
                "rgsml.dsp.gain.parameters/1.0.0",
                binding->parameters.gain_db(),
                GainExecutionDisposition::PROCESSED});
        }

        auto result_buffer = rgsml::audio::AudioBuffer::create(
            source.format(),
            source.timebase().frame_domain_id(),
            request.render_window().begin(),
            *range_length.value());
        if (!result_buffer) {
            return rgsml::core::Result<RenderResult>::failure(*result_buffer.error());
        }

        std::int64_t completed = 0;
        while (completed < range_length.value()->value()) {
            const auto remaining = range_length.value()->value() - completed;
            const auto chunk_value = std::min(
                remaining, request.maximum_block_frames().value());
            const auto chunk_count = *rgsml::core::FrameCount::create(chunk_value).value();
            const rgsml::core::FrameIndex chunk_start{
                request.render_window().begin().value() + completed};
            const rgsml::core::FrameIndex chunk_end{chunk_start.value() + chunk_value};
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
                    auto destination = index % 2U == 0U
                        ? first_block.value()->mutable_view()
                        : second_block.value()->mutable_view();
                    const rgsml::dsp::DspProcessContext context{
                        chunk_range,
                        chunk_start == source.absolute_start_frame(),
                        chunk_end == source.absolute_end_frame()};
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
            completed += chunk_value;
        }

        return rgsml::core::Result<RenderResult>::success(RenderResult{
            std::move(*result_buffer.value()),
            request.render_window(),
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
