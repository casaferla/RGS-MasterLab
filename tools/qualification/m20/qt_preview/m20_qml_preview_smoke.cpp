#include "m20_qt_preview_bridge.hpp"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace rgsml::analysis::m20_private;
static void require(bool v,const char* message){
    if(!v)throw std::runtime_error(message);
}
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    QQmlEngine engine;
    QQmlComponent c(&engine,QUrl::fromLocalFile(
        QString::fromUtf8(M20_QML_SOURCE)));
    if(!c.isReady()){
        for(const auto& e:c.errors())
            std::cerr<<e.toString().toStdString()<<'\n';
        return 2;
    }
    std::unique_ptr<QObject> root(c.create());
    require(root!=nullptr,"M20_QML_COULD_NOT_INSTANTIATE");
    require(root->findChild<QObject*>("m20StaticSpectrumCanvas")!=nullptr,
        "M20_QML_SPECTRUM_CANVAS_MISSING");
    require(root->findChild<QObject*>("m20FractionalBandsCanvas")!=nullptr,
        "M20_QML_PROFILE_CANVAS_MISSING");
    require(root->findChild<QObject*>("m20CorrelationRecentCanvas")!=nullptr,
        "M20_QML_CORRELATION_CANVAS_MISSING");
    QObject* noData=root->findChild<QObject*>("m20NoDataPanel");
    QObject* data=root->findChild<QObject*>("m20DataPanel");
    QObject* width=root->findChild<QObject*>("m20WidthReadout");
    require(noData&&data&&width,"M20_QML_CARDS_MISSING");
    require(noData->property("visible").toBool()&&!data->property("visible").toBool(),
        "M20_QML_UNAVAILABLE_INITIAL_VISIBILITY");
    const auto g=spectral_geometry(48000);
    require(g.has_value(),"M20_QML_GEOMETRY");
    M20ScopeCandidate s;
    s.absolute_scope_start=100;
    s.scope_frames=48000;
    s.spectrum.diagnostic=Diagnostic::valid;
    s.spectrum.geometry=*g;
    s.spectrum.average_psd.resize(static_cast<std::size_t>(g->fft/2+1),1e-6);
    s.spectrum.average_psd[100]=0.0;
    s.spectrum.all_frames_fallback=true;
    s.width.diagnostic=Diagnostic::valid;
    s.width.ratio=2.25;
    s.correlation.diagnostic=Diagnostic::valid;
    s.correlation.window_frames=19200;
    s.correlation.p10=-0.8;s.correlation.p50=0;s.correlation.p90=0.7;
    s.correlation.series.push_back({100,Diagnostic::valid,0.75});
    const auto geom=fractional_band_geometry(48000,12);
    std::vector<std::uint8_t> policy(geom.size(),std::uint8_t{1});
    const auto profile=make_profile_candidate(s.spectrum.average_psd,
       48000,g->fft,12,policy);
    require(profile.diagnostic==ProfileDiagnostic::valid,
        "M20_QML_PROFILE_NOT_VALID");
    auto presentation=make_m20_readonly_view_candidate(s,&profile);
    require(presentation.any_actual_content,"M20_QML_PRESENTER_EMPTY");
    require(root->setProperty("presentation",to_qt_preview_candidate(presentation)),
        "M20_QML_BINDING_FAILURE");
    QCoreApplication::processEvents();
    require(!noData->property("visible").toBool()&&data->property("visible").toBool(),
        "M20_QML_VALID_VISIBILITY");
    require(width->property("text").toString()==QStringLiteral("2.25"),
        "M20_QML_WIDTH_TEXT_NOT_CPP_BACKED");
    s.width.diagnostic=Diagnostic::not_applicable;
    s.width.ratio.reset();
    s.correlation.diagnostic=Diagnostic::not_applicable;
    s.correlation.p10.reset();s.correlation.p50.reset();s.correlation.p90.reset();
    presentation=make_m20_readonly_view_candidate(s,&profile);
    require(root->setProperty("presentation",to_qt_preview_candidate(presentation)),
        "M20_QML_BIND_MONO");
    QCoreApplication::processEvents();
    require(width->property("text").toString()==QString::fromUtf8("N/A · mono"),
        "M20_QML_MONO_FAKES_ZERO");
    require(root->setProperty("presentation",to_qt_preview_candidate(
        M20ReadOnlyViewCandidate{})),"M20_QML_CLEAR_PRESENTATION");
    QCoreApplication::processEvents();
    require(noData->property("visible").toBool()&&!data->property("visible").toBool(),
        "M20_QML_STALE_RESULT_AFTER_WITHDRAWAL");
    std::cout<<"M20_GENUINE_QT_QUICK_PREVIEW_BINDING_PASS_NOT_FQ\n";
}
