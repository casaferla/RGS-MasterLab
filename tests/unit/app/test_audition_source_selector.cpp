#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "gold_selection_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include "source_selection_view_model.hpp"

#include "../audio/wav_test_support.hpp"
#include "../platform/fake_playback_service.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace rgsml::tests {
namespace {

[[nodiscard]] render::RenderResult realization(
    std::int64_t begin, std::int64_t frames)
{
    auto rate = core::SampleRate::create(48'000);
    auto format = audio::AudioFormat::create(
        *rate.value(), audio::ChannelLayout::STEREO_LR);
    auto count = core::FrameCount::create(frames);
    auto buffer = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{begin}, *count.value());
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = dsp::ProcessingChain::create(
        *registry.value(),
        dsp::ProcessingChainContext{
            dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});
    auto request = render::RenderRequest::create(
        buffer.value()->view(), buffer.value()->view().absolute_range(),
        *chain.value(), {}, *core::FrameCount::create(7).value());
    auto result = render::render_preview(*request.value(), *registry.value());
    Q_ASSERT(result);
    return std::move(*result.value());
}

[[nodiscard]] render::RenderResult compressor_realization(
    std::int64_t begin, std::int64_t frames)
{
    auto rate = core::SampleRate::create(48'000);
    auto format = audio::AudioFormat::create(
        *rate.value(), audio::ChannelLayout::STEREO_LR);
    auto count = core::FrameCount::create(frames);
    auto buffer = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{begin}, *count.value());
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = dsp::ProcessingChain::create(
        *registry.value(),
        dsp::ProcessingChainContext{
            dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL});

    const auto uuid =
        core::Uuid::parse("61000000-0000-0000-0000-000000000001");
    Q_ASSERT(uuid);
    const auto compressorId =
        dsp::ModuleInstanceId::from_uuid(*uuid.value());
    Q_ASSERT(compressorId);
    const auto addCompressor = chain.value()->add(
        *compressorId.value(), "rgsml.dsp.compressor", 0);
    Q_ASSERT(addCompressor);

    const auto parameters = dsp::CompressorParameters::create_default();
    Q_ASSERT(parameters);
    auto request = render::RenderRequest::create(
        buffer.value()->view(), buffer.value()->view().absolute_range(),
        *chain.value(),
        {dsp::ModuleExecutionBinding{
            *compressorId.value(), *parameters.value()}},
        *core::FrameCount::create(64).value());
    Q_ASSERT(request);
    auto result = render::render_preview(
        *request.value(), *registry.value());
    Q_ASSERT(result);
    Q_ASSERT(result.value()->compressor_telemetry_sidecar().has_value());
    Q_ASSERT(
        !result.value()->compressor_telemetry_sidecar()
             ->realization_id.has_value());
    return std::move(*result.value());
}

void install_pcm_handler(
    app::PlaybackTransportViewModel& transport,
    FakePlaybackService* service)
{
    transport.set_pcm_prepare_handler(
        [service](
            audio::AudioBufferView view,
            std::shared_ptr<const void>,
            std::optional<core::RealizationId> realizationId) {
            service->state = core::PlaybackState::STOPPED;
            service->position = view.absolute_start_frame();
            service->duration = *core::FrameCount::create(
                view.absolute_end_frame().value()).value();
            service->loop.reset();
            service->audibleRealization = realizationId
                ? core::AudibleRealizationState{
                    core::AudibleHandoffPhase::NEW, realizationId}
                : core::AudibleRealizationState{};
            return core::Status::success();
        });
    transport.set_pcm_handoff_handler(
        [service](
            audio::AudioBufferView view,
            std::shared_ptr<const void>,
            std::optional<core::RealizationId> realizationId) {
            service->duration = *core::FrameCount::create(
                view.absolute_end_frame().value()).value();
            if (service->position == view.absolute_end_frame()) {
                service->position = view.absolute_start_frame();
            }
            service->audibleRealization = realizationId
                ? core::AudibleRealizationState{
                    core::AudibleHandoffPhase::NEW, realizationId}
                : core::AudibleRealizationState{};
            return core::Status::success();
        });
}

[[nodiscard]] QString write_valid_wav(
    QTemporaryDir& directory, const QString& name, std::int64_t seed,
    std::uint32_t sourceRate = 48'000U)
{
    using namespace wav_support;
    std::vector<std::int64_t> samples(64U);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = (static_cast<std::int64_t>(index) + seed) % 100;
    }
    const auto bytes = make_wav(
        1U, 16U, 1U, sourceRate, pcm_payload(samples, 16U));
    const auto path = directory.filePath(name);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size())) {
        return {};
    }
    file.close();
    return path;
}

