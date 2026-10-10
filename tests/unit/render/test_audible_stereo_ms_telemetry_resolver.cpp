#include <rgsml/render/audible_stereo_ms_telemetry_resolver.hpp>
#include <rgsml/core/uuid.hpp>

#include <QtTest/QTest>

#include <memory>

namespace rgsml::tests {
namespace {

using rgsml::render::AudibleStereoMsTelemetryResolver;
using rgsml::render::StereoMsAudibleStatus;
using rgsml::core::AudibleHandoffPhase;
using rgsml::core::PlaybackSnapshot;
using rgsml::core::PlaybackState;
using rgsml::core::RealizationId;

[[nodiscard]] std::shared_ptr<rgsml::render::StereoMsStageOutputSidecar>
stage(RealizationId rid, std::int64_t limit = 1600)
{
    const auto uuid = rgsml::core::Uuid::parse(
        "24700000-0000-0000-0000-000000000003");
    auto id = rgsml::dsp::ModuleInstanceId::from_uuid(*uuid.value());
    auto out = std::make_shared<rgsml::render::StereoMsStageOutputSidecar>(
        *id.value());
    out->realization_id = rid;
    out->channel_layout = rgsml::audio::ChannelLayout::STEREO_LR;
    out->density_status = rgsml::render::StereoMsDensityStatus::COMPLETE;
    out->correlation_status = rgsml::render::StereoMsCorrelationStatus::COMPLETE;
    out->side_low_status = rgsml::render::StereoMsSideLowStatus::COMPLETE;
    for (std::int64_t f = 0; f + 100 <= limit; f += 100) {
        rgsml::render::StereoMsDensityBucket b;
        b.begin_frame = f;
        b.end_frame = f + 100;
        b.frame_count = 100;
        b.valid_frame_count = 100;
        b.occupancy[16 * 33 + 16] = 100;
        b.zero_vector_count = 100;
        out->density_buckets.push_back(b);
    }
    for (std::int64_t f = 0; f + 400 <= limit; f += 100) {
        out->correlation_windows.push_back({
            f, f + 400,
            rgsml::render::StereoMsCorrelationWindowValidity::VALID, 0.5});
        out->side_low_windows.push_back({f, f + 400, 0.25, 0.125});
    }
    return out;
}

[[nodiscard]] PlaybackSnapshot playback(
    std::int64_t position, RealizationId id,
    AudibleHandoffPhase phase = AudibleHandoffPhase::NEW)
{
    PlaybackSnapshot s;
    s.state = PlaybackState::PLAYING;
    s.position = rgsml::core::FrameIndex{position};
    s.traversalSerial = 1;
    s.audibleRealization.phase = phase;
    s.audibleRealization.realizationId = id;
    return s;
}

class StereoMsAudibleResolverTest final : public QObject {
    Q_OBJECT
private slots:
    void oldReadyTransitionNewNeverMixes();
    void pauseSeekLoopAndNonProcessedClear();
    void boundedStallGapAndMetricWindow();
    void bypassMissingOrInvalidFailClosed();
};

void StereoMsAudibleResolverTest::oldReadyTransitionNewNeverMixes()
{
    AudibleStereoMsTelemetryResolver r;
    const RealizationId oldId{1}, newId{2};
    r.register_sidecar(stage(oldId));
    auto snap = playback(0, oldId, AudibleHandoffPhase::OLD);
    r.update(snap, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::ACTIVE);
    QVERIFY(r.density_history().empty());

    r.register_sidecar(stage(newId)); // READY candidate is NOT audible.
    snap.position = rgsml::core::FrameIndex{250};
    r.update(snap, true);
    QCOMPARE(r.active_realization_id()->value, oldId.value);
    QCOMPARE(r.density_history().size(), std::size_t{2});
    QCOMPARE(r.density_history().front().begin_frame, std::int64_t{0});

    snap.audibleRealization.phase = AudibleHandoffPhase::TRANSITION;
    snap.position = rgsml::core::FrameIndex{400};
    r.update(snap, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::TRANSITION);
    QVERIFY(r.density_history().empty());
    QVERIFY(!r.correlation().has_value());

    snap = playback(950, newId);
    snap.audibleRealization.handoffEndFrame = 750;
    r.update(snap, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::ACTIVE);
    QCOMPARE(r.active_realization_id()->value, newId.value);
    QCOMPARE(r.density_history().size(), std::size_t{1});
    QCOMPARE(r.density_history().front().begin_frame, std::int64_t{800});
    QVERIFY(r.gap_detected());
    QVERIFY(!r.correlation().has_value()); // 400ms never fully audible yet.
}

void StereoMsAudibleResolverTest::pauseSeekLoopAndNonProcessedClear()
{
    AudibleStereoMsTelemetryResolver r;
    const RealizationId id{11};
    r.register_sidecar(stage(id));
    auto s = playback(0, id);
    r.update(s, true);
    s.position = rgsml::core::FrameIndex{250};
    r.update(s, true);
    QCOMPARE(r.density_history().size(), std::size_t{2});

    s.state = PlaybackState::PAUSED;
    s.position = rgsml::core::FrameIndex{450};
    r.update(s, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::PAUSED);
    QCOMPARE(r.density_history().size(), std::size_t{2});

    s.state = PlaybackState::PLAYING;
    s.position = rgsml::core::FrameIndex{550};
    r.update(s, true);
    QCOMPARE(r.density_history().size(), std::size_t{5});

    s.seekSerial = 1;
    s.position = rgsml::core::FrameIndex{325};
    r.update(s, true);
    QVERIFY(r.density_history().empty());
    s.position = rgsml::core::FrameIndex{520};
    r.update(s, true);
    QCOMPARE(r.density_history().size(), std::size_t{1});
    QCOMPARE(r.density_history().front().begin_frame, std::int64_t{400});

    auto loop_range = rgsml::core::FrameRange::create(
        rgsml::core::FrameIndex{200}, rgsml::core::FrameIndex{800});
    QVERIFY(loop_range);
    s.loop = *loop_range.value();
    s.loopWrapCount = 1;
    s.position = rgsml::core::FrameIndex{450};
    r.update(s, true);
    QCOMPARE(r.density_history().size(), std::size_t{2});
    QCOMPARE(r.density_history().front().begin_frame, std::int64_t{200});

    r.update(s, false);
    QCOMPARE(r.status(), StereoMsAudibleStatus::NOT_AUDITIONED);
    QVERIFY(r.density_history().empty());
    QVERIFY(!r.correlation());
    r.update(s, true);
    QVERIFY(r.density_history().empty()); // No pre-return phantom playback.
}

void StereoMsAudibleResolverTest::boundedStallGapAndMetricWindow()
{
    AudibleStereoMsTelemetryResolver r;
    const RealizationId id{21};
    r.register_sidecar(stage(id, 2000));
    auto s = playback(0, id);
    r.update(s, true);
    s.position = rgsml::core::FrameIndex{1600};
    r.update(s, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::ACTIVE);
    QCOMPARE(r.density_history().size(),
             AudibleStereoMsTelemetryResolver::kMaxHistoryBuckets);
    QCOMPARE(r.density_history().front().begin_frame, std::int64_t{1000});
    QVERIFY(r.gap_detected());
    QVERIFY(r.correlation().has_value());
    QCOMPARE(r.correlation()->end_frame, std::int64_t{1600});
    QCOMPARE(*r.correlation()->rho, 0.5);
    QVERIFY(r.side_low().has_value());
    QCOMPARE(r.side_low()->rms_before, 0.25);
    QCOMPARE(r.side_low()->rms_after, 0.125);
}

void StereoMsAudibleResolverTest::bypassMissingOrInvalidFailClosed()
{
    AudibleStereoMsTelemetryResolver r;
    const RealizationId bypassId{31}, missingId{32}, invalidId{33};
    r.register_bypass(bypassId);
    auto s = playback(0, bypassId);
    r.update(s, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::BYPASS);
    QVERIFY(r.density_history().empty());

    s = playback(0, missingId);
    r.update(s, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::UNAVAILABLE);

    auto invalid = stage(invalidId);
    invalid->channel_layout = rgsml::audio::ChannelLayout::MONO_C;
    r.register_sidecar(invalid);
    s = playback(0, invalidId);
    r.update(s, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::UNAVAILABLE);

    s.state = PlaybackState::STOPPED;
    r.update(s, true);
    QCOMPARE(r.status(), StereoMsAudibleStatus::STOPPED);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsAudibleResolverTest)
#include "test_audible_stereo_ms_telemetry_resolver.moc"
