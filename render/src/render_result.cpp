#include <rgsml/render/render_result.hpp>
#include <rgsml/render/linked_activity_render_capture.hpp>

#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::render {

RenderResult::RenderResult(
    rgsml::audio::AudioBuffer buffer,
    rgsml::core::FrameRange render_window,
    rgsml::audio::FrameDomainId frame_domain_id,
    std::uint64_t chain_revision,
    std::vector<ModuleExecutionSignature> signatures,
    std::optional<CompressorTelemetrySidecar> compressor_telemetry_sidecar,
    std::vector<StereoMsStageOutputSidecar> stereo_ms_stage_output_sidecars) noexcept
    : buffer_(std::move(buffer))
    , render_window_(render_window)
    , frame_domain_id_(frame_domain_id)
    , chain_revision_(chain_revision)
    , signatures_(std::move(signatures))
    , compressor_telemetry_sidecar_(std::move(compressor_telemetry_sidecar))
    , stereo_ms_stage_output_sidecars_(std::move(stereo_ms_stage_output_sidecars))
{
}

bool RenderResult::adopt_linked_activity(
    std::unique_ptr<linked_activity::LinkedActivityRenderCapture> capture,
    std::string_view exact_plan_checksum) noexcept
{
    if(linked_activity_sidecar_||!capture||!capture->sealed()||
       exact_plan_checksum.empty()||
       frame_domain_id_!=audio::FrameDomainId::SOURCE_PROCESSING_RATE||
       view().absolute_start_frame().value()!=0||
       capture->frame_count()!=static_cast<std::uint64_t>(view().frame_count().value())||
       capture->sample_rate_hz()!=static_cast<std::uint32_t>(view().format().sample_rate().value()))
        return false;
    try {
        linked_activity_sidecar_.emplace(LinkedDspActivitySidecar{
            std::shared_ptr<const linked_activity::LinkedActivityRenderCapture>{std::move(capture)},
            std::string{exact_plan_checksum},std::nullopt});
        return true;
    } catch(...) {return false;}
}

void RenderResult::bind_linked_activity_realization_id(core::RealizationId id) noexcept
{
    if(!linked_activity_sidecar_)return;
    auto& evidence=*linked_activity_sidecar_;
    if(id.value==0||(evidence.realization_id&&*evidence.realization_id!=id)) {
        // Optional evidence never prevents a legitimate PCM publication.
        linked_activity_sidecar_.reset();
        return;
    }
    evidence.realization_id=id;
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

const std::vector<StereoMsStageOutputSidecar>&
RenderResult::stereo_ms_stage_output_sidecars() const noexcept
{
    return stereo_ms_stage_output_sidecars_;
}

rgsml::core::Status RenderResult::bind_stereo_ms_stage_output_realization_id(
    rgsml::core::RealizationId realization_id) noexcept
{
    // Validate ALL carriers before binding any: late conflicts cannot
    // partially mutate the identity of an unpublished RenderResult.
    for (const auto& sidecar : stereo_ms_stage_output_sidecars_) {
        if (sidecar.realization_id && *sidecar.realization_id != realization_id) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidState,
                "Stereo/M-S stage capture realization identity conflicts with the accepted Processed realization."});
        }
    }
    for (auto& sidecar : stereo_ms_stage_output_sidecars_) {
        sidecar.realization_id = realization_id;
    }
    return rgsml::core::Status::success();
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