[[nodiscard]] core::ResourceReference reference_for(const QString& path)
{
    const auto pathUtf8 = path.toUtf8();
    const auto nameUtf8 = QFileInfo{path}.fileName().toUtf8();
    auto reference = platform::windows::WindowsResourceReader::make_read_reference(
        std::string_view{pathUtf8.constData(), static_cast<std::size_t>(pathUtf8.size())},
        std::string_view{nameUtf8.constData(), static_cast<std::size_t>(nameUtf8.size())});
    Q_ASSERT(reference);
    return std::move(*reference.value());
}

}  // namespace

class AuditionSourceSelectorTest final : public QObject {
    Q_OBJECT

private slots:
    void availabilityCuesSwitchingAndFallbackAreTruthful();
    void partialRealizationRejectsCueOutsideAbsoluteRange();
    void failedGoldSelectionPreservesPriorGoldAndSourceRealization();
    void loopOutsideRealizationIsNotAppliedAndLifecycleClearsBorrowedPcm();
    void playIntentIsRestoredAcrossTargetSwitches();
    void activeProcessedRealizationReplacementPreservesCueAndState();
    void eofCueIsCanonicalizedToRangeBegin();
    void loopRegionIntentPreservedAcrossAuditionRebinds();
    void seamlessProcessedHandoffPreservesStateAndTargetRaces();
    void processedRealizationIdentityIsMonotonicAndFailureAtomic();
    void terminalIdentityReservationChecksOwnersPhaseAndSharedSequence();
    void privateTerminalReturnRestoresExactManualSourceCueAndLoop();
    void terminalActivationIntentCapturesExactPlayingAndPausedSource();
    void processedTelemetryBindsAcceptedRealizationIdentity();
    void initialUnsupportedDeviceOfflineMasterProvenance_data();
    void initialUnsupportedDeviceOfflineMasterProvenance();
};


