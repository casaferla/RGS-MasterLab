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
    void liveCoordinatorResyncsConsumedSuffixAfterGuiPollingStall();
    void postLoopRecoveryBindsOnlyFreshExactConsumedSuffix();
    void c2D14LoopTraversalMustMatchConsumedWrapAndOwner();
    void c2D14LiveCoordinatorResetsOneSecondHoldAcrossWrap();
    void c2D15SrcBindingRequiresOwnerAndConservativeConsumedProof();
    void c2D15SrcLiveCoordinatorWaitsForPreSrc100msAndClearsOnSeek();
    void c2D15SrcLiveCoordinatorOneSecondHoldRequiresCompleteConsumedHistory();
    void c2D16SrcLongGuiStallCannotInventOneSecondHold();
    void c2D17SrcSeekClearsPreviousMeterAndRebuildsNewOwnerInterval();
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


void TruePeakStaticBindingTest::liveCoordinatorResyncsConsumedSuffixAfterGuiPollingStall()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    auto fixture = make_render(
        144000, frame_range(0,144000), 48000,
        audio::ChannelLayout::STEREO_LR, 0.9, 50);
    QVERIFY(fixture);
    core::PlaybackSnapshot playback{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{4799};
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{2201}, std::nullopt};
    platform::windows::WindowsConsumedOutputPrefix prefix{};
    prefix.generation = 71;
    prefix.reason = WindowsConsumedGapReason::MISSING_CHRONOLOGICAL_LEDGER;
    prefix.partitionRepresented = true;
    prefix.count = 1;
    prefix.processedWatermarkExclusive = 4800;
    auto& exact = prefix.slices[0];
    exact.generation = 71;
    exact.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    exact.realizationId = core::RealizationId{2201};
    exact.outputBegin = 0;
    exact.outputEnd = 4800;
    exact.sourceBegin = 0;
    exact.sourceEnd = 4800;
    app::TruePeakLiveCoordinator live{
        [&] { return core::Result<platform::windows::WindowsConsumedOutputPrefix>::success(prefix); },
        [&] { return core::Result<core::PlaybackSnapshot>::success(playback); },
        [&]() -> std::optional<app::TruePeakLiveSource> {
            return app::TruePeakLiveSource{
                fixture.value()->result, core::RealizationId{2201},
                fixture.value()->expected};
        }
    };
    QTRY_VERIFY_WITH_TIMEOUT(live.recent_available(), 4500);
    QVERIFY(live.peak_text() != QStringLiteral("−∞ dBTP"));
    QVERIFY(!live.hold_available());
    // Simulated GUI polling stall of 1.9 s. The prefix remains ONE
    // contiguous, identical-owner, source-mapped consumed interval.
    // The earlier pulse must NOT leak into the new 100-ms reading.
    prefix.processedWatermarkExclusive = 96000;
    exact.outputEnd = 96000;
    exact.sourceEnd = 96000;
    playback.position = core::FrameIndex{95999};
    QTRY_COMPARE_WITH_TIMEOUT(live.peak_text(), QStringLiteral("−∞ dBTP"), 6000);
    QVERIFY(live.recent_available());
    QVERIFY(!live.hold_available()); // no invented cross-stall 1-s window
    prefix.processedWatermarkExclusive = 115200;
    exact.outputEnd = 115200;
    exact.sourceEnd = 115200;
    playback.position = core::FrameIndex{115199};
    QTest::qWait(160);
    QVERIFY(live.recent_available());
    QVERIFY(!live.hold_available());
    prefix.processedWatermarkExclusive = 122400;
    exact.outputEnd = 122400;
    exact.sourceEnd = 122400;
    playback.position = core::FrameIndex{122399};
    QTRY_VERIFY_WITH_TIMEOUT(live.hold_available(), 5000);
    QCOMPARE(live.peak_text(), QStringLiteral("−∞ dBTP"));
}


