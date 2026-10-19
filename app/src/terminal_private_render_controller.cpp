#include "terminal_private_render_controller.hpp"
#include <rgsml/core/error.hpp>
#include <QMetaObject>
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
    { std::lock_guard lock(mutex_);stopping_=true;pending_.reset(); }
    cv_.notify_all();
    if(worker_.joinable())worker_.join();
}
void TerminalPrivateRenderController::invalidate() {
    ++serial_;
    gate_->invalidate();
    {std::lock_guard lock(mutex_);pending_.reset();}
    status_=QStringLiteral("IDLE");error_.clear();emit changed();
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
        error_=QString::fromStdString(captured.error()->message());
        emit changed();
        return core::Status::failure(*captured.error());
    }
    auto block=core::FrameCount::create(4096);
    if(!block)return core::Status::failure(*block.error());
    Job job{serial_,std::move(*captured.value()),std::move(chain),
        terminal_->chain(),terminal_->execution_bindings(),*block.value()};
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
        auto captured=job->capture;
        auto result=render::render_true_peak_limiter_owned_output_rate_candidate(
            captured.manual_owner,job->manualChain,
            captured.full_source_frames,captured.manual_chain_revision,
            captured.output_rate,job->terminalChain,job->bindings,
            registry_,job->block);
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
        error_=outcome&&outcome->error()
            ?QString::fromStdString(outcome->error()->message())
            :QStringLiteral("M17 private rendering returned no output.");
        emit changed();return;
    }
    auto chain=chain_();
    auto accepted=gate_->accept(captured,std::move(*outcome->value()),
        prepared_(),manual_(),id_(),chain,*terminal_);
    if(!accepted) {
        ++staleDiscarded_;
        status_=QStringLiteral("STALE");error_.clear();emit changed();return;
    }
    status_=QStringLiteral("READY_PRIVATE");error_.clear();emit changed();
}
} // namespace rgsml::app