void AuditionSourceSelectorTest::initialUnsupportedDeviceOfflineMasterProvenance_data()
{
    QTest::addColumn<std::uint32_t>("sourceRate");
    QTest::newRow("44k1") << std::uint32_t{44'100};
    QTest::newRow("48k") << std::uint32_t{48'000};
    QTest::newRow("192k") << std::uint32_t{192'000};
}

void AuditionSourceSelectorTest::initialUnsupportedDeviceOfflineMasterProvenance()
{
    QFETCH(std::uint32_t,sourceRate);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path=write_valid_wav(
        directory,QStringLiteral("unsupported-device-source.wav"),1,sourceRate);
    QVERIFY(!path.isEmpty());

    auto service=std::make_unique<FakePlaybackService>();
    auto* device=service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    transport.set_pcm_prepare_handler(
        [](audio::AudioBufferView,std::shared_ptr<const void>,
           std::optional<core::RealizationId>)->core::Status {
            return core::Status::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "TEST_WINDOWS_INITIAL_SOURCE_FORMAT_REJECTED"});
        });
    app::AuditionSourceSelector selector{&transport};
    selector.set_source_loop_provider([] {
        return std::optional<core::FrameRange>{};
    });
    QVERIFY(selector.source_committed(reference_for(path)));
    QVERIFY(selector.prepared_available());
    QVERIFY(!selector.active_target());
    const auto prepared=selector.prepared_realization_snapshot();
    QVERIFY(prepared);
    QCOMPARE(prepared->view().format().sample_rate(),
             *core::SampleRate::create(sourceRate).value());
    const auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chain=dsp::ProcessingChain::create(
        *registry.value(),{dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto request=render::RenderRequest::create(
        prepared->view(),prepared->view().absolute_range(),
        *chain.value(),{},*core::FrameCount::create(64).value());
    QVERIFY(request);
    auto output=render::render_preview(*request.value(),*registry.value());
    QVERIFY(output);
    QVERIFY(selector.set_processed_realization(std::move(*output.value())));
    // The hardware rejects the INITIAL source/processed selection. No
    // activeTarget or audible RealizationId may be fabricated to fix it.
    QVERIFY(!selector.switch_to(app::AuditionTarget::PROCESSED));
    QVERIFY(!selector.active_target());
    QVERIFY(!selector.offline_processed_selected_for_final_master());
    QVERIFY(selector.authorize_offline_processed_for_final_master());
    QVERIFY(selector.offline_processed_selected_for_final_master());
    const auto empty=transport.playback_snapshot();
    QVERIFY(empty);
    QCOMPARE(empty.value()->state,core::PlaybackState::NO_SOURCE);
    QCOMPARE(empty.value()->audibleRealization.phase,
             core::AudibleHandoffPhase::UNAVAILABLE);
    QVERIFY(!empty.value()->audibleRealization.realizationId);
    QVERIFY(!empty.value()->duration);

    const auto region=core::FrameRange::create(
        core::FrameIndex{0},core::FrameIndex{32});
    QVERIFY(region);
    std::optional<core::FrameRange> loop=*region.value();
    selector.set_source_loop_provider([&loop] {return loop;});
    QVERIFY(!selector.offline_processed_selected_for_final_master());
    QVERIFY(!selector.authorize_offline_processed_for_final_master());
    loop.reset();
    QVERIFY(selector.authorize_offline_processed_for_final_master());

    // Foreign ownership is never inferred to be the manual PREPARED source.
    device->state=core::PlaybackState::STOPPED;
    device->audibleRealization={
        core::AudibleHandoffPhase::NEW,core::RealizationId{0xAA55}};
    QVERIFY(!selector.offline_processed_selected_for_final_master());
    QVERIFY(!selector.authorize_offline_processed_for_final_master());
    // A test-controlled external device removal permits a new admission.
    QVERIFY(device->clear());
    QVERIFY(selector.authorize_offline_processed_for_final_master());
    // Even the same file becomes a new SOURCE epoch: old tickets are void.
    QVERIFY(selector.source_committed(reference_for(path)));
    QVERIFY(!selector.offline_processed_selected_for_final_master());
    QVERIFY(!selector.processed_realization_snapshot());
}

void AuditionSourceSelectorTest::availabilityCuesSwitchingAndFallbackAreTruthful()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};

    QVERIFY(!selector.prepared_available());
    QVERIFY(!selector.processed_available());
    QVERIFY(!selector.gold_available());
    QVERIFY(!selector.switch_to(app::AuditionTarget::PREPARED));
    QVERIFY(!selector.active_target());

    QVERIFY(selector.set_prepared_realization(realization(0, 100)));
    QVERIFY(selector.set_processed_realization(realization(0, 100)));
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});
    QVERIFY(selector.source_playhead_visible());
    observed->position = core::FrameIndex{42};

    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{42});
    QCOMPARE(observed->position.value(), std::int64_t{42});

    auto gold = core::ResourceReference::create(
        "rgsml.windows.local-file", "C:/Gold/reference.wav", true, false, "reference.wav");
    QVERIFY(gold);
    QVERIFY(selector.set_gold(
        std::move(*gold.value()), *core::SampleRate::create(48'000).value(),
        *core::FrameCount::create(100).value()));
    observed->position = core::FrameIndex{55};
    QVERIFY(selector.switch_to(app::AuditionTarget::GOLD));
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{55});
    QVERIFY(selector.gold_active());
    QVERIFY(!selector.source_playhead_visible());
    observed->position = core::FrameIndex{77};

    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.gold_cue().value(), std::int64_t{77});
    QCOMPARE(observed->position.value(), std::int64_t{55});
    QVERIFY(selector.switch_to(app::AuditionTarget::GOLD));
    QCOMPARE(observed->position.value(), std::int64_t{77});
    QVERIFY(selector.clear_gold());
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});
    QVERIFY(!selector.gold_available());
    QCOMPARE(observed->position.value(), std::int64_t{55});
}

void AuditionSourceSelectorTest::partialRealizationRejectsCueOutsideAbsoluteRange()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};
    QVERIFY(selector.set_prepared_realization(realization(0, 300)));
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    observed->position = core::FrameIndex{90};
    auto gold = core::ResourceReference::create(
        "rgsml.windows.local-file", "C:/Gold/reference.wav", true, false);
    QVERIFY(gold);
    QVERIFY(selector.set_gold(
        std::move(*gold.value()), *core::SampleRate::create(48'000).value(),
        *core::FrameCount::create(100).value()));
    QVERIFY(selector.switch_to(app::AuditionTarget::GOLD));
    QVERIFY(selector.set_prepared_realization(realization(100, 100)));
    QVERIFY(!selector.switch_to(app::AuditionTarget::PREPARED));
    QVERIFY(!selector.active_target());
    QCOMPARE(observed->state, core::PlaybackState::NO_SOURCE);
}

void AuditionSourceSelectorTest::failedGoldSelectionPreservesPriorGoldAndSourceRealization()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto sourcePath = write_valid_wav(directory, QStringLiteral("source.wav"), 1);
    const auto goldPath = write_valid_wav(directory, QStringLiteral("gold.wav"), 2);
    QVERIFY(!sourcePath.isEmpty() && !goldPath.isEmpty());

    auto service = std::make_unique<FakePlaybackService>();
    app::PlaybackTransportViewModel transport{std::move(service)};
    app::AuditionSourceSelector selector{&transport};
    app::GoldSelectionViewModel gold{&selector};
    gold.selectGold(QUrl::fromLocalFile(goldPath));
    QVERIFY(gold.has_gold());
    QCOMPARE(gold.display_name(), QStringLiteral("gold.wav"));

    const auto sourceReference = reference_for(sourcePath);
    QVERIFY(selector.source_committed(sourceReference));
    QVERIFY(selector.prepared_available());
    gold.selectGold(QUrl::fromLocalFile(sourcePath));
    QVERIFY(gold.has_gold());
    QCOMPARE(gold.display_name(), QStringLiteral("gold.wav"));
    QVERIFY(!gold.error_message().isEmpty());
    QVERIFY(selector.prepared_available());
}

