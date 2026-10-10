#include <rgsml/render/audible_stereo_ms_telemetry_resolver.hpp>

#include <algorithm>
#include <utility>

namespace rgsml::render {

void AudibleStereoMsTelemetryResolver::clear_observation() noexcept
{
    history_.clear();
    correlation_.reset();
    side_low_.reset();
    eligible_begin_.reset();
    last_cursor_.reset();
    gap_detected_ = false;
}

void AudibleStereoMsTelemetryResolver::reset() noexcept
{
    clear_observation();
    registered_.clear();
    active_realization_id_.reset();
    traversal_serial_.reset();
    seek_serial_ = 0;
    loop_wrap_count_ = 0;
    previous_processed_ = false;
    status_ = StereoMsAudibleStatus::UNAVAILABLE;
}

void AudibleStereoMsTelemetryResolver::insert(
    rgsml::core::RealizationId id, Entry entry)
{
    registered_.insert_or_assign(id.value, std::move(entry));
    // Realization IDs are monotonic within their publication domain.
    // Preserve the currently audible OLD realization across a queued handoff.
    while (registered_.size() > 2U) {
        auto it = registered_.begin();
        if (active_realization_id_ && it->first == active_realization_id_->value) {
            ++it;
        }
        if (it == registered_.end()) break;
        registered_.erase(it);
    }
}

void AudibleStereoMsTelemetryResolver::register_sidecar(
    std::shared_ptr<const StereoMsStageOutputSidecar> sidecar)
{
    if (sidecar && sidecar->realization_id) {
        insert(*sidecar->realization_id, Entry{std::move(sidecar), false});
    }
}

void AudibleStereoMsTelemetryResolver::register_bypass(
    rgsml::core::RealizationId realization_id)
{
    insert(realization_id, Entry{nullptr, true});
}

void AudibleStereoMsTelemetryResolver::update(
    const rgsml::core::PlaybackSnapshot& snap, bool auditioning_processed)
{
    using rgsml::core::AudibleHandoffPhase;
    using rgsml::core::PlaybackState;

    if (!auditioning_processed) {
        clear_observation();
        active_realization_id_.reset();
        status_ = StereoMsAudibleStatus::NOT_AUDITIONED;
        previous_processed_ = false;
        return;
    }

    const auto cursor = snap.position.value();
    bool new_epoch = !traversal_serial_.has_value() ||
        snap.traversalSerial != *traversal_serial_ ||
        snap.seekSerial != seek_serial_ ||
        snap.loopWrapCount != loop_wrap_count_ ||
        !previous_processed_;
    const bool loop_wrapped = traversal_serial_.has_value() &&
        snap.traversalSerial == *traversal_serial_ &&
        snap.seekSerial == seek_serial_ &&
        snap.loopWrapCount != loop_wrap_count_;
    const bool multiple_wraps = loop_wrapped &&
        snap.loopWrapCount > loop_wrap_count_ + 1U;
    if (last_cursor_ && cursor < *last_cursor_) new_epoch = true;

    if (new_epoch) {
        clear_observation();
        active_realization_id_.reset();
        eligible_begin_ = loop_wrapped && snap.loop
            ? std::optional<std::int64_t>{snap.loop->begin().value()}
            : std::optional<std::int64_t>{cursor};
        gap_detected_ = multiple_wraps;
    }
    traversal_serial_ = snap.traversalSerial;
    seek_serial_ = snap.seekSerial;
    loop_wrap_count_ = snap.loopWrapCount;
    previous_processed_ = true;

    if (snap.state == PlaybackState::STOPPED ||
        snap.state == PlaybackState::NO_SOURCE) {
        status_ = StereoMsAudibleStatus::STOPPED;
        return;
    }
    if (snap.state == PlaybackState::PAUSED) {
        status_ = StereoMsAudibleStatus::PAUSED;
        return;
    }
    if (snap.state != PlaybackState::PLAYING) {
        status_ = StereoMsAudibleStatus::UNAVAILABLE;
        return;
    }

    const auto& audible = snap.audibleRealization;
    if (audible.phase == AudibleHandoffPhase::TRANSITION) {
        clear_observation(); // No mixing two sidecars or numeric metrics.
        active_realization_id_.reset();
        gap_detected_ = true;
        status_ = StereoMsAudibleStatus::TRANSITION;
        return;
    }
    if (audible.phase != AudibleHandoffPhase::OLD &&
        audible.phase != AudibleHandoffPhase::NEW) {
        clear_observation();
        active_realization_id_.reset();
        status_ = StereoMsAudibleStatus::UNAVAILABLE;
        return;
    }
    if (!audible.realizationId) {
        clear_observation();
        active_realization_id_.reset();
        status_ = StereoMsAudibleStatus::UNAVAILABLE;
        return;
    }
    if (active_realization_id_ != audible.realizationId) {
        clear_observation();
        active_realization_id_ = audible.realizationId;
        // A changed audible identity means a boundary, even if a short fade
        // occurred entirely between two GUI polls.
        eligible_begin_ = audible.phase == AudibleHandoffPhase::NEW &&
            audible.handoffEndFrame
            ? audible.handoffEndFrame : std::optional<std::int64_t>{cursor};
        gap_detected_ = true;
    }

    const auto found = registered_.find(audible.realizationId->value);
    if (found == registered_.end()) {
        clear_observation();
        status_ = StereoMsAudibleStatus::UNAVAILABLE;
        return;
    }
    if (found->second.bypass) {
        clear_observation();
        status_ = StereoMsAudibleStatus::BYPASS;
        return;
    }
    const auto& stage_ptr = found->second.sidecar;
    if (!stage_ptr || stage_ptr->realization_id != audible.realizationId ||
        stage_ptr->channel_layout != rgsml::audio::ChannelLayout::STEREO_LR ||
        stage_ptr->density_status == StereoMsDensityStatus::UNAVAILABLE ||
        stage_ptr->density_buckets.empty()) {
        clear_observation();
        status_ = StereoMsAudibleStatus::UNAVAILABLE;
        return;
    }
    if (!eligible_begin_) eligible_begin_ = cursor;
    const auto& stage = *stage_ptr;
    const auto anchor = *eligible_begin_;
    // Deliberately reject any current/partially heard bucket and any
    // pre-anchor history, including seek and post-crossfade fragments.
    std::vector<const StereoMsDensityBucket*> ready;
    ready.reserve(stage.density_buckets.size());
    const auto last_end = history_.empty()
        ? anchor : history_.back().end_frame;
    for (const auto& bucket : stage.density_buckets) {
        if (bucket.begin_frame >= anchor &&
            bucket.end_frame <= cursor &&
            bucket.end_frame > last_end &&
            bucket.end_frame > bucket.begin_frame &&
            bucket.valid_frame_count + bucket.invalid_count ==
                bucket.frame_count) {
            ready.push_back(&bucket);
        }
    }
    if (ready.size() > kMaxHistoryBuckets) {
        history_.clear();
        ready.erase(ready.begin(),
                    ready.end() - static_cast<std::ptrdiff_t>(kMaxHistoryBuckets));
        gap_detected_ = true; // A stalled GUI must not simulate backlogged animation.
    }
    for (const auto* bucket : ready) {
        history_.push_back(*bucket);
        if (history_.size() > kMaxHistoryBuckets) history_.erase(history_.begin());
    }

    // Secondary windows always come from the SAME registered M15 stage and
    // must be complete and fully passed by the audible cursor.
    correlation_.reset();
    if (stage.correlation_status != StereoMsCorrelationStatus::UNAVAILABLE) {
        for (const auto& window : stage.correlation_windows) {
            if (window.begin_frame >= anchor && window.end_frame <= cursor &&
                window.end_frame > window.begin_frame) correlation_ = window;
        }
    }
    side_low_.reset();
    if (stage.side_low_status != StereoMsSideLowStatus::UNAVAILABLE) {
        for (const auto& window : stage.side_low_windows) {
            if (window.begin_frame >= anchor && window.end_frame <= cursor &&
                window.end_frame > window.begin_frame) side_low_ = window;
        }
    }
    last_cursor_ = cursor;
    status_ = StereoMsAudibleStatus::ACTIVE;
}

}  // namespace rgsml::render
