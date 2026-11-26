#pragma once
// No UI-owned clock. Resolve only a sealed candidate attached to the
// Product-accepted PCM and stable NEW playback identity.
#include <rgsml/render/render_result.hpp>
#include <rgsml/render/linked_activity_render_capture.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <string_view>

namespace rgsml::render::linked_activity {
struct LinkedActivityPlaybackContext final {
    core::RealizationId accepted_id{};
    std::string_view exact_plan_checksum;
    std::uint64_t linked_revision{};
    audio::FrameDomainId playback_domain{audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    std::uint32_t playback_rate_hz{};
    bool processed_audition{};
    bool active_node{};
};
struct LinkedActivityWindow final {
    core::RealizationId realization_id{};
    std::uint64_t audible_frame{};
    std::uint32_t sample_rate_hz{};
    bool paused{};
    std::vector<C01ActivityBucket> dynamic;
    std::vector<AppliedGainBucket> transient;
    [[nodiscard]] bool available() const noexcept {
        return !dynamic.empty()||!transient.empty();
    }
};
[[nodiscard]] inline LinkedActivityWindow project_linked_activity(
    const RenderResult& accepted_pcm,const LinkedActivityPlaybackContext& context,
    const core::PlaybackSnapshot& playback,const dsp::ModuleInstanceId& instance)
{
    const auto& sidecar=accepted_pcm.linked_activity_sidecar();
    if(!sidecar||!sidecar->capture||!sidecar->capture->sealed()||
       !sidecar->realization_id||context.accepted_id.value==0||
       *sidecar->realization_id!=context.accepted_id||
       context.exact_plan_checksum.empty()||
       sidecar->exact_plan_checksum!=context.exact_plan_checksum||
       sidecar->capture->revision()!=context.linked_revision||
       !context.processed_audition||!context.active_node||
       context.playback_domain!=audio::FrameDomainId::SOURCE_PROCESSING_RATE||
       accepted_pcm.frame_domain_id()!=context.playback_domain||
       context.playback_rate_hz==0||
       context.playback_rate_hz!=sidecar->capture->sample_rate_hz()||
       (playback.state!=core::PlaybackState::PLAYING&&
        playback.state!=core::PlaybackState::PAUSED)||
       playback.audibleRealization.phase!=core::AudibleHandoffPhase::NEW||
       playback.audibleRealization.realizationId!=sidecar->realization_id||
       playback.position.value()<0||
       !playback.duration||
       playback.duration->value()!=accepted_pcm.view().frame_count().value())
        return {};
    const auto frame=static_cast<std::uint64_t>(playback.position.value());
    const auto width=static_cast<std::uint64_t>(context.playback_rate_hz)*5U;
    LinkedActivityWindow out;
    out.dynamic=sidecar->capture->dynamic_window(instance,frame,width);
    out.transient=sidecar->capture->transient_window(instance,frame,width);
    if(!out.available())return {};
    out.realization_id=context.accepted_id;
    out.audible_frame=frame;
    out.sample_rate_hz=context.playback_rate_hz;
    out.paused=playback.state==core::PlaybackState::PAUSED;
    return out;
}
} // namespace rgsml::render::linked_activity