void AuditionSourceSelectorTest::loopOutsideRealizationIsNotAppliedAndLifecycleClearsBorrowedPcm()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    const auto outside = *core::FrameRange::create(
        core::FrameIndex{50}, core::FrameIndex{150}).value();
    {
        app::AuditionSourceSelector selector{&transport};
        selector.set_source_loop_provider([outside] { return std::optional{outside}; });
        QVERIFY(selector.set_prepared_realization(realization(0, 100)));
        QCOMPARE(observed->clearCalls, 0);
        QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
        QCOMPARE(observed->state, core::PlaybackState::STOPPED);
        QVERIFY(!observed->loop);
    }
    QVERIFY(observed->clearCalls >= 2);
    QCOMPARE(observed->state, core::PlaybackState::NO_SOURCE);
}

void AuditionSourceSelectorTest::playIntentIsRestoredAcrossTargetSwitches()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};

    QVERIFY(selector.set_prepared_realization(realization(0, 100)));
    QVERIFY(selector.set_processed_realization(realization(0, 100)));

    // 1. PREPARED playing -> PROCESSED
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    transport.playOrResume();
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);
    observed->position = core::FrameIndex{30};

    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{30});
    QCOMPARE(observed->position.value(), std::int64_t{30});
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);

    // 2. PROCESSED playing -> PREPARED
    observed->position = core::FrameIndex{45};
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{45});
    QCOMPARE(observed->position.value(), std::int64_t{45});
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);

    // 3. PAUSED switch -> prepares new target at cue point in STOPPED state
    transport.pause();
    QCOMPARE(observed->state, core::PlaybackState::PAUSED);
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(observed->state, core::PlaybackState::STOPPED);

    // 4. STOPPED switch -> remains STOPPED
    transport.stop();
    QCOMPARE(observed->state, core::PlaybackState::STOPPED);
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(observed->state, core::PlaybackState::STOPPED);
}

