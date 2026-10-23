#pragma once
#include "terminal_private_render_controller.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_candidate_gate.hpp"
#include "playback_transport_view_model.hpp"
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <optional>

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
    using RateProbe=std::function<core::Result<core::SampleRate>(
        audio::ChannelLayout)>;

    TerminalPrivateProductSession(
        TerminalPrivateRenderController& renderer,
        TerminalPrivatePublicationCoordinator& publisher,
        TerminalCandidateGate& candidates,
        PlaybackTransportViewModel& transport,
        Snapshot prepared,Snapshot manual,ManualId manualId,
        Loop sourceLoop,Selected selected,QObject* parent=nullptr);

    // An opt-in deferred request renders while the MANUAL PCM keeps playing.
    // Only an explicit conductor handoff may publish the accepted terminal.
    [[nodiscard]] core::Status request_private(core::SampleRate outputRate,
                                               bool deferred=false);
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
    // Changes to a Loop Region cancel pending handoffs but do not stop
    // an already installed SOURCE-mapped session: its own live loop
    // controller processes active Loop Region edits.
    void cancel_pending_on_region_change();
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
    struct Pending {
        std::shared_ptr<const render::RenderResult> prepared;
        std::shared_ptr<const render::RenderResult> manual;
        core::RealizationId manualId;
        std::optional<core::FrameRange> loop;
        core::SampleRate outputRate;
        bool deferred{false};
    };
    void on_renderer_changed();
    [[nodiscard]] core::Status install_accepted_candidate();
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
    RateProbe rateProbe_;
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
