#pragma once

#include "terminal_candidate_gate.hpp"
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/audio/monitoring_frame_map.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

#include <cmath>
#include <cstddef>
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

// A pure, immutable SOURCE->MEASURED MASTER->DEVICE handoff projection.
// These coordinates never give a real Windows sink, LIVE meter, verified
// ceiling or export permission. The original SOURCE transport is retained
// so any later conditional return can be proven against its owner.
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
    if(original.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||original.absolute_start_frame().value()!=0
       ||original.frame_count().value()<=0
       ||original.format().sample_rate()!=captured->rendered.output_rate
       ||original.frame_count()!=captured->rendered.programme_frames
       ||measured.terminal.bypassed
       ||measured.terminal.applied_gain_frames.size()
           !=static_cast<std::size_t>(original.frame_count().value()))
        return refuse("M17_MEASURED_DEVICE_MASTER_PROVENANCE_INVALID");
    const auto checked=audio::validate_monitoring_pcm_candidate(*converted);
    if(!checked)return Result::failure(*checked.error());
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
        map.value()->master_rate(),map.value()->device_rate()
    });
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
