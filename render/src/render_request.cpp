#include <rgsml/render/render_request.hpp>

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace rgsml::render {
namespace {

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";
constexpr auto kCompressorTypeId = "rgsml.dsp.compressor";

[[nodiscard]] rgsml::core::Error request_error(
    rgsml::core::ErrorCode code,
    std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] bool is_supported_parameterized_builtin(std::string_view type_id) noexcept
{
    return type_id == kGainTypeId || type_id == kEqTypeId || type_id == kCompressorTypeId;
}

}  // namespace

rgsml::core::Result<RenderRequest> RenderRequest::create(
    rgsml::audio::AudioBufferView source,
    rgsml::core::FrameRange render_window,
    const rgsml::dsp::ProcessingChain& chain,
    std::vector<rgsml::dsp::ModuleExecutionBinding> bindings,
    rgsml::core::FrameCount maximum_block_frames,
    std::optional<std::size_t> max_telemetry_bytes)
{
    if (source.timebase().frame_domain_id()
        != rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE) {
        return rgsml::core::Result<RenderRequest>::failure(request_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_RENDER_FRAME_DOMAIN",
            "Render Preview requires the Source processing-rate frame domain."));
    }
    if (render_window.begin() < source.absolute_start_frame()
        || render_window.end() > source.absolute_end_frame()
        || maximum_block_frames.value() <= 0) {
        return rgsml::core::Result<RenderRequest>::failure(request_error(
            rgsml::core::ErrorCode::OutOfRange,
            "INVALID_RENDER_REQUEST",
            "Render window or maximum block size is outside the accepted Source range."));
    }

    // Check duplicate bindings
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        const auto duplicate = std::find_if(
            bindings.begin(),
            bindings.begin() + static_cast<std::ptrdiff_t>(index),
            [&bindings, index](const rgsml::dsp::ModuleExecutionBinding& candidate) {
                return candidate.instance_id == bindings[index].instance_id;
            });
        if (duplicate != bindings.begin() + static_cast<std::ptrdiff_t>(index)) {
            return rgsml::core::Result<RenderRequest>::failure(request_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "DUPLICATE_PARAMETER_BINDING",
                "A module instance has more than one parameter binding."));
        }
    }

    // Check each binding targets an instance in chain, and if that instance is a supported builtin, payload matches type
    for (const auto& binding : bindings) {
        const auto instance_it = std::find_if(
            chain.instances().begin(),
            chain.instances().end(),
            [&binding](const rgsml::dsp::ModuleInstance& inst) {
                return inst.instance_id() == binding.instance_id;
            });
        if (instance_it == chain.instances().end()) {
            return rgsml::core::Result<RenderRequest>::failure(request_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "ORPHAN_PARAMETER_BINDING",
                "A parameter binding does not identify an instance in the chain snapshot."));
        }

        const auto type_id = instance_it->module_type_id();
        if (!is_supported_parameterized_builtin(type_id)) {
            return rgsml::core::Result<RenderRequest>::failure(request_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "UNSUPPORTED_MODULE_BINDING",
                "A parameter binding was provided for an unsupported or non-parameterized module."));
        }

        if (type_id == kGainTypeId) {
            if (!std::holds_alternative<rgsml::dsp::GainParameters>(binding.parameters)) {
                return rgsml::core::Result<RenderRequest>::failure(request_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "MODULE_PARAMETER_PAYLOAD_MISMATCH",
                    "A Gain binding contained a non-Gain payload."));
            }
        } else if (type_id == kEqTypeId) {
            if (!std::holds_alternative<rgsml::dsp::ParametricEqParameters>(binding.parameters)) {
                return rgsml::core::Result<RenderRequest>::failure(request_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "MODULE_PARAMETER_PAYLOAD_MISMATCH",
                    "A Parametric EQ binding contained a non-Parametric EQ payload."));
            }
        } else if (type_id == kCompressorTypeId) {
            if (!std::holds_alternative<rgsml::dsp::CompressorParameters>(binding.parameters)) {
                return rgsml::core::Result<RenderRequest>::failure(request_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "MODULE_PARAMETER_PAYLOAD_MISMATCH",
                    "A Compressor binding contained a non-Compressor payload."));
            }
        }
    }

    // Check each supported built-in instance in chain has EXACTLY ONE binding (even if bypassed)
    for (const auto& instance : chain.instances()) {
        if (is_supported_parameterized_builtin(instance.module_type_id())) {
            const auto binding = std::find_if(
                bindings.begin(),
                bindings.end(),
                [&instance](const rgsml::dsp::ModuleExecutionBinding& candidate) {
                    return candidate.instance_id == instance.instance_id();
                });
            if (binding == bindings.end()) {
                return rgsml::core::Result<RenderRequest>::failure(request_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "MISSING_PARAMETER_BINDING",
                    "Every supported parameterized module snapshot entry requires exactly one binding."));
            }
        }
    }

    try {
        std::optional<std::size_t> effective_telemetry_bytes;
        if (max_telemetry_bytes.has_value()) {
            constexpr std::size_t kAbsoluteMaxMemoryBytes = 128U * 1024U * 1024U;
            effective_telemetry_bytes = std::min(*max_telemetry_bytes, kAbsoluteMaxMemoryBytes);
        }

        return rgsml::core::Result<RenderRequest>::success(RenderRequest{
            source,
            render_window,
            chain.context(),
            chain.revision(),
            std::vector<rgsml::dsp::ModuleInstance>{
                chain.instances().begin(), chain.instances().end()},
            std::move(bindings),
            maximum_block_frames,
            effective_telemetry_bytes});
    } catch (...) {
        return rgsml::core::Result<RenderRequest>::failure(request_error(
            rgsml::core::ErrorCode::IoFailure,
            "RENDER_REQUEST_ALLOCATION_FAILURE",
            "Render Preview could not own its immutable chain snapshot."));
    }
}

