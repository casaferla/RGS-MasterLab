#include "true_peak_live_input_binding.hpp"

#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::app {

core::Result<BoundLiveTruePeakInput> bind_processed_live_input(
    std::shared_ptr<const render::RenderResult> accepted,
    core::RealizationId expectedRealization,
    const TruePeakExpectedSource& expectedSource,
    const core::PlaybackSnapshot& playback,
    const platform::windows::WindowsConsumedOutputPrefix& consumed)
{
    const auto fail = [](const char* why) {
        return core::Result<BoundLiveTruePeakInput>::failure(
            core::Error{core::ErrorCode::InvalidState, why});
    };
    // In C2D6 a structural prefix, even a complete one, NEVER claims that
    // the authoritative product target has been independently verified.
    if (consumed.certifiedForLiveTruePeak || !accepted
        || expectedRealization.value == 0
        || !consumed.partitionRepresented || consumed.count != 1U
        || consumed.processedWatermarkExclusive <= 0) {
        return fail("C2D6 requires exactly one complete source-candidate prefix and a retained owner.");
    }
    if (playback.state != core::PlaybackState::PLAYING
        || playback.audibleRealization.phase != core::AudibleHandoffPhase::NEW
        || playback.audibleRealization.realizationId != std::optional{expectedRealization}) {
        return fail("C2D6 heard source is not independently PROCESSED NEW.");
    }
    const auto range = core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{expectedSource.frameCount.value()});
    if (!range || expectedSource.frameCount.value() <= 0) {
        return fail("C2D6 invalid expected source range.");
    }
    const auto view = accepted->view();
    if (accepted->render_window() != *range.value()
        || accepted->buffer().view().absolute_range() != *range.value()
        || view.absolute_range() != *range.value()
        || accepted->frame_domain_id() != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.timebase().frame_domain_id() != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || view.frame_count() != expectedSource.frameCount
        || view.format().sample_rate() != expectedSource.sampleRate
        || view.format().channel_layout() != expectedSource.layout) {
        return fail("C2D6 PROCESSED owner lacks whole canonical source-domain coverage.");
    }
    const auto& slice = consumed.slices[0];
    if (slice.kind != platform::windows::WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED
        || slice.generation != consumed.generation
        || slice.outputBegin != 0 || slice.outputEnd != consumed.processedWatermarkExclusive
        || slice.sourceBegin != std::optional<std::int64_t>{0}
        || slice.sourceEnd != std::optional{consumed.processedWatermarkExclusive}
        || slice.realizationId != std::optional{expectedRealization}
        || slice.outputEnd > expectedSource.frameCount.value()) {
        return fail("C2D6 source mapping is discontinuous, SRC, stale or mismatched.");
    }
    const auto revision = accepted->chain_revision();
    return core::Result<BoundLiveTruePeakInput>::success(
        BoundLiveTruePeakInput{std::move(accepted), expectedRealization,
            revision, consumed.generation, consumed.processedWatermarkExclusive,
            consumed.processedWatermarkExclusive, expectedSource.layout,
            expectedSource.sampleRate});
}

} // namespace rgsml::app
