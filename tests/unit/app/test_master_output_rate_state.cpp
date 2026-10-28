#include "master_output_rate_state.hpp"
#include <QSignalSpy>
#include <QtTest>
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
 void monitoringGuardNeverRetargetsOrInvalidatesMaster() {
    MasterOutputRateState s;
    s.source_committed(192000);
    QVERIFY(s.choose_master_rate(96000));
    const auto priorRevision=s.rate_revision();
    QSignalSpy changes(&s,&MasterOutputRateState::changed);
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
    QVERIFY(changes.count()>=3); // guard, availability, chosen rate
    monitoring=true;
    s.source_committed(44100); // genuine new SOURCE always invalidates
    QCOMPARE(s.master_rate_hz(),qint64{44100});
    QCOMPARE(s.source_rate_hz(),qint64{44100});
    QVERIFY(!s.can_edit_target());
    QCOMPARE(s.rate_revision(),priorRevision+2);
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
