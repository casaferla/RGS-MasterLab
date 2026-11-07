#pragma once
#include "terminal_private_render_controller.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_measured_device_audition_ticket.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/platform/windows/windows_audio_playback_service.hpp>
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <QObject>
#include <QThread>
#include <QString>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace rgsml::audio {
struct MonitoringPcmCandidate;
struct MonitoringPcmPreallocationAdvice;
}
namespace rgsml::audio::internal { class M18OwnedMasterFrameProvider; }

namespace rgsml::app {

// Product-owned but PRIVATE orchestration: render -> accepted output candidate
// -> exact ID reservation -> device OUTPUT_RATE install -> SOURCE controls.
// Intentionally has NO Q_INVOKABLE, default rate, UI publication or M16 LIVE
// privilege. Only an explicit test/future qualifying caller can start it.
class TerminalPrivateProductSession final : public QObject {
    Q_OBJECT
public:
    using Snapshot=std::function<std::shared_ptr<const render::RenderResult>()>;
    using ManualId=std::function<std::optional<core::RealizationId>()>;
    using Loop=std::function<std::optional<core::FrameRange>()>;
    using Selected=std::function<bool()>;
    using OfflineSelected=std::function<bool()>;
    using OfflineProvenance=std::function<bool()>;
    using RateProbe=std::function<core::Result<core::SampleRate>(
        audio::ChannelLayout)>;

    TerminalPrivateProductSession(
        TerminalPrivateRenderController& renderer,
        TerminalPrivatePublicationCoordinator& publisher,
        TerminalCandidateGate& candidates,
        PlaybackTransportViewModel& transport,
        Snapshot prepared,Snapshot manual,ManualId manualId,
        Loop sourceLoop,Selected selected,QObject* parent=nullptr);

    // Explicit offline provenance is NEVER an audible PROCESSED target.
    // This opt-in predicate authorizes only MASTER-only private rendering
    // with a clean NO_SOURCE sink. Ordinary M17 installs remain unchanged.
    void set_offline_final_master_selection_checker(OfflineSelected checker)
    { offlineSelected_=std::move(checker); }
    // Unlike OfflineSelected, this does not assert an EMPTY endpoint;
    // it ONLY asserts the same immutable SOURCE epoch while the exact
    // independently-owned DEVICE PCM is installed.
    void set_offline_source_provenance_checker(OfflineProvenance checker)
    { offlineProvenance_=std::move(checker); }
    [[nodiscard]] bool monitor_returns_to_empty_source() const noexcept {
        return static_cast<bool>(installedMonitor_) && pending_
            && pending_->offlineNoSink
            && pending_->offlineNonAudibleSelection;
    }

    // An opt-in deferred request renders while the MANUAL PCM keeps playing.
    // Only an explicit conductor handoff may publish the accepted terminal.
    [[nodiscard]] core::Status request_private(core::SampleRate outputRate,
                                               bool deferred=false);
    // The chosen FINAL MASTER rate is provided by a future authoritative
    // render setting, NEVER probed from the hardware. This method creates a
    // purely private OUTPUT_RATE candidate: it cannot install a sink or
    // expose an M16 LIVE identity until qualified Monitoring SRC is wired.
    // It deliberately has no Q_INVOKABLE or UI callback at this milestone.
    [[nodiscard]] core::Status request_private_for_final_master(
        core::SampleRate selectedFinalMasterRate);
    // PRIVATE verified-master -> monitor PCM bridge. The caller explicitly
    // supplies a materializer (e.g. Windows' fresh-device/RAM-preflight seam).
    // No device install, new RealizationId, UI activation, or M16 LIVE claim.
    using MonitoringMaterializer = std::function<core::Result<
        std::shared_ptr<const audio::MonitoringPcmCandidate>>(
            audio::AudioBufferView immutableMaster)>;
    [[nodiscard]] core::Result<
        std::shared_ptr<const audio::MonitoringPcmCandidate>>
    materialize_private_final_master_monitoring(
        const MonitoringMaterializer& materializer);

    // Split-phase, owning-thread-only MASTER-ticket boundary for a FUTURE
    // nonblocking DEVICE conversion. Capture retains the exact immutable
    // M17 stage; an asynchronous worker must consume only its PCM, never
    // access this QObject, selectors, Qt audio-device services or the sink.
    // Admission is called back on the original UI thread and rechecks the
    // identical live stage BEFORE minting a separate DEVICE ticket.
    using MonitoringMasterCapture=
        std::shared_ptr<const TerminalCandidateGate::StagedCandidate>;
    [[nodiscard]] MonitoringMasterCapture capture_monitor_master_for_conversion() const;
    [[nodiscard]] core::Status admit_converted_monitor_owner(
        MonitoringMasterCapture captured,
        std::shared_ptr<const audio::MonitoringPcmCandidate> candidate);

