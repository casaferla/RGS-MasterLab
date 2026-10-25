#pragma once
#include "audition_source_selector.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_limiter_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <optional>

namespace rgsml::app {
// Audio truth coordinator for the first M17 product ON/BYPASS transition.
// The output-rate PCM is an AUDITION realization only, not a final WAV;
// a limiter state bit alone never counts as audible M17 verification.
class TerminalProductActivationCoordinator final : public QObject {
    Q_OBJECT
public:
    TerminalProductActivationCoordinator(
        TerminalLimiterState&,TerminalLimiterViewModel&,
        AuditionSourceSelector&,PlaybackTransportViewModel&,
        TerminalPrivateProductSession&,QObject* parent=nullptr);
    [[nodiscard]] bool device_active() const noexcept {return deviceActive_;}
    // Keep Active user intent across ordinary accepted MANUAL rerenders and
    // SOURCE Loop Region edits; never claim terminal PCM during preparation.
    void manual_preview_changed(const QString& previewStatus);
    void loop_context_changed();
    // Called only after a newly selected SOURCE has been committed and
    // its PREPARED owner accepted. Clear an obsolete M17 audio error only
    // when no private DEVICE/MASTER transaction is still owned.
    void source_committed_successfully();
private:
    void limiter_accepted();
    void private_session_changed();
    void external_context_changed();
    void begin_activation();
    void prepare_requested();
    void cancel_requested();
    void preview_requested();
    void preview_nonconforming_requested();
    void deactivate(bool keepActiveIntent);
    void finish_activation(bool nonconforming=false);
    void commit_ready_output(bool nonconforming=false);
    void update_render_elapsed();
    void fail_closed(const QString&);
    void resume_previous_state();
    TerminalLimiterState* state_;
    TerminalLimiterViewModel* view_;
    AuditionSourceSelector* selector_;
    PlaybackTransportViewModel* transport_;
    TerminalPrivateProductSession* session_;
    std::optional<AuditionSourceSelector::TerminalActivationIntent> intent_;
    bool transitioning_{false};
    bool deviceActive_{false};
    bool handoffStarted_{false};
    bool readyToPreview_{false};
    bool manualPreviewInProgress_{false};
    bool waitingForManualPreview_{false};
    // Bypass changes the terminal ProcessingChain revision but NOT its
    // sonic Ceiling revision. Only the latter invalidates a cached A/B PCM.
    std::uint64_t lastParameterRevision_{0};
    QElapsedTimer renderElapsed_;
    QElapsedTimer corePhaseElapsed_;
    bool corePhaseObserved_{false};
    QTimer renderHeartbeat_;
};
} // namespace rgsml::app
