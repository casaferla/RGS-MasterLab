#include "transient_activity_qt_bridge.hpp"
#include "transient_c00_activity_sink.hpp"
#include "transient_research_controls.hpp"
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QVariant>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <span>

using namespace rgsml;
using namespace rgsml::research::transient_ui;

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    QGuiApplication app(argc,argv);
    TransientResearchControls controls;
    assert(!controls.manualEditable());
    controls.setAttackAmountDb(6.0);
    assert(controls.attackAmountDb()==0.0);
    controls.setResearchManualEditable(true);
    controls.setAttackAmountDb(6.0);
    controls.setSustainAmountDb(-3.0);
    assert(controls.attackAmountDb()==6.0);
    assert(controls.sustainAmountDb()==-3.0);
    controls.setAttackAmountDb(13.0);
    assert(controls.attackAmountDb()==6.0);
    controls.setResearchManualEditable(false);
    controls.setSustainAmountDb(5.0);
    assert(controls.sustainAmountDb()==-3.0);

    const auto p=dsp::TransientShaperParameters::create(6.0,-3.0);
    assert(p);
    auto e=dsp::internal::TransientShaperC00Engine::create(
        *p.value(),48000U,1U,256U,true);
    assert(e);
    TransientActivityCollector collector(48000U,0,6);
    TransientC00ActivitySink sink(collector);
    constexpr std::uint64_t frames=2880;
    std::array<double,256> in{},out{};
    for(std::uint64_t offset=0;offset<frames;offset+=256U) {
        const auto n=static_cast<std::size_t>(
            std::min<std::uint64_t>(256U,frames-offset));
        for(std::size_t j=0;j<n;++j) {
            const auto sample=offset+j;
            const double x=double(sample);
            in[j]=0.2*std::sin(2.0*std::numbers::pi*441.0*x/48000.0);
            if(sample%810U==0U)in[j]+=0.6;
        }
        assert(e.value()->process_block(
            std::span<const double>(in.data(),n),{},
            std::span<double>(out.data(),n),{},&sink,offset));
    }
    assert(!sink.observation_failed());
    assert(collector.finish());

    core::PlaybackSnapshot snap;
    snap.state=core::PlaybackState::PLAYING;
    snap.position=core::FrameIndex(1920);
    snap.audibleRealization.phase=core::AudibleHandoffPhase::NEW;
    snap.audibleRealization.realizationId=core::RealizationId{4242};
    const TransientActivityRenderIdentity render{
        core::RealizationId{4242},9,
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,48000U,true};
    auto project=[&](std::uint64_t revision=9) {
        return project_transient_activity(
            collector,render,snap,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            48000U,revision,true,false);
    };
    TransientActivityQtBridge telemetry;
    telemetry.apply(project());
    assert(telemetry.activityTelemetryAvailable());
    assert(telemetry.activityHistory().size()==2);
    assert(telemetry.telemetryRealizationId()==QStringLiteral("4242"));
    assert(telemetry.audibleFrame()==1920);
    assert(telemetry.activityHistory().first().toMap()
            .value(QStringLiteral("attackMaxDb")).toDouble()>0.0);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("researchControls"),&controls);
    engine.rootContext()->setContextProperty(
        QStringLiteral("researchTelemetry"),&telemetry);
    engine.load(QUrl::fromLocalFile(
        QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    if(engine.rootObjects().size()!=1) {
        std::cerr<<"Cannot load QML ResearchHarness\n";
        return 3;
    }
    QObject* root=engine.rootObjects().at(0);
    QObject* editor=root->findChild<QObject*>(
        QStringLiteral("transientShaperEditorResearch"));
    QObject* scope=root->findChild<QObject*>(
        QStringLiteral("transientShaperActivityScope"));
    if(!editor || !scope)return 4;
    app.processEvents();
    if(editor->property("editorEnabled").toBool()
        || !scope->property("graphAvailable").toBool())return 5;
    if(!(scope->property("currentAttackDb").toDouble()>0.0))return 6;
    controls.setResearchManualEditable(true);
    app.processEvents();
    if(!editor->property("editorEnabled").toBool())return 7;
    controls.setResearchManualEditable(false);

    telemetry.apply(project(8));
    app.processEvents();
    if(scope->property("graphAvailable").toBool())return 8;
    snap.audibleRealization.realizationId=core::RealizationId{99};
    telemetry.apply(project());
    app.processEvents();
    if(scope->property("graphAvailable").toBool())return 9;
    snap.audibleRealization.realizationId=core::RealizationId{4242};
    snap.state=core::PlaybackState::PAUSED;
    telemetry.apply(project());
    app.processEvents();
    if(!scope->property("graphAvailable").toBool())return 10;
    std::cout<<"Qt/QML authentic C00 bridge + manual gate: PASS\n";
    return 0;
}