    // App-to-Windows registration, without an automatic PREPARE or QML entry.
    // The real Windows adapter probes the endpoint and OS memory AGAIN at
    // imminent allocation; the independent caller chooses any policy.
    // No callback or explicit decision means no PCM allocation or device sink.
    using MonitoringPreallocationDecision = std::function<core::Status(
        const audio::MonitoringPcmPreallocationAdvice&)>;
    using MonitoringPreflightMaterializer = std::function<core::Result<
        std::shared_ptr<const audio::MonitoringPcmCandidate>>(
            audio::AudioBufferView immutableMaster,
            core::FrameCount maximumBlockFrames,
            std::uint64_t reservedOtherPrepareBytes,
            const MonitoringPreallocationDecision& explicitDecision)>;
    void set_monitoring_preflight_materializer(
        MonitoringPreflightMaterializer materializer)
    { monitoringPreflight_=std::move(materializer); }
    [[nodiscard]] core::Result<
        std::shared_ptr<const audio::MonitoringPcmCandidate>>
    materialize_private_final_master_monitoring_with_preflight(
        core::FrameCount maximumBlockFrames,
        std::uint64_t reservedOtherPrepareBytes,
        const MonitoringPreallocationDecision& explicitDecision);

    // Private exact-owner handoff. The offline MASTER is unchanged;
    // the DEVICE monitor receives mapped source cue/loop/play state only
    // after an independently verified Windows PCM installation.
    // Replacing a nonzero/playing/looped MANUAL requires the registered
    // monitor control + exact display ports, otherwise fail closed.
    // Require an exact minted M17 MASTER + separately-owned DEVICE candidate.
    using MonitoringOwner=std::shared_ptr<const audio::MonitoringPcmCandidate>;
    using MonitoringInstall=std::function<core::Status(MonitoringOwner)>;
    using MonitoringOwnership=std::function<core::Result<bool>(MonitoringOwner)>;
    using MonitoringRetire=std::function<core::Status(MonitoringOwner)>;
    using ManualRestore=std::function<core::Status(
        std::shared_ptr<const render::RenderResult>,
        core::RealizationId,core::FrameIndex)>;
    // Optional product-supplied Windows admission before retiring the
    // owned MASTER device. The callback MUST NOT install or stop a sink.
    // A rejection leaves the currently playing monitor fully owned.
    using ManualRestorePreflight=std::function<core::Status(
        std::shared_ptr<const render::RenderResult>)>;
    void set_private_monitor_manual_restore_preflight(
        ManualRestorePreflight preflight)
    { manualRestorePreflight_=std::move(preflight); }
    void set_private_monitor_transport(
        MonitoringInstall install,MonitoringOwnership owned,
        MonitoringRetire retire,ManualRestore restore)
    {
        monitorInstall_=std::move(install);
        monitorOwned_=std::move(owned);
        monitorRetire_=std::move(retire);
        manualRestore_=std::move(restore);
    }
    [[nodiscard]] core::Status install_private_monitor_stopped(
        MonitoringOwner candidate);
    [[nodiscard]] core::Status return_private_monitor_to_manual();
    // Explicit SOURCE-loop commit only after exact monitor retirement and
    // original MANUAL reinstallation. Never alters the monitoring sink.
    using SourceLoopReconciliation=std::function<core::Status(
        core::FrameRange projectedSourceRegion)>;
    void set_private_monitor_source_loop_reconciliation(
        SourceLoopReconciliation transaction)
    { sourceLoopReconcile_=std::move(transaction); }

