#include "master_output_rate_state.hpp"
#include "prepare_memory_advice_source_policy.hpp"
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
