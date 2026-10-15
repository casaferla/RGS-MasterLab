#include "true_peak_static_binding.hpp"
#include "true_peak_live_input_binding.hpp"
#include "true_peak_live_coordinator.hpp"
#include "true_peak_static_coordinator.hpp"
#include "render_test_support.hpp"

#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QCoreApplication>
#include <QTest>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

namespace rgsml::tests {
namespace {

using namespace render_support;
using app::BoundStaticTruePeak;
using app::TruePeakExpectedSource;

struct RenderFixture final {
    std::shared_ptr<const render::RenderResult> result;
    TruePeakExpectedSource expected;
};

[[nodiscard]] core::Result<RenderFixture> make_render(
    std::int64_t count, core::FrameRange window,
    std::int64_t rate = 48000,
    audio::ChannelLayout layout = audio::ChannelLayout::STEREO_LR,
    double leftPulse = 0.75, std::int64_t leftPulseFrame = 1)
{
    const auto fmt = format(layout, rate);
    const auto frames = frame_count(count);
    auto source = audio::AudioBuffer::create(
        fmt, audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, frames);
    if (!source) return core::Result<RenderFixture>::failure(*source.error());
    {
        auto left = source.value()->mutable_view().channel(0);
        if (!left) return core::Result<RenderFixture>::failure(*left.error());
        for (std::int64_t i = 0; i < count; ++i) {
            (*left.value())[static_cast<std::size_t>(i)] =
                i == leftPulseFrame ? leftPulse : 0.0;
        }
        if (layout == audio::ChannelLayout::STEREO_LR) {
            auto right = source.value()->mutable_view().channel(1);
            if (!right) return core::Result<RenderFixture>::failure(*right.error());
            for (std::int64_t i = 0; i < count; ++i) {
                (*right.value())[static_cast<std::size_t>(i)] =
                    i == leftPulseFrame + 1 ? -0.5 : 0.0;
            }
        }
    }
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    if (!registry) return core::Result<RenderFixture>::failure(*registry.error());
    auto chain = dsp::ProcessingChain::create(
        *registry.value(),
        dsp::ProcessingChainContext{dsp::ProcessingStage::MASTER,
                                    dsp::ChainSegment::MANUAL});
    if (!chain) return core::Result<RenderFixture>::failure(*chain.error());
    auto request = render::RenderRequest::create(
        source.value()->view(), window, *chain.value(),
        std::vector<dsp::ModuleExecutionBinding>{},
        frame_count(16));
    if (!request) return core::Result<RenderFixture>::failure(*request.error());
    auto rendered = render::render_preview(*request.value(), *registry.value());
    if (!rendered) return core::Result<RenderFixture>::failure(*rendered.error());
    return core::Result<RenderFixture>::success(RenderFixture{
        std::make_shared<const render::RenderResult>(std::move(*rendered.value())),
        TruePeakExpectedSource{frames, *core::SampleRate::create(rate).value(), layout}
    });
}

class TruePeakStaticBindingTest final : public QObject {
    Q_OBJECT
private slots:
    void fullSourceProvenanceAndIdentity();
    void partialRenderFailsClosed();
    void sourceMismatchAndMissingIdentityFailClosed();
    void monoSilenceAndUnqualifiedStatus();
    void asynchronousSourceAndRealizationStaleness();
    void consumedProcessedLiveInputBindingRejectsGapsAndMismatchedOwners();
    void liveCoordinatorMeasuresSpikeAndInvalidatesTransitions();
    void consumedProcessedSeekRequiresProvenSourceMapping();
    void liveCoordinatorSeekReacquiresOnlyCompleteWindows();
    void postLoopRecoveryBindsOnlyFreshExactConsumedSuffix();
};

void TruePeakStaticBindingTest::fullSourceProvenanceAndIdentity()
{
    auto fixture = make_render(4, frame_range(0,4));
    QVERIFY(fixture);
    const auto source = fixture.value()->result;
    auto got = app::measure_accepted_processed_true_peak(
        source, core::RealizationId{27}, fixture.value()->expected);
    QVERIFY(got);
    const auto& b = *got.value();
    QVERIFY(b.owner == source);
    QCOMPARE(b.realizationId.value, std::uint64_t{27});
    QVERIFY(b.coveredSourceFrames == frame_range(0,4));
    QCOMPARE(b.chainRevision, source->chain_revision());
    QCOMPARE(b.measurement.qualification_status(),
             analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
    auto numeric = analysis::measure_true_peak_full_resource(source->buffer());
    QVERIFY(numeric);
    QCOMPARE(b.measurement.global_linear_peak(), numeric.value()->global_linear_peak());
    QVERIFY(b.measurement.global_db_tp() == numeric.value()->global_db_tp());
    auto playback = core::PlaybackSnapshot{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{2};
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{27}, std::nullopt};
    QVERIFY(b.matches_audible(playback));
    playback.audibleRealization.realizationId = core::RealizationId{26};
    QVERIFY(!b.matches_audible(playback));
    playback.audibleRealization.realizationId = core::RealizationId{27};
    playback.audibleRealization.phase = core::AudibleHandoffPhase::OLD;
    QVERIFY(!b.matches_audible(playback));
    playback.audibleRealization.phase = core::AudibleHandoffPhase::TRANSITION;
    QVERIFY(!b.matches_audible(playback));
    playback.audibleRealization.phase = core::AudibleHandoffPhase::NEW;
    playback.state = core::PlaybackState::PAUSED;
    QVERIFY(!b.matches_audible(playback));
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{4};
    QVERIFY(!b.matches_audible(playback));
}

void TruePeakStaticBindingTest::partialRenderFailsClosed()
{
    auto partial = make_render(4, frame_range(1,3));
    QVERIFY(partial);
    auto res = app::measure_accepted_processed_true_peak(
        partial.value()->result, core::RealizationId{28}, partial.value()->expected);
    QVERIFY(!res);
    QCOMPARE(res.error()->code(), core::ErrorCode::InvalidState);
}

void TruePeakStaticBindingTest::sourceMismatchAndMissingIdentityFailClosed()
{
    auto full = make_render(4, frame_range(0,4));
    QVERIFY(full);
    auto expected = full.value()->expected;
    auto missing = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{0}, expected);
    QVERIFY(!missing);
    auto nullRender = app::measure_accepted_processed_true_peak(
        nullptr, core::RealizationId{7}, expected);
    QVERIFY(!nullRender);
    expected.frameCount = frame_count(5);
    auto badCount = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{7}, expected);
    QVERIFY(!badCount);
    expected = full.value()->expected;
    expected.sampleRate = *core::SampleRate::create(44100).value();
    auto badRate = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{7}, expected);
    QVERIFY(!badRate);
    expected = full.value()->expected;
    expected.layout = audio::ChannelLayout::MONO_C;
    auto badLayout = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{7}, expected);
    QVERIFY(!badLayout);
}

