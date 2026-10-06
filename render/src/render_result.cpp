#include <rgsml/render/render_result.hpp>

#include <utility>

namespace rgsml::render {

RenderResult::RenderResult(
    rgsml::audio::AudioBuffer buffer,
    rgsml::core::FrameRange render_window,
    rgsml::audio::FrameDomainId frame_domain_id,
    std::uint64_t chain_revision,
    std::vector<ModuleExecutionSignature> signatures,
    std::optional<CompressorTelemetrySidecar> compressor_telemetry_sidecar) noexcept
    : buffer_(std::move(buffer))
    , render_window_(render_window)
    , frame_domain_id_(frame_domain_id)
    , chain_revision_(chain_revision)
    , signatures_(std::move(signatures))
    , compressor_telemetry_sidecar_(std::move(compressor_telemetry_sidecar))
{
}

const rgsml::audio::AudioBuffer& RenderResult::buffer() const noexcept
{
    return buffer_;
}

rgsml::audio::AudioBufferView RenderResult::view() const noexcept
{
    return buffer_.view();
}

rgsml::core::FrameRange RenderResult::render_window() const noexcept
{
    return render_window_;
}

rgsml::audio::FrameDomainId RenderResult::frame_domain_id() const noexcept
{
    return frame_domain_id_;
}

std::uint64_t RenderResult::chain_revision() const noexcept
{
    return chain_revision_;
}

const std::vector<ModuleExecutionSignature>& RenderResult::signatures() const noexcept
{
    return signatures_;
}

const std::optional<CompressorTelemetrySidecar>&
RenderResult::compressor_telemetry_sidecar() const noexcept
{
    return compressor_telemetry_sidecar_;
}

}  // namespace rgsml::render
