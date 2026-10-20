#include "audition_source_selector.hpp"
#include "playback_transport_view_model.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_product_activation_coordinator.hpp"
#include "terminal_limiter_view_model.hpp"
#include "../platform/fake_playback_service.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/core/uuid.hpp>
#include <QTest>
#include <memory>

namespace rgsml::tests {
class TerminalActivationIntegrationTest final:public QObject {
    Q_OBJECT
private slots:
    void audibleOnBypassRestoresExactManualSourceAndLoop();
};

void TerminalActivationIntegrationTest::audibleOnBypassRestoresExactManualSourceAndLoop()
{
    using namespace rgsml;
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
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
    const auto renderManual=[&]() -> render::RenderResult {
        auto request=render::RenderRequest::create(
            source.value()->view(),source.value()->view().absolute_range(),
            *chain.value(),{},*block.value());
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
    app::AuditionSourceSelector selector{&transport};
    const auto region=core::FrameRange::create(
        core::FrameIndex{500},core::FrameIndex{3500});
    QVERIFY(region);
    selector.set_source_loop_provider([&]{
        return std::optional{*region.value()};
    });
    QVERIFY(selector.set_prepared_realization(renderManual()));
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
        [&]{return *chain.value();}};
    app::TerminalPrivateProductSession product{
        renderer,publisher,candidates,transport,
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector]{return selector.processed_realization_snapshot();},
        [&selector]{return selector.processed_realization_id();},
        [&]{return std::optional{*region.value()};},
        [&selector]{return selector.active_target()
            ==app::AuditionTarget::PROCESSED;}};
    product.set_preferred_output_rate_probe(
        [deviceRate](audio::ChannelLayout)->core::Result<core::SampleRate>{
            return core::Result<core::SampleRate>::success(*deviceRate.value());
        });
    app::TerminalProductActivationCoordinator coordinator{
        *limiter.value(),vm,selector,transport,product};
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("BYPASS"));
    vm.setBypass(false);
    // Preparing the terminal must not stop the MANUAL playback or freeze
    // its playhead during a full-programme asynchronous render.
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    // Authoritative M17 output is not installed yet: show a readable,
    // time-aware pending state while manual audio continues unchanged.
    QCOMPARE(vm.audio_status(),QStringLiteral("Preparing audio"));
    QVERIFY(!coordinator.device_active());
    // Simulate a progressed source cue, including one reached after a loop.
    device->position=core::FrameIndex{1600};
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QTRY_VERIFY_WITH_TIMEOUT(coordinator.device_active(),12000);
    QVERIFY(!limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),
        QStringLiteral("ACTIVE · AUDITION UNQUALIFIED"));
    const auto terminalId=device->audibleRealization.realizationId;
    QVERIFY(terminalId);
    QVERIFY(terminalId!=originalId);
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QVERIFY(device->loop);
    // Native Windows owner is 48k; all UI seek/playhead positions remain
    // mapped to the original source domain, never a 48k master decision.
    QCOMPARE(device->duration->value(),std::int64_t{4800});
    QCOMPARE(selector.processed_realization_snapshot(),original);
    QCOMPARE(selector.processed_realization_id(),originalId);
    vm.setBypass(true);
    QVERIFY(limiter.value()->bypassed());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->position,core::FrameIndex{1600});
    QCOMPARE(device->duration,*frames.value());
    QCOMPARE(device->loop,std::optional{*region.value()});
    QCOMPARE(selector.processed_realization_snapshot(),original);
    QCOMPARE(selector.processed_realization_id(),originalId);
    QCOMPARE(vm.audio_status(),QStringLiteral("BYPASS · MANUAL PROCESSED"));
    QVERIFY(!product.private_id());
    QVERIFY(!candidates.staged());
    QVERIFY(!audioGate.has_session());

    // PAUSED is not STOPPED: ON must reconstruct PAUSED after installing
    // exact new OUTPUT_RATE PCM, and BYPASS must restore PAUSED manual PCM.
    QVERIFY(transport.seek_source_frame(core::FrameIndex{2600}));
    transport.pause();
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    vm.setBypass(false);
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QTRY_VERIFY_WITH_TIMEOUT(coordinator.device_active(),12000);
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    auto pausedTerminalId=device->audibleRealization.realizationId;
    QVERIFY(pausedTerminalId);
    QVERIFY(pausedTerminalId!=originalId);
    QVERIFY(pausedTerminalId!=terminalId);
    auto pausedPosition=transport.source_display_snapshot();
    QVERIFY(pausedPosition);
    QVERIFY(pausedPosition.value()->position.value()>=2599);
    QVERIFY(pausedPosition.value()->position.value()<=2601);
    // An accepted Ceiling change must retire and rerender the whole
    // exact-owned terminal PCM while preserving the PAUSED state.
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-6.0));
    QVERIFY(vm.commitDraft());
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QTRY_VERIFY_WITH_TIMEOUT(coordinator.device_active(),12000);
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QVERIFY(device->audibleRealization.realizationId!=pausedTerminalId);
    QCOMPARE(vm.audio_status(),
        QStringLiteral("ACTIVE · AUDITION UNQUALIFIED"));
    QVERIFY(transport.seek_source_frame(core::FrameIndex{3100}));
    vm.setBypass(true);
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->position,core::FrameIndex{3100});
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!product.private_id());
    QVERIFY(!candidates.staged());

    // STOPPED activation must not unexpectedly start playback; BYPASS
    // retains SOURCE cue, current Loop Region and canonical manual owner.
    transport.stop();
    QVERIFY(transport.seek_source_frame(core::FrameIndex{800}));
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    vm.setBypass(false);
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QTRY_VERIFY_WITH_TIMEOUT(coordinator.device_active(),12000);
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    auto outputPosition=transport.source_display_snapshot();
    QVERIFY(outputPosition);
    QVERIFY(outputPosition.value()->position.value()>=799);
    QVERIFY(outputPosition.value()->position.value()<=801);
    vm.setBypass(true);
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->position,core::FrameIndex{800});
    QCOMPARE(device->loop,std::optional{*region.value()});
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!audioGate.has_session());

    // Device rate discovery failure MUST preserve the original PCM and
    // visibly revert the module to BYPASS without invented ACTIVE data.
    product.set_preferred_output_rate_probe(
        [](audio::ChannelLayout)->core::Result<core::SampleRate> {
            return core::Result<core::SampleRate>::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "Synthetic audition rate-probe failure."});
        });
    vm.setBypass(false);
    QVERIFY(limiter.value()->bypassed());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(device->position,core::FrameIndex{800});
    QCOMPARE(vm.audio_status(),QStringLiteral("ERROR · BYPASS"));
    QVERIFY(!vm.validation_message().isEmpty());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QVERIFY(!candidates.staged());
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TerminalActivationIntegrationTest)
#include "test_terminal_product_activation.moc"
