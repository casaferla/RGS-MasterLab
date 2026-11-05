#pragma once

#include "terminal_candidate_gate.hpp"
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/audio/monitoring_frame_map.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace rgsml::app {

// Fresh independent Windows endpoint identity. Must be probed again on the
// Qt owner thread AFTER offline SRC; a shared rate never proves same device.
struct MeasuredDeviceEndpointObservation final {
    std::string id;
    core::SampleRate rate;
};

// Pure metadata predicate, not a physical Windows-device availability
// certificate. Before re-opening MANUAL after retiring an anonymous M17
// audition, this is one REQUIRED condition in addition to Qt owner/source
// identity, Windows format preflight and conditional install-time checks.
[[nodiscard]] inline bool measured_empty_sink_snapshot_proven(
    const core::PlaybackSnapshot& snapshot) noexcept
{
    return snapshot.state==core::PlaybackState::NO_SOURCE
        &&snapshot.position==core::FrameIndex{0}
        &&snapshot.audibleRealization.phase
            ==core::AudibleHandoffPhase::UNAVAILABLE
        &&!snapshot.audibleRealization.realizationId
        &&!snapshot.duration &&!snapshot.loop;
}

// A pure, immutable SOURCE->MEASURED MASTER->DEVICE handoff projection.
// These coordinates never give a real Windows sink, LIVE meter, verified
// ceiling or export permission. The original SOURCE transport is retained
// so any later conditional return can be proven against its owner.
// The actual MANUAL restore selector maps a SOURCE EOF cue back to the
// beginning, not to a non-audible cursor past the track. Original SOURCE
// loop boundaries take precedence; never create a new loop or extend N.
[[nodiscard]] inline core::Result<core::FrameIndex>
canonical_measured_source_return_cue(
    core::FrameIndex proposed,core::FrameCount originalFrames,
    std::optional<core::FrameRange> originalLoop)
{
    using Result=core::Result<core::FrameIndex>;
    const auto fail=[](const char* reason)->Result {
        return Result::failure(core::Error{
            core::ErrorCode::InvalidArgument,reason});
    };
    if(originalFrames.value()<=0||proposed.value()<0
       ||proposed.value()>originalFrames.value())
        return fail("M17_MEASURED_RETURN_INVALID_SOURCE_CUE");
    if(originalLoop) {
        if(originalLoop->begin().value()<0
           ||originalLoop->begin()>=originalLoop->end()
           ||originalLoop->end().value()>originalFrames.value())
            return fail("M17_MEASURED_RETURN_INVALID_SOURCE_LOOP");
        if(proposed<originalLoop->begin()||proposed>=originalLoop->end())
            return Result::success(originalLoop->begin());
    }
    return Result::success(
        proposed.value()==originalFrames.value()
            ?core::FrameIndex{0}:proposed);
}

struct MeasuredDeviceTransportPlan final {
    core::RealizationId manual_owner;
    core::PlaybackState source_state;
    core::FrameIndex source_cue;
    std::optional<core::FrameRange> source_loop;
    core::FrameIndex master_cue;
    std::optional<core::FrameRange> master_loop;
    core::FrameIndex device_cue;
    std::optional<core::FrameRange> device_loop;
    core::SampleRate source_rate;
    core::SampleRate master_rate;
    core::SampleRate device_rate;
    // SOURCE transition epochs, never an audible device-clock certificate.
    std::uint64_t source_seek_serial{0};
    std::uint64_t source_traversal_serial{0};
    std::uint64_t source_loop_wrap_count{0};
};

// M17 CI PREPRODUCTION ONLY: a strongly typed audition candidate that cannot
// be mistaken for StagedCandidate / M16-verified MASTER. The device-rate PCM
// is a DIFFERENT anonymous allocation. This type grants no SOURCE identity,
// M16 LIVE certificate, GR chronology, playback permission or export right.
class MeasuredDeviceAuditionTicket final {
public:
    [[nodiscard]] const auto& measured_master() const noexcept
    { return measuredMaster_; }
    [[nodiscard]] const auto& audition_device_pcm() const noexcept
    { return devicePcm_; }
    [[nodiscard]] core::RealizationId original_manual_realization_id()
        const noexcept { return originalManualId_; }

    // Pre-install check is NON-AUTHORIZING: caller must also verify the
    // MANUAL Windows sink and Qt owner epoch before retiring any DEVICE.
    // This prevents a long background conversion from replaying a stale
    // measured master, hotplugged endpoint, or withdrawn user consent.
    [[nodiscard]] core::Status recheck_preinstall(
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            currentMeasured,
        core::RealizationId currentManualId,
        const MeasuredDeviceEndpointObservation& latestEndpoint,
        bool explicitUserRequest) const;

    // The caller must use an exact MANUAL owner-thread transport snapshot
    // and revalidate it again immediately before any Windows sink mutation.
    // All output-rate projections are rational and fail closed on duration,
    // SOURCE loop, device identity or stale measured-MASTER ownership.
    [[nodiscard]] core::Result<MeasuredDeviceTransportPlan> plan_handoff(
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            currentMeasured,
        const core::PlaybackSnapshot& sourcePlayback,
        std::optional<core::FrameRange> currentSourceLoop,
        const MeasuredDeviceEndpointObservation& latestEndpoint,
        bool explicitUserRequest) const;

    // Re-observe SOURCE directly before physical handoff. Ordinary forward
    // playback without a seek may advance the cue; recompute its mapping.
    // A new seek, loop wrap, Play/Pause/Stop, owner or endpoint requires
    // a new user-controlled audition transaction instead of a stale install.
    [[nodiscard]] core::Result<MeasuredDeviceTransportPlan> rebase_handoff(
        const MeasuredDeviceTransportPlan& prior,
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            currentMeasured,
        const core::PlaybackSnapshot& currentSource,
        std::optional<core::FrameRange> currentSourceLoop,
        const MeasuredDeviceEndpointObservation& latestEndpoint,
        bool explicitUserRequest) const;

    [[nodiscard]] static core::Result<MeasuredDeviceAuditionTicket> admit(
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            captured,
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            current,
        std::shared_ptr<const audio::MonitoringPcmCandidate> converted,
        bool explicitUserRequest,
    const MeasuredDeviceEndpointObservation& freshEndpoint);
private:
    // No public aggregate or default constructor: only the explicit,
    // owner-checked admission may construct an audition candidate.
    MeasuredDeviceAuditionTicket(
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            master,
        std::shared_ptr<const audio::MonitoringPcmCandidate> device,
        core::RealizationId originalId)
        : measuredMaster_(std::move(master)),devicePcm_(std::move(device)),
          originalManualId_(originalId) {}
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        measuredMaster_;
    std::shared_ptr<const audio::MonitoringPcmCandidate> devicePcm_;
    core::RealizationId originalManualId_;
};

// Only a user-explicitly-requested, current, independently M16-measured
// NON-CONFORMING stage may be associated with a separately converted device
// buffer. The calling coordinator must obtain 'current' on the app OWNER
// thread AFTER conversion. Windows must then recheck the opaque endpoint at
// install time; even a valid ticket here is NOT permission to start playback.
inline core::Result<MeasuredDeviceAuditionTicket>
MeasuredDeviceAuditionTicket::admit(
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        captured,
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        current,
    std::shared_ptr<const audio::MonitoringPcmCandidate> converted,
    bool explicitUserRequest,
    const MeasuredDeviceEndpointObservation& freshEndpoint)
{
    using Result=core::Result<MeasuredDeviceAuditionTicket>;
    const auto refuse=[](const char* reason)->Result {
        return Result::failure(core::Error{
            core::ErrorCode::InvalidState,reason});
    };
    if(!explicitUserRequest)
        return refuse("M17_MEASURED_DEVICE_EXPLICIT_CONSENT_REQUIRED");
    if(!captured ||captured!=current)
        return refuse("M17_MEASURED_DEVICE_STAGE_REVOKED");
    if(!converted)
        return refuse("M17_MEASURED_DEVICE_CONVERTED_OWNER_MISSING");
    if(!captured->prepared_owner ||!captured->rendered.manual_owner
       ||captured->original_manual_realization_id.value==0)
        return refuse("M17_MEASURED_DEVICE_ORIGINAL_OWNER_MISSING");

    const auto& measured=captured->rendered.terminal;
    const auto original=measured.terminal.compensated_pcm.view();
    // The M16 stage is allowed to be SUPPORTED_UNQUALIFIED only;
    // a fabricated QUALIFIED tag, unsupported sample or impossible
    // Ceiling/miss relationship must never enter the audio-device lane.
    // The renderer calculates the linear threshold with exactly this
    // expression. Permit only a few binary64 rounding ULPs at the
    // independent admission boundary; never round the *miss* itself.
    const double expectedCeiling=std::pow(
        10.0,measured.requested_ceiling_db_tp/20.0);
    const double ceilingDelta=std::abs(
        measured.ceiling_linear-expectedCeiling);
    const double ceilingTolerance=8.0*
        std::numeric_limits<double>::epsilon()*expectedCeiling;
    const bool knownReason=measured.reason
        ==render::M17NonconformingReason::TRIM_GUARD_EXCEEDED
        ||measured.reason
            ==render::M17NonconformingReason::POST_TRIM_ABOVE_CEILING;
    if(!std::isfinite(measured.measured_linear_peak)
       ||!std::isfinite(measured.ceiling_linear)
       ||!std::isfinite(measured.requested_ceiling_db_tp)
       ||measured.requested_ceiling_db_tp< -12.0
       ||measured.requested_ceiling_db_tp>0.0
       ||measured.measured_linear_peak<=measured.ceiling_linear
       ||measured.ceiling_linear<=0.0
       ||!std::isfinite(expectedCeiling)
       ||ceilingDelta>ceilingTolerance
       ||!knownReason
       ||measured.measurement_status
           !=analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED)
        return refuse("M17_MEASURED_DEVICE_NO_AUTHENTIC_CEILING_MISS");
    const auto source=captured->rendered.manual_owner->view();
    const auto sourceRate=source.format().sample_rate();
    const auto masterRate=original.format().sample_rate();
    // programme_frames refers to the original SOURCE, not the M17
    // OUTPUT_RATE timeline. Comparing those two frame counts wrongly
    // rejects every genuine SOURCE->different-MASTER conversion.
    if(original.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||original.absolute_start_frame().value()!=0
       ||original.frame_count().value()<=0
       ||masterRate!=captured->rendered.output_rate
       ||source.absolute_start_frame().value()!=0
       ||source.frame_count()!=captured->rendered.programme_frames
       ||source.format().channel_layout()!=original.format().channel_layout()
       ||captured->rendered.used_candidate_src!=(sourceRate!=masterRate)
       ||(captured->rendered.used_candidate_src
           &&captured->rendered.src_kernel_sha256.empty())
       ||measured.terminal.bypassed
       ||measured.terminal.applied_gain_frames.size()
           !=static_cast<std::size_t>(original.frame_count().value()))
        return refuse("M17_MEASURED_DEVICE_MASTER_PROVENANCE_INVALID");
    const auto checked=audio::validate_monitoring_pcm_candidate(*converted);
    if(!checked)return Result::failure(*checked.error());
    // Use the existing checked rational mapping contract to prove that
    // SOURCE and M17 MASTER span the SAME duration to within one SOURCE
    // frame. No rounding, sample-rate substitution, or false same-rate
    // shortcut is permitted at this audition-only admission boundary.
    const auto map=audio::MonitoringFrameMap::create(*converted);
    if(!map)return Result::failure(*map.error());
    const auto sourceOrigin=map.value()->source_to_master(
        core::FrameIndex{0},sourceRate,captured->rendered.programme_frames);
    if(!sourceOrigin ||*sourceOrigin.value()!=core::FrameIndex{0})
        return refuse("M17_MEASURED_DEVICE_SOURCE_MASTER_DURATION_MISMATCH");
    if(converted->immutable_master_rate!=original.format().sample_rate()
       ||converted->immutable_master_frames!=original.frame_count()
       ||converted->device_pcm.view().format().channel_layout()
           !=original.format().channel_layout())
        return refuse("M17_MEASURED_DEVICE_RATE_OR_GEOMETRY_MISMATCH");
    if(converted->windows_prepared_endpoint_id.empty())
        return refuse("M17_MEASURED_DEVICE_ENDPOINT_TICKET_MISSING");
    // A Windows hotplug may replace the physical endpoint at the same rate.
    // The caller must provide a SECOND independently observed device ticket.
    if(freshEndpoint.id.empty()
       ||freshEndpoint.id!=converted->windows_prepared_endpoint_id
       ||freshEndpoint.rate!=converted->device_rate)
        return refuse("M17_MEASURED_DEVICE_ENDPOINT_REVOKED");
    // Admission is just a retained pair; no untrusted device rate can
    // replace the immutable master or its *original* measured TP evidence.
    return Result::success(MeasuredDeviceAuditionTicket{
        std::move(captured),std::move(converted),
        current->original_manual_realization_id});
}


inline core::Status MeasuredDeviceAuditionTicket::recheck_preinstall(
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        currentMeasured,
    core::RealizationId currentManualId,
    const MeasuredDeviceEndpointObservation& latestEndpoint,
    bool explicitUserRequest) const
{
    const auto refused=[](const char* reason)->core::Status {
        return core::Status::failure(
            core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!explicitUserRequest)
        return refused("M17_MEASURED_DEVICE_PREINSTALL_CONSENT_REVOKED");
    if(!measuredMaster_ ||measuredMaster_!=currentMeasured)
        return refused("M17_MEASURED_DEVICE_PREINSTALL_STAGE_REVOKED");
    if(currentManualId.value==0
       ||currentManualId!=originalManualId_
       ||currentMeasured->original_manual_realization_id!=currentManualId)
        return refused("M17_MEASURED_DEVICE_PREINSTALL_MANUAL_REVOKED");
    if(!devicePcm_)
        return refused("M17_MEASURED_DEVICE_PREINSTALL_PCM_MISSING");
    if(latestEndpoint.id.empty()
       ||latestEndpoint.id!=devicePcm_->windows_prepared_endpoint_id
       ||latestEndpoint.rate!=devicePcm_->device_rate)
        return refused("M17_MEASURED_DEVICE_PREINSTALL_ENDPOINT_REVOKED");
    // The detached PCM and its format must remain an exact independently
    // converted AUDIO owner; this does not assert true-peak conformity.
    const auto valid=audio::validate_monitoring_pcm_candidate(*devicePcm_);
    if(!valid)return valid;
    return core::Status::success();
}

inline core::Result<MeasuredDeviceTransportPlan>
MeasuredDeviceAuditionTicket::plan_handoff(
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        currentMeasured,
    const core::PlaybackSnapshot& sourcePlayback,
    std::optional<core::FrameRange> currentSourceLoop,
    const MeasuredDeviceEndpointObservation& latestEndpoint,
    bool explicitUserRequest) const
{
    using Result=core::Result<MeasuredDeviceTransportPlan>;
    const auto rejected=[](const char* reason)->Result {
        return Result::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    const auto current=recheck_preinstall(
        currentMeasured,originalManualId_,latestEndpoint,explicitUserRequest);
    if(!current)return Result::failure(*current.error());
    if(!currentMeasured->rendered.manual_owner)
        return rejected("M17_MEASURED_HANDOFF_MANUAL_OWNER_MISSING");
    const auto source=currentMeasured->rendered.manual_owner->view();
    const auto sourceCount=source.frame_count();
    const auto actualState=sourcePlayback.state;
    if(source.absolute_start_frame()!=core::FrameIndex{0}
       ||(actualState!=core::PlaybackState::STOPPED
           &&actualState!=core::PlaybackState::PLAYING
           &&actualState!=core::PlaybackState::PAUSED)
       ||sourcePlayback.audibleRealization.phase
            !=core::AudibleHandoffPhase::NEW
       ||sourcePlayback.audibleRealization.realizationId
            !=std::optional{originalManualId_}
       ||!sourcePlayback.duration
       ||*sourcePlayback.duration!=sourceCount
       ||sourcePlayback.loop!=currentSourceLoop
       ||sourcePlayback.position.value()<0
       ||sourcePlayback.position.value()>sourceCount.value())
        return rejected("M17_MEASURED_HANDOFF_SOURCE_SNAPSHOT_STALE");
    const auto map=audio::MonitoringFrameMap::create(*devicePcm_);
    if(!map)return Result::failure(*map.error());
    const auto sourceRate=source.format().sample_rate();
    const auto masterCue=map.value()->source_to_master(
        sourcePlayback.position,sourceRate,sourceCount);
    if(!masterCue)return Result::failure(*masterCue.error());
    const auto deviceCue=map.value()->master_to_device(*masterCue.value());
    if(!deviceCue)return Result::failure(*deviceCue.error());
    std::optional<core::FrameRange> masterLoop;
    std::optional<core::FrameRange> deviceLoop;
    if(currentSourceLoop) {
        const auto m=map.value()->source_loop_to_master(
            *currentSourceLoop,sourceRate,sourceCount);
        if(!m)return Result::failure(*m.error());
        const auto d=map.value()->master_loop_to_device(*m.value());
        if(!d)return Result::failure(*d.error());
        masterLoop=*m.value();
        deviceLoop=*d.value();
    }
    return Result::success({
        originalManualId_,actualState,sourcePlayback.position,
        currentSourceLoop,*masterCue.value(),masterLoop,
        *deviceCue.value(),deviceLoop,sourceRate,
        map.value()->master_rate(),map.value()->device_rate(),
        sourcePlayback.seekSerial,sourcePlayback.traversalSerial,
        sourcePlayback.loopWrapCount
    });
}

inline core::Result<MeasuredDeviceTransportPlan>
MeasuredDeviceAuditionTicket::rebase_handoff(
    const MeasuredDeviceTransportPlan& prior,
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        currentMeasured,
    const core::PlaybackSnapshot& currentSource,
    std::optional<core::FrameRange> currentSourceLoop,
    const MeasuredDeviceEndpointObservation& latestEndpoint,
    bool explicitUserRequest) const
{
    using Result=core::Result<MeasuredDeviceTransportPlan>;
    const auto fail=[](const char* why)->Result {
        return Result::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(prior.manual_owner!=originalManualId_
       ||!devicePcm_
       ||prior.master_rate!=devicePcm_->immutable_master_rate
       ||prior.device_rate!=devicePcm_->device_rate
       ||prior.source_state!=currentSource.state
       ||prior.source_loop!=currentSourceLoop
       ||prior.source_seek_serial!=currentSource.seekSerial
       ||prior.source_traversal_serial!=currentSource.traversalSerial
       ||prior.source_loop_wrap_count!=currentSource.loopWrapCount)
        return fail("M17_MEASURED_HANDOFF_TRANSPORT_EPOCH_REVOKED");
    // PLAY advances naturally without a seek. Other states must be frozen
    // until a new explicit request; backward travel without a Loop wrap is
    // never a valid continuous playing clock.
    const auto next=currentSource.position.value();
    const auto old=prior.source_cue.value();
    if((prior.source_state==core::PlaybackState::PLAYING &&next<old)
       ||(prior.source_state!=core::PlaybackState::PLAYING &&next!=old))
        return fail("M17_MEASURED_HANDOFF_CUE_DISCONTINUITY");
    const auto fresh=plan_handoff(
        std::move(currentMeasured),currentSource,currentSourceLoop,
        latestEndpoint,explicitUserRequest);
    if(!fresh)return fresh;
    if(fresh.value()->manual_owner!=prior.manual_owner
       ||fresh.value()->source_rate!=prior.source_rate
       ||fresh.value()->master_rate!=prior.master_rate
       ||fresh.value()->device_rate!=prior.device_rate)
        return fail("M17_MEASURED_HANDOFF_RATE_OR_SOURCE_CHANGED");
    return fresh;
}

// This named helper is retained for unit test readability; construction
// remains strictly private to the typed admission method.
[[nodiscard]] inline core::Result<MeasuredDeviceAuditionTicket>
admit_measured_device_audition_ticket(
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        captured,
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
        current,
    std::shared_ptr<const audio::MonitoringPcmCandidate> converted,
    bool explicitUserRequest,
    const MeasuredDeviceEndpointObservation& freshEndpoint)
{
    return MeasuredDeviceAuditionTicket::admit(
        std::move(captured),std::move(current),std::move(converted),
        explicitUserRequest,freshEndpoint);
}

} // namespace rgsml::app
