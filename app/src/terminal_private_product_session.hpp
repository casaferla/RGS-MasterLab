#pragma once
#include "terminal_private_render_controller.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_candidate_gate.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/platform/windows/windows_audio_playback_service.hpp>
#include <QObject>
#include <QString>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace rgsml::audio {
struct MonitoringPcmCandidate;
struct MonitoringPcmPreallocationAdvice;
}
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

    // PRIVATE ONLY: first handoff restricted to STOPPED, SOURCE cue zero,
    // and no Loop Region. Genuine Play/Pause/Seek and LIVE are not wired.
    // Require an exact minted M17 MASTER + separately-owned DEVICE candidate.
    using MonitoringOwner=std::shared_ptr<const audio::MonitoringPcmCandidate>;
    using MonitoringInstall=std::function<core::Status(MonitoringOwner)>;
    using MonitoringOwnership=std::function<core::Result<bool>(MonitoringOwner)>;
    using MonitoringRetire=std::function<core::Status(MonitoringOwner)>;
    using ManualRestore=std::function<core::Status(
        std::shared_ptr<const render::RenderResult>,
        core::RealizationId,core::FrameIndex)>;
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
    // SECOND P0 PRIVATE BRIDGE: derive DEVICE PCM from the very same
    // verified M17 owner, with exact rate/owner recheck after a potentially
    // long SRC. This never installs a sink, runs another M17/M16 Prepare,
    // or mints a monitor ticket; product transport wiring comes later.
    [[nodiscard]] core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>>
    materialize_borrowed_verified_m17_monitoring(
        core::SampleRate selectedMasterRate,
        const MonitoringMaterializer& materializer);
    // Changes to a Loop Region cancel pending handoffs but do not stop
    // an already installed SOURCE-mapped session: its own live loop
    // controller processes active Loop Region edits.
    void cancel_pending_on_region_change();
    // Interlock: MASTER-only render and anonymous monitor PCM are not
    // terminal SOURCE audio owned by the ordinary Qt M17 conductor.
    [[nodiscard]] bool monitoring_transaction_active() const noexcept {
        return monitorTransitioning_ || static_cast<bool>(installedMonitor_)
            || (pending_.has_value() && pending_->masterOnly);
    }
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
    [[nodiscard]] core::Status install_accepted_candidate();
    [[nodiscard]] std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
    current_verified_private_master() const;
    void failed(const core::Error& error);
    void stale();
    TerminalPrivateRenderController* renderer_;
    TerminalPrivatePublicationCoordinator* publisher_;
    TerminalCandidateGate* candidates_;
    PlaybackTransportViewModel* transport_;
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
    SourceLoopReconciliation sourceLoopReconcile_;
    MonitorControl monitorControl_;
    MonitorDisplay monitorDisplay_;
    MonitoringOwner approvedMonitor_;
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate>
        approvedMonitorMaster_;
    MonitoringOwner installedMonitor_;
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