    // Private monitoring control plane (NOT QML); the Windows callbacks
    // guard actual device owner and command atomically on the worker.
    using MonitorControl=std::function<core::Status(
        MonitoringOwner,platform::windows::MonitoringTransportRequest)>;
    using MonitorDisplay=std::function<core::Result<
        audio::MonitoringMasterDisplaySnapshot>(MonitoringOwner)>;
    void set_private_monitor_controls(
        MonitorControl control,MonitorDisplay display)
    { monitorControl_=std::move(control);monitorDisplay_=std::move(display); }
    [[nodiscard]] core::Status control_private_monitor(
        platform::windows::MonitoringTransportRequest request);
    [[nodiscard]] core::Result<audio::MonitoringMasterDisplaySnapshot>
    private_monitor_master_snapshot() const;
    // Explicit opt-in, still product-private: probe device format on its
    // owner thread, capture exact whole-manual format, then request render.
    // No Q_INVOKABLE and no automatic/UI activation.
    void set_preferred_output_rate_probe(RateProbe probe)
    { rateProbe_=std::move(probe); }
    [[nodiscard]] core::Status request_private_for_device(bool deferred=false);
    [[nodiscard]] core::Status install_ready_private();
    // Explicitly requested by the NON-CONFORMING preview button only.
    [[nodiscard]] core::Status install_measured_private();
    [[nodiscard]] core::Status cancel();
    // Unlike cancel(), retire ONLY device ownership. Immutable accepted PCM
    // and its M16 verification/nonconforming evidence remain PRIVATE for
    // explicit MANUAL/M17 A/B, with new ownership and output-rate checks.
    [[nodiscard]] core::Status suspend_for_comparison();
    [[nodiscard]] core::Status resume_comparison();
    [[nodiscard]] bool comparison_cached() const noexcept {
        return status_==QStringLiteral("COMPARISON_VERIFIED")
            ||status_==QStringLiteral("COMPARISON_NONCONFORMING");
    }
    [[nodiscard]] bool comparison_owners_current() const;
    // FIRST P0 PRODUCT BRIDGE: read-only borrowing of the same verified
    // immutable M17 MASTER from a pending/retained A/B Prepare. No new M17
    // render, DEVICE SRC, WAV export or sink/RealizationId publication.
    // The caller MUST supply the exact selected PROJECT MASTER rate and
    // independently guard later DEVICE allocation/transport.
    [[nodiscard]] std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
    borrow_verified_m17_master_for_device(core::SampleRate selectedMasterRate) const;
    // Private, nonpublishing M18 read lease of current M16-verified MASTER.
    // Session and output identity must be rechecked before any file commit.
    [[nodiscard]] std::shared_ptr<audio::internal::M18OwnedMasterFrameProvider>
    capture_m18_verified_master_for_read(core::SampleRate selectedMasterRate) const;
    // M18 private, owner-thread-only immutable M17 STAGE snapshot.
    // Unlike a PCM-only provider, this preserves the exact checked M16
    // terminal candidate and original MANUAL identity for worker ticket
    // admission. Never grants an export right or changes audible state.
    [[nodiscard]] std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
    capture_m18_verified_stage_for_worker(
        core::SampleRate selectedMasterRate) const;
    // PREPRODUCTION synchronous Product->M18 authorizer, no Q_INVOKABLE/UI.
    // Must be invoked on this QObject's owning thread. Caller supplies an
    // explicit, non-reentrant Product consent predicate. The transaction
    // MUST run synchronously on the SAME thread: the UI event loop must not
    // be pumped, since thread serialization pins the live stage across
    // last commit check+rename+postcommit readback. Not a scalable product
    // export API; async export requires a separately designed epoch lock.
    // PREPRODUCTION ONLY: optional Product-owned epoch mutation admission.
    // If unconfigured, ALL M17 behavior stays byte-for-byte equivalent.
    // The real Product must install an admission which advances the SAME
    // epoch BEFORE every Source/Project/MASTER mutation; other upstream
    // controllers and TerminalCandidateGate are not yet covered.
    using M18MutationAdmission=std::function<core::Status(const char*)>;
    void set_private_m18_mutation_admission(M18MutationAdmission gate)
    {
        // A foreign worker must not uninstall the Product's shared
        // authority or read its non-atomic reentrancy flag.
        if(QThread::currentThread()!=thread())return;
        // The callback that owns the shared epoch cannot remove itself
        // while it is deciding whether the Product write is admitted.
        if(m18PrivateAdmitting_)return;
        m18MutationAdmission_=std::move(gate);
    }
    using M18ExplicitExportConsent=std::function<core::Status()>;
    [[nodiscard]] core::Result<
        platform::windows::M18TransactionalAudioExportRequest>
    make_private_m18_synchronous_export_request(
        core::SampleRate selectedMasterRate,
        core::ResourceReference source,
        core::ResourceReference destination,
        audio::WavSampleFormat format,
        std::optional<std::uint64_t> seed,
        double ceilingDbTp,
        const M18ExplicitExportConsent& explicitConsent) const;

