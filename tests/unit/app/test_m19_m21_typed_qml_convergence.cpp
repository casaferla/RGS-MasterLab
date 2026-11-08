// Isolated, non-FQ convergence of PR #91 QML and PR #92 typed analysis.
// All QML values are explicitly watermarked TEST FIXTURES. Production Main
// is untouched; no canonical Product owner authorization is implied.
#include "internal/m21_independent_panels_review_candidate.hpp"
#include "internal/m21_real_product_metrics_port.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlError>
#include <QtTest>
#include <QVariantMap>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>

namespace rgsml::tests {
using namespace app::m21_private;
using namespace app::m21_private::ui_candidate;
using namespace app::m21_private::ui_candidate::panel_review;
namespace {
constexpr std::uint32_t kExplicitFixture=0x4D323147U;
QDir qml_folder() {return QDir(QStringLiteral(RGSML_ANALYSIS_GUI_ROOT));}
std::unique_ptr<QQuickView> open_panel(const QString& file) {
    auto view=std::make_unique<QQuickView>();
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->resize(1184,688);
    view->setSource(QUrl::fromLocalFile(qml_folder().filePath(file)));
    if(view->status()!=QQuickView::Ready) {
        for(const auto& error:view->errors())qWarning()<<error.toString();
    }
    return view;
}
QQuickItem* visual_by_name(QQuickItem* item,const QString& name) {
    if(!item)return nullptr;
    if(item->objectName()==name)return item;
    for(auto* child:item->childItems()) {
        if(auto* found=visual_by_name(child,name))return found;
    }
    return nullptr;
}
Snapshot example(Column column,std::uint64_t generation,
                 const std::string& id,double rms) {
    Snapshot s;
    s.column=column;
    s.generation_token=generation;
    s.resource_identity=id;
    s.analysis_scope_label="Full Track";
    s.absolute_scope_begin=12;
    s.scope_frames=19200;
    s.sample_rate_hz=48000;
    s.lifecycle=Lifecycle::current;
    s.metrics[1]={rms,Quality::present,{}};
    s.metrics[3]={-1.0,Quality::present,{}}; // Mock TP never displayed
    return s;
}
ExpectedOwnerReceipt claim(const Snapshot& s) {
    return {s.column,s.generation_token,s.resource_identity,
            s.analysis_scope_label,s.absolute_scope_begin,
            s.scope_frames,s.sample_rate_hz,true,true};
}
// Only Windows CI explicitly requests screenshots. Never write files in a
// production session; this test executable is not linked into the app.
bool optional_test_screenshot(QQuickView& view,const QString& name) {
    const QString destination=qEnvironmentVariable("RGSML_TYPED_QT_SCREENSHOT_DIR");
    if(destination.isEmpty())return true;
    if(!QDir().mkpath(destination))return false;
    view.resize(1184,688);
    view.show();
    QTest::qWait(450);
    const auto image=view.grabWindow();
    if(image.isNull() || image.width()<1184 || image.height()<688)return false;
    return image.save(QDir(destination).filePath(name), "PNG");
}
void mount_explicit_demo(QQuickView& view,const QVariantMap& data) {
    auto* root=view.rootObject();
    if(!root)return;
    // This local test-only path does not connect the QML component to Main.
    root->setProperty("presentation",data);
    root->setProperty("analysisStatus",data.value("analysisStatus","MISSING"));
    root->setProperty("fixtureMode",true);
    QCoreApplication::processEvents();
}
} // namespace

class M19M21TypedQmlConvergenceTest final: public QObject {
    Q_OBJECT
private slots:
    void threeActualIndependentQmlRootsOpenFailClosed();
    void explicitTypedM19TestFixtureRendersReadout();
    void actualPcmM19M20ReachesThreeQmlPanels();
    void spectrumAndTonalProfileRemainUnavailableWithoutVerifiedPsd();
    void missingAndStaleSourceNeverShowPriorNumericData();
    void independentSelectorsDoNotCrossModifySource();
    void staleRevisionDeniesQueuedQtFrame();
    void m21ComparisonRendersOnlyEligibleTypedRows();
    void truePeakIsNeverAuthorizedByTestPreview();
    void dockHostRemainsPreviewOnly();
    void revokedFixtureImmediatelyMasksReadouts();
    void missingStateBlocksValidLookingOldPresenter();
    void staleDisplayStateBlocksEvenReadyDemoData();
    void readyPresentationWithoutFixtureRemainsMasked();
};

void M19M21TypedQmlConvergenceTest::threeActualIndependentQmlRootsOpenFailClosed() {
    const QStringList names{
        "M19LevelsPanelPreview.qml","M20TonalStereoPanelPreview.qml",
        "M21MetricsDeltaPanelPreview.qml"};
    for(int tab=0;tab<3;++tab) {
        auto view=open_panel(names.at(tab));
        QVERIFY(view->status()==QQuickView::Ready);
        auto* root=view->rootObject();
        QVERIFY(root!=nullptr);
        QCOMPARE(root->property("activeTab").toInt(),tab);
        QVERIFY(root->property("standaloneModuleMode").toBool());
        QVERIFY(!root->property("fixtureMode").toBool());
        QVERIFY(!root->property("available").toBool());
    }
}
void M19M21TypedQmlConvergenceTest::explicitTypedM19TestFixtureRendersReadout() {
    ReviewEpochStore store;
    const auto prepared=example(Column::prepared,7,"pcm-prepared",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(prepared,claim(prepared)));
    IndependentPanelRouting routing;
    const auto frame=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QVERIFY(review_frame_current(store,routing,frame));
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    mount_explicit_demo(*view,frame.m19);
    QVERIFY(view->rootObject()->property("available").toBool());
    QCOMPARE(view->rootObject()->property("fixtureMode").toBool(),true);
    QCOMPARE(frame.m19.value("levels").toMap().value("rms").toString(),
             QStringLiteral("-18.00"));
    QCOMPARE(frame.m19.value("levelsQuality").toMap().value("rms").toString(),
             QStringLiteral("CURRENT"));
    QVERIFY(!frame.m19.value("productionAuthorized").toBool());
}
void M19M21TypedQmlConvergenceTest::actualPcmM19M20ReachesThreeQmlPanels() {
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto fmt=audio::AudioFormat::create(*rate.value(),audio::ChannelLayout::MONO_C);
    const auto frames=core::FrameCount::create(19200);
    QVERIFY(fmt&&frames);
    auto pcm=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{12},*frames.value());
    QVERIFY(pcm);
    auto channel=pcm.value()->mutable_view().channel(0);
    QVERIFY(channel);
    for(std::size_t i=0;i<channel.value()->size();++i) {
        (*channel.value())[i]=0.25*std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
    }
    const auto level=analysis::measure_basic_level_full_resource(*pcm.value());
    const auto stereo=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{12},*frames.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(level&&stereo);
    const auto imported=import_real_product_metrics(*level.value(),*stereo.value(),
        {Column::prepared,11,"real-m19m20-qml-test","Full Track",true});
    QVERIFY(imported.snapshot);
    QVERIFY(imported.problem==RealProductPortProblem::none);
    ReviewEpochStore store;
    QVERIFY(store.begin(Column::prepared,11));
    QVERIFY(store.receive(*imported.snapshot,claim(*imported.snapshot)));
    IndependentPanelRouting routing;
    const auto frame=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QVERIFY(review_frame_current(store,routing,frame));
    const QStringList files{
        "M19LevelsPanelPreview.qml","M20TonalStereoPanelPreview.qml",
        "M21MetricsDeltaPanelPreview.qml"};
    const std::array<QVariantMap,3> maps{frame.m19,frame.m20,frame.m21};
    for(int i=0;i<3;++i) {
        auto view=open_panel(files.at(i));
        QVERIFY(view->status()==QQuickView::Ready);
        mount_explicit_demo(*view,maps[static_cast<std::size_t>(i)]);
        QVERIFY(view->rootObject()->property("available").toBool());
        QVERIFY(view->rootObject()->property("fixtureMode").toBool());
        const QString filename=QStringLiteral("typed-")
            +QString::number(i)+QStringLiteral("-real-pcm-1184x688.png");
        QVERIFY(optional_test_screenshot(*view,filename));
    }
    bool parsed=false;
    const double rms=frame.m19.value("levels").toMap()
                         .value("rms").toString().toDouble(&parsed);
    QVERIFY(parsed);
    QVERIFY(std::abs(rms-*imported.snapshot->metrics[1].unrounded)<=0.0051);
    QCOMPARE(frame.m20.value("stereo").toMap().value("width").toString(),
             QStringLiteral("NOT APPLICABLE"));
    QVERIFY(!frame.m21.value("productionAuthorized").toBool());
}
void M19M21TypedQmlConvergenceTest::
spectrumAndTonalProfileRemainUnavailableWithoutVerifiedPsd() {
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"prepared",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(p,claim(p)));
    IndependentPanelRouting routing;
    const auto frame=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QVERIFY(frame.m20.value("ready").toBool());
    QVERIFY(frame.m20.value("spectrum").toList().isEmpty());
    QVERIFY(frame.m20.value("bands").toList().isEmpty());
    auto view=open_panel("M20TonalStereoPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    mount_explicit_demo(*view,frame.m20);
    QVERIFY(view->rootObject()->property("available").toBool());
    QVERIFY(!view->rootObject()->property("spectrumAvailable").toBool());
    QVERIFY(!view->rootObject()->property("tonalProfileAvailable").toBool());
    auto* notice=visual_by_name(view->rootObject(),"m20MissingSpectrumNotice");
    QVERIFY(notice);
    QVERIFY(notice->isVisible());
    QVERIFY(notice->property("text").toString().contains("UNAVAILABLE"));
    auto* profile=visual_by_name(view->rootObject(),"m20MissingProfileNotice");
    QVERIFY(profile&&profile->isVisible());
}
void M19M21TypedQmlConvergenceTest::
missingAndStaleSourceNeverShowPriorNumericData() {
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"old",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(p,claim(p)));
    IndependentPanelRouting routing;
    const auto old=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QVERIFY(store.begin(Column::prepared,8));
    QVERIFY(!review_frame_current(store,routing,old));
    const auto stale=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    mount_explicit_demo(*view,stale.m19);
    QVERIFY(!view->rootObject()->property("available").toBool());
    QCOMPARE(view->rootObject()->property("analysisStatus").toString(),
             QStringLiteral("STALE"));
    QVERIFY(stale.m19.value("levels").toMap().isEmpty());
    QVERIFY(optional_test_screenshot(*view,
        QStringLiteral("typed-m19-stale-1184x688.png")));
}
void M19M21TypedQmlConvergenceTest::independentSelectorsDoNotCrossModifySource() {
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"p",-18.0);
    auto g=example(Column::gold,3,"g",-13.0);
    p.metrics[5]={0.8,Quality::present,{}};
    g.metrics[5]={1.1,Quality::present,{}};
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.receive(p,claim(p)));
    QVERIFY(store.receive(g,claim(g)));
    IndependentPanelRouting routing;
    QVERIFY(routing.select(AnalysisPanel::tonal_m20,Column::gold));
    const auto f=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QCOMPARE(f.m19.value("levels").toMap().value("rms").toString(),QStringLiteral("-18.00"));
    QCOMPARE(f.m20.value("stereo").toMap().value("width").toString(),QStringLiteral("1.10"));
    QVERIFY(review_frame_current(store,routing,f));
    auto levelView=open_panel("M19LevelsPanelPreview.qml");
    auto stereoView=open_panel("M20TonalStereoPanelPreview.qml");
    QVERIFY(levelView->status()==QQuickView::Ready);
    QVERIFY(stereoView->status()==QQuickView::Ready);
    mount_explicit_demo(*levelView,f.m19);
    mount_explicit_demo(*stereoView,f.m20);
    QVERIFY(levelView->rootObject()->property("available").toBool());
    QVERIFY(stereoView->rootObject()->property("available").toBool());
}
void M19M21TypedQmlConvergenceTest::staleRevisionDeniesQueuedQtFrame() {
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(p,claim(p)));
    IndependentPanelRouting routing;
    const auto previous=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QVERIFY(review_frame_current(store,routing,previous));
    QVERIFY(routing.select(AnalysisPanel::levels_m19,Column::gold));
    QVERIFY(!review_frame_current(store,routing,previous));
    const auto changed=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QVERIFY(review_frame_current(store,routing,changed));
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    mount_explicit_demo(*view,changed.m19);
    QVERIFY(!view->rootObject()->property("available").toBool());
    QVERIFY(changed.m19.value("levels").toMap().isEmpty());
}
void M19M21TypedQmlConvergenceTest::m21ComparisonRendersOnlyEligibleTypedRows() {
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    const auto g=example(Column::gold,3,"g",-13.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.receive(p,claim(p)));
    QVERIFY(store.receive(g,claim(g)));
    IndependentPanelRouting routing;
    const auto frame=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    QVERIFY(review_frame_current(store,routing,frame));
    QCOMPARE(frame.m21.value("metrics").toList().size(),9);
    auto view=open_panel("M21MetricsDeltaPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    mount_explicit_demo(*view,frame.m21);
    QVERIFY(view->rootObject()->property("available").toBool());
    QVERIFY(view->rootObject()->property("comparisonAvailable").toBool());
    QCOMPARE(frame.m21.value("metrics").toList().at(1).toMap()
                 .value("preparedDelta").toString(),QStringLiteral("+5.00"));
    QCOMPARE(frame.m21.value("metrics").toList().at(1).toMap()
                 .value("processed").toString(),QStringLiteral("UNAVAILABLE"));
}
void M19M21TypedQmlConvergenceTest::truePeakIsNeverAuthorizedByTestPreview() {
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(p,claim(p)));
    IndependentPanelRouting routing;
    const auto frame=capture_test_panels(store,routing,qt_review::ExplicitTestFixture{kExplicitFixture});
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    mount_explicit_demo(*view,frame.m19);
    QCOMPARE(frame.m19.value("levels").toMap().value("truePeak").toString(),
             QStringLiteral("UNAVAILABLE"));
    QCOMPARE(frame.m19.value("levelsQuality").toMap().value("truePeak").toString(),
             QStringLiteral("M16 UNQUALIFIED"));
    QVERIFY(!frame.m19.value("productionAuthorized").toBool());
}
void M19M21TypedQmlConvergenceTest::dockHostRemainsPreviewOnly() {
    auto view=open_panel("AnalysisModuleDockHostPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    auto* root=view->rootObject();
    QVERIFY(root);
    QVERIFY(!root->property("fixtureMode").toBool());
    QVERIFY(!root->property("detached").toBool());
    QCOMPARE(root->property("moduleIndex").toInt(),0);
    QVERIFY(root->property("presentation").toMap().value("ready").toBool()==false);
}
void M19M21TypedQmlConvergenceTest::readyPresentationWithoutFixtureRemainsMasked()
{
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    QVERIFY(view->rootObject());
    QVERIFY(view->rootObject()->setProperty("presentation",
        QVariantMap{{"ready",true},{"levels",QVariantMap{{"rms","-18.00"}}}}));
    QVERIFY(view->rootObject()->setProperty("analysisStatus",QStringLiteral("CURRENT")));
    QCoreApplication::processEvents();
    QVERIFY(!view->rootObject()->property("fixtureMode").toBool());
    QVERIFY(!view->rootObject()->property("available").toBool());
}

void M19M21TypedQmlConvergenceTest::staleDisplayStateBlocksEvenReadyDemoData()
{
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    QVariantMap map{{"ready",true},{"levels",QVariantMap{{"rms","-18.00"}}}};
    mount_explicit_demo(*view,map);
    QVERIFY(!view->rootObject()->property("available").toBool());
    QVERIFY(view->rootObject()->setProperty("analysisStatus",QStringLiteral("CURRENT")));
    QVERIFY(view->rootObject()->property("available").toBool());
    QVERIFY(view->rootObject()->setProperty("analysisStatus",QStringLiteral("STALE")));
    QVERIFY(!view->rootObject()->property("available").toBool());
}

void M19M21TypedQmlConvergenceTest::missingStateBlocksValidLookingOldPresenter()
{
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    QVariantMap map{{"ready",true},{"analysisStatus","CURRENT"},
                    {"levels",QVariantMap{{"rms","-18.00"}}}};
    mount_explicit_demo(*view,map);
    QVERIFY(view->rootObject()->property("available").toBool());
    QVERIFY(view->rootObject()->setProperty("analysisStatus",QStringLiteral("MISSING")));
    QVERIFY(!view->rootObject()->property("available").toBool());
}

void M19M21TypedQmlConvergenceTest::revokedFixtureImmediatelyMasksReadouts()
{
    auto view=open_panel("M19LevelsPanelPreview.qml");
    QVERIFY(view->status()==QQuickView::Ready);
    QVariantMap map{{"ready",true},{"analysisStatus","CURRENT"},
                    {"levels",QVariantMap{{"rms","-18.00"}}}};
    mount_explicit_demo(*view,map);
    QVERIFY(view->rootObject()->property("available").toBool());
    QVERIFY(view->rootObject()->setProperty("fixtureMode",false));
    QVERIFY(!view->rootObject()->property("available").toBool());
    QVERIFY(view->rootObject()->setProperty("fixtureMode",true));
    QVERIFY(view->rootObject()->property("available").toBool());
}

} // namespace rgsml::tests
QTEST_MAIN(rgsml::tests::M19M21TypedQmlConvergenceTest)
#include "test_m19_m21_typed_qml_convergence.moc"
