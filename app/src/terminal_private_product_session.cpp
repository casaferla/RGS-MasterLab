#include "terminal_private_product_session.hpp"
#include <rgsml/core/error.hpp>
#include <utility>

namespace rgsml::app {
namespace {
[[nodiscard]] core::Error rejected(const char* reason) {
    return core::Error{core::ErrorCode::InvalidState,reason};
}
}
TerminalPrivateProductSession::TerminalPrivateProductSession(
    TerminalPrivateRenderController& renderer,
    TerminalPrivatePublicationCoordinator& publisher,
    TerminalCandidateGate& candidates,
    PlaybackTransportViewModel& transport,
    Snapshot prepared,Snapshot manual,ManualId manualId,
    Loop sourceLoop,Selected selected,QObject* parent)
    : QObject(parent),renderer_(&renderer),publisher_(&publisher),
      candidates_(&candidates),transport_(&transport),
      prepared_(std::move(prepared)),manual_(std::move(manual)),
      manualId_(std::move(manualId)),sourceLoop_(std::move(sourceLoop)),
      selected_(std::move(selected))
{
    QObject::connect(renderer_,&TerminalPrivateRenderController::changed,
                     this,&TerminalPrivateProductSession::on_renderer_changed);
}

void TerminalPrivateProductSession::failed(const core::Error& error) {
    pending_.reset();
    status_=QStringLiteral("ERROR");
    error_=QString::fromStdString(error.message());
    emit changed();
}
void TerminalPrivateProductSession::stale() {
    pending_.reset();
    status_=QStringLiteral("STALE");
    error_.clear();
    emit changed();
}

core::Status TerminalPrivateProductSession::request_private(core::SampleRate rate)
{
    if(pending_ || privateId_)
        return core::Status::failure(rejected(
            "A private M17 transaction is already pending or installed."));
    if(!prepared_||!manual_||!manualId_||!sourceLoop_||!selected_
       ||!selected_())
        return core::Status::failure(rejected(
            "Private M17 requires a selected PROCESSED source."));
    auto prepared=prepared_(),manual=manual_();
    auto manualId=manualId_();
    if(!prepared||!manual||!manualId||manualId->value==0)
        return core::Status::failure(rejected(
            "Private M17 requires a published manual predecessor."));
    auto snapshot=transport_->playback_snapshot();
    if(!snapshot) return core::Status::failure(*snapshot.error());
    if(snapshot.value()->state!=core::PlaybackState::STOPPED
       ||snapshot.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||snapshot.value()->audibleRealization.realizationId!=manualId
       ||!snapshot.value()->duration
       ||*snapshot.value()->duration!=manual->view().frame_count())
        return core::Status::failure(rejected(
            "M17 may render only a stopped exact audible manual predecessor."));
    auto loop=sourceLoop_();
    if(loop && (loop->begin().value()<0
       ||loop->end()>manual->view().absolute_range().end()))
        return core::Status::failure(rejected(
            "M17 SOURCE loop lies outside the full manual programme."));
    // request_private internally invalidates the gate, then emits RENDERING;
    // pending ownership is captured before any queued worker completion.
    pending_.emplace(Pending{prepared,manual,*manualId,loop,rate});
    privateId_.reset();
    status_=QStringLiteral("RENDERING");
    error_.clear();
    auto submitted=renderer_->request_private(rate);
    if(!submitted) {
        failed(*submitted.error());
        return submitted;
    }
    emit changed();
    return core::Status::success();
}

core::Result<TerminalPrivateProductSession::InstalledOutputIdentity>
TerminalPrivateProductSession::private_installed_output_identity() const
{
    using Identity=InstalledOutputIdentity;
    auto refused=[](const char* reason) -> core::Result<Identity> {
        return core::Result<Identity>::failure(
            core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!privateId_ || privateId_->value==0 || pending_
       ||status_!=QStringLiteral("PRIVATE_INSTALLED")
       ||!selected_ || !selected_() || !prepared_ || !manual_
       ||!manualId_)
        return refused("M17 is not a current installed PROCESSED terminal.");
    auto candidate=candidates_->staged();
    if(!candidate || candidate->prepared_owner!=prepared_()
       ||candidate->rendered.manual_owner!=manual_()
       ||candidate->original_manual_realization_id
           !=manualId_().value_or(core::RealizationId{0}))
        return refused("M17 immutable candidate no longer has its manual owner.");
    const auto pcm=candidate->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    if(pcm.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||pcm.frame_count().value()<=0
       ||pcm.format().sample_rate()!=candidate->rendered.output_rate)
        return refused("M17 candidate lacks exact OUTPUT_RATE PCM provenance.");
    auto audible=transport_->playback_snapshot();
    if(!audible) return core::Result<Identity>::failure(*audible.error());
    if(audible.value()->state==core::PlaybackState::NO_SOURCE
       ||audible.value()->audibleRealization.phase
           !=core::AudibleHandoffPhase::NEW
       ||audible.value()->audibleRealization.realizationId
           !=std::optional{*privateId_}
       ||audible.value()->duration!=std::optional{pcm.frame_count()})
        return refused("M17 terminal is not the independently audible exact device owner.");
    auto source=transport_->source_display_snapshot();
    if(!source) return core::Result<Identity>::failure(*source.error());
    if(source.value()->duration
       !=candidate->rendered.manual_owner->view().frame_count()
       ||source.value()->position.value()<0
       ||source.value()->position.value()>source.value()->duration.value())
        return refused("M17 SOURCE clock is not mapped to the full predecessor.");
    return core::Result<Identity>::success(Identity{
        std::move(candidate),*privateId_,*audible.value(),*source.value()});
}

core::Status TerminalPrivateProductSession::cancel()
{
    // Withdraw transaction first: renderer.invalidate() emits changed and
    // must not reinterpret its own invalidation as a new acceptance.
    pending_.reset();
    privateId_.reset();
    auto retired=publisher_->revoke_private();
    renderer_->invalidate();
    if(!retired) {
        failed(*retired.error());
        return retired;
    }
    status_=QStringLiteral("IDLE");
    error_.clear();
    emit changed();
    return core::Status::success();
}

void TerminalPrivateProductSession::cancel_pending_on_region_change()
{
    if(pending_)static_cast<void>(cancel());
}

void TerminalPrivateProductSession::on_renderer_changed()
{
    if(!pending_)return;
    const QString state=renderer_->status();
    if(state==QStringLiteral("ERROR")) {
        failed(rejected("M17 private renderer returned an error."));
        return;
    }
    if(state==QStringLiteral("STALE")) {stale();return;}
    if(state!=QStringLiteral("READY_PRIVATE"))return;
    const Pending authority=*pending_;
    // No user-visible publication if input, target or Loop Region changed
    // while the worker calculated its OUTPUT_RATE candidate.
    if(!selected_()||prepared_()!=authority.prepared
       ||manual_()!=authority.manual
       ||manualId_()!=std::optional{authority.manualId}
       ||sourceLoop_()!=authority.loop) {
        renderer_->invalidate();
        stale();
        return;
    }
    auto accepted=candidates_->staged();
    if(!accepted || accepted->prepared_owner!=authority.prepared
       ||accepted->rendered.manual_owner!=authority.manual
       ||accepted->original_manual_realization_id!=authority.manualId
       ||accepted->rendered.output_rate!=authority.outputRate) {
        renderer_->invalidate();
        stale();
        return;
    }
    pending_.reset();
    auto installed=publisher_->install_private(authority.prepared,
        authority.manual,authority.manualId,authority.loop);
    if(!installed) {
        failed(*installed.error());
        renderer_->invalidate();
        return;
    }
    privateId_=*installed.value();
    status_=QStringLiteral("PRIVATE_INSTALLED");
    error_.clear();
    emit changed();
}
} // namespace rgsml::app
