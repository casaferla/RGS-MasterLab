// RGS MasterLab — PRIVATE M19/M20/M21 typed C++ -> real QML acceptance.
// Does not install or activate these panels in Main.qml or authenticate data.
#include "internal/m21_independent_panels_review_candidate.hpp"
#include "internal/m21_real_product_metrics_port.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QQuickItem>
#include <QQuickView>
#include <QUrl>
#include <QtTest>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace rgsml::tests {
using namespace app::m21_private;
using namespace app::m21_private::ui_candidate;
using namespace app::m21_private::ui_candidate::panel_review;

static QQuickItem* visual(QQuickItem* current,const QString& name) {
    if(!current)return nullptr;
    if(current->objectName()==name)return current;
    for(QQuickItem* child:current->childItems())
        if(QQuickItem* found=visual(child,name))return found;
    return nullptr;
}

// A real 48-kHz M19/M20 measurement. Only the TEST-ONLY fixture mapper
// may transport these values to the isolated screenshot QML.
static bool populate_from_real_pcm(ReviewEpochStore& store) {
    const auto rate=core::SampleRate::create(48000);
    if(!rate)return false;
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    const auto count=core::FrameCount::create(19200);
    if(!format||!count)return false;
    auto pcm=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{12},*count.value());
    if(!pcm)return false;
    auto samples=pcm.value()->mutable_view().channel(0);
    if(!samples)return false;
    for(std::size_t i=0;i<samples.value()->size();++i)
        (*samples.value())[i]=0.25*std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
    const auto level=analysis::measure_basic_level_full_resource(*pcm.value());
    const auto tonal=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{12},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    if(!level||!tonal)return false;
    const auto imported=import_real_product_metrics(
        *level.value(),*tonal.value(),
        {Column::prepared,17,"qt-convergence-real-48k","Full Track",true});
    if(!imported.snapshot || imported.problem!=RealProductPortProblem::none)
        return false;
    const auto& snapshot=*imported.snapshot;
    return store.begin(Column::prepared,17)
        && store.receive(snapshot,
            {Column::prepared,snapshot.generation_token,
             snapshot.resource_identity,snapshot.analysis_scope_label,
             snapshot.absolute_scope_begin,snapshot.scope_frames,
             snapshot.sample_rate_hz,true,true});
}

static bool inject_fixture(QQuickView& view,int tab,const QVariantMap& data) {
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl::fromLocalFile(
        QString::fromUtf8(RGSML_CONVERGENCE_QML_SOURCE)));
    if(view.status()!=QQuickView::Ready || !view.rootObject())return false;
    QQuickItem* item=view.rootObject();
    if(!item->setProperty("activeTab",tab))return false;
    if(!item->setProperty("standaloneModuleMode",true))return false;
    if(!item->setProperty("fixtureMode",true))return false;
    if(!item->setProperty("analysisStatus",QStringLiteral("CURRENT")))
        return false;
    return item->setProperty("presentation",data);
}

class M21QmlTypedConvergenceTest final: public QObject {
    Q_OBJECT
private slots:
    void realM19RmsAppearsInReadOnlyConsolas36DemoOnly();
    void realM20StereoButMissingPsdShowsUnavailableNotice();
    void m21PartialGoldIsUnavailableAndNewEpochMasksOldGui();
};

void M21QmlTypedConvergenceTest::
realM19RmsAppearsInReadOnlyConsolas36DemoOnly() {
    ReviewEpochStore store;
    QVERIFY(populate_from_real_pcm(store));
    IndependentPanelRouting routing;
    const auto bundle=capture_test_panels(
        store,routing,qt_review::ExplicitTestFixture{0x4D323147U});
    QVERIFY(review_frame_current(store,routing,bundle));
    QVERIFY(!bundle.m19.value("productionAuthorized").toBool());
    QQuickView view;
    view.resize(1184,688);
    QVERIFY(inject_fixture(view,0,bundle.m19));
    view.show();
    QQuickItem* item=view.rootObject();
    QTRY_VERIFY(item->property("available").toBool());
    auto* rms=visual(item,QStringLiteral("m19MetricValue_rms"));
    QVERIFY2(rms!=nullptr,"M19 true readout delegate missing from Qt visual tree");
    QVERIFY(rms->property("dataAvailable").toBool());
    QCOMPARE(rms->property("numericPixelSize").toInt(),36);
    QCOMPARE(rms->property("rawText").toString(),
             bundle.m19.value("levels").toMap().value("rms").toString());
    QVERIFY(rms->property("readOnly").toBool());
    QVERIFY(item->setProperty("fixtureMode",false));
    QTRY_VERIFY(!item->property("available").toBool());
    QVERIFY(!rms->property("dataAvailable").toBool());
    QVERIFY(!bundle.m19.value("productionAuthorized").toBool());
    QVERIFY(!unpublished_panels(store).m19.value("ready").toBool());
}

