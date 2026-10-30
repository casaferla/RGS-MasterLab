#include "master_output_rate_state.hpp"
#include "prepare_memory_advice_source_policy.hpp"
#include "accepted_prepare_observation_policy.hpp"
#include <limits>
#include <QSignalSpy>
#include <QtTest>
#include <array>
namespace rgsml::app {
class MasterRateTest final : public QObject {
Q_OBJECT
private slots:
 void sourceDefaultAndNoDeviceDependency(){
    MasterOutputRateState s;QSignalSpy changes(&s,&MasterOutputRateState::changed);
    QVERIFY(!s.choose_master_rate(48000));s.source_committed(44100);
    QCOMPARE(s.master_rate_hz(),qint64{44100});QCOMPARE(s.rate_revision(),quint64{1});
    QVERIFY(s.has_supported_master_rate());QCOMPARE(changes.count(),1);
    for(const qint64 hz:{44100,48000,88200,96000,176400,192000}){
       QVERIFY(s.choose_master_rate(hz));QCOMPARE(s.master_rate_hz(),hz);
    }
    QCOMPARE(s.master_rate_hz(),qint64{192000}); // DEVICE Hz not read
    const auto revision=s.rate_revision();
    QVERIFY(!s.choose_master_rate(384000));QCOMPARE(s.rate_revision(),revision);
    QVERIFY(s.choose_master_rate(192000));QCOMPARE(s.rate_revision(),revision);
 }
 void sixRateCandidateStatusHonestForAll36Pairs() {
    MasterOutputRateState state;
    QVERIFY(!state.m17_candidate_rate_admissible(48000));
    QCOMPARE(state.final_src_path_status(48000),
        QStringLiteral("SOURCE / target rate unavailable"));
    const std::array<qint64,6> rates{
        44100,48000,88200,96000,176400,192000};
    int identity=0,legacy=0,unqualified=0;
    for(const auto from:rates) {
        state.source_committed(from);
        for(const auto to:rates) {
            const bool admitted=state.m17_candidate_rate_admissible(to);
            const auto label=state.final_src_path_status(to);
            if(from==to) {
                QVERIFY(admitted);
                QVERIFY(label.contains(QStringLiteral("IDENTITY")));
                ++identity;
            } else if((from==44100&&to==48000)
                   ||(from==48000&&to==44100)) {
                QVERIFY(admitted);
                QVERIFY(label.contains(QStringLiteral("candidate")));
                QVERIFY(label.contains(QStringLiteral("release pending")));
                ++legacy;
            } else {
                QVERIFY(!admitted);
                QVERIFY(label.contains(QStringLiteral("UNQUALIFIED")));
                ++unqualified;
            }
        }
    }
    QCOMPARE(identity,6);
    QCOMPARE(legacy,2);
    QCOMPARE(unqualified,28);
    state.source_committed(32000);
    QVERIFY(!state.m17_candidate_rate_admissible(192000));
 }
 void prepareMemoryDisplayNeverRecommendsUnqualifiedSrc() {
    const std::array<qint64,6> rates{
        44100,48000,88200,96000,176400,192000};
    int permitted=0,unqualified=0;
    for(const auto source:rates) {
        for(const auto master:rates) {
            const auto display=prepare_memory_display_admission(source,master);
            if(source==master
                ||(source==44100&&master==48000)
                ||(source==48000&&master==44100)) {
                QCOMPARE(display,
                    PrepareMemoryDisplayAdmission::CANDIDATE_ADMISSIBLE);
                ++permitted;
            } else {
                QCOMPARE(display,
                    PrepareMemoryDisplayAdmission::FINAL_SRC_UNQUALIFIED);
                ++unqualified;
            }
        }
    }
    QCOMPARE(permitted,8);
    QCOMPARE(unqualified,28);
    for(const auto bad:{0,32000,384000}) {
        QCOMPARE(prepare_memory_display_admission(bad,192000),
            PrepareMemoryDisplayAdmission::SOURCE_OR_RATE_UNAVAILABLE);
        QCOMPARE(prepare_memory_display_admission(192000,bad),
            PrepareMemoryDisplayAdmission::SOURCE_OR_RATE_UNAVAILABLE);
    }
 }
 void prepareMemorySnapshotRejectsStaleOrForgedResourceCosts() {
    constexpr std::uint64_t gib=UINT64_C(1024)*1024*1024;
    auto source=core::SampleRate::create(44100);
    auto duration=core::FrameCount::create(132300);
    auto device=core::SampleRate::create(48000);
    QVERIFY(source&&duration&&device);
    const audio::MonitoringMemoryObservation memory{
        16*gib,12*gib,1*gib};
    auto advice=audio::advise_monitoring_master_rates_for_prepare(
        *source.value(),*duration.value(),*device.value(),
        audio::ChannelLayout::STEREO_LR,memory);
    QVERIFY(advice);
    const auto rows=*advice.value();
    const auto valid=[&](const auto& r) {
        return prepare_rate_advice_snapshot_valid(
            r,*source.value(),*duration.value(),
            audio::ChannelLayout::STEREO_LR);
    };
    QVERIFY(valid(rows));
    {
        auto wrong=rows;
        wrong[1].proposed_master_rate=wrong[0].proposed_master_rate;
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        wrong[1].observed_device_rate=*source.value();
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        wrong[2].estimated_master_frames=*duration.value();
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        ++wrong[3].resource.combined_pcm_bytes;
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        ++wrong[4].resource.experimental_fir_scratch_bytes;
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        ++wrong[0].memory_observation.available_commit_bytes;
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        ++wrong[1].memory_headroom_after_reserve_bytes;
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        wrong[5].cpu_performance_qualified=true;
        QVERIFY(!valid(wrong));
    }
    {
        auto wrong=rows;
        wrong.pop_back();
        QVERIFY(!valid(wrong));
    }
    auto otherSource=core::SampleRate::create(96000);
    QVERIFY(otherSource);
    QVERIFY(!prepare_rate_advice_snapshot_valid(
        rows,*otherSource.value(),*duration.value(),
        audio::ChannelLayout::STEREO_LR));
    QVERIFY(!prepare_rate_advice_snapshot_valid(
        rows,*source.value(),*duration.value(),
        audio::ChannelLayout::MONO_C));
 }
 void approvedWholePrepareHistoryNeverInventsElapsedTime() {
    AcceptedPrepareObservationHistory h;
    AcceptedPrepareObservation x{
        1,44100,44100,132300,2,-1.0,7,8,9,
        110000,1540000,-1,1000000,130000};
    QVERIFY(AcceptedPrepareObservationHistory::eligible(x));
    QVERIFY(h.observe(x));
    auto r=h.latest(44100,132300,2,44100);
    QVERIFY(r);
    QCOMPARE(r->last.queue_us+r->last.wall_us,1650000);
    QCOMPARE(r->exact_revision_count,std::size_t{1});
    const AcceptedPrepareRevisionKey exact{7,8,9};
    QVERIFY(h.latest(44100,132300,2,44100,exact));
    QVERIFY(!h.latest(44100,132300,2,44100,
        AcceptedPrepareRevisionKey{8,8,9}));
    QVERIFY(!h.latest(44100,132300,2,44100,
        AcceptedPrepareRevisionKey{7,9,9}));
    QVERIFY(!h.latest(44100,132300,2,44100,
        AcceptedPrepareRevisionKey{7,8,10}));
    QVERIFY(!h.observe(x)); // identical invocation cannot be relearned
    QVERIFY(!h.latest(44100,132300,2,96000));
    QVERIFY(!h.latest(48000,132300,2,44100));
    QVERIFY(!h.latest(44100,132300,1,44100));
    auto y=x;
    y.invocation=2;y.master_hz=96000;y.src_us=-1;
    QVERIFY(!h.observe(y)); // cross-rate must measure genuine SRC
    y.invocation=3;y.src_us=90000;
    QVERIFY(h.observe(y));
    y.invocation=4;y.queue_us=std::numeric_limits<std::int64_t>::max();
    QVERIFY(!h.observe(y)); // total elapsed cannot overflow
    y=x;y.invocation=5;y.wall_us=1200000;
    QVERIFY(h.observe(y));
    r=h.latest(44100,132300,2,44100);
    QVERIFY(r);
    QCOMPARE(r->exact_revision_count,std::size_t{2});
    QCOMPARE(r->minimum_total_us,1310000);
    QCOMPARE(r->maximum_total_us,1650000);
    y=x;y.invocation=6;y.manual_revision=99;
    QVERIFY(h.observe(y));
    r=h.latest(44100,132300,2,44100);
    QVERIFY(r);
    QCOMPARE(r->exact_revision_count,std::size_t{1});
    // History retains old evidence. Only querying the CURRENT monotonic
    // chain revision can authorize historical ACT for the UI.
    QVERIFY(h.latest(44100,132300,2,44100,exact));
    QVERIFY(h.latest(44100,132300,2,44100,
        AcceptedPrepareRevisionKey{99,8,9}));
    QVERIFY(!h.latest(44100,132300,2,44100,
        AcceptedPrepareRevisionKey{99,8,10}));
    h.source_changed();
    QVERIFY(!h.latest(44100,132300,2,44100));
    QVERIFY(!h.observe(y)); // same old invocation after source epoch
    y.invocation=7;
    QVERIFY(h.observe(y));
 }
 void acceptedPrepareStageBreakdownDoesNotInventElapsedBuckets() {
    AcceptedPrepareObservation x{
        1,44100,44100,132300,2,-1.0,7,8,9,
        110000,1540000,-1,1000000,130000};
    QVERIFY(AcceptedPrepareObservationHistory::eligible(x));
    auto parts=AcceptedPrepareObservationHistory::stage_breakdown(x);
    QVERIFY(parts);
    QVERIFY(!parts->converted);
    QCOMPARE(parts->src_us,std::int64_t{0});
    QCOMPARE(parts->other_render_us,std::int64_t{410000});
    x.master_hz=96000;x.src_us=90000;
    parts=AcceptedPrepareObservationHistory::stage_breakdown(x);
    QVERIFY(parts);
    QVERIFY(parts->converted);
    QCOMPARE(parts->src_us,std::int64_t{90000});
    QCOMPARE(parts->other_render_us,std::int64_t{320000});
    x.src_us=-1;
    QVERIFY(!AcceptedPrepareObservationHistory::stage_breakdown(x));
    x.src_us=900000;
    QVERIFY(AcceptedPrepareObservationHistory::eligible(x));
    QVERIFY(!AcceptedPrepareObservationHistory::stage_breakdown(x));
    x.master_hz=44100;x.src_us=-1;x.wall_us=50000;
    QVERIFY(AcceptedPrepareObservationHistory::eligible(x));
    QVERIFY(!AcceptedPrepareObservationHistory::stage_breakdown(x));
    x.queue_us=0;
    x.wall_us=std::numeric_limits<std::int64_t>::max();
    QVERIFY(AcceptedPrepareObservationHistory::stage_breakdown(x));
    x.queue_us=1;
    QVERIFY(!AcceptedPrepareObservationHistory::stage_breakdown(x));
 }
 void acceptedHistoryBoundedAndSourceRevocationUnderManyPrepares() {
    AcceptedPrepareObservationHistory h;
    AcceptedPrepareObservation x{
        1,48000,48000,96000,1,-3.9,4,5,6,1500,120000,
        -1,70000,20000};
    for(std::uint64_t serial=1;serial<=30;++serial) {
        x.invocation=serial;
        x.wall_us=120000+static_cast<std::int64_t>(serial);
        QVERIFY(h.observe(x));
    }
    auto last=h.latest(48000,96000,1,48000);
    QVERIFY(last);
    QCOMPARE(last->last.invocation,std::uint64_t{30});
    QCOMPARE(last->exact_revision_count,std::size_t{24});
    QCOMPARE(last->minimum_total_us,std::int64_t{121507});
    QCOMPARE(last->maximum_total_us,std::int64_t{121530});
    h.source_changed(); // same shape, DIFFERENT SOURCE identity
    QVERIFY(!h.latest(48000,96000,1,48000));
    QVERIFY(!h.observe(x)); // old render ticket cannot repopulate
    x.invocation=31;
    QVERIFY(h.observe(x));
    x.invocation=32;x.ceiling=std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!h.observe(x));
    x.invocation=33;x.ceiling=-13.0;
    QVERIFY(!h.observe(x));
    x.invocation=34;x.ceiling=-3.9;x.m16_us=-1;
    QVERIFY(!h.observe(x));
    x.invocation=35;x.m16_us=20000;x.src_us=-1;x.master_hz=96000;
    QVERIFY(!h.observe(x));
 }
 void hostWholePrepareEstimateLearnsOnlyGenuineSameConfiguration() {
    AcceptedPrepareHostTimingHistory host;
    AcceptedPrepareObservation x{
        1,44100,44100,132300,2,-1.0,10,20,30,
        100000,1500000,-1,1200000,250000};
    auto predict=[&](std::int64_t sourceFrames,std::int64_t sourceHz,
                     std::int64_t masterHz,int channels,
                     double ceiling,std::uint64_t rev,std::uint64_t param) {
        return host.predict(sourceHz,sourceFrames,channels,masterHz,
                            ceiling,rev,param);
    };
    QVERIFY(!predict(132300,44100,44100,2,-1.0,20,30));
    for(int i=1;i<=3;++i) {
        x.invocation=static_cast<std::uint64_t>(i);
        x.wall_us=1500000+100000*i;
        QVERIFY(host.observe(x));
    }
    const auto base=predict(132300,44100,44100,2,-1.0,20,30);
    QVERIFY(base);
    QCOMPARE(base->matched_observations,std::size_t{3});
    QVERIFY(base->low_us>0);
    QVERIFY(base->high_us>base->low_us);
    // A different SOURCE file with similar duration is legal to estimate;
    // this host profile stores no SOURCE identity or PCM. Revisions of M17
    // parameters remain mandatory, including A -> B -> A invalidation.
    const auto doubled=predict(264600,44100,44100,2,-1.0,20,30);
    QVERIFY(doubled);
    QCOMPARE(doubled->low_us,base->low_us*2);
    QCOMPARE(doubled->high_us,base->high_us*2);
    QVERIFY(!predict(132300,44100,192000,2,-1.0,20,30));
    QVERIFY(!predict(132300,44100,44100,1,-1.0,20,30));
    QVERIFY(!predict(132300,44100,44100,2,-12.0,20,30));
    QVERIFY(!predict(132300,44100,44100,2,-1.0,21,30));
    QVERIFY(!predict(132300,44100,44100,2,-1.0,20,31));
    QVERIFY(!predict(132300*5,44100,44100,2,-1.0,20,30));
    QVERIFY(!predict(132300,44100,96000,2,-1.0,20,30));
    auto stale=x;stale.invocation=3;
    QVERIFY(!host.observe(stale));
    stale.invocation=4;stale.master_hz=192000;stale.src_us=-1;
    QVERIFY(!host.observe(stale)); // impossible unqualified SRC timing
    stale.invocation=5;stale.master_hz=44100;
    stale.wall_us=1;stale.m17_us=1200000;
    QVERIFY(!host.observe(stale)); // measured stages exceed whole wall
    host.clear();
    QVERIFY(!predict(132300,44100,44100,2,-1.0,20,30));
 }
 void monitoringGuardNeverRetargetsOrInvalidatesMaster() {
    MasterOutputRateState s;
    s.source_committed(192000);
    QVERIFY(s.choose_master_rate(96000));
    const auto priorRevision=s.rate_revision();
    QSignalSpy changes(&s,&MasterOutputRateState::changed);
    QSignalSpy availability(&s,
        &MasterOutputRateState::selectionAvailabilityChanged);
    bool monitoring=true;
    s.set_target_selection_guard([&monitoring]() noexcept {
        return !monitoring;
    });
    QVERIFY(!s.can_edit_target());
    QVERIFY(!s.choose_master_rate(48000));
    QCOMPARE(s.master_rate_hz(),qint64{96000});
    QCOMPARE(s.rate_revision(),priorRevision);
    // Rechoosing the same rate is not a mutation even during MONITOR.
    QVERIFY(s.choose_master_rate(96000));
    QCOMPARE(s.rate_revision(),priorRevision);
    monitoring=false;
    s.refresh_selection_availability();
    QVERIFY(s.can_edit_target());
    QVERIFY(s.choose_master_rate(48000));
    QCOMPARE(s.master_rate_hz(),qint64{48000});
    QCOMPARE(s.rate_revision(),priorRevision+1);
    QCOMPARE(changes.count(),1); // only genuine 192->48 change
    QCOMPARE(availability.count(),2); // guard and monitor Return
    monitoring=true;
    s.source_committed(44100); // genuine new SOURCE always invalidates
    QCOMPARE(s.master_rate_hz(),qint64{44100});
    QCOMPARE(s.source_rate_hz(),qint64{44100});
    QVERIFY(!s.can_edit_target());
    QCOMPARE(s.rate_revision(),priorRevision+2);
    QCOMPARE(changes.count(),2);
    QCOMPARE(availability.count(),3);
 }
 void sourceChangeInvalidatesAndUnsupportedFailsClosed(){
    MasterOutputRateState s;s.source_committed(48000);
    QVERIFY(s.choose_master_rate(192000));const auto old=s.rate_revision();
    s.source_committed(48000); // same-rate new source
    QCOMPARE(s.master_rate_hz(),qint64{48000});
    QCOMPARE(s.rate_revision(),old+1);
    s.source_committed(32000);QCOMPARE(s.master_rate_hz(),qint64{0});
    QVERIFY(!s.has_supported_master_rate());
    const auto invalidRevision=s.rate_revision();
    QVERIFY(!s.choose_master_rate(96000)); // unsupported SOURCE cannot choose valid MASTER
    QVERIFY(!s.choose_master_rate(32000)); // both unsupported
    QCOMPARE(s.rate_revision(),invalidRevision);
    QCOMPARE(s.master_rate_hz(),qint64{0});
    s.source_cleared();QCOMPARE(s.source_rate_hz(),qint64{0});
    QCOMPARE(s.master_rate_hz(),qint64{0});
 }
};
}
QTEST_GUILESS_MAIN(rgsml::app::MasterRateTest)
#include "test_master_output_rate_state.moc"
