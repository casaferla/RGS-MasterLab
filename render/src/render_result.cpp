#include <rgsml/render/render_result.hpp>

#include <rgsml/core/error.hpp>

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

rgsml::core::Status RenderResult::bind_compressor_telemetry_realization_id(
    rgsml::core::RealizationId realization_id) noexcept
{
    if (!compressor_telemetry_sidecar_) {
        return rgsml::core::Status::success();
    }

    const auto conflicts = [realization_id](
        const std::optional<rgsml::core::RealizationId>& candidate) noexcept {
        return candidate.has_value() && *candidate != realization_id;
    };

    auto& sidecar = *compressor_telemetry_sidecar_;
    if (conflicts(sidecar.realization_id)) {
        return rgsml::core::Status::failure(rgsml::core::Error{
            rgsml::core::ErrorCode::InvalidState,
            "Compressor telemetry sidecar realization identity conflicts with the accepted Processed realization."});
    }
    for (const auto& lane : sidecar.channel_lanes) {
        for (const auto& bucket : lane.buckets) {
            if (conflicts(bucket.realization_id)) {
                return rgsml::core::Status::failure(rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidState,
                    "Compressor telemetry bucket realization identity conflicts with the accepted Processed realization."});
            }
        }
    }

    sidecar.realization_id = realization_id;
    for (auto& lane : sidecar.channel_lanes) {
        for (auto& bucket : lane.buckets) {
            bucket.realization_id = realization_id;
        }
    }
    return rgsml::core::Status::success();
}

}  // namespace rgsml::render
