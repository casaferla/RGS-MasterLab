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
    QTemporaryDir& directory, const QString& name, std::int64_t seed)
{
    using namespace wav_support;
    std::vector<std::int64_t> samples(64U);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = (static_cast<std::int64_t>(index) + seed) % 100;
    }
    const auto bytes = make_wav(
        1U, 16U, 1U, 48'000U, pcm_payload(samples, 16U));
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
    void processedTelemetryBindsAcceptedRealizationIdentity();
};

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