RenderRequest::RenderRequest(
    rgsml::audio::AudioBufferView source,
    rgsml::core::FrameRange render_window,
    rgsml::dsp::ProcessingChainContext chain_context,
    std::uint64_t chain_revision,
    std::vector<rgsml::dsp::ModuleInstance> chain_instances,
    std::vector<rgsml::dsp::ModuleExecutionBinding> bindings,
    rgsml::core::FrameCount maximum_block_frames,
    std::optional<std::size_t> max_telemetry_bytes) noexcept
    : source_(source)
    , render_window_(render_window)
    , chain_context_(chain_context)
    , chain_revision_(chain_revision)
    , chain_instances_(std::move(chain_instances))
    , bindings_(std::move(bindings))
    , maximum_block_frames_(maximum_block_frames)
    , max_telemetry_bytes_(max_telemetry_bytes)
{
}

rgsml::audio::AudioBufferView RenderRequest::source() const noexcept { return source_; }
rgsml::core::FrameRange RenderRequest::render_window() const noexcept { return render_window_; }
rgsml::dsp::ProcessingChainContext RenderRequest::chain_context() const noexcept
{
    return chain_context_;
}
std::uint64_t RenderRequest::chain_revision() const noexcept { return chain_revision_; }
std::span<const rgsml::dsp::ModuleInstance> RenderRequest::chain_instances() const noexcept
{
    return chain_instances_;
}
std::span<const rgsml::dsp::ModuleExecutionBinding> RenderRequest::bindings() const noexcept
{
    return bindings_;
}
rgsml::core::FrameCount RenderRequest::maximum_block_frames() const noexcept
{
    return maximum_block_frames_;
}
RenderMode RenderRequest::mode() const noexcept { return RenderMode::PREVIEW; }
std::optional<std::size_t> RenderRequest::max_telemetry_bytes() const noexcept
{
    return max_telemetry_bytes_;
}

}  // namespace rgsml::render