void TruePeakStaticBindingTest::monoSilenceAndUnqualifiedStatus()
{
    auto fixture = make_render(4, frame_range(0,4), 96000,
                               audio::ChannelLayout::MONO_C);
    QVERIFY(fixture);
    auto result = app::measure_accepted_processed_true_peak(
        fixture.value()->result, core::RealizationId{29}, fixture.value()->expected);
    QVERIFY(result);
    QCOMPARE(result.value()->measurement.channel_count(), std::size_t{1});
    QCOMPARE(result.value()->measurement.qualification_status(),
             analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
}

void TruePeakStaticBindingTest::asynchronousSourceAndRealizationStaleness()
{
    auto full = make_render(4, frame_range(0,4));
    auto partial = make_render(4, frame_range(1,3));
    QVERIFY(full && partial);
    auto one = core::ResourceReference::create("test", "source-a", true, false);
    auto two = core::ResourceReference::create("test", "source-b", true, false);
    QVERIFY(one && two);
    QVERIFY(QCoreApplication::instance() != nullptr);
    app::TruePeakStaticCoordinator coordinator;
    coordinator.source_committed(*one.value(), full.value()->expected);
    QCOMPARE(coordinator.state(), app::TruePeakStaticCoordinator::State::NO_PROCESSED);
    coordinator.processed_changed(full.value()->result, core::RealizationId{31});
    QCOMPARE(coordinator.state(), app::TruePeakStaticCoordinator::State::MEASURING);
    QTRY_VERIFY(coordinator.state() == app::TruePeakStaticCoordinator::State::READY);
    QVERIFY(coordinator.ready());
    QCOMPARE(coordinator.ready()->realizationId.value, std::uint64_t{31});
    QCOMPARE(coordinator.ready()->measurement.qualification_status(),
             analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
    QCOMPARE(coordinator.measured_channel_count(), 2);
    QVERIFY(coordinator.full_left_text().endsWith(QStringLiteral(" dBTP")));
    QVERIFY(coordinator.full_right_text().endsWith(QStringLiteral(" dBTP")));
    QVERIFY(!coordinator.full_left_over_zero());
    QVERIFY(!coordinator.full_right_over_zero());
    QVERIFY(coordinator.full_left_meter_level() > 0.0);
    QVERIFY(coordinator.full_right_meter_level() > 0.0);
    QVERIFY(coordinator.full_left_meter_level() > coordinator.full_right_meter_level());

    // Old source result is invalidated synchronously, even if its worker is busy.
    coordinator.processed_changed(full.value()->result, core::RealizationId{32});
    coordinator.invalidate_source();
    QVERIFY(coordinator.ready() == nullptr);
    QCOMPARE(coordinator.measured_channel_count(), 0);
    QCOMPARE(coordinator.full_left_text(), QStringLiteral("—"));
    QCOMPARE(coordinator.full_right_text(), QStringLiteral("—"));
    QCOMPARE(coordinator.full_left_meter_level(), 0.0);
    QCOMPARE(coordinator.full_right_meter_level(), 0.0);
    QVERIFY(!coordinator.full_left_over_zero());
    QVERIFY(!coordinator.full_right_over_zero());
    QCOMPARE(coordinator.state(), app::TruePeakStaticCoordinator::State::NO_SOURCE);
    coordinator.source_committed(*two.value(), full.value()->expected);
    QVERIFY(coordinator.source_reference()->same_resource_identity(*two.value()));
    coordinator.processed_changed(partial.value()->result, core::RealizationId{33});
    QTRY_VERIFY(coordinator.state() == app::TruePeakStaticCoordinator::State::UNAVAILABLE);
    QVERIFY(coordinator.ready() == nullptr);

    coordinator.processed_changed(full.value()->result, core::RealizationId{34});
    QTRY_VERIFY(coordinator.state() == app::TruePeakStaticCoordinator::State::READY);
    QVERIFY(coordinator.ready());
    QCOMPARE(coordinator.ready()->realizationId.value, std::uint64_t{34});
    coordinator.processed_changed(full.value()->result, core::RealizationId{34});
    QCOMPARE(coordinator.state(), app::TruePeakStaticCoordinator::State::READY);
    QCOMPARE(coordinator.ready()->realizationId.value, std::uint64_t{34});

    // Real >0 dBTP is a digital-zero diagnostic, not a project Ceiling.
    auto overZero = make_render(4, frame_range(0,4), 48000,
                                audio::ChannelLayout::STEREO_LR, 1.30);
    QVERIFY(overZero);
    coordinator.processed_changed(overZero.value()->result,
                                  core::RealizationId{35});
    QTRY_VERIFY(coordinator.state()
                == app::TruePeakStaticCoordinator::State::READY);
    QVERIFY(coordinator.full_left_over_zero());
    QVERIFY(!coordinator.full_right_over_zero());
    QVERIFY(coordinator.full_left_meter_level()
            > coordinator.full_right_meter_level());
}

void TruePeakStaticBindingTest::consumedProcessedLiveInputBindingRejectsGapsAndMismatchedOwners()
{
    using platform::windows::WindowsConsumedSliceKind;
    auto fixture = make_render(4800, frame_range(0,4800));
    QVERIFY(fixture);
    auto owner = fixture.value()->result;
    auto playback = core::PlaybackSnapshot{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{500};
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{512}, std::nullopt};
    auto prefix = platform::windows::WindowsConsumedOutputPrefix{};
    prefix.generation = 6;
    prefix.reason = platform::windows::WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
    prefix.processedWatermarkExclusive = 500;
    prefix.count = 1;
    prefix.partitionRepresented = true;
    prefix.slices[0].generation = 6;
    prefix.slices[0].outputBegin = 0;
    prefix.slices[0].outputEnd = 500;
    prefix.slices[0].sourceBegin = 0;
    prefix.slices[0].sourceEnd = 500;
    prefix.slices[0].kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    prefix.slices[0].realizationId = core::RealizationId{512};
    auto bound = app::bind_processed_live_input(
        owner, core::RealizationId{512}, fixture.value()->expected,
        playback, prefix);
    QVERIFY(bound);
    QVERIFY(bound.value()->owner == owner);
    QCOMPARE(bound.value()->chainRevision, owner->chain_revision());
    QCOMPARE(bound.value()->consumedSourceEnd, std::int64_t{500});
    QVERIFY(!prefix.certifiedForLiveTruePeak); // input is NOT live-qualified

    prefix.slices[0].kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{512},
        fixture.value()->expected, playback, prefix));
    prefix.slices[0].kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    prefix.slices[0].realizationId = core::RealizationId{513};
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{512},
        fixture.value()->expected, playback, prefix));
    prefix.slices[0].realizationId = core::RealizationId{512};
    prefix.slices[0].sourceBegin = 100;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{512},
        fixture.value()->expected, playback, prefix));
    prefix.slices[0].sourceBegin = 0;
    playback.audibleRealization.phase = core::AudibleHandoffPhase::TRANSITION;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{512},
        fixture.value()->expected, playback, prefix));
    playback.audibleRealization.phase = core::AudibleHandoffPhase::NEW;
    prefix.partitionRepresented = false;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{512},
        fixture.value()->expected, playback, prefix));
    prefix.partitionRepresented = true;
    prefix.certifiedForLiveTruePeak = true; // impossible premature certification
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{512},
        fixture.value()->expected, playback, prefix));
}


