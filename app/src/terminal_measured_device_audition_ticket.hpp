#pragma once

#include "terminal_candidate_gate.hpp"
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>

namespace rgsml::app {

// FQ99 PREPRODUCTION ONLY: a strongly typed audition candidate that cannot
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

    [[nodiscard]] static core::Result<MeasuredDeviceAuditionTicket> admit(
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            captured,
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            current,
        std::shared_ptr<const audio::MonitoringPcmCandidate> converted,
        bool explicitUserRequest);
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
    bool explicitUserRequest)
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
    if(!std::isfinite(measured.measured_linear_peak)
       ||!std::isfinite(measured.ceiling_linear)
       ||!std::isfinite(measured.requested_ceiling_db_tp)
       ||measured.measured_linear_peak<=measured.ceiling_linear
       ||measured.ceiling_linear<=0.0
       ||measured.measurement_status
           ==analysis::TruePeakQualificationStatus::UNSUPPORTED)
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
    // Admission is just a retained pair; no untrusted device rate can
    // replace the immutable master or its *original* measured TP evidence.
    return Result::success(MeasuredDeviceAuditionTicket{
        std::move(captured),std::move(converted),
        current->original_manual_realization_id});
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
    bool explicitUserRequest)
{
    return MeasuredDeviceAuditionTicket::admit(
        std::move(captured),std::move(current),std::move(converted),
        explicitUserRequest);
}

} // namespace rgsml::app