void TruePeakStaticBindingTest::c2D14LoopTraversalMustMatchConsumedWrapAndOwner()
{
    using platform::windows::WindowsConsumedOutputPrefix;
    using platform::windows::WindowsConsumedSliceKind;
    using platform::windows::WindowsConsumedGapReason;
    auto fixture = make_render(48000, frame_range(0,48000));
    QVERIFY(fixture);
    auto owner = fixture.value()->result;
    core::PlaybackSnapshot playback{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{11499};
    playback.loop = *core::FrameRange::create(
        core::FrameIndex{4800}, core::FrameIndex{14400}).value();
    playback.loopWrapCount = 2;
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{2301}, std::nullopt};
    WindowsConsumedOutputPrefix prefix{};
    prefix.generation = 51;
    prefix.partitionRepresented = true;
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_LOOP;
    prefix.processedWatermarkExclusive = 28000;
    prefix.count = 3;
    auto& first = prefix.slices[0];
    first.generation = 51;
    first.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    first.realizationId = core::RealizationId{2301};
    first.loopTraversalProven = true;
    first.loopWrapCount = 0;
    first.sourceBegin = 0;
    first.sourceEnd = 14400;
    first.outputBegin = 0;
    first.outputEnd = 14400;
    auto& second = prefix.slices[1];
    second = first;
    second.loopWrapCount = 1;
    second.outputBegin = 14400;
    second.outputEnd = 24000;
    second.sourceBegin = 4800;
    second.sourceEnd = 14400;
    auto& third = prefix.slices[2];
    third = second;
    third.loopWrapCount = 2;
    third.outputBegin = 24000;
    third.outputEnd = 28000;
    third.sourceBegin = 4800;
    third.sourceEnd = 8800;
    auto bound = app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix);
    QVERIFY(bound);
    QCOMPARE(bound.value()->consumedSourceBegin, std::int64_t{4800});
    QCOMPARE(bound.value()->consumedSourceEnd, std::int64_t{8800});
    playback.loopWrapCount = 3;
    QVERIFY(!app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix));
    playback.loopWrapCount = 2;
    third.loopTraversalProven = false; // lost provenance of final traversal
    // A previous proven loop is not enough if current slice is unproven.
    // No fabricated wrap identity or future source may qualify.
    third.realizationId = core::RealizationId{9999};
    QVERIFY(!app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix));
    third.realizationId = core::RealizationId{2301};
    third.loopWrapCount = 2;
    third.sourceEnd = 48001;
    QVERIFY(!app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix));
    // A pruned prefix cannot invent the missing old traversals, but a
    // fully consumed, exact retained suffix can independently bind.
    third.sourceEnd = 8800;
    third.loopTraversalProven = true;
    prefix.slices[0] = third;
    prefix.count = 1;
    prefix.retainedOutputBegin = 24000;
    auto retained = app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix);
    QVERIFY(retained);
    QCOMPARE(retained.value()->consumedSourceBegin, std::int64_t{4800});
    prefix.retainedOutputBegin = 23999; // unrepresented start must fail
    QVERIFY(!app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix));
    // After the old mixed handoff GAP was safely PRUNED from the consumed
    // ledger, a new owner-stamped, pure NEW candidate keeps its independent
    // output-time attestation through later loop traversals.
    prefix.retainedOutputBegin = 24000;
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_HANDOFF;
    prefix.slices[0].handoffPureNewProven = true;
    QVERIFY(app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix));
    prefix.slices[0].handoffPureNewProven = false;
    QVERIFY(!app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix));
    prefix.slices[0].handoffPureNewProven = true;
    playback.loopWrapCount = 3; // cannot present a previous traversal
    QVERIFY(!app::bind_processed_live_input(
        owner, core::RealizationId{2301}, fixture.value()->expected, playback, prefix));
}