void AuditionSourceSelectorTest::activeProcessedRealizationReplacementPreservesCueAndState()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};

    QVERIFY(selector.set_prepared_realization(realization(0, 200)));
    QVERIFY(selector.set_processed_realization(realization(0, 200)));

    // Case 1: PROCESSED is PLAYING at cue 42 -> replace Processed realization
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    transport.playOrResume();
    observed->position = core::FrameIndex{42};
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);

    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{42});
    QCOMPARE(observed->position.value(), std::int64_t{42});
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);

    // Case 2: PROCESSED is PAUSED at cue 55 -> replace Processed realization
    observed->position = core::FrameIndex{55};
    transport.pause();
    QCOMPARE(observed->state, core::PlaybackState::PAUSED);

    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{55});
    QCOMPARE(observed->position.value(), std::int64_t{55});
    QCOMPARE(observed->state, core::PlaybackState::PAUSED);

    // Case 3: PROCESSED is STOPPED -> replace Processed realization
    transport.stop();
    QCOMPARE(observed->state, core::PlaybackState::STOPPED);

    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QCOMPARE(observed->state, core::PlaybackState::STOPPED);

    // Case 4: PREPARED is active -> replace Processed realization
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});

    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});

    // Case 5: GOLD is active -> replace Processed realization
    auto gold = core::ResourceReference::create(
        "rgsml.windows.local-file", "C:/Gold/reference.wav", true, false, "reference.wav");
    QVERIFY(gold);
    QVERIFY(selector.set_gold(
        std::move(*gold.value()), *core::SampleRate::create(48'000).value(),
        *core::FrameCount::create(200).value()));
    QVERIFY(selector.switch_to(app::AuditionTarget::GOLD));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::GOLD});

    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::GOLD});

    // Case 6: Atomic failure path on invalid replacement range
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    observed->position = core::FrameIndex{150};

    // Realization range [0..100) does not contain cue 150 -> fails atomically
    QVERIFY(!selector.set_processed_realization(realization(0, 100)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QCOMPARE(observed->position.value(), std::int64_t{150});
    QVERIFY(selector.processed_available());
    QCOMPARE(selector.processed_realization_snapshot()->render_window().end().value(), std::int64_t{200});
    QVERIFY(!selector.status_text().isEmpty());
}

void AuditionSourceSelectorTest::eofCueIsCanonicalizedToRangeBegin()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};

    QVERIFY(selector.set_prepared_realization(realization(0, 100)));
    QVERIFY(selector.set_processed_realization(realization(0, 100)));

    // 1. PREPARED replay at EOF (cue == range.end() == 100) -> canonicalizes to 0
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    observed->position = core::FrameIndex{100}; // at EOF
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED)); // store cue 100, prepare_realization canonicalizes in-place
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{0});

    // Replay PREPARED at EOF -> should canonicalize cue 100 to 0 and succeed
    observed->position = core::FrameIndex{100};
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{0});
    QCOMPARE(observed->position.value(), std::int64_t{0});

    // 2. PROCESSED replay at EOF
    observed->position = core::FrameIndex{100}; // at EOF
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{0});
    QCOMPARE(observed->position.value(), std::int64_t{0});

    // 3. Processed realization replacement at EOF
    observed->position = core::FrameIndex{100};
    QVERIFY(selector.set_processed_realization(realization(0, 100)));
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{0});
    QCOMPARE(observed->position.value(), std::int64_t{0});

    // 4. Out of bounds cue rejections (cue < begin or cue > end)
    observed->position = core::FrameIndex{150}; // cue > end (100)
    QVERIFY(!selector.set_processed_realization(realization(0, 100)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QVERIFY(selector.processed_available());
}

void AuditionSourceSelectorTest::seamlessProcessedHandoffPreservesStateAndTargetRaces()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};

    QVERIFY(selector.set_prepared_realization(realization(0, 200)));
    QVERIFY(selector.set_processed_realization(realization(0, 200)));

    // 1. Target race condition: switch to PREPARED while PROCESSED replacement is pending
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});

    // User switches away to PREPARED
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});

    // Processed realization completes afterwards -> MUST NOT switch target back to PROCESSED
    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});

    // 2. PAUSED state handoff: replacement when PAUSED preserves PAUSED state
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    transport.playOrResume();
    observed->position = core::FrameIndex{60};
    transport.pause();
    QCOMPARE(observed->state, core::PlaybackState::PAUSED);

    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QCOMPARE(observed->state, core::PlaybackState::PAUSED);
    QCOMPARE(observed->position.value(), std::int64_t{60});

    // 3. Failure atomicity: failed replacement preserves old realization, cue, and state
    observed->position = core::FrameIndex{80};
    // Incompatible candidate (e.g. range [0..50) when cue is 80)
    QVERIFY(!selector.set_processed_realization(realization(0, 50)));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QCOMPARE(observed->state, core::PlaybackState::PAUSED);
    QCOMPARE(observed->position.value(), std::int64_t{80});
    QVERIFY(selector.processed_available());
    QCOMPARE(selector.processed_realization_snapshot()->render_window().end().value(), std::int64_t{200});
}

void AuditionSourceSelectorTest::processedRealizationIdentityIsMonotonicAndFailureAtomic()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};

    QVERIFY(selector.set_prepared_realization(realization(0, 200)));
    QVERIFY(selector.set_processed_realization(realization(0, 200)));

    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(
        observed->audibleRealization.phase,
        core::AudibleHandoffPhase::UNAVAILABLE);
    QVERIFY(!observed->audibleRealization.realizationId.has_value());

    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    const auto firstId = observed->audibleRealization.realizationId;
    QVERIFY(firstId.has_value());
    QCOMPARE(
        observed->audibleRealization.phase,
        core::AudibleHandoffPhase::NEW);

    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    const auto secondId = observed->audibleRealization.realizationId;
    QVERIFY(secondId.has_value());
    QVERIFY(secondId->value > firstId->value);

    // A handoff-layer failure must not publish or consume the candidate ID.
    transport.set_pcm_handoff_handler(
        [](audio::AudioBufferView,
           std::shared_ptr<const void>,
           std::optional<core::RealizationId>) {
            return core::Status::failure(core::Error{
                core::ErrorCode::IoFailure,
                "Injected PCM handoff failure."});
        });
    QVERIFY(!selector.set_processed_realization(realization(0, 200)));
    QCOMPARE(observed->audibleRealization.realizationId, secondId);
    install_pcm_handler(transport, observed);

    // A pre-handoff validation failure is likewise identity-neutral.
    observed->position = core::FrameIndex{150};
    QVERIFY(!selector.set_processed_realization(realization(0, 100)));
    QCOMPARE(observed->audibleRealization.realizationId, secondId);

    observed->position = core::FrameIndex{50};
    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    const auto thirdId = observed->audibleRealization.realizationId;
    QVERIFY(thirdId.has_value());
    QCOMPARE(thirdId->value, secondId->value + 1U);
}

