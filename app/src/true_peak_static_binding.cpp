#include "true_peak_static_binding.hpp"

#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::app {

bool BoundStaticTruePeak::matches_audible(
    const core::PlaybackSnapshot& snapshot) const noexcept
{
    return snapshot.state == core::PlaybackState::PLAYING
        && snapshot.audibleRealization.phase == core::AudibleHandoffPhase::NEW
        && snapshot.audibleRealization.realizationId.has_value()
        && *snapshot.audibleRealization.realizationId == realizationId
        && snapshot.position >= coveredSourceFrames.begin()
        && snapshot.position < coveredSourceFrames.end();
}

core::Result<BoundStaticTruePeak>
measure_accepted_processed_true_peak(
    std::shared_ptr<const render::RenderResult> accepted,
    core::RealizationId processedRealizationId,
    const TruePeakExpectedSource& expected)
{
    const auto invalid = [](const char* why) {
        return core::Result<BoundStaticTruePeak>::failure(
            core::Error{core::ErrorCode::InvalidState, why});
    };
    if (!accepted || processedRealizationId.value == 0
        || expected.frameCount.value() <= 0) {
        return invalid("True-Peak C1 requires a published, identified, nonempty resource.");
    }

    const auto expectedRange = core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{expected.frameCount.value()});
    if (!expectedRange) {
        return core::Result<BoundStaticTruePeak>::failure(*expectedRange.error());
    }

    const auto view = accepted->view();
    const auto ownedView = accepted->buffer().view();
    if (accepted->render_window() != *expectedRange.value()
        || view.absolute_range() != *expectedRange.value()
        || ownedView.absolute_range() != *expectedRange.value()
        || accepted->frame_domain_id() != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.timebase().frame_domain_id() != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.frame_count() != expected.frameCount
        || view.format().sample_rate() != expected.sampleRate
        || view.format().channel_layout() != expected.layout) {
        return invalid("True-Peak C1 cannot certify incomplete or mismatched source coverage.");
    }

    // Existing unmodified M16 full-resource numeric implementation.
    auto measured = analysis::measure_true_peak_full_resource(accepted->buffer());
    if (!measured) {
        return core::Result<BoundStaticTruePeak>::failure(*measured.error());
    }

    const auto chainRevision = accepted->chain_revision();
    return core::Result<BoundStaticTruePeak>::success(BoundStaticTruePeak{
        std::move(*measured.value()), std::move(accepted),
        processedRealizationId, *expectedRange.value(), chainRevision});
}

} // namespace rgsml::app