void TruePeakStaticBindingTest::liveCoordinatorMeasuresSpikeAndInvalidatesTransitions()
{
    auto fixture = make_render(4800, frame_range(0,4800));
    QVERIFY(fixture);
    const auto owner = fixture.value()->result;
    auto playback = core::PlaybackSnapshot{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{2399};
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{601}, std::nullopt};
    auto prefix = platform::windows::WindowsConsumedOutputPrefix{};
    prefix.generation = 18;
    prefix.reason = platform::windows::WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
    prefix.processedWatermarkExclusive = 2400;
    prefix.count = 1;
    prefix.partitionRepresented = true;
    prefix.slices[0].generation = 18;
    prefix.slices[0].outputBegin = 0;
    prefix.slices[0].outputEnd = 2400;
    prefix.slices[0].sourceBegin = 0;
    prefix.slices[0].sourceEnd = 2400;
    prefix.slices[0].realizationId = core::RealizationId{601};
    prefix.slices[0].kind =
        platform::windows::WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    bool selectedProcessed = true;

    app::TruePeakLiveCoordinator coordinator{
        [&] {
            return core::Result<platform::windows::WindowsConsumedOutputPrefix>::success(prefix);
        },
        [&] {
            return core::Result<core::PlaybackSnapshot>::success(playback);
        },
        [&]() -> std::optional<app::TruePeakLiveSource> {
            if (!selectedProcessed) return std::nullopt;
            return app::TruePeakLiveSource{
                owner, core::RealizationId{601}, fixture.value()->expected};
        }
    };

    // True spike is not published until one full 100ms sink-consumed span.
    QTRY_VERIFY_WITH_TIMEOUT(coordinator.monitoring_attempted(), 3500);
    QTest::qWait(110);
    QVERIFY(!coordinator.recent_available());
    QVERIFY(!coordinator.hold_available());
    prefix.processedWatermarkExclusive = 4800;
    prefix.slices[0].outputEnd = 4800;
    prefix.slices[0].sourceEnd = 4800;
    playback.position = core::FrameIndex{4799};
    QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
    QVERIFY(coordinator.monitoring_attempted());
    QVERIFY(!coordinator.hold_available());
    QVERIFY(coordinator.left_level() > 0.0);
    QVERIFY(coordinator.right_level() > 0.0);
    QVERIFY(coordinator.peak_text().endsWith(QStringLiteral(" dBTP")));

    // PAUSE with the same source and chronology freezes genuine LIVE.
    playback.state = core::PlaybackState::PAUSED;
    QTest::qWait(110);
    QVERIFY(coordinator.recent_available());
    // Paused seek invalidates the old timeline instead of freezing it.
    ++playback.seekSerial;
    QTRY_VERIFY_WITH_TIMEOUT(!coordinator.recent_available(), 3500);
    playback.state = core::PlaybackState::PLAYING;
    // Audible transition must not inherit even a previously measured peak.
    playback.audibleRealization.phase = core::AudibleHandoffPhase::TRANSITION;
    QTRY_VERIFY_WITH_TIMEOUT(!coordinator.recent_available(), 4500);
    QVERIFY(coordinator.monitoring_attempted());
    selectedProcessed = false;
    coordinator.invalidate(); // source-change signal path is synchronous
    QVERIFY(!coordinator.monitoring_attempted());
    QTRY_VERIFY_WITH_TIMEOUT(!coordinator.monitoring_attempted(), 4500);
    QVERIFY(!coordinator.recent_available());
    QCOMPARE(coordinator.channel_count(), 0);
}


