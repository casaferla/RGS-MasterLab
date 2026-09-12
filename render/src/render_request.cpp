#include <rgsml/render/render_request.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace rgsml::render {
namespace {

constexpr auto kGainTypeId = "rgsml.dsp.gain";

[[nodiscard]] rgsml::core::Error request_error(
    rgsml::core::ErrorCode code,
    std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

}  // namespace

rgsml::core::Result<RenderRequest> RenderRequest::create(
    rgsml::audio::AudioBufferView source,
    rgsml::core::FrameRange render_window,
    const rgsml::dsp::ProcessingChain& chain,
    std::vector<GainParameterBinding> gain_bindings,
    rgsml::core::FrameCount maximum_block_frames)
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

    for (std::size_t index = 0; index < gain_bindings.size(); ++index) {
        const auto duplicate = std::find_if(
            gain_bindings.begin(),
            gain_bindings.begin() + static_cast<std::ptrdiff_t>(index),
            [&gain_bindings, index](const GainParameterBinding& candidate) {
                return candidate.instance_id == gain_bindings[index].instance_id;
            });
        if (duplicate != gain_bindings.begin() + static_cast<std::ptrdiff_t>(index)) {
            return rgsml::core::Result<RenderRequest>::failure(request_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "DUPLICATE_GAIN_PARAMETER_BINDING",
                "A Gain instance has more than one parameter binding."));
        }
    }

    for (const auto& instance : chain.instances()) {
        const auto binding = std::find_if(
            gain_bindings.begin(),
            gain_bindings.end(),
            [&instance](const GainParameterBinding& candidate) {
                return candidate.instance_id == instance.instance_id();
            });
        if (instance.module_type_id() == kGainTypeId) {
            if (binding == gain_bindings.end()) {
                return rgsml::core::Result<RenderRequest>::failure(request_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "MISSING_GAIN_PARAMETER_BINDING",
                    "Every Gain snapshot entry requires exactly one value-owned binding."));
            }
        } else if (binding != gain_bindings.end()) {
            return rgsml::core::Result<RenderRequest>::failure(request_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "GAIN_PARAMETER_TYPE_MISMATCH",
                "A Gain binding targets a non-Gain chain instance."));
        }
    }
    if (gain_bindings.size()
        != static_cast<std::size_t>(std::ranges::count_if(
            chain.instances(),
            [](const rgsml::dsp::ModuleInstance& instance) {
                return instance.module_type_id() == kGainTypeId;
            }))) {
        return rgsml::core::Result<RenderRequest>::failure(request_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "UNKNOWN_GAIN_PARAMETER_BINDING",
            "A Gain binding does not identify an instance in the chain snapshot."));
    }

    try {
        return rgsml::core::Result<RenderRequest>::success(RenderRequest{
            source,
            render_window,
            chain.context(),
            chain.revision(),
            std::vector<rgsml::dsp::ModuleInstance>{
                chain.instances().begin(), chain.instances().end()},
            std::move(gain_bindings),
            maximum_block_frames});
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
    std::vector<GainParameterBinding> gain_bindings,
    rgsml::core::FrameCount maximum_block_frames) noexcept
    : source_(source)
    , render_window_(render_window)
    , chain_context_(chain_context)
    , chain_revision_(chain_revision)
    , chain_instances_(std::move(chain_instances))
    , gain_bindings_(std::move(gain_bindings))
    , maximum_block_frames_(maximum_block_frames)
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
std::span<const GainParameterBinding> RenderRequest::gain_bindings() const noexcept
{
    return gain_bindings_;
}
rgsml::core::FrameCount RenderRequest::maximum_block_frames() const noexcept
{
    return maximum_block_frames_;
}
RenderMode RenderRequest::mode() const noexcept { return RenderMode::PREVIEW; }

}  // namespace rgsml::render
