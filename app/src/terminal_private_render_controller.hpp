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
    // A REAL accepted whole-master Prepare observation, not a microprobe,
    // predicted ETA, released Final SRC, or evidence of M16 LIVE. UI-owned
    // lifetime only; no PCM, file path, personal metadata or device identity.
    struct ApprovedPrepareTiming final {
        core::SampleRate source_rate;
        core::SampleRate master_rate;
        core::FrameCount source_frames;
        audio::ChannelLayout layout;
        double ceiling_db_tp{0.0};
        std::int64_t queue_us{-1};
        std::int64_t render_wall_us{-1};
        std::int64_t src_us{-1};
        std::int64_t m17_core_us{-1};
        std::int64_t m16_primary_us{-1};
        std::int64_t verification_trim_us{-1};
        std::int64_t m16_secondary_us{-1};
    };
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
    // Passive proof of actual M17 whole-resource invocation count. This is
    // not a WAV-export certificate, GUI progress metric or audio callback.
    // Monitor SRC/replay must NOT increment it for the same MASTER owner.
    // Last still-current accepted MASTER only. Discarded, canceled, stale,
    // measured-nonconforming and errored runs MUST NOT train timing profiles.
    [[nodiscard]] std::optional<ApprovedPrepareTiming>
    last_approved_prepare_timing() const { return lastApprovedPrepareTiming_; }
    struct ApprovedPrepareHistoricalRange final {
        std::int64_t minimum_wall_us;
        std::int64_t maximum_wall_us;
        std::size_t compatible_samples;
    };
    // Small process-local REAL accepted history. Never a six-rate total ETA:
    // it only compares EXACT (SOURCE,MASTER,frames,channels,Ceiling) matches
    // and refuses to predict from one sample or unrelated DSP work.
    [[nodiscard]] std::optional<ApprovedPrepareHistoricalRange>
    exact_historical_prepare_range(
        core::SampleRate source,core::SampleRate master,
        core::FrameCount frames,audio::ChannelLayout layout,double ceiling) const noexcept;
    [[nodiscard]] std::uint64_t render_invocation_count() const noexcept {
        return renderInvocationCount_.load(std::memory_order_acquire);
    }
    // Lock-free GUI-thread observation; worker reports only complete
    // post-block counters, never partial audio publication.
    [[nodiscard]] render::TerminalRenderProgress progress() const noexcept {
        const auto phase=progressPhase_.load(std::memory_order_acquire);
        return {static_cast<render::TerminalRenderProgressPhase>(phase),
            progressFrames_.load(std::memory_order_relaxed),
            progressTotal_.load(std::memory_order_relaxed)};
    }
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
        std::shared_ptr<core::Result<render::TruePeakLimiterOwnedSourceCandidate>>,
        std::shared_ptr<std::optional<render::TruePeakLimiterOwnedMeasuredNonconformingCandidate>>,
    ApprovedPrepareTiming);
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
    std::atomic<std::uint8_t> progressPhase_{0};
    std::atomic<std::int64_t> progressFrames_{0};
    std::atomic<std::int64_t> progressTotal_{0};
    std::uint64_t staleDiscarded_{0};
    std::optional<ApprovedPrepareTiming> lastApprovedPrepareTiming_;
    std::vector<ApprovedPrepareTiming> approvedPrepareHistory_;
    std::atomic<std::uint64_t> renderInvocationCount_{0};
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