void AuditionSourceSelectorTest::terminalIdentityReservationChecksOwnersPhaseAndSharedSequence()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* audible = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, audible);
    app::AuditionSourceSelector selector{&transport};

    QVERIFY(!selector.reserve_private_terminal_realization_id({}, {}, {}));
    QVERIFY(selector.set_prepared_realization(realization(0, 200)));
    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    const auto prepared = selector.prepared_realization_snapshot();
    const auto manual = selector.processed_realization_snapshot();
    const auto manualId = selector.processed_realization_id();
    QVERIFY(prepared && manual && manualId);
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId)); // PROCESSED not selected/audible
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        manual, manual, manualId)); // wrong PREPARED owner
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, prepared, manualId)); // wrong MANUAL owner
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, core::RealizationId{0}));
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, core::RealizationId{manualId->value + 50U}));
    audible->audibleRealization.phase = core::AudibleHandoffPhase::OLD;
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId));
    audible->audibleRealization.phase = core::AudibleHandoffPhase::NEW;
    audible->duration = *core::FrameCount::create(199).value();
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId));
    audible->duration = *core::FrameCount::create(200).value();
    transport.playOrResume();
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId));
    transport.pause();
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId));
    transport.stop();
    auto terminal1 = selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId);
    QVERIFY(terminal1);
    QCOMPARE(terminal1.value()->value, manualId->value + 1U);
    QCOMPARE(selector.processed_realization_id(), manualId);
    QCOMPARE(audible->audibleRealization.realizationId, manualId);
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    // A failed or stale future terminal installation must not reuse its ID.
    auto terminal2 = selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId);
    QVERIFY(terminal2);
    QCOMPARE(terminal2.value()->value, terminal1.value()->value + 1U);
    QVERIFY(selector.set_processed_realization(realization(0, 200)));
    const auto nextManualId = selector.processed_realization_id();
    QVERIFY(nextManualId);
    QCOMPARE(nextManualId->value, terminal2.value()->value + 1U);
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, manual, manualId)); // stale prior MANUAL owner
    const auto freshManual = selector.processed_realization_snapshot();
    QVERIFY(selector.reserve_private_terminal_realization_id(
        prepared, freshManual, nextManualId));
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QVERIFY(!selector.reserve_private_terminal_realization_id(
        prepared, freshManual, nextManualId));
}

void AuditionSourceSelectorTest::terminalActivationIntentCapturesExactPlayingAndPausedSource()
{
    auto service=std::make_unique<FakePlaybackService>();
    auto* audio=service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport,audio);
    app::AuditionSourceSelector selector{&transport};
    QVERIFY(!selector.capture_terminal_activation_intent());
    QVERIFY(selector.set_prepared_realization(realization(0,200)));
    QVERIFY(selector.set_processed_realization(realization(0,200)));
    QVERIFY(!selector.capture_terminal_activation_intent()); // Not selected.
    const auto owner=selector.processed_realization_snapshot();
    const auto id=selector.processed_realization_id();
    QVERIFY(id);
    const auto loop=core::FrameRange::create(core::FrameIndex{20},
        core::FrameIndex{90});
    QVERIFY(loop);
    selector.set_source_loop_provider([&] {
        return std::optional{*loop.value()};
    });
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    auto intent=selector.capture_terminal_activation_intent();
    QVERIFY(intent);
    QVERIFY(intent.value()->manualOwner==owner);
    QCOMPARE(intent.value()->manualId,*id);
    QCOMPARE(intent.value()->previousPlaybackState,core::PlaybackState::STOPPED);
    QCOMPARE(intent.value()->sourceLoop,std::optional{*loop.value()});
    QVERIFY(transport.seek_source_frame(core::FrameIndex{37}));
    transport.playOrResume();
    intent=selector.capture_terminal_activation_intent();
    QVERIFY(intent);
    QCOMPARE(intent.value()->sourceCue,core::FrameIndex{37});
    QCOMPARE(intent.value()->previousPlaybackState,core::PlaybackState::PLAYING);
    transport.pause();
    intent=selector.capture_terminal_activation_intent();
    QVERIFY(intent);
    QCOMPARE(intent.value()->previousPlaybackState,core::PlaybackState::PAUSED);
    audio->audibleRealization.realizationId=core::RealizationId{id->value+22};
    QVERIFY(!selector.capture_terminal_activation_intent());
    audio->audibleRealization.realizationId=id;
    audio->audibleRealization.phase=core::AudibleHandoffPhase::TRANSITION;
    QVERIFY(!selector.capture_terminal_activation_intent());
    audio->audibleRealization.phase=core::AudibleHandoffPhase::NEW;
    audio->duration=*core::FrameCount::create(199).value();
    QVERIFY(!selector.capture_terminal_activation_intent());
    audio->duration=*core::FrameCount::create(200).value();
    QVERIFY(selector.capture_terminal_activation_intent());
}

