#include <rgsml/render/audible_compressor_telemetry_resolver.hpp>

#include <algorithm>
#include <cmath>

namespace rgsml::render {

AudibleCompressorTelemetryResolver::AudibleCompressorTelemetryResolver(
    std::size_t max_buckets_per_lane,
    std::size_t max_memory_bytes) noexcept
    : history_(max_buckets_per_lane, max_memory_bytes)
{
}

void AudibleCompressorTelemetryResolver::reset() noexcept
{
    history_.clear();
    status_ = AudibleTelemetryStatus::UNAVAILABLE;
    is_dry_only_ = false;
    active_realization_id_.reset();
    registered_sidecars_.clear();
    current_consuming_realization_.reset();
    next_bucket_index_ = 0;
    last_consumed_frame_.reset();
    last_consumed_bucket_index_.reset();
    eligible_start_frame_.reset();
    traversal_serial_ = 0;
    seek_serial_ = 0;
    loop_wrap_count_ = 0;
    previous_audition_target_ = AuditionTarget::PROCESSED;
}

void AudibleCompressorTelemetryResolver::register_sidecar(
    CompressorTelemetrySidecar sidecar,
    bool bypass,
    double mix_percent)
{
    if (!sidecar.realization_id.has_value()) {
        return;
    }
    const auto rid = sidecar.realization_id->value;
    RegisteredSidecarEntry entry{
        std::move(sidecar),
        bypass,
        mix_percent
    };
    registered_sidecars_.insert_or_assign(rid, std::move(entry));

    // Bounded Sidecar Retention: Keep at most 2 sidecars
    if (registered_sidecars_.size() > 2) {
        // Keep active realization, current consuming realization, or latest registered
        std::uint64_t keep1 = rid; // latest registered
        std::uint64_t keep2 = active_realization_id_ ? active_realization_id_->value : rid;

        for (auto it = registered_sidecars_.begin(); it != registered_sidecars_.end(); ) {
            if (it->first != keep1 && it->first != keep2) {
                it = registered_sidecars_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void AudibleCompressorTelemetryResolver::unregister_sidecar(
    rgsml::core::RealizationId realization_id)
{
    registered_sidecars_.erase(realization_id.value);
}

void AudibleCompressorTelemetryResolver::prune_sidecars(
    const rgsml::core::AudibleRealizationState& state,
    AuditionTarget target)
{
    if (registered_sidecars_.size() <= 1) {
        return;
    }

    std::uint64_t active_id = state.realizationId ? state.realizationId->value : 0;
    bool has_active = state.realizationId.has_value();

    for (auto it = registered_sidecars_.begin(); it != registered_sidecars_.end(); ) {
        const auto key = it->first;
        bool keep = false;

        if (has_active && key == active_id) {
            keep = true;
        }

        if (state.phase == rgsml::core::AudibleHandoffPhase::OLD ||
            state.phase == rgsml::core::AudibleHandoffPhase::TRANSITION) {
            if (key == registered_sidecars_.rbegin()->first) {
                keep = true;
            }
            if (current_consuming_realization_.has_value() && key == current_consuming_realization_->value) {
                keep = true;
            }
        }

        if (target != AuditionTarget::PROCESSED) {
            if (key == registered_sidecars_.rbegin()->first) {
                keep = true;
            }
        }

        if (!keep) {
            it = registered_sidecars_.erase(it);
        } else {
            ++it;
        }
    }
}

void AudibleCompressorTelemetryResolver::update(const ResolverUpdateContext& context)
{
    if (!history_.valid()) {
        status_ = AudibleTelemetryStatus::UNAVAILABLE;
        return;
    }

    const auto audition_target = context.audition_target;
    const auto& snap = context.playback_snapshot;

    // 1. Audition Target Check
    if (audition_target != AuditionTarget::PROCESSED) {
        status_ = AudibleTelemetryStatus::NOT_AUDITIONED;
        is_dry_only_ = false;
        previous_audition_target_ = audition_target;
        eligible_start_frame_.reset();
        return;
    }

    if (previous_audition_target_ != AuditionTarget::PROCESSED) {
        // Switched BACK to PROCESSED -> Anchor eligible start frame at current position!
        eligible_start_frame_ = snap.position.value();
        next_bucket_index_ = 0;
    }
    previous_audition_target_ = audition_target;

    // 2. Playback Traversal / Seek / Loop Control-Plane Events
    if (snap.traversalSerial != traversal_serial_) {
        // Monotonic new playback traversal / replay event!
        traversal_serial_ = snap.traversalSerial;
        seek_serial_ = snap.seekSerial;
        loop_wrap_count_ = snap.loopWrapCount;
        history_.clear();
        current_consuming_realization_.reset();
        next_bucket_index_ = 0;
        last_consumed_frame_.reset();
        last_consumed_bucket_index_.reset();
        eligible_start_frame_ = snap.position.value(); // Anchored at traversal start!
    } else if (snap.seekSerial != seek_serial_) {
        // Explicit seek event!
        seek_serial_ = snap.seekSerial;
        loop_wrap_count_ = snap.loopWrapCount;
        history_.clear();
        current_consuming_realization_.reset();
        next_bucket_index_ = 0;
        last_consumed_frame_.reset();
        last_consumed_bucket_index_.reset();
        eligible_start_frame_ = snap.position.value(); // Anchored at seek target!
    } else if (snap.loopWrapCount != loop_wrap_count_) {
        // Explicit loop wrap event!
        loop_wrap_count_ = snap.loopWrapCount;
        next_bucket_index_ = 0;
        if (snap.loop.has_value()) {
            eligible_start_frame_ = snap.loop->begin().value(); // Anchored at loop.begin!
        } else {
            eligible_start_frame_ = snap.position.value();
        }
    }

    // 3. Transport State Check
    if (snap.state == rgsml::core::PlaybackState::STOPPED ||
        snap.state == rgsml::core::PlaybackState::NO_SOURCE) {
        status_ = AudibleTelemetryStatus::STOPPED;
        return;
    }

    if (snap.state == rgsml::core::PlaybackState::PAUSED) {
        status_ = AudibleTelemetryStatus::PAUSED;
        return;
    }

    // 4. Audible Realization Handoff Phase Check
    const auto phase = snap.audibleRealization.phase;
    const auto real_id_opt = snap.audibleRealization.realizationId;

    if (phase == rgsml::core::AudibleHandoffPhase::UNAVAILABLE) {
        status_ = AudibleTelemetryStatus::UNAVAILABLE;
        active_realization_id_.reset();
        return;
    }

    if (phase == rgsml::core::AudibleHandoffPhase::TRANSITION) {
        // No numeric GR emitted/appended during transition interval!
        status_ = AudibleTelemetryStatus::TRANSITION;
        eligible_start_frame_.reset(); // Un-anchor so NEW phase anchors at handoff end
        return;
    }

    if (phase == rgsml::core::AudibleHandoffPhase::OLD ||
        phase == rgsml::core::AudibleHandoffPhase::NEW) {
        if (!real_id_opt.has_value()) {
            status_ = AudibleTelemetryStatus::UNAVAILABLE;
            active_realization_id_.reset();
            return;
        }

        auto it = registered_sidecars_.find(real_id_opt->value);
        if (it == registered_sidecars_.end()) {
            status_ = AudibleTelemetryStatus::UNAVAILABLE;
            active_realization_id_.reset();
            return;
        }

        const auto& sidecar_entry = it->second;

        // Check Bypass first
        if (sidecar_entry.bypass ||
            sidecar_entry.sidecar.status == CompressorTelemetryStatus::BYPASS) {
            status_ = AudibleTelemetryStatus::BYPASS;
            active_realization_id_ = real_id_opt;
            return;
        }

        if (!sidecar_entry.sidecar.valid ||
            sidecar_entry.sidecar.status != CompressorTelemetryStatus::OK ||
            !sidecar_entry.sidecar.realization_id.has_value() ||
            sidecar_entry.sidecar.realization_id != real_id_opt) {
            status_ = AudibleTelemetryStatus::UNAVAILABLE;
            active_realization_id_ = real_id_opt;
            return;
        }

        // Check Mix = 0% (Dry Only)
        if (std::abs(sidecar_entry.mix_percent) < 1e-6) {
            is_dry_only_ = true;
            status_ = AudibleTelemetryStatus::ACTIVE_DRY_ONLY;
        } else {
            is_dry_only_ = false;
            status_ = AudibleTelemetryStatus::ACTIVE_WET;
        }

        if (current_consuming_realization_ != real_id_opt) {
            current_consuming_realization_ = real_id_opt;
            next_bucket_index_ = 0;
            if (snap.audibleRealization.handoffEndFrame.has_value()) {
                eligible_start_frame_ = *snap.audibleRealization.handoffEndFrame;
            } else if (!eligible_start_frame_.has_value()) {
                eligible_start_frame_ = snap.position.value();
            }
        }
        active_realization_id_ = real_id_opt;

        // Prune stale sidecars once no longer needed
        prune_sidecars(snap.audibleRealization, audition_target);

        // 5. Consume Completed Audible Buckets
        const auto& lanes = sidecar_entry.sidecar.channel_lanes;
        if (lanes.empty()) {
            return;
        }

        if (!eligible_start_frame_.has_value()) {
            eligible_start_frame_ = snap.position.value();
        }

        const auto min_eligible = *eligible_start_frame_;
        const auto max_eligible = snap.position.value();

        while (next_bucket_index_ < lanes[0].buckets.size()) {
            const auto& bucket = lanes[0].buckets[next_bucket_index_];
            if (bucket.begin_frame >= min_eligible && bucket.end_frame <= max_eligible) {
                // Fully eligible bucket!
                for (std::size_t lane_idx = 0; lane_idx < lanes.size(); ++lane_idx) {
                    history_.push_bucket(lane_idx, lanes[lane_idx].buckets[next_bucket_index_]);
                    if (!history_.valid()) {
                        status_ = AudibleTelemetryStatus::UNAVAILABLE;
                        active_realization_id_.reset();
                        return;
                    }
                }
                last_consumed_frame_ = bucket.end_frame;
                last_consumed_bucket_index_ = next_bucket_index_;
                ++next_bucket_index_;
            } else if (bucket.begin_frame < min_eligible && bucket.end_frame <= max_eligible) {
                // Mixed / partial bucket that started before eligible_start_frame -> skip it!
                ++next_bucket_index_;
            } else {
                // bucket.end_frame > max_eligible -> bucket not yet completed at current position
                break;
            }
        }
    }
}

}  // namespace rgsml::render