void TruePeakStaticBindingTest::c2D14LiveCoordinatorResetsOneSecondHoldAcrossWrap()
{
    using platform::windows::WindowsConsumedOutputPrefix;
    using platform::windows::WindowsConsumedSliceKind;
    using platform::windows::WindowsConsumedGapReason;
    auto fixture = make_render(
        100000, frame_range(0,100000), 48000,
        audio::ChannelLayout::STEREO_LR, 0.92, 40000);
    QVERIFY(fixture);
    core::PlaybackSnapshot playback{};
    playback.state = core::PlaybackState::PLAYING;
    playback.loop = *core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{60000}).value();
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{2304}, std::nullopt};
    WindowsConsumedOutputPrefix prefix{};
    prefix.generation = 53;
    prefix.reason = WindowsConsumedGapReason::UNPROVEN_LOOP;
    prefix.partitionRepresented = true;
    prefix.count = 1;
    auto& first = prefix.slices[0];
    first.generation = 53;
    first.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    first.realizationId = core::RealizationId{2304};
    first.loopTraversalProven = true;
    first.loopWrapCount = 0;
    first.outputBegin = 0;
    first.sourceBegin = 0;
    app::TruePeakLiveCoordinator live{
        [&] { return core::Result<WindowsConsumedOutputPrefix>::success(prefix); },
        [&] { return core::Result<core::PlaybackSnapshot>::success(playback); },
        [&]() -> std::optional<app::TruePeakLiveSource> {
            return app::TruePeakLiveSource{
                fixture.value()->result, core::RealizationId{2304},
                fixture.value()->expected};
        }
    };
    // Real sink-consumed 100-ms increments accumulate one second inside
    // a single loop traversal, with no synthetic hold from the next wrap.
    for (int bucket = 1; bucket <= 10; ++bucket) {
        const auto frames = std::int64_t{4800} * bucket;
        prefix.processedWatermarkExclusive = frames;
        first.outputEnd = frames;
        first.sourceEnd = frames;
        playback.position = core::FrameIndex{frames - 1};
        QTest::qWait(110);
        if (bucket >= 2) QTRY_VERIFY_WITH_TIMEOUT(live.recent_available(), 4000);
    }
    QTRY_VERIFY_WITH_TIMEOUT(live.hold_available(), 5000);
    QVERIFY(live.hold_text() != QStringLiteral("−∞ dBTP"));
    // A consumed jump to the end is still within the same known owner.
    prefix.processedWatermarkExclusive = 60000;
    first.outputEnd = 60000;
    first.sourceEnd = 60000;
    playback.position = core::FrameIndex{59999};
    QTest::qWait(150);
    // Audible wrap advances only when the sink crosses the source boundary.
    prefix.count = 2;
    auto& second = prefix.slices[1];
    second = first;
    second.outputBegin = 60000;
    second.outputEnd = 64800;
    second.sourceBegin = 0;
    second.sourceEnd = 4800;
    second.loopWrapCount = 1;
    prefix.processedWatermarkExclusive = 64800;
    playback.loopWrapCount = 1;
    playback.position = core::FrameIndex{4799};
    QTRY_VERIFY_WITH_TIMEOUT(!live.hold_available(), 5000);
    QTest::qWait(150);
    QVERIFY(!live.recent_available()); // 100ms plus FIR start is not yet covered
    second.outputEnd = 70080;
    second.sourceEnd = 10080;
    prefix.processedWatermarkExclusive = 70080;
    playback.position = core::FrameIndex{10079};
    QTRY_VERIFY_WITH_TIMEOUT(live.recent_available(), 5000);
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