void AuditionSourceSelectorTest::privateTerminalReturnRestoresExactManualSourceCueAndLoop()
{
    auto service=std::make_unique<FakePlaybackService>();
    auto* audio=service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport,audio);
    app::AuditionSourceSelector selector{&transport};
    QVERIFY(selector.set_prepared_realization(realization(0,200)));
    QVERIFY(selector.set_processed_realization(realization(0,200)));
    const auto manual=selector.processed_realization_snapshot();
    const auto id=selector.processed_realization_id();
    QVERIFY(manual && id);
    auto loop=core::FrameRange::create(
        core::FrameIndex{20},core::FrameIndex{60});
    QVERIFY(loop);
    selector.set_source_loop_provider([&] {
        return std::optional{*loop.value()};
    });
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(audio->state,core::PlaybackState::STOPPED);
    QCOMPARE(audio->loop,std::optional{*loop.value()});
    QCOMPARE(audio->audibleRealization.realizationId,id);
    QVERIFY(!selector.restore_manual_processed_after_terminal(
        manual,*id,core::FrameIndex{37})); // Never replace a live MANUAL.

    // A terminal device can leave NO_SOURCE only after its own separately
    // authorized retirement. Simulate that exact state without inventing
    // an OUTPUT_RATE owner or claiming the limiter was actually heard.
    QVERIFY(transport.stop_and_clear());
    QVERIFY(!selector.restore_manual_processed_after_terminal(
        manual,core::RealizationId{id->value+1},core::FrameIndex{37}));
    QVERIFY(!selector.restore_manual_processed_after_terminal(
        manual,*id,core::FrameIndex{201}));
    QVERIFY(!selector.restore_manual_processed_after_terminal(
        {},*id,core::FrameIndex{37}));
    QVERIFY(selector.restore_manual_processed_after_terminal(
        manual,*id,core::FrameIndex{37}));
    QCOMPARE(selector.active_target(),std::optional{app::AuditionTarget::PROCESSED});
    QVERIFY(selector.processed_realization_snapshot()==manual);
    QCOMPARE(selector.processed_realization_id(),id);
    QCOMPARE(audio->audibleRealization.realizationId,id);
    QCOMPARE(audio->position,core::FrameIndex{37});
    QVERIFY(audio->duration.has_value());
    QCOMPARE(audio->duration->value(),std::int64_t{200});
    QCOMPARE(audio->loop,std::optional{*loop.value()});
    QCOMPARE(audio->state,core::PlaybackState::STOPPED);
    QVERIFY(!selector.restore_manual_processed_after_terminal(
        manual,*id,core::FrameIndex{38})); // no double-install
    // Canonical EOF cue returns to beginning, never one sample beyond.
    QVERIFY(transport.stop_and_clear());
    QVERIFY(selector.restore_manual_processed_after_terminal(
        manual,*id,core::FrameIndex{200}));
    QCOMPARE(audio->position,core::FrameIndex{0});
    QCOMPARE(selector.processed_realization_id(),id);
}

void AuditionSourceSelectorTest::processedTelemetryBindsAcceptedRealizationIdentity()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);
    app::AuditionSourceSelector selector{&transport};

    // The published Processed identity is absent until publication.
    QVERIFY(!selector.processed_realization_id().has_value());
    QVERIFY(selector.set_prepared_realization(realization(0, 200)));
    QVERIFY(!selector.processed_realization_id().has_value());
    QVERIFY(selector.set_processed_realization(
        compressor_realization(0, 200)));

    auto first = selector.processed_realization_snapshot();
    QVERIFY(first);
    QVERIFY(first->compressor_telemetry_sidecar().has_value());
    const auto firstId =
        first->compressor_telemetry_sidecar()->realization_id;
    QVERIFY(firstId.has_value());
    QCOMPARE(selector.processed_realization_id(), firstId);
    for (const auto& lane :
         first->compressor_telemetry_sidecar()->channel_lanes) {
        for (const auto& bucket : lane.buckets) {
            QCOMPARE(bucket.realization_id, firstId);
        }
    }

    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(observed->audibleRealization.realizationId, firstId);

    QVERIFY(selector.set_processed_realization(
        compressor_realization(0, 200)));
    auto second = selector.processed_realization_snapshot();
    QVERIFY(second);
    QVERIFY(second->compressor_telemetry_sidecar().has_value());
    const auto secondId =
        second->compressor_telemetry_sidecar()->realization_id;
    QVERIFY(secondId.has_value());
    QCOMPARE(selector.processed_realization_id(), secondId);
    QVERIFY(secondId->value > firstId->value);
    QCOMPARE(observed->audibleRealization.realizationId, secondId);
    for (const auto& lane :
         second->compressor_telemetry_sidecar()->channel_lanes) {
        for (const auto& bucket : lane.buckets) {
            QCOMPARE(bucket.realization_id, secondId);
        }
    }
}

