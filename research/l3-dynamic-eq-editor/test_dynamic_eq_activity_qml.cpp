#include "dynamic_eq_activity_qt_bridge.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <span>
#include <vector>

using namespace rgsml;
using namespace rgsml::dsp;
using namespace rgsml::dsp::internal;
using namespace rgsml::research::dynamic_ui;
static DynamicEqParameters fixture() {
    DynamicEqBandSpec a;
    auto ida=core::Uuid::parse("11111111-1111-4111-8111-111111111111");
    auto idb=core::Uuid::parse("22222222-2222-4222-8222-222222222222");
    assert(ida && idb);a.band_id=*ida.value();
    a.control.range_db=-9.0;a.control.threshold_dbfs=-52.0;
    a.routing=EqRouting::STEREO;
    DynamicEqBandSpec b;
    b.band_id=*idb.value();
    b.control.range_db=6.0;b.control.threshold_dbfs=-48.0;
    b.routing=EqRouting::LEFT;
    b.filter_type=DynamicEqFilterType::LOW_SHELF;
    b.shape=DynamicEqShelfShape{800.0,0.8};
    auto aa=DynamicEqBandParameters::create(a);
    auto bb=DynamicEqBandParameters::create(b);
    assert(aa && bb);
    auto all=DynamicEqParameters::create({*aa.value(),*bb.value()});
    assert(all);
    return *all.value();
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    QGuiApplication app(argc,argv);
    constexpr auto fs=48000U;
    auto e=C01FrameEngine::create(fixture(),fs,2);
    assert(e);
    C01ActivityHistoryCollector history(fs,0,400);
    std::vector<C01FrameActivitySample> scratch(1024);
    std::vector<double> left(1024),right(1024),out_l(1024),out_r(1024);
    constexpr std::size_t frames=fs*6U;
    for(std::size_t at=0;at<frames;) {
        const auto n=std::min(left.size(),frames-at);
        for(std::size_t i=0;i<n;++i) {
            const auto t=static_cast<double>(at+i);
            const auto beat=std::fmod(t,24000.0)/24000.0;
            const double env=0.015+0.34*std::exp(-20.0*beat);
            left[i]=env*std::sin(2*std::numbers::pi*441.0*t/fs);
            right[i]=0.20*std::cos(2*std::numbers::pi*1101.0*t/fs);
            if((at+i)%24000==0)left[i]+=0.5;
        }
        assert(e.value()->process_block(
            std::span<const double>(left.data(),n),
            std::span<const double>(right.data(),n),
            std::span<double>(out_l.data(),n),
            std::span<double>(out_r.data(),n),
            &history,at,scratch));
        at+=n;
    }
    if(history.status()!=C01HistoryStatus::OK || !history.finish())
        return 3;
    core::PlaybackSnapshot snap;
    snap.state=core::PlaybackState::PLAYING;
    snap.position=core::FrameIndex(fs*5U);
    snap.audibleRealization.phase=core::AudibleHandoffPhase::NEW;
    snap.audibleRealization.realizationId=core::RealizationId{4242};
    C01ActivityRenderProvenance prov{
        core::RealizationId{4242},11,
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,fs,true};
    auto projection=[&](std::uint64_t rev=11) {
        return project_c01_activity(
            history,prov,snap,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            fs,rev,true,false);
    };
    C01ActivityQtBridge qt;
    qt.apply(projection());
    if(!qt.available() || qt.bandCount()!=2 || qt.buckets().size()!=250)
        return 4;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("c01ActivityQt",&qt);
    engine.load(QUrl::fromLocalFile(
        QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    if(engine.rootObjects().size()!=1)return 5;
    QObject* root=engine.rootObjects().first();
    QObject* editor=root->findChild<QObject*>("dynamicEqActivityResearchEditor");
    QObject* companion=root->findChild<QObject*>("dynamicEqActivityCompanion");
    QObject* graph=root->findChild<QObject*>("dynamicEqActivityHistoryGraph");
    QObject* btn=root->findChild<QObject*>("dynamicEqActivityToggleButton");
    auto* mainWindow=qobject_cast<QQuickWindow*>(root);
    auto* side=qobject_cast<QQuickWindow*>(companion);
    if(!editor || !companion || !graph || !btn || !mainWindow || !side)
        return 6;
    mainWindow->show();
    side->show();
    app.processEvents();
    if(!graph->property("graphAvailable").toBool()
       ||!side->isVisible() || !btn->property("enabled").toBool()
       ||!side->transientParent()
       ||side->transientParent()!=mainWindow
       ||editor->property("manualEditable").toBool())
        return 7;
    // An old chain revision must remove the entire real Activity surface.
    qt.apply(projection(10));
    app.processEvents();
    if(qt.available() || btn->property("enabled").toBool()
       ||graph->property("graphAvailable").toBool()
       ||side->isVisible())
        return 8;
    // A new valid accepted projection restores the window, no QML timer.
    qt.apply(projection());
    editor->setProperty("showActivity",true);
    app.processEvents();
    if(!graph->property("graphAvailable").toBool() || !side->isVisible())
        return 9;
    const QString dir=qEnvironmentVariable("RGSML_DYNAMIC_ACTIVITY_SCREENSHOT_DIR");
    if(!dir.isEmpty()) {
        if(!QDir().mkpath(dir))return 10;
        const QImage a=mainWindow->grabWindow();
        const QImage b=side->grabWindow();
        if(a.isNull() || b.isNull()
           ||!a.save(QDir(dir).filePath("DynamicEq-Activity-Main.png"),"PNG")
           ||!b.save(QDir(dir).filePath("DynamicEq-C01-Real-Gain-History.png"),"PNG"))
            return 11;
    }
    std::cout<<"C01 Qt real 5s Activity + provenance gate: PASS\n";
}
