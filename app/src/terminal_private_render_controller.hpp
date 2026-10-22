#pragma once
#include "terminal_candidate_gate.hpp"
#include "terminal_limiter_state.hpp"
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <QObject>
#include <QString>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace rgsml::app {
// Product-owned but PRIVATE. Never grants UI ON, audibility or M16 LIVE.
// Output rate must be independently negotiated, never assumed to be 48 kHz.
class TerminalPrivateRenderController final : public QObject {
    Q_OBJECT
public:
    using Snapshot=std::function<std::shared_ptr<const render::RenderResult>()>;
    using ManualId=std::function<std::optional<core::RealizationId>()>;
    using ManualChain=std::function<dsp::ProcessingChain()>;
    TerminalPrivateRenderController(
        TerminalCandidateGate&,TerminalLimiterState&,
        Snapshot prepared,Snapshot manual,ManualId,ManualChain,
        QObject* parent=nullptr);
    ~TerminalPrivateRenderController() override;
    TerminalPrivateRenderController(const TerminalPrivateRenderController&)=delete;
    TerminalPrivateRenderController& operator=(const TerminalPrivateRenderController&)=delete;
    [[nodiscard]] core::Status request_private(core::SampleRate outputRate);
    void invalidate();
    [[nodiscard]] QString status() const {return status_;}
    [[nodiscard]] QString error() const {return error_;}
    // Preserve the original renderer category and code for accurate
    // worker -> session -> product diagnostics; text is not an oracle.
    [[nodiscard]] std::optional<core::Error> last_error() const
    { return lastError_; }
    [[nodiscard]] std::uint64_t stale_discarded() const noexcept {return staleDiscarded_;}
signals:
    void changed();
private:
    struct Job {
        std::uint64_t serial;
        TerminalCandidateGate::Capture capture;
        dsp::ProcessingChain manualChain;
        dsp::ProcessingChain terminalChain;
        std::vector<dsp::ModuleExecutionBinding> bindings;
        core::FrameCount block;
        std::chrono::steady_clock::time_point enqueuedAt;
    };
    void worker_loop();
    void complete(std::uint64_t,TerminalCandidateGate::Capture,
        std::shared_ptr<core::Result<render::TruePeakLimiterOwnedSourceCandidate>>);
    TerminalCandidateGate* gate_;
    TerminalLimiterState* terminal_;
    Snapshot prepared_;
    Snapshot manual_;
    ManualId id_;
    ManualChain chain_;
    dsp::ModuleRegistry registry_{
        *dsp::ModuleRegistry::create_dsp_package_v1().value()};
    // Only the UI thread increments serial_; the worker reads the atomic
    // mirror to cancel stale jobs at canonical block boundaries.
    std::uint64_t serial_{0};
    std::atomic<std::uint64_t> activeSerial_{0};
    std::uint64_t staleDiscarded_{0};
    QString status_{QStringLiteral("IDLE")};
    QString error_;
    std::optional<core::Error> lastError_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Job> pending_;
    bool stopping_{false};
    std::jthread worker_;
};
} // namespace rgsml::app
