#include "dynamic_eq_research_dynamics_model.hpp"
#include <rgsml/core/uuid.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <cassert>
#include <iostream>

using namespace rgsml;
using namespace rgsml::research::dynamic_ui;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    QGuiApplication app(argc,argv);
    DynamicEqResearchDynamicsModel model;
    if(model.loaded()||model.manualEditable())return 3;
    auto id=core::Uuid::parse("a1000000-0000-4000-8000-000000000001");
    if(!id)return 4;
    dsp::DynamicEqBandSpec s;
    s.band_id=*id.value();
    s.control.base_gain_db=8.0;
    s.control.range_db=2.0;
    auto b=dsp::DynamicEqBandParameters::create(s);
    if(!b)return 5;
    std::vector<dsp::DynamicEqBandParameters> v;
    v.push_back(std::move(*b.value()));
    auto p=dsp::DynamicEqParameters::create(std::move(v));
    if(!p||!model.loadAcceptedParameters(*p.value()))return 6;
    if(!model.validatedSnapshot()
       ||model.values().value("range").toDouble()!=2.0)return 7;
    model.setDraftFieldText("range","12");
    if(model.commitDraft()||model.values().value("range").toDouble()!=2.0)
        return 8; // no manual edits during L3
    model.setResearchManualEditable(true);
    model.setDraftFieldText("range","12");
    if(model.commitDraft()||model.validationField()!="range")return 9;
    model.setDraftFieldText("range","5");
    if(!model.commitDraft()||model.values().value("range").toDouble()!=5)
        return 10;
    for(const auto& bad: {std::pair{"threshold","2"},{"ratio","0.5"},
                          {"rmsTime","600"},{"attack","0.01"},
                          {"release","7000"}}) {
        model.setDraftFieldText(bad.first,bad.second);
        if(model.commitDraft())return 11;
    }
    model.setDraftFieldText("attack","48.5");
    if(!model.commitDraft()||!model.validatedSnapshot()
       ||model.values().value("attack").toDouble()!=48.5)return 12;
    model.setResearchManualEditable(false);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("researchDynamics",&model);
    engine.load(QUrl::fromLocalFile(
        QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
    if(engine.rootObjects().size()!=1)return 13;
    QObject* win=engine.rootObjects().first();
    QObject* ed=win->findChild<QObject*>("dynamicEqResearchEditor");
    QObject* strip=win->findChild<QObject*>("dynamicEqDynamicsStrip");
    if(!ed||!strip||!strip->property("visible").toBool())return 14;
    if(ed->property("manualEditable").toBool())return 15;
    auto* window=qobject_cast<QQuickWindow*>(win);
    if(!window)return 16;
    window->show();app.processEvents();
    const QString dir=qEnvironmentVariable("RGSML_DYNAMIC_CONTROLS_SCREENSHOT_DIR");
    if(!dir.isEmpty()) {
        if(!QDir().mkpath(dir))return 17;
        QImage image=window->grabWindow();
        if(image.isNull()||!image.save(
            QDir(dir).filePath("DynamicEq-C01-Dynamics-Research.png"),"PNG"))
            return 18;
    }
    std::cout<<"C01 frozen typed controls and QML overlay: PASS\n";
    return 0;
}
