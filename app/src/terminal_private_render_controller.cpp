#include "terminal_private_render_controller.hpp"
#include <rgsml/core/error.hpp>
#include <QMetaObject>
#include <QDebug>
#include <chrono>
#include <utility>
namespace rgsml::app {
TerminalPrivateRenderController::TerminalPrivateRenderController(
    TerminalCandidateGate& gate,TerminalLimiterState& terminal,
    Snapshot prepared,Snapshot manual,ManualId id,ManualChain chain,QObject* parent)
    : QObject(parent),gate_(&gate),terminal_(&terminal),
      prepared_(std::move(prepared)),manual_(std::move(manual)),
      id_(std::move(id)),chain_(std::move(chain)),
      worker_([this]{worker_loop();}) {}
TerminalPrivateRenderController::~TerminalPrivateRenderController() {
    // Cancel an in-flight resource before joining; never wait for a retired
    // entire 12s or 3min M17 render if a block boundary can stop it.
    activeSerial_.fetch_add(1,std::memory_order_release);
    { std::lock_guard lock(mutex_);stopping_=true;pending_.reset(); }
    cv_.notify_all();
    if(worker_.joinable())worker_.join();
}
void TerminalPrivateRenderController::invalidate() {
    ++serial_;
    activeSerial_.store(serial_,std::memory_order_release);
    progressPhase_.store(0,std::memory_order_release);
    progressFrames_.store(0,std::memory_order_relaxed);
    progressTotal_.store(0,std::memory_order_relaxed);
    gate_->invalidate();
    {std::lock_guard lock(mutex_);pending_.reset();}
    status_=QStringLiteral("IDLE");error_.clear();lastError_.reset();emit changed();
}
core::Status TerminalPrivateRenderController::request_private(core::SampleRate rate) {
    invalidate();
    if(!prepared_||!manual_||!id_||!chain_)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,"Private M17 render owner providers missing."});
    auto prepared=prepared_();auto manual=manual_();
    auto id=id_();auto chain=chain_();
    auto captured=gate_->capture(prepared,manual,id,chain,*terminal_,rate);
    if(!captured) {
        status_=QStringLiteral("ERROR");
        lastError_=*captured.error();
        error_=QString::fromStdString(lastError_->message());
        emit changed();
        return core::Status::failure(*captured.error());
    }
    auto block=core::FrameCount::create(4096);
    if(!block)return core::Status::failure(*block.error());
    Job job{serial_,std::move(*captured.value()),std::move(chain),
        terminal_->chain(),terminal_->execution_bindings(),*block.value(),
        std::chrono::steady_clock::now()};
    {std::lock_guard lock(mutex_);pending_.emplace(std::move(job));}
    status_=QStringLiteral("RENDERING");error_.clear();
    cv_.notify_one();emit changed();return core::Status::success();
}
void TerminalPrivateRenderController::worker_loop() {
    for(;;) {
        std::optional<Job> job;
        {std::unique_lock lock(mutex_);
         cv_.wait(lock,[this]{return stopping_||pending_.has_value();});
         if(stopping_)return;
         job.swap(pending_);}
        if(!job)continue;
        const auto started=std::chrono::steady_clock::now();
        auto captured=job->capture;
        // The predicate never touches Qt, DSP state or floating-point data;
        // it only observes the serial of this immutable private job.
        const render::TerminalRenderCancel cancelled=[this,serial=job->serial] {
            return activeSerial_.load(std::memory_order_acquire)!=serial;
        };
        render::TerminalRenderStageTimings stages{};
        stages.progress=[this,serial=job->serial](
            render::TerminalRenderProgress observation) {
            if(activeSerial_.load(std::memory_order_acquire)!=serial)
                return;
            // A passive atomic sidecar; no QObject/QML work on worker thread.
            // Phase is published last, so a new stage has current counters.
            progressFrames_.store(observation.processed_frames,
                                  std::memory_order_relaxed);
            progressTotal_.store(observation.total_frames,
                                 std::memory_order_relaxed);
            progressPhase_.store(static_cast<std::uint8_t>(observation.phase),
                                 std::memory_order_release);
        };
        auto result=render::render_true_peak_limiter_owned_output_rate_candidate(
            captured.manual_owner,job->manualChain,
            captured.full_source_frames,captured.manual_chain_revision,
            captured.output_rate,job->terminalChain,job->bindings,
            registry_,job->block,cancelled,&stages,true);
        const auto finished=std::chrono::steady_clock::now();
        const auto queueMs=std::chrono::duration_cast<
            std::chrono::milliseconds>(started-job->enqueuedAt).count();
        const auto renderMs=std::chrono::duration_cast<
            std::chrono::milliseconds>(finished-started).count();
        // One diagnostic per completed job, never per DSP frame/tick.
        // No filenames, samples, rendering shortcuts or invented progress.
        qInfo().nospace() << "RGS M17 render serial=" << job->serial
            << " queue_ms=" << queueMs
            << " render_ms=" << renderMs
            << " src_us=" << stages.src_us
            << " m17_core_us=" << stages.m17_core_us
            << " m16_primary_us=" << stages.m16_primary_us
            << " trim_us=" << stages.verification_trim_us
            << " m16_secondary_us=" << stages.m16_secondary_us
            << " result=" << (result ? "accepted_for_gate" : "failure");
        auto delivery=std::make_shared<
            core::Result<render::TruePeakLimiterOwnedSourceCandidate>>(
            std::move(result));
        static_cast<void>(QMetaObject::invokeMethod(
            this,[this,serial=job->serial,captured,delivery] {
                complete(serial,captured,delivery);
            },Qt::QueuedConnection));
    }
}
void TerminalPrivateRenderController::complete(
    std::uint64_t serial,TerminalCandidateGate::Capture captured,
    std::shared_ptr<core::Result<render::TruePeakLimiterOwnedSourceCandidate>> outcome)
{
    if(serial!=serial_||!prepared_||!manual_||!id_||!chain_
       ||prepared_()!=captured.prepared_owner
       ||manual_()!=captured.manual_owner
       ||id_()!=std::optional{captured.manual_realization_id}) {
        ++staleDiscarded_;return;
    }
    if(!outcome||!*outcome) {
        gate_->invalidate();
        status_=QStringLiteral("ERROR");
        lastError_=outcome&&outcome->error()
            ?std::optional<core::Error>{*outcome->error()}
            :std::optional<core::Error>{core::Error{
                core::ErrorCode::InvalidState,
                "M17 private rendering returned no output."}};
        error_=QString::fromStdString(lastError_->message());
        emit changed();return;
    }
    auto chain=chain_();
    auto accepted=gate_->accept(captured,std::move(*outcome->value()),
        prepared_(),manual_(),id_(),chain,*terminal_);
    if(!accepted) {
        ++staleDiscarded_;
        status_=QStringLiteral("STALE");error_.clear();emit changed();return;
    }
    status_=QStringLiteral("READY_PRIVATE");error_.clear();lastError_.reset();emit changed();
}
} // namespace rgsml::app
