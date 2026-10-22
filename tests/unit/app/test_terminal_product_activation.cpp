#include "audition_source_selector.hpp"
#include "playback_transport_view_model.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_product_activation_coordinator.hpp"
#include "mastering_preview_controller.hpp"
#include "terminal_limiter_view_model.hpp"
#include "../platform/fake_playback_service.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/core/uuid.hpp>
#include <QTest>
#include <memory>
#include <bit>
#include <cstdint>

namespace rgsml::tests {
class TerminalActivationIntegrationTest final:public QObject {
    Q_OBJECT
private slots:
    void audibleOnBypassRestoresExactManualSourceAndLoop();
    void staleM17TerminalJobCancelsWithoutPublishingPcm();
};

void TerminalActivationIntegrationTest::audibleOnBypassRestoresExactManualSourceAndLoop()
{
    using namespace rgsml;
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto previewChainId=*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000418").value();
    const auto gainId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000419").value());
    const auto eqId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000420").value());
    QVERIFY(gainId&&eqId);
    auto upstream=app::MasteringChainState::create_default(
        *registry.value(),previewChainId,*gainId.value(),*eqId.value());
    QVERIFY(upstream);
    auto limiterId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000417").value());
    QVERIFY(limiterId);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*limiterId.value());
    QVERIFY(limiter);
    app::TerminalLimiterViewModel vm{limiter.value()};
    const auto sourceRate=core::SampleRate::create(44100);
    const auto deviceRate=core::SampleRate::create(48000);
    const auto frames=core::FrameCount::create(4410);
    const auto block=core::FrameCount::create(257);
    QVERIFY(sourceRate&&deviceRate&&frames&&block);
    const auto format=audio::AudioFormat::create(
        *sourceRate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    auto source=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(source);
    // Real non-zero PCM: an Input Gain edit must change the accepted
    // MANUAL samples, not merely swap two identical zero-filled owners.
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=source.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        for(auto& sample:*lane.value())
            sample=ch==0 ? 0.125 : -0.0625;
    }
    const auto renderPrepared=[&]() -> render::RenderResult {
        auto request=render::RenderRequest::create(
            source.value()->view(),source.value()->view().absolute_range(),
            *chain.value(),{},*block.value());
        Q_ASSERT(request);
        auto output=render::render_preview(*request.value(),*registry.value());
        Q_ASSERT(output);
        return std::move(*output.value());
    };

    const auto renderManual=[&]() -> render::RenderResult {
        auto request=render::RenderRequest::create(
            source.value()->view(),source.value()->view().absolute_range(),
            upstream.value()->chain(),upstream.value()->execution_bindings(),
            *block.value());
        Q_ASSERT(request);
        auto output=render::render_preview(*request.value(),*registry.value());
        Q_ASSERT(output);
        return std::move(*output.value());
    };