void AuditionSourceSelectorTest::loopRegionIntentPreservedAcrossAuditionRebinds()
{
    auto service = std::make_unique<FakePlaybackService>();
    auto* observed = service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    install_pcm_handler(transport, observed);

    app::AuditionSourceSelector selector{&transport};
    app::AuditionRegionViewModel regionModel{&transport};

    const auto frameCount = *core::FrameCount::create(200).value();
    const auto sampleRate = *core::SampleRate::create(48000).value();
    regionModel.source_committed(frameCount, sampleRate);
    regionModel.set_waveform_ready(true);

    selector.set_source_loop_provider([&regionModel] {
        return regionModel.loop_enabled() ? regionModel.region() : std::nullopt;
    });

    QVERIFY(selector.set_prepared_realization(realization(0, 200)));
    QVERIFY(selector.set_processed_realization(realization(0, 200)));

    // 1. PREPARED active, set region [10, 50], enable Loop Region
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    const auto testRange = *core::FrameRange::create(core::FrameIndex{10}, core::FrameIndex{50}).value();
    QVERIFY(regionModel.set_region(testRange));
    QVERIFY(regionModel.set_loop_enabled(true));
    QVERIFY(regionModel.loop_enabled());
    QVERIFY(observed->loop.has_value());
    QCOMPARE(observed->loop->begin().value(), std::int64_t{10});
    QCOMPARE(observed->loop->end().value(), std::int64_t{50});

    // Start playback
    transport.playOrResume();
    observed->position = core::FrameIndex{25};
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);

    // 2. Switch PREPARED -> PROCESSED while PLAYING with armed loop
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
    QVERIFY(regionModel.loop_enabled());
    QVERIFY(observed->loop.has_value());
    QCOMPARE(observed->loop->begin().value(), std::int64_t{10});
    QCOMPARE(observed->loop->end().value(), std::int64_t{50});
    QCOMPARE(observed->position.value(), std::int64_t{25});
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);

    // 3. Switch PROCESSED -> PREPARED while PLAYING with armed loop
    observed->position = core::FrameIndex{35};
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PREPARED});
    QVERIFY(regionModel.loop_enabled());
    QVERIFY(observed->loop.has_value());
    QCOMPARE(observed->loop->begin().value(), std::int64_t{10});
    QCOMPARE(observed->loop->end().value(), std::int64_t{50});
    QCOMPARE(observed->position.value(), std::int64_t{35});
    QCOMPARE(observed->state, core::PlaybackState::PLAYING);

    // 4. Active PROCESSED replacement (e.g. EQ parameter edit / A-B toggle)
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    observed->position = core::FrameIndex{40};

    // Replace Processed realization multiple times
    for (int rep = 1; rep <= 3; ++rep) {
        QVERIFY(selector.set_processed_realization(realization(0, 200)));
        QCOMPARE(selector.active_target(), std::optional{app::AuditionTarget::PROCESSED});
        QVERIFY(regionModel.loop_enabled());
        QVERIFY(observed->loop.has_value());
        QCOMPARE(observed->loop->begin().value(), std::int64_t{10});
        QCOMPARE(observed->loop->end().value(), std::int64_t{50});
        QCOMPARE(observed->position.value(), std::int64_t{40});
        QCOMPARE(observed->state, core::PlaybackState::PLAYING);
    }

    // 5. Explicit user disable Loop Region disarms intent and backend loop
    QVERIFY(regionModel.set_loop_enabled(false));
    QVERIFY(!regionModel.loop_enabled());
    QVERIFY(!observed->loop.has_value());

    // Switch targets after user disable -> loop remains disarmed
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QVERIFY(!regionModel.loop_enabled());
    QVERIFY(!observed->loop.has_value());

    // 6. User re-enables loop and clears region
    QVERIFY(regionModel.set_loop_enabled(true));
    QVERIFY(regionModel.loop_enabled());
    QVERIFY(observed->loop.has_value());

    QVERIFY(regionModel.clear_region());
    QVERIFY(!regionModel.has_region());
    QVERIFY(!regionModel.loop_enabled());
    QVERIFY(!observed->loop.has_value());
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::AuditionSourceSelectorTest)

#include "test_audition_source_selector.moc"