void TruePeakStaticBindingTest::
c2D15SrcBindingRequiresOwnerAndConservativeConsumedProof()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    for (const auto& pair :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        auto fixture = make_render(7'000, frame_range(0, 7'000), pair.first);
        QVERIFY(fixture);
        const auto owner = fixture.value()->result;
        const auto expected = fixture.value()->expected;
        const auto id = core::RealizationId{9'101};
        core::PlaybackSnapshot playback{};
        playback.state = core::PlaybackState::PLAYING;
        playback.audibleRealization = {
            core::AudibleHandoffPhase::NEW, id, std::nullopt};
        platform::windows::WindowsConsumedOutputPrefix prefix{};
        prefix.generation = 25;
        prefix.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.processedWatermarkExclusive = 6'000;
        prefix.partitionRepresented = true;
        prefix.count = 1;
        auto& gap = prefix.slices[0];
        gap.generation = 25;
        gap.outputBegin = 0;
        gap.outputEnd = 6'000;
        gap.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        gap.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
        prefix.srcRetiredSourceEnd = 5'000;
        prefix.srcRealizationId = id;
        prefix.srcRetiredSourceBegin = 0;
        prefix.srcInputRateHz = pair.first;
        prefix.srcOutputRateHz = pair.second;
        const auto bind = [&]() {
            return app::bind_processed_live_input(
                owner, id, expected, playback, prefix);
        };
        const auto good = bind();
        QVERIFY(good);
        QVERIFY(good.value()->owner == owner);
        QCOMPARE(good.value()->consumedSourceBegin, std::int64_t{0});
        QCOMPARE(good.value()->consumedSourceEnd, std::int64_t{5'000});
        QCOMPARE(good.value()->consumedOutputEnd, std::int64_t{6'000});
        QVERIFY(!prefix.certifiedForLiveTruePeak);

        prefix.srcRealizationId = core::RealizationId{9'102};
        QVERIFY(!bind());
        prefix.srcRealizationId = id;
        prefix.srcRetiredSourceEnd.reset();
        QVERIFY(!bind());
        // Conservative SOURCE retirement is an exclusive, positive prefix.
        // Neither an empty nor an inverted prefix may invent LIVE evidence.
        prefix.srcRetiredSourceEnd = 0;
        QVERIFY(!bind());
        prefix.srcRetiredSourceEnd = -1;
        QVERIFY(!bind());
        prefix.srcRetiredSourceEnd = 7'001;
        QVERIFY(!bind());
        prefix.srcRetiredSourceEnd = 5'000;
        prefix.srcInputRateHz = pair.second;
        QVERIFY(!bind());
        prefix.srcInputRateHz = pair.first;
        prefix.srcOutputRateHz = pair.first;
        QVERIFY(!bind());
        prefix.srcOutputRateHz = pair.second;
        prefix.partitionRepresented = false;
        QVERIFY(!bind());
        prefix.partitionRepresented = true;
        prefix.retainedOutputBegin = 1;
        QVERIFY(!bind());
        prefix.retainedOutputBegin = 0;
        // The output ledger must be stamped with the very generation being
        // consumed: a stale slice cannot authorize a SOURCE prefix.
        ++gap.generation;
        QVERIFY(!bind());
        --gap.generation;
        gap.kind = WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
        QVERIFY(!bind());
        gap.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
        gap.realizationId = id; // A GAP must never fake an exact source.
        QVERIFY(!bind());
        gap.realizationId.reset();
        gap.sourceBegin = 0;
        QVERIFY(!bind());
        gap.sourceBegin.reset();
        gap.reason = WindowsConsumedGapReason::UNPROVEN_HANDOFF;
        QVERIFY(!bind());
        gap.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        gap.outputEnd = 6'001;
        QVERIFY(!bind());
        gap.outputEnd = 6'000;
        playback.audibleRealization.phase = core::AudibleHandoffPhase::TRANSITION;
        QVERIFY(!bind());
        playback.audibleRealization.phase = core::AudibleHandoffPhase::NEW;
        playback.loop = frame_range(200, 900);
        QVERIFY(!bind());
        playback.loop.reset();
        playback.loopWrapCount = 1;
        QVERIFY(!bind());
        playback.loopWrapCount = 0;
        const auto restored = bind();
        QVERIFY(restored);
        QCOMPARE(restored.value()->consumedSourceEnd, std::int64_t{5'000});
    }
}

void TruePeakStaticBindingTest::
c2D15SrcLiveCoordinatorWaitsForPreSrc100msAndClearsOnSeek()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    for (const auto& rates :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        auto fixture = make_render(7'000, frame_range(0, 7'000),
                                   rates.first, audio::ChannelLayout::STEREO_LR,
                                   0.75, 4'900);
        QVERIFY(fixture);
        const auto owner = fixture.value()->result;
        const auto id = core::RealizationId{9'151};
        core::PlaybackSnapshot playback{};
        playback.state = core::PlaybackState::PLAYING;
        playback.audibleRealization = {
            core::AudibleHandoffPhase::NEW, id, std::nullopt};
        platform::windows::WindowsConsumedOutputPrefix prefix{};
        prefix.generation = 27;
        prefix.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.processedWatermarkExclusive = 3'000;
        prefix.partitionRepresented = true;
        prefix.count = 1;
        prefix.slices[0].generation = 27;
        prefix.slices[0].outputBegin = 0;
        prefix.slices[0].outputEnd = 3'000;
        prefix.slices[0].kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
        prefix.slices[0].reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.srcRealizationId = id;
        prefix.srcRetiredSourceEnd = 3'000;
        prefix.srcInputRateHz = rates.first;
        prefix.srcOutputRateHz = rates.second;
        prefix.srcRetiredSourceBegin = 0;
        app::TruePeakLiveCoordinator coordinator{
            [&] {
                return core::Result<
                    platform::windows::WindowsConsumedOutputPrefix>::success(prefix);
            },
            [&] {
                return core::Result<core::PlaybackSnapshot>::success(playback);
            },
            [&]() -> std::optional<app::TruePeakLiveSource> {
                return app::TruePeakLiveSource{owner, id,
                    fixture.value()->expected};
            },
        };
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.monitoring_attempted(), 3500);
        QTest::qWait(110);
        QVERIFY(!coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());
        // RECENT availability is not evidence of a nonzero peak: a
        // partially consumed 10-ms bucket must never publish its impulse.
        // At 44.1k and 48k, the pulse at SOURCE frame 4900 belongs to
        // [4851,5292) and [4800,5280), respectively.
        constexpr std::int64_t pulseFrame = 4'900;
        const auto framesPer10ms = rates.first / 100;
        const auto firstCompletePulseBucketEnd =
            ((pulseFrame / framesPer10ms) + 1) * framesPer10ms;
        QCOMPARE(firstCompletePulseBucketEnd,
                 rates.first == 44'100 ? std::int64_t{5'292}
                                       : std::int64_t{5'280});
        const auto advanceConsumed = [&](std::int64_t sourceEnd,
                                         std::int64_t outputEnd) {
            prefix.processedWatermarkExclusive = outputEnd;
            prefix.slices[0].outputEnd = outputEnd;
            prefix.srcRetiredSourceEnd = sourceEnd;
        };
        advanceConsumed(5'100, 6'000);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
        QCOMPARE(coordinator.peak_text(), QStringLiteral("−∞ dBTP"));
        QCOMPARE(coordinator.left_level(), 0.0);
        QCOMPARE(coordinator.right_level(), 0.0);
        QVERIFY(!coordinator.hold_available());

        // Even one frame before closing the pulse bucket, the completed
        // RECENT window remains genuine silence on both sample-rate paths.
        advanceConsumed(firstCompletePulseBucketEnd - 1, 6'000);
        QTest::qWait(160);
        QVERIFY(coordinator.recent_available());
        QCOMPARE(coordinator.left_level(), 0.0);
        QCOMPARE(coordinator.right_level(), 0.0);

        // A genuinely advanced, owner-stamped sink watermark retires the
        // first complete pulse bucket and makes both L/R spikes measurable.
        advanceConsumed(firstCompletePulseBucketEnd, 6'400);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.left_level() > 0.0, 4500);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.right_level() > 0.0, 4500);
        QVERIFY(coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());

        // A one-frame hole in the actually accepted OUTPUT chronology must
        // invalidate the display; SOURCE retirement alone is insufficient.
        prefix.slices[0].outputEnd = 6'399;
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.recent_available(), 3500);
        QVERIFY(!coordinator.hold_available());

        // A new seek/generation starts a fresh proof. It cannot inherit
        // the positive result from the previous worker or bucket history.
        ++playback.seekSerial;
        ++prefix.generation;
        prefix.slices[0].generation = prefix.generation;
        advanceConsumed(5'100, 6'000);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
        QCOMPARE(coordinator.left_level(), 0.0);
        QCOMPARE(coordinator.right_level(), 0.0);

        // Replaced SOURCE ownership and stale async completions cannot
        // revive the old RECENT reading, even after the sink clock advances.
        prefix.srcRealizationId = core::RealizationId{9'152};
        coordinator.invalidate();
        QVERIFY(!coordinator.recent_available());
        advanceConsumed(firstCompletePulseBucketEnd, 6'400);
        QTest::qWait(160);
        QVERIFY(!coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());
        ++playback.seekSerial;
        prefix.srcRetiredSourceEnd.reset();
        QTest::qWait(110);
        QVERIFY(!coordinator.recent_available());
    }
}


void TruePeakStaticBindingTest::
c2D15SrcLiveCoordinatorOneSecondHoldRequiresCompleteConsumedHistory()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    // Both SRC directions retain the canonical SOURCE_PROCESSING_RATE meter
    // timeline. An OUTPUT watermark alone is not proof of a SOURCE window.
    for (const auto& rates :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        const auto pulseFrame = rates.first / 5 + 20;
        auto fixture = make_render(
            2 * rates.first, frame_range(0, 2 * rates.first),
            rates.first, audio::ChannelLayout::STEREO_LR,
            0.82, pulseFrame);
        QVERIFY(fixture);
        const auto owner = fixture.value()->result;
        const auto id = core::RealizationId{9'171};
        core::PlaybackSnapshot playback{};
        playback.state = core::PlaybackState::PLAYING;
        playback.audibleRealization = {
            core::AudibleHandoffPhase::NEW, id, std::nullopt};
        platform::windows::WindowsConsumedOutputPrefix prefix{};
        prefix.generation = 71;
        prefix.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.partitionRepresented = true;
        prefix.count = 1;
        auto& gap = prefix.slices[0];
        gap.generation = prefix.generation;
        gap.outputBegin = 0;
        gap.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
        gap.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.srcRealizationId = id;
        prefix.srcInputRateHz = rates.first;
        prefix.srcOutputRateHz = rates.second;
        prefix.srcRetiredSourceBegin = 0;
        prefix.srcRetiredSourceEnd = rates.first / 20;
        prefix.processedWatermarkExclusive = rates.second / 20 + 768;
        gap.outputEnd = prefix.processedWatermarkExclusive;
        app::TruePeakLiveCoordinator coordinator{
            [&] {
                return core::Result<
                    platform::windows::WindowsConsumedOutputPrefix>::success(prefix);
            },
            [&] {
                return core::Result<core::PlaybackSnapshot>::success(playback);
            },
            [&]() -> std::optional<app::TruePeakLiveSource> {
                return app::TruePeakLiveSource{
                    owner, id, fixture.value()->expected};
            },
        };
        const auto consumed = [&](std::int64_t sourceEnd,
                                  std::int64_t outputEnd) {
            prefix.srcRetiredSourceEnd = sourceEnd;
            prefix.processedWatermarkExclusive = outputEnd;
            gap.outputEnd = outputEnd;
        };
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.monitoring_attempted(), 3500);
        QVERIFY(!coordinator.hold_available());

        // Advance through genuine, ordered 100-ms SOURCE retirements, never
        // skipping more than the worker's 500-ms bounded suffix. The OUTPUT
        // witness is independently monotone and ahead of the SOURCE frontier
        // to allow for FIR lookahead; neither value is a DAC measurement.
        for (std::int64_t tenth = 1; tenth <= 9; ++tenth) {
            consumed(tenth * rates.first / 10,
                     tenth * rates.second / 10 + 768);
            QTest::qWait(135);
            if (tenth >= 2)
                QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
            QVERIFY(!coordinator.hold_available());
        }
        // A one-frame-short SOURCE proof must not invent a 1-second HOLD.
        consumed(rates.first - 1, rates.second + 768);
        QTest::qWait(160);
        QVERIFY(coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());

        // Precisely 100 complete 10-ms SOURCE buckets now permit HOLD.
        // The impulse at 0.2 s is outside the latest RECENT 100 ms but is
        // still inside the actual 1-second consumed history.
        consumed(rates.first, rates.second + 768);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.hold_available(), 5000);
        QCOMPARE(coordinator.peak_text(), QStringLiteral("−∞ dBTP"));
        QVERIFY(coordinator.hold_text() != QStringLiteral("−∞ dBTP"));
        QVERIFY(coordinator.left_hold_level() > 0.0);
        QVERIFY(coordinator.right_hold_level() > 0.0);
        QCOMPARE(coordinator.left_level(), 0.0);
        QCOMPARE(coordinator.right_level(), 0.0);

        // A further 300 ms of continuous same-owner SOURCE consumption
        // expires the old impulse from the rolling HOLD while retaining a
        // genuinely complete, now silent, 1-second window.
        consumed(13 * rates.first / 10,
                 13 * rates.second / 10 + 768);
        QTRY_COMPARE_WITH_TIMEOUT(
            coordinator.hold_text(), QStringLiteral("−∞ dBTP"), 6000);
        QVERIFY(coordinator.hold_available());
        QVERIFY(coordinator.recent_available());

        // Even with SOURCE retirement still present, one missing OUTPUT
        // frame defeats the consumed-chronology proof synchronously at poll.
        gap.outputEnd = prefix.processedWatermarkExclusive - 1;
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.hold_available(), 4500);
        QVERIFY(!coordinator.recent_available());
        gap.outputEnd = prefix.processedWatermarkExclusive;
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
        QVERIFY(!coordinator.hold_available()); // no inherited 1-s history

        // PAUSE freezes only an already matching reading. A paused SEEK
        // must invalidate it before a new worker may publish anything.
        playback.state = core::PlaybackState::PAUSED;
        QTest::qWait(140);
        QVERIFY(coordinator.recent_available());
        ++playback.seekSerial;
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.recent_available(), 3500);
        QVERIFY(!coordinator.hold_available());

        playback.state = core::PlaybackState::PLAYING;
        ++prefix.generation;
        gap.generation = prefix.generation;
        consumed(rates.first / 5, rates.second / 5 + 768);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
        QCOMPARE(coordinator.peak_text(), QStringLiteral("−∞ dBTP"));
        QVERIFY(!coordinator.hold_available());

        // A stale/unmatched source owner cannot revive either observation.
        prefix.srcRealizationId = core::RealizationId{9'172};
        coordinator.invalidate();
        QTest::qWait(150);
        QVERIFY(!coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());
        playback.state = core::PlaybackState::STOPPED;
        QTest::qWait(110);
        QVERIFY(!coordinator.recent_available());
    }
}


void TruePeakStaticBindingTest::
c2D16SrcLongGuiStallCannotInventOneSecondHold()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    for (const auto& rates :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        const auto sourceHz = rates.first;
        const auto outputHz = rates.second;
        // The source impulse is in discarded history following a long GUI
        // stall, so any later nonzero HOLD would reveal inherited history.
        auto fixture = make_render(
            3 * sourceHz, frame_range(0, 3 * sourceHz),
            sourceHz, audio::ChannelLayout::STEREO_LR,
            0.84, sourceHz / 3);
        QVERIFY(fixture);
        const auto owner = fixture.value()->result;
        const auto id = core::RealizationId{9'191};
        core::PlaybackSnapshot playback{};
        playback.state = core::PlaybackState::PLAYING;
        playback.audibleRealization = {
            core::AudibleHandoffPhase::NEW, id, std::nullopt};

        platform::windows::WindowsConsumedOutputPrefix prefix{};
        prefix.generation = 81;
        prefix.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.partitionRepresented = true;
        prefix.count = 1;
        auto& gap = prefix.slices[0];
        gap.generation = prefix.generation;
        gap.outputBegin = 0;
        gap.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
        gap.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.srcRealizationId = id;
        prefix.srcInputRateHz = sourceHz;
        prefix.srcOutputRateHz = outputHz;
        prefix.srcRetiredSourceBegin = 0;
        const auto advance = [&](std::int64_t sourceEnd,
                                 std::int64_t outputEnd) {
            prefix.srcRetiredSourceEnd = sourceEnd;
            prefix.processedWatermarkExclusive = outputEnd;
            gap.outputEnd = outputEnd;
        };
        advance(sourceHz / 20, outputHz / 20 + 1'024);
        app::TruePeakLiveCoordinator coordinator{
            [&] {
                return core::Result<
                    platform::windows::WindowsConsumedOutputPrefix>::success(prefix);
            },
            [&] {
                return core::Result<core::PlaybackSnapshot>::success(playback);
            },
            [&]() -> std::optional<app::TruePeakLiveSource> {
                return app::TruePeakLiveSource{
                    owner, id, fixture.value()->expected};
            },
        };
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.monitoring_attempted(), 3500);
        QVERIFY(!coordinator.hold_available());

        // The GUI/event loop loses >500 ms of observation. A single later
        // watermark can establish RECENT only after rebuilding genuine FIR
        // support; it must never create 1-second HOLD from a skipped span.
        advance(6 * sourceHz / 5, 6 * outputHz / 5 + 1'024);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 5000);
        QVERIFY(!coordinator.hold_available());
        QCOMPARE(coordinator.peak_text(), QStringLiteral("−∞ dBTP"));

        // Ten further distinct sink-consumed 100-ms advances are not a
        // cosmetic timer: this is newly observed chronological SOURCE
        // coverage, required before a genuine rolling HOLD can reappear.
        for (std::int64_t tenth = 13; tenth <= 23; ++tenth) {
            advance(tenth * sourceHz / 10,
                    tenth * outputHz / 10 + 1'024);
            QTest::qWait(130);
            QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4000);
        }
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.hold_available(), 5000);
        QCOMPARE(coordinator.hold_text(), QStringLiteral("−∞ dBTP"));
        QVERIFY(coordinator.recent_available());

        // Absent output chronology or changed owner must invalidate both
        // observations; the worker must not inherit the pre-stall peak.
        gap.outputEnd = prefix.processedWatermarkExclusive - 1;
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.recent_available(), 4000);
        QVERIFY(!coordinator.hold_available());
        prefix.srcRealizationId = core::RealizationId{9'192};
        coordinator.invalidate();
        QTest::qWait(120);
        QVERIFY(!coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());
    }
}


void TruePeakStaticBindingTest::
c2D17SrcSeekClearsPreviousMeterAndRebuildsNewOwnerInterval()
{
    using platform::windows::WindowsConsumedGapReason;
    using platform::windows::WindowsConsumedSliceKind;
    for (const auto& rates :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        const auto sourceHz = rates.first;
        const auto outputHz = rates.second;
        // An old spike must not be republished after an internal seek;
        // complete SOURCE buckets and True Peak FIR support start fresh.
        auto fixture = make_render(
            3 * sourceHz, frame_range(0, 3 * sourceHz),
            sourceHz, audio::ChannelLayout::STEREO_LR,
            0.9, sourceHz / 10 + 10);
        QVERIFY(fixture);
        const auto owner = fixture.value()->result;
        const auto id = core::RealizationId{9'211};
        core::PlaybackSnapshot playback{};
        playback.state = core::PlaybackState::PLAYING;
        playback.audibleRealization = {
            core::AudibleHandoffPhase::NEW, id, std::nullopt};

        platform::windows::WindowsConsumedOutputPrefix prefix{};
        prefix.generation = 99;
        prefix.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.partitionRepresented = true;
        prefix.count = 1;
        auto& gap = prefix.slices[0];
        gap.generation = prefix.generation;
        gap.outputBegin = 0;
        gap.kind = WindowsConsumedSliceKind::GAP_UNPROVEN;
        gap.reason = WindowsConsumedGapReason::UNPROVEN_SRC;
        prefix.srcRealizationId = id;
        prefix.srcInputRateHz = sourceHz;
        prefix.srcOutputRateHz = outputHz;
        const auto setWindow = [&](std::int64_t begin,
                                   std::int64_t sourceEnd,
                                   std::int64_t outputEnd) {
            prefix.srcRetiredSourceBegin = begin;
            prefix.srcRetiredSourceEnd = sourceEnd;
            prefix.processedWatermarkExclusive = outputEnd;
            gap.outputEnd = outputEnd;
        };
        setWindow(0, sourceHz / 5, outputHz / 5 + 512);
        app::TruePeakLiveCoordinator coordinator{
            [&] {
                return core::Result<
                    platform::windows::WindowsConsumedOutputPrefix>::success(prefix);
            },
            [&] {
                return core::Result<core::PlaybackSnapshot>::success(playback);
            },
            [&]() -> std::optional<app::TruePeakLiveSource> {
                return app::TruePeakLiveSource{
                    owner, id, fixture.value()->expected};
            },
        };
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
        QVERIFY(coordinator.left_level() > 0.0);
        QVERIFY(!coordinator.hold_available());

        // Seeking DURING PLAY reuses the immutable PROCESSED owner but must
        // change consumed generation/seek identity and seek source origin.
        ++playback.seekSerial;
        ++prefix.generation;
        gap.generation = prefix.generation;
        const auto seekSource = sourceHz;
        setWindow(seekSource, seekSource + sourceHz / 20,
                  outputHz / 20 + 512);
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.recent_available(), 4500);
        QVERIFY(!coordinator.hold_available());

        // A complete 100ms with FIR warmup and no old pulse proves the
        // recovered meter now follows ONLY the post-seek SOURCE interval.
        setWindow(seekSource, seekSource + sourceHz / 5,
                  outputHz / 5 + 512);
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.recent_available(), 4500);
        QCOMPARE(coordinator.peak_text(), QStringLiteral("−∞ dBTP"));
        QVERIFY(!coordinator.hold_available());

        // Cannot borrow 1s history from before seek. The first complete
        // 1-second post-seek HOLD is entirely silent on this fixture.
        for (std::int64_t tenth = 3; tenth <= 10; ++tenth) {
            setWindow(seekSource,
                      seekSource + tenth * sourceHz / 10,
                      tenth * outputHz / 10 + 512);
            QTest::qWait(135);
            QVERIFY(coordinator.recent_available());
        }
        QTRY_VERIFY_WITH_TIMEOUT(coordinator.hold_available(), 5000);
        QCOMPARE(coordinator.hold_text(), QStringLiteral("−∞ dBTP"));

        // Incomplete or forged new SOURCE origin must fail closed.
        prefix.srcRetiredSourceBegin.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!coordinator.recent_available(), 3500);
        QVERIFY(!coordinator.hold_available());
        setWindow(seekSource + sourceHz / 2, seekSource + sourceHz / 4,
                  outputHz / 4 + 512);
        QTest::qWait(140);
        QVERIFY(!coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());

        // One output frame missing from the accepted ledger is not proof.
        setWindow(seekSource, seekSource + sourceHz / 5,
                  outputHz / 5 + 512);
        gap.outputEnd = prefix.processedWatermarkExclusive - 1;
        QTest::qWait(140);
        QVERIFY(!coordinator.recent_available());
        QVERIFY(!coordinator.hold_available());
    }
}

} // namespace
} // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::TruePeakStaticBindingTest)
#include "test_true_peak_static_binding.moc"
