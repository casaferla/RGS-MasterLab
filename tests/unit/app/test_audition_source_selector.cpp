#include "audition_source_selector.hpp"
#include "gold_selection_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include "source_selection_view_model.hpp"

#include "../audio/wav_test_support.hpp"
#include "../platform/fake_playback_service.hpp"

#include <rgsml/audio/audio_buffer.hpp>
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

void install_pcm_handler(
    app::PlaybackTransportViewModel& transport,
    FakePlaybackService* service)
{
    transport.set_pcm_prepare_handler(
        [service](audio::AudioBufferView view) {
            service->state = core::PlaybackState::STOPPED;
            service->position = view.absolute_start_frame();
            service->duration = *core::FrameCount::create(
                view.absolute_end_frame().value()).value();
            service->loop.reset();
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
    QCOMPARE(observed->state, core::PlaybackState::STOPPED);

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

    // Case 6: Fail-closed path on invalid replacement range
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    observed->position = core::FrameIndex{150};

    // Realization range [0..100) does not contain cue 150
    QVERIFY(!selector.set_processed_realization(realization(0, 100)));
    QVERIFY(!selector.active_target());
    QCOMPARE(observed->state, core::PlaybackState::NO_SOURCE);
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
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED)); // store cue 100
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{100});

    // Replay PREPARED at EOF -> should canonicalize cue 100 to 0 and succeed
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{0});
    QCOMPARE(observed->position.value(), std::int64_t{0});

    // 2. PROCESSED replay at EOF
    observed->position = core::FrameIndex{100}; // at EOF
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED)); // store cue 100
    QCOMPARE(selector.source_derived_cue().value(), std::int64_t{100});

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
    QVERIFY(!selector.active_target());
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::AuditionSourceSelectorTest)

#include "test_audition_source_selector.moc"