    // SECOND P0 PRIVATE BRIDGE: derive DEVICE PCM from the very same
    // verified M17 owner, with exact rate/owner recheck after a potentially
    // long SRC. This never installs a sink, runs another M17/M16 Prepare,
    // or mints a monitor ticket; product transport wiring comes later.
    [[nodiscard]] core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>>
    materialize_borrowed_verified_m17_monitoring(
        core::SampleRate selectedMasterRate,
        const MonitoringMaterializer& materializer);
    // The product wiring counterpart uses the SAME registered Windows
    // pre-allocation source as MASTER-only monitoring. Fresh physical and
    // commit memory/endpoint must be checked by that service before DEVICE
    // PCM is allocated. No callback or policy means NO allocation.
    [[nodiscard]] core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>>
    materialize_borrowed_verified_m17_monitoring_with_preflight(
        core::SampleRate selectedMasterRate,
        core::FrameCount maximumBlockFrames,
        std::uint64_t reservedOtherPrepareBytes,
        const MonitoringPreallocationDecision& explicitDecision);
    // One-Prepare cross-editor monitor authority: return the exact CURRENT
    // verified MASTER rate whether it originated in M17 READY or the legacy
    // offline MASTER-only lane. Never advertise a measured nonconforming PCM.
    [[nodiscard]] std::optional<core::SampleRate>
    verified_master_rate_for_monitoring() const;
    // Direct M17 Preview is legal only when the current Windows endpoint
    // independently reports exactly the same rate as the checked MASTER.
    // A mismatch is NOT an error or permission to rerender the MASTER.
    [[nodiscard]] bool current_master_matches_device_for_preview() const;
    // Measured NON-CONFORMING PCM has no authorized MASTER->DEVICE SRC.
    // A direct Preview Anyway must therefore match the freshly probed
    // exact device rate without modifying the retained measured candidate.
    [[nodiscard]] bool current_measured_matches_device_for_preview() const;
    // Isolated UNQUALIFIED cross-rate admission. This returns a retained
    // data ticket only; not a Windows sink, M16 LIVE certificate or QML Play.
    // An offline worker may convert captured immutable PCM, but must never
    // access QObject state. Revalidate the measured stage on the owner
    // thread after the conversion, before any future transport action.
    // Explicit fresh endpoint identity and rate must also be re-probed.
    using MeasuredMasterCapture=
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>;
    [[nodiscard]] MeasuredMasterCapture
    capture_measured_master_for_device_conversion() const;
    [[nodiscard]] core::Result<MeasuredDeviceAuditionTicket>
    admit_measured_device_conversion(
        MeasuredMasterCapture captured,
        std::shared_ptr<const audio::MonitoringPcmCandidate> device,
        bool explicitUserRequest,
        const MeasuredDeviceEndpointObservation& freshEndpoint) const;
    // Guard the later physical Windows install using the SAME typed ticket.
    // No sink mutation, Play, verified identity or gain-reduction witness.
    [[nodiscard]] core::Status recheck_measured_device_preinstall(
        const MeasuredDeviceAuditionTicket& ticket,
        const MeasuredDeviceEndpointObservation& latestEndpoint,
        bool explicitUserRequest) const;
    // Producer-side, Qt-owning-thread, exact-MANUAL snapshot admission for
    // measured NON-CONFORMING transport. No playback or sink mutation.
    [[nodiscard]] core::Result<MeasuredDeviceTransportPlan>
    plan_measured_device_handoff(
        const MeasuredDeviceAuditionTicket& ticket,
        const MeasuredDeviceEndpointObservation& latestEndpoint,
        bool explicitUserRequest) const;
    [[nodiscard]] core::Result<MeasuredDeviceTransportPlan>
    rebase_measured_device_handoff(
        const MeasuredDeviceAuditionTicket& ticket,
        const MeasuredDeviceTransportPlan& prior,
        const MeasuredDeviceEndpointObservation& latestEndpoint,
        bool explicitUserRequest) const;
    // Distinct measured NON-CONFORMING DEVICE transport. This reuses the
    // anonymous Windows monitor sink without minting an M17 RealizationId,
    // M16 VERIFIED/LIVE authority, GR history or exportable MASTER.
    // The installing caller must independently re-probe Windows endpoint
    // ID+rate on the owning Qt thread after asynchronous SRC.
    [[nodiscard]] core::Status install_measured_device_audition(
        const MeasuredDeviceAuditionTicket& ticket,
        const MeasuredDeviceEndpointObservation& currentEndpoint,
        bool explicitUserRequest);
    // Exact-owned conditional DEVICE retirement before MANUAL restoration.
    // If restore fails after retirement, an explicit retry keeps the
    // original MANUAL cue/state and never touches another device owner.
    [[nodiscard]] core::Status return_measured_device_audition();
    [[nodiscard]] core::Status control_measured_device_audition(
        platform::windows::MonitoringTransportRequest command);
    [[nodiscard]] core::Result<audio::MonitoringMasterDisplaySnapshot>
    measured_device_snapshot() const;
    [[nodiscard]] bool measured_device_active() const noexcept {
        return installedMeasuredTicket_.has_value()
            ||measuredSourceRecovery_;
    }
    // A SOURCE Loop Region is PLAYBACK METADATA, not M17 full-track DSP
    // input. A valid region edit during deferred Prepare / READY may adopt
    // its new SOURCE loop mapping without invalidating the immutable PCM,
    // changing any M17 math or starting another worker. Required owner,
    // region range and currently selected SOURCE must still match.
    [[nodiscard]] bool adopt_pending_source_loop_without_rerender();
    // Hard cancel is still available for a genuine SOURCE epoch retirement
    // or explicitly cancelled user transaction, never mere loop presence.
    void cancel_pending_on_region_change();
    // Interlock: MASTER-only render and anonymous monitor PCM are not
    // terminal SOURCE audio owned by the ordinary Qt M17 conductor.
    [[nodiscard]] bool monitoring_transaction_active() const noexcept {
        return monitorTransitioning_ || static_cast<bool>(installedMonitor_)
            || measured_device_active()
            || static_cast<bool>(returnRecovery_)
            || (pending_.has_value() && pending_->masterOnly);
    }
    // Read-only actual M17 applied-gain evidence for the GR HISTORY UI.
    // This is OFFLINE programme gain, never Windows consumed-output LIVE
    // certification, and grants no sink, WAV or export authority.
    struct OfflineGrCapture final {
        std::shared_ptr<const TerminalCandidateGate::StagedCandidate> verified;
        std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
            nonconforming;
    };
    [[nodiscard]] OfflineGrCapture capture_offline_gr_for_display() const;