void TruePeakStaticBindingTest::consumedProcessedSeekRequiresProvenSourceMapping()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    auto fixture = make_render(16000, frame_range(0,16000));
    QVERIFY(fixture);
    auto owner = fixture.value()->result;
    core::PlaybackSnapshot playback{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{5500};
    playback.seekSerial = 1;
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{813}, std::nullopt};
    platform::windows::WindowsConsumedOutputPrefix prefix{};
    prefix.generation = 20;
    prefix.reason = WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
    prefix.partitionRepresented = true;
    prefix.count = 1;
    prefix.processedWatermarkExclusive = 300;
    auto& run = prefix.slices[0];
    run.generation = prefix.generation;
    run.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    run.realizationId = core::RealizationId{813};
    run.outputBegin = 0;
    run.outputEnd = 300;
    run.sourceBegin = 5303;
    run.sourceEnd = 5603;
    auto bound = app::bind_processed_live_input(
        owner, core::RealizationId{813}, fixture.value()->expected,
        playback, prefix);
    QVERIFY(bound);
    QCOMPARE(bound.value()->consumedSourceBegin, std::int64_t{5303});
    QCOMPARE(bound.value()->consumedSourceEnd, std::int64_t{5603});
    QCOMPARE(bound.value()->consumedOutputEnd, std::int64_t{300});
    // Reject every possibility of reading outside canonical owner support.
    run.sourceEnd = 5700; // non-1:1 offset mapping
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    run.sourceEnd = 5603;
    run.sourceBegin = -1;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    run.sourceBegin = 5303;
    run.sourceEnd = 16001;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    run.sourceEnd = 5603;
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_LOOP;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_HANDOFF;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    prefix.reason = WindowsConsumedGapReason::STALE_GENERATION;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    prefix.reason = WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
    run.generation = 19;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
    run.generation = 20;
    QVERIFY(app::bind_processed_live_input(owner, core::RealizationId{813},
        fixture.value()->expected, playback, prefix));
}