void M21QmlTypedConvergenceTest::
realM20StereoButMissingPsdShowsUnavailableNotice() {
    ReviewEpochStore store;
    QVERIFY(populate_from_real_pcm(store));
    IndependentPanelRouting routing;
    const auto bundle=capture_test_panels(
        store,routing,qt_review::ExplicitTestFixture{0x4D323147U});
    QVERIFY(bundle.m20.value("ready").toBool());
    QVERIFY(bundle.m20.value("spectrum").toList().isEmpty());
    QQuickView view;
    view.resize(1184,688);
    QVERIFY(inject_fixture(view,1,bundle.m20));
    view.show();
    QQuickItem* item=view.rootObject();
    QTRY_VERIFY(item->property("available").toBool());
    QVERIFY(!item->property("spectrumAvailable").toBool());
    QVERIFY(!item->property("tonalBandsAvailable").toBool());
    auto* spectrum=visual(item,QStringLiteral("m20MissingSpectrumNotice"));
    auto* bands=visual(item,QStringLiteral("m20MissingProfileNotice"));
    QVERIFY(spectrum && bands);
    QTRY_VERIFY(spectrum->isVisible());
    QTRY_VERIFY(bands->isVisible());
    auto* width=visual(item,QStringLiteral("m20StereoWidthReadout"));
    QVERIFY(width);
    QCOMPARE(width->property("rawText").toString(),
             QStringLiteral("NOT APPLICABLE")); // actual mono M20 measurement
    QVERIFY(!bundle.m20.value("productionAuthorized").toBool());
}

void M21QmlTypedConvergenceTest::
m21PartialGoldIsUnavailableAndNewEpochMasksOldGui() {
    ReviewEpochStore store;
    QVERIFY(populate_from_real_pcm(store));
    IndependentPanelRouting routing;
    const auto first=capture_test_panels(
        store,routing,qt_review::ExplicitTestFixture{0x4D323147U});
    const auto rows=first.m21.value("metrics").toList();
    QCOMPARE(rows.size(),9);
    QCOMPARE(rows.at(1).toMap().value("gold").toString(),
             QStringLiteral("UNAVAILABLE"));
    QCOMPARE(rows.at(1).toMap().value("preparedDelta").toString(),
             QStringLiteral("UNAVAILABLE"));
    QQuickView view;
    view.resize(1184,688);
    QVERIFY(inject_fixture(view,2,first.m21));
    view.show();
    QQuickItem* item=view.rootObject();
    QTRY_VERIFY(item->property("available").toBool());
    QVERIFY(store.begin(Column::prepared,18));
    QVERIFY(!review_frame_current(store,routing,first));
    const auto stale=capture_test_panels(
        store,routing,qt_review::ExplicitTestFixture{0x4D323147U});
    QVERIFY(!stale.m21.value("ready").toBool());
    QVERIFY(stale.m21.value("metrics").toList().isEmpty());
    QVERIFY(item->setProperty("analysisStatus",QStringLiteral("STALE")));
    QVERIFY(item->setProperty("presentation",stale.m21));
    QTRY_VERIFY(!item->property("available").toBool());
    auto* notice=visual(item,QStringLiteral("m21MissingComparisonNotice"));
    QVERIFY(notice);
    QTRY_VERIFY(notice->isVisible());
    QVERIFY(!store.unpublished().value("ready").toBool());
}
} // namespace rgsml::tests
QTEST_MAIN(rgsml::tests::M21QmlTypedConvergenceTest)
#include "test_m21_qml_typed_convergence.moc"