    auto service=std::make_unique<FakePlaybackService>();
    auto* device=service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    transport.set_pcm_prepare_handler(
        [device](audio::AudioBufferView view,
            std::shared_ptr<const void> lease,
            std::optional<core::RealizationId> id)->core::Status {
            if(!lease)return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"Missing MANUAL PCM lease."});
            device->state=core::PlaybackState::STOPPED;
            device->position=view.absolute_start_frame();
            device->duration=view.frame_count();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        });
    // Manual replacement uses a separate no-terminal SOURCE handoff seam.
    transport.set_pcm_handoff_handler(
        [device](audio::AudioBufferView view,
            std::shared_ptr<const void> lease,
            std::optional<core::RealizationId> id)->core::Status {
            if(!lease)return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"Missing successor MANUAL lease."});
            device->state=core::PlaybackState::STOPPED;
            device->position=view.absolute_start_frame();
            device->duration=view.frame_count();
            device->loop.reset();
            device->audibleRealization={core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        });
    app::AuditionSourceSelector selector{&transport};
    const auto region=core::FrameRange::create(
        core::FrameIndex{500},core::FrameIndex{3500});
    QVERIFY(region);
    std::optional<core::FrameRange> desiredLoop=*region.value();
    selector.set_source_loop_provider([&]{return desiredLoop;});
    QVERIFY(selector.set_prepared_realization(renderPrepared()));
    QVERIFY(selector.set_processed_realization(renderManual()));
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    const auto original=selector.processed_realization_snapshot();
    const auto originalId=selector.processed_realization_id();
    QVERIFY(original&&originalId);
    QCOMPARE(device->loop,std::optional{*region.value()});
    QVERIFY(transport.seek_source_frame(core::FrameIndex{1450}));
    transport.playOrResume();
    QCOMPARE(device->state,core::PlaybackState::PLAYING);

    app::TerminalCandidateGate candidates;
    app::TerminalAudioSessionGate audioGate{
        candidates,*device,
        [device,deviceRate](audio::AudioBufferView pcm,
            std::shared_ptr<const void> lease,
            core::RealizationId id)->core::Status {
            if(!lease||id.value==0
               ||pcm.format().sample_rate()!=*deviceRate.value()
               ||pcm.timebase().frame_domain_id()
                    !=audio::FrameDomainId::OUTPUT_RATE)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,"Unowned OUTPUT_RATE M17 PCM."});
            device->state=core::PlaybackState::STOPPED;
            device->position=core::FrameIndex{0};
            device->duration=pcm.frame_count();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        }};
    app::TerminalPrivatePublicationCoordinator publisher{
        candidates,audioGate,transport,
        [&selector](std::shared_ptr<const render::RenderResult> prepared,
            std::shared_ptr<const render::RenderResult> manual,
            std::optional<core::RealizationId> id) {
            return selector.reserve_private_terminal_realization_id(
                prepared,manual,id);
        }};
    app::TerminalPrivateRenderController renderer{
        candidates,*limiter.value(),
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector]{return selector.processed_realization_snapshot();},
        [&selector]{return selector.processed_realization_id();},
        [&]{return upstream.value()->chain();}};
    app::TerminalPrivateProductSession product{
        renderer,publisher,candidates,transport,
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector]{return selector.processed_realization_snapshot();},
        [&selector]{return selector.processed_realization_id();},
        [&]{return desiredLoop;},
        [&selector]{return selector.active_target()
            ==app::AuditionTarget::PROCESSED;}};
    product.set_preferred_output_rate_probe(
        [deviceRate](audio::ChannelLayout)->core::Result<core::SampleRate>{
            return core::Result<core::SampleRate>::success(*deviceRate.value());
        });
    app::TerminalProductActivationCoordinator coordinator{
        *limiter.value(),vm,selector,transport,product};
    // Mirror the REAL app/src/main.cpp signal hookup. The historical test
    // manually fed RENDERING/READY and could not detect a broken preview
    // signal, asynchronous publication or upstream gain revision.
    app::MasteringPreviewController preview{
        upstream.value(),
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector](render::RenderResult output) {
            return selector.set_processed_realization(std::move(output));
        }};
    QObject::connect(&preview,&app::MasteringPreviewController::changed,
                     &coordinator,[&preview,&coordinator] {
        coordinator.manual_preview_changed(preview.preview_status());
    });
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("BYPASS"));
    vm.setBypass(false);
    // Armed is NOT preparing. Play still uses the exact upstream MANUAL PCM.
    QVERIFY(!limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!coordinator.device_active());
    vm.requestPrepare();
    QCOMPARE(vm.audio_status(),QStringLiteral("Preparing audio"));
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    // Completed rendering stays PRIVATE; no automatic switch on Play or ON.
    device->position=core::FrameIndex{1600};
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    vm.previewPrepared();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QVERIFY(coordinator.device_active());
    QCOMPARE(vm.audio_status(),
        QStringLiteral("ACTIVE · AUDITION UNQUALIFIED"));
    auto firstTerminalId=device->audibleRealization.realizationId;
    QVERIFY(firstTerminalId);
    QVERIFY(firstTerminalId!=originalId);
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->duration->value(),std::int64_t{4800});
    QCOMPARE(selector.processed_realization_snapshot(),original);
    vm.setBypass(true);
    QVERIFY(limiter.value()->bypassed());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(device->position,core::FrameIndex{1600});
    QCOMPARE(device->loop,std::optional{*region.value()});
    QVERIFY(!product.private_id());
    QVERIFY(!candidates.staged());
    QVERIFY(!audioGate.has_session());

    // Prepared but NOT previewed can be cancelled without ever handing off.
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    vm.cancelPreparation();
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!product.private_id());
    QVERIFY(!candidates.staged());

    // PAUSED stays paused while preparing; Preview Prepared installs an
    // OUTPUT_RATE owner only after an explicit request.
    QVERIFY(transport.seek_source_frame(core::FrameIndex{2600}));
    transport.pause();
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    const auto pausedTerminalId=device->audibleRealization.realizationId;
    QVERIFY(pausedTerminalId && pausedTerminalId!=firstTerminalId);

    // Committing Ceiling must invalidate the prepared result but MUST NOT
    // schedule another full-resource M17 job automatically.
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-6.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QVERIFY(device->audibleRealization.realizationId!=pausedTerminalId);
    QVERIFY(transport.seek_source_frame(core::FrameIndex{3100}));
    vm.setBypass(true);
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->position,core::FrameIndex{3100});
    QCOMPARE(device->audibleRealization.realizationId,originalId);

    // Upstream Input Gain changes retire M17 before MANUAL publication.
    // Neither preview completion nor Live Play can silently reprepare.
    vm.setBypass(false);
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    auto gain3=dsp::GainParameters::create(+3.0);
    QVERIFY(gain3);
    QVERIFY(upstream.value()->set_gain_parameters(*gain3.value()));
    preview.request_preview();
    QCOMPARE(preview.preview_status(),QStringLiteral("RENDERING"));
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QVERIFY(!coordinator.device_active());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!limiter.value()->bypassed());
    QTRY_COMPARE_WITH_TIMEOUT(preview.preview_status(),
        QStringLiteral("READY"),12000);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    const auto updatedManualId=selector.processed_realization_id();
    const auto updatedManual=selector.processed_realization_snapshot();
    QVERIFY(updatedManualId&&updatedManual&&updatedManualId!=originalId);
    const auto oldLane=original->view().channel(0);
    const auto newLane=updatedManual->view().channel(0);
    QVERIFY(oldLane&&newLane);
    QVERIFY((*oldLane.value())[0]!=(*newLane.value())[0]);
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(device->audibleRealization.realizationId,updatedManualId);

    // SOURCE Loop edit cancels only the pending/READY private job. New
    // request is explicit, source ownership is never silently substituted.
    const auto shiftedRegion=core::FrameRange::create(
        core::FrameIndex{700},core::FrameIndex{3300});
    QVERIFY(shiftedRegion);
    desiredLoop=*shiftedRegion.value();
    QVERIFY(transport.set_loop_source_range(desiredLoop));
    coordinator.loop_context_changed();
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!coordinator.device_active());
    QVERIFY(!product.private_id());
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    QCOMPARE(transport.source_display_snapshot().value()->loop,desiredLoop);

    // A second edit invalidates an already installed result, leaving
    // ordinary preview functional and M17 visibly due for preparation.
    auto gainDown=dsp::GainParameters::create(-3.0);
    QVERIFY(gainDown);
    QVERIFY(upstream.value()->set_gain_parameters(*gainDown.value()));
    preview.request_preview();
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QTRY_COMPARE_WITH_TIMEOUT(preview.preview_status(),
        QStringLiteral("READY"),12000);
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!limiter.value()->bypassed());
    vm.setBypass(true);
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());

    // An authentic rate-probe failure remains a real error, not CANCELLED.
    product.set_preferred_output_rate_probe(
        [](audio::ChannelLayout)->core::Result<core::SampleRate> {
            return core::Result<core::SampleRate>::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "Synthetic audition rate-probe failure."});
        });
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    vm.requestPrepare();
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("ERROR · BYPASS"));
    QVERIFY(vm.validation_message().contains(
        QStringLiteral("Synthetic audition rate-probe failure")));
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());

    // A wrong target is rejected at explicit Prepare, not when arming ON.
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    vm.requestPrepare();
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("ERROR · BYPASS"));
    QVERIFY(vm.validation_message().contains(
        QStringLiteral("requires PROCESSED audition target")));
    QVERIFY(!coordinator.device_active());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
}
void TerminalActivationIntegrationTest::staleM17TerminalJobCancelsWithoutPublishingPcm()
{
    // This is a product-render boundary test: the protected per-block M17
    // process and its strict FP recurrence are untouched.
    using namespace rgsml;
    const auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    const auto params=dsp::TruePeakLimiterParameters::create(-12.0);
    const auto rate=core::SampleRate::create(44100);
    const auto block=core::FrameCount::create(64);
    const auto frames=core::FrameCount::create(320);
    QVERIFY(registry && params && rate && block && frames);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto input=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(input);
    std::size_t polls=0;
    const render::TerminalRenderCancel cancelled=[&] {
        return ++polls>=3;
    };
    const auto aborted=render::render_true_peak_limiter_terminal_candidate(
        input.value()->view(),*params.value(),*registry.value(),
        *block.value(),false,cancelled);
    QVERIFY(!aborted);
    QVERIFY(aborted.error());
    // An abandoned candidate cannot return even a partial owned PCM/GR.
    QVERIFY(std::string_view{aborted.error()->message()} ==
        std::string_view{
            "Stale terminal render was cancelled before complete publication."});
    QCOMPARE(polls,std::size_t{3});

    // A live, never-cancelled job remains binary64 bit-identical with the
    // previous no-callback render across both real stereo output lanes.
    const auto baseline=render::render_true_peak_limiter_terminal_candidate(
        input.value()->view(),*params.value(),*registry.value(),
        *block.value(),false);
    const render::TerminalRenderCancel never=[] {return false;};
    const auto live=render::render_true_peak_limiter_terminal_candidate(
        input.value()->view(),*params.value(),*registry.value(),
        *block.value(),false,never);
    QVERIFY(baseline && live);
    QCOMPARE(baseline.value()->applied_gain_frames.size(),std::size_t{320});
    QCOMPARE(live.value()->applied_gain_frames.size(),std::size_t{320});
    const auto a=baseline.value()->compensated_pcm.view();
    const auto b=live.value()->compensated_pcm.view();
    for (std::size_t ch=0;ch<2;++ch) {
        const auto ca=a.channel(ch);
        const auto cb=b.channel(ch);
        QVERIFY(ca && cb);
        QCOMPARE(ca.value()->size(),cb.value()->size());
        for (std::size_t i=0;i<ca.value()->size();++i)
            QCOMPARE(std::bit_cast<std::uint64_t>((*ca.value())[i]),
                     std::bit_cast<std::uint64_t>((*cb.value())[i]));
    }
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TerminalActivationIntegrationTest)
#include "test_terminal_product_activation.moc"