void TruePeakStaticBindingTest::liveCoordinatorSeekReacquiresOnlyCompleteWindows()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    // A pulse just AFTER the seek, never included by reading the future.
    constexpr std::int64_t start = 4801; // intentionally not 10-ms aligned
    auto fixture = make_render(
        15000, frame_range(0,15000), 48000,
        audio::ChannelLayout::STEREO_LR, 0.85, start + 480 + 2);
    QVERIFY(fixture);
    const auto owner = fixture.value()->result;
    core::PlaybackSnapshot playback{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{start};
    playback.seekSerial = 1;
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{814}, std::nullopt};
    platform::windows::WindowsConsumedOutputPrefix prefix{};
    prefix.generation = 31;
    prefix.reason = WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
    prefix.partitionRepresented = true;
    prefix.count = 1;
    auto& run = prefix.slices[0];
    run.generation = 31;
    run.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    run.realizationId = core::RealizationId{814};
    run.outputBegin = 0;
    run.sourceBegin = start;
    prefix.processedWatermarkExclusive = 4800;
    run.outputEnd = 4800;
    run.sourceEnd = start + 4800;

    app::TruePeakLiveCoordinator live{
        [&] {
            return core::Result<platform::windows::WindowsConsumedOutputPrefix>::success(prefix);
        },
        [&] { return core::Result<core::PlaybackSnapshot>::success(playback); },
        [&]() -> std::optional<app::TruePeakLiveSource> {
            return app::TruePeakLiveSource{
                owner, core::RealizationId{814}, fixture.value()->expected};
        }
    };
    QTRY_VERIFY_WITH_TIMEOUT(live.monitoring_attempted(), 3500);
    QTest::qWait(120);
    // An unaligned first fragment lacks the eleven FIR history samples.
    QVERIFY(!live.recent_available());
    QVERIFY(!live.hold_available());
    // First fully valid ten 10-ms buckets end at absolute frame 10080.
    prefix.processedWatermarkExclusive = 10080 - start;
    run.outputEnd = prefix.processedWatermarkExclusive;
    run.sourceEnd = 10080;
    playback.position = core::FrameIndex{10079};
    QTRY_VERIFY_WITH_TIMEOUT(live.recent_available(), 4500);
    QVERIFY(live.left_level() > 0.0);
    QVERIFY(live.right_level() > 0.0);
    QVERIFY(!live.hold_available());
    // A second seek cannot inherit the old impulse, peak, or FIR history.
    constexpr std::int64_t secondStart = 9603;
    ++playback.seekSerial;
    ++prefix.generation;
    run.generation = prefix.generation;
    run.sourceBegin = secondStart;
    run.sourceEnd = secondStart + 2400;
    prefix.processedWatermarkExclusive = 2400;
    run.outputEnd = 2400;
    playback.position = core::FrameIndex{secondStart + 2399};
    QTRY_VERIFY_WITH_TIMEOUT(!live.recent_available(), 3500);
    QTest::qWait(110);
    QVERIFY(!live.recent_available());
    // With complete new source-domain coverage, genuine zero is valid.
    run.sourceEnd = 14880;
    prefix.processedWatermarkExclusive = 14880 - secondStart;
    run.outputEnd = prefix.processedWatermarkExclusive;
    playback.position = core::FrameIndex{14879};
    QTRY_VERIFY_WITH_TIMEOUT(live.recent_available(), 4500);
    QCOMPARE(live.peak_text(), QStringLiteral("−∞ dBTP"));
    QVERIFY(!live.hold_available());
}


