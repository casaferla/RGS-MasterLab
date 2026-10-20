#pragma once
#include "audition_source_selector.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_limiter_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include <QObject>
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
private:
    void limiter_accepted();
    void private_session_changed();
    void external_context_changed();
    void begin_activation();
    void deactivate(bool keepActiveIntent);
    void finish_activation();
    void commit_ready_output();
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
};
} // namespace rgsml::app
