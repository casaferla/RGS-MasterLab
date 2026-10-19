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

    TerminalPrivateProductSession(
        TerminalPrivateRenderController& renderer,
        TerminalPrivatePublicationCoordinator& publisher,
        TerminalCandidateGate& candidates,
        PlaybackTransportViewModel& transport,
        Snapshot prepared,Snapshot manual,ManualId manualId,
        Loop sourceLoop,Selected selected,QObject* parent=nullptr);

    [[nodiscard]] core::Status request_private(core::SampleRate outputRate);
    [[nodiscard]] core::Status cancel();
    // Changes to a Loop Region cancel pending handoffs but do not stop
    // an already installed SOURCE-mapped session: its own live loop
    // controller processes active Loop Region edits.
    void cancel_pending_on_region_change();
    [[nodiscard]] QString status() const {return status_;}
    [[nodiscard]] QString error() const {return error_;}
    [[nodiscard]] std::optional<core::RealizationId> private_id() const
    {return privateId_;}
signals:
    void changed();
private:
    struct Pending {
        std::shared_ptr<const render::RenderResult> prepared;
        std::shared_ptr<const render::RenderResult> manual;
        core::RealizationId manualId;
        std::optional<core::FrameRange> loop;
        core::SampleRate outputRate;
    };
    void on_renderer_changed();
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
    std::optional<Pending> pending_;
    std::optional<core::RealizationId> privateId_;
    QString status_{QStringLiteral("IDLE")};
    QString error_;
};
} // namespace rgsml::app