void TruePeakStaticBindingTest::postLoopRecoveryBindsOnlyFreshExactConsumedSuffix()
{
    using platform::windows::WindowsConsumedOutputPrefix;
    using platform::windows::WindowsConsumedSliceKind;
    using platform::windows::WindowsConsumedGapReason;
    auto fixture = make_render(10000, frame_range(0,10000));
    QVERIFY(fixture);
    const auto owner = fixture.value()->result;
    core::PlaybackSnapshot playback{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{3499};
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{971}, std::nullopt};
    WindowsConsumedOutputPrefix prefix{};
    prefix.generation = 91;
    prefix.partitionRepresented = true;
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_LOOP;
    prefix.processedWatermarkExclusive = 900;
    prefix.count = 2;
    auto& old = prefix.slices[0];
    old.generation = 91;
    old.outputBegin = 0;
    old.outputEnd = 400;
    old.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
    old.reason = WindowsConsumedGapReason::UNPROVEN_LOOP;
    auto& fresh = prefix.slices[1];
    fresh.generation = 91;
    fresh.outputBegin = 400;
    fresh.outputEnd = 900;
    fresh.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    fresh.sourceBegin = 3000;
    fresh.sourceEnd = 3500;
    fresh.realizationId = core::RealizationId{971};

    auto bound = app::bind_processed_live_input(owner,
        core::RealizationId{971}, fixture.value()->expected, playback, prefix);
    QVERIFY(bound);
    QCOMPARE(bound.value()->consumedSourceBegin, std::int64_t{3000});
    QCOMPARE(bound.value()->consumedSourceEnd, std::int64_t{3500});
    // Last, fresh direct PCM is independently source-owned. Old loop PCM
    // remains explicitly GAP rather than becoming fake valid history.
    fresh.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    fresh.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    old.outputEnd = 401; // hole/overlap
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    old.outputEnd = 400;
    fresh.sourceEnd = 3501; // source/output not exact 1:1
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    fresh.sourceEnd = 3500;
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_HANDOFF;
    // A LOOP gap cannot be silently reclassified as handoff evidence.
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    // A genuine source-unproven handoff followed by exact pure-NEW source
    // may recover, but no mixed/OLD sample is promoted.
    old.reason = WindowsConsumedGapReason::UNPROVEN_HANDOFF;
    QVERIFY(app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    old.reason = WindowsConsumedGapReason::UNPROVEN_LOOP;
    prefix.reason = WindowsConsumedGapReason::ACCEPTED_LEDGER_OVERFLOW;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_LOOP;
    prefix.slices[0].kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    QVERIFY(!app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
    prefix.slices[0].kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
    QVERIFY(app::bind_processed_live_input(owner, core::RealizationId{971},
        fixture.value()->expected, playback, prefix));
}

} // namespace
} // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::TruePeakStaticBindingTest)
#include "test_true_peak_static_binding.moc"