    [[nodiscard]] QString status() const {return status_;}
    [[nodiscard]] QString error() const {return error_;}
    [[nodiscard]] render::TerminalRenderProgress render_progress() const noexcept {
        return renderer_->progress();
    }
    // Passive inspection only: no user opt-in, device audio or WAV.
    [[nodiscard]] std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate>
    measured_candidate() const {
        if(status_!=QStringLiteral("MEASURED_NONCONFORMING_READY")
           ||!pending_||!selected_||!selected_()
           ||!prepared_||!manual_||!manualId_||!sourceLoop_)
            return {};
        auto evidence=candidates_->staged_measured();
        if(!evidence ||evidence->prepared_owner!=prepared_()
           ||evidence->rendered.manual_owner!=manual_()
           ||evidence->original_manual_realization_id
               !=manualId_().value_or(core::RealizationId{0})
           // A measured stage with a different MASTER output clock may
           // still have apparently valid frame geometry; it must never
           // borrow the current user-selected M17 rendering authority.
           ||evidence->rendered.output_rate!=pending_->outputRate
           ||sourceLoop_()!=pending_->loop)
            return {};
        return evidence;
    }
    [[nodiscard]] std::optional<core::RealizationId> private_id() const
    {return privateId_;}

    // Private DEVICE ownership observation. This is intentionally NOT
    // a sink-consumed sample, true-peak LIVE or gain reduction certificate.
    // The caller must independently bind a WindowsConsumedOutputPrefix
    // before reporting any real-time M16/M17 reading.
    struct InstalledOutputIdentity {
        std::shared_ptr<const TerminalCandidateGate::StagedCandidate> owner;
        core::RealizationId realizationId;
        core::PlaybackSnapshot playback;
        TerminalSourceTransportPort::DisplaySnapshot source;
    };
    [[nodiscard]] core::Result<InstalledOutputIdentity>
    private_installed_output_identity() const;
signals:
    void changed();
private:
    // Offline-only submission can retain authoritative SOURCE/MANUAL
    // ownership even when the Windows sink is NO_SOURCE. Ordinary audible
    // M17 submissions keep their exact installed MANUAL owner requirement.
    [[nodiscard]] core::Status request_private_impl(
        core::SampleRate rate, bool deferred, bool masterOnly);
    [[nodiscard]] bool current_selection_for_pending() const;
    struct Pending {
        std::shared_ptr<const render::RenderResult> prepared;
        std::shared_ptr<const render::RenderResult> manual;
        core::RealizationId manualId;
        std::optional<core::FrameRange> loop;
        core::SampleRate outputRate;
        bool deferred{false};
        bool masterOnly{false};
        // Captured only when the explicit MASTER-only job began from a
        // demonstrably empty endpoint. Not a permission to install AUDIO.
        bool offlineNoSink{false};
        bool offlineNonAudibleSelection{false};
    };
    void on_renderer_changed();
    [[nodiscard]] core::Status retry_source_return_after_monitor_retirement();
    [[nodiscard]] core::Status install_accepted_candidate();
    [[nodiscard]] std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
    current_verified_private_master() const;
    [[nodiscard]] core::Status admit_m18_private_mutation(
        const char* operation) const;
    void failed(const core::Error& error);
    void stale();
    TerminalPrivateRenderController* renderer_;
    TerminalPrivatePublicationCoordinator* publisher_;
    TerminalCandidateGate* candidates_;
    PlaybackTransportViewModel* transport_;
    // Opt-in isolated safety seam. NO production use until every upstream
    // mutation and candidate-gate invalidation participates in the epoch.
    M18MutationAdmission m18MutationAdmission_;
    mutable bool m18PrivateAdmitting_{false};
    Snapshot prepared_;
    Snapshot manual_;
    ManualId manualId_;
    Loop sourceLoop_;
    Selected selected_;
    OfflineSelected offlineSelected_;
    OfflineProvenance offlineProvenance_;
    RateProbe rateProbe_;
    MonitoringPreflightMaterializer monitoringPreflight_;
    MonitoringInstall monitorInstall_;
    MonitoringOwnership monitorOwned_;
    MonitoringRetire monitorRetire_;
    ManualRestore manualRestore_;
    ManualRestorePreflight manualRestorePreflight_;
    SourceLoopReconciliation sourceLoopReconcile_;
    MonitorControl monitorControl_;
    MonitorDisplay monitorDisplay_;
    MonitoringOwner approvedMonitor_;
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
        approvedMonitorMaster_;
    MonitoringOwner installedMonitor_;
    // The same anonymous Windows PCM may be owned by either VERIFIED
    // monitor or measured NON-CONFORMING audition, never both.
    std::optional<MeasuredDeviceAuditionTicket> installedMeasuredTicket_;
    std::optional<MeasuredDeviceTransportPlan> installedMeasuredPlan_;
    std::optional<core::PlaybackSnapshot> measuredOriginalManual_;
    bool measuredSourceRecovery_{false};
    core::FrameIndex measuredReturnCue_{0};
    core::PlaybackState measuredReturnState_{core::PlaybackState::STOPPED};
    // Exact pre-install MANUAL transport for recovering a partially seeded
    // DEVICE sink, never the anonymous monitor's incomplete cue/state.
    std::optional<core::PlaybackSnapshot> monitorRecoveryOriginal_;
    // The monitor has been PROVEN retired, but Windows denied reinstalling
    // the immutable MANUAL. Retain exact source cue/loop/state plus pending
    // M17 PCM; no new DEVICE owner or M16 LIVE/GR claim may arise here.
    struct ReturnRecovery final {
        core::FrameIndex sourceCue{0};
        core::PlaybackState priorTransport{core::PlaybackState::STOPPED};
        std::optional<core::FrameRange> reconciledSourceLoop;
        // Keep the exact verified MASTER gate alive across a denied
        // Windows reinstall, and reject any later replacement/revocation.
        std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
            capturedVerifiedMaster;
    };
    std::optional<ReturnRecovery> returnRecovery_;
    bool monitorTransitioning_{false};
    std::optional<Pending> pending_;
    // Exact immutable admission identity; never confuse an M16 failed
    // candidate with the verified-only gate on a subsequent A/B switch.
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> comparisonVerified_;
    std::shared_ptr<const TerminalCandidateGate::StagedMeasuredCandidate> comparisonMeasured_;
    std::optional<core::RealizationId> privateId_;
    QString status_{QStringLiteral("IDLE")};
    QString error_;
};
} // namespace rgsml::app
