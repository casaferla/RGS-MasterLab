#include "m20_qt_preview_bridge.hpp"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QFontDatabase>
#include <QRawFont>
#include <QFont>
#include <QFileInfo>
#include <QImage>
#include <QEventLoop>
#include <QTimer>
#include <QSet>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <numbers>
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
    s.scope_frames=480000;
    s.spectrum.diagnostic=Diagnostic::valid;
    s.spectrum.geometry=*g;
    s.spectrum.average_psd.resize(static_cast<std::size_t>(g->fft/2+1),1e-6);
    // Rich but explicitly synthetic PSD fixture for visual evidence.
    for(std::size_t k=0;k<s.spectrum.average_psd.size();++k){
      const double hz=static_cast<double>(k)*48000.0/g->fft;
      const double logf=std::log2(std::max(20.0,hz)/20.0);
      const auto hump=[&](double center,double spread,double amplitude){
        const double z=(logf-center)/spread;
        return amplitude*std::exp(-0.5*z*z);
      };
      s.spectrum.average_psd[k]=1.0e-11+hump(3.2,0.5,1.2e-7)
        +hump(5.8,0.65,4.3e-8)+hump(8.6,0.8,9e-9);
    }
    s.spectrum.all_frames_fallback=true;
    s.width.diagnostic=Diagnostic::valid;
    s.width.ratio=2.25;
    s.correlation.diagnostic=Diagnostic::valid;
    s.correlation.window_frames=19200;
    s.correlation.p10=-0.8;s.correlation.p50=0;s.correlation.p90=0.7;
    for(int i=0;i<50;++i){
      const double t=static_cast<double>(i)/49.0;
      const double rho=0.25+0.48*std::sin(2.0*std::numbers::pi*2.6*t);
      s.correlation.series.push_back({
        100+240000ULL+static_cast<std::uint64_t>(i)*4800ULL,
        Diagnostic::valid,rho
      });
    }
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
    // Evidence-only: capture the UNMODIFIED M20 QML from this copied branch,
    // with a genuine Qt renderer, not a redrawn mockup.
    const QString screenshot=qEnvironmentVariable("M20_SCREENSHOT_PATH");
    if(!screenshot.isEmpty()){
      const QString arial=QStringLiteral("C:/Windows/Fonts/arial.ttf");
      require(QFileInfo::exists(arial),"M20_WINDOWS_FONT_FILE_NOT_FOUND");
      const int fontId=QFontDatabase::addApplicationFont(arial);
      require(fontId>=0,"M20_WINDOWS_FONT_REGISTER_FAILED");
      const auto families=QFontDatabase::applicationFontFamilies(fontId);
      require(!families.isEmpty(),"M20_WINDOWS_FONT_FAMILY_EMPTY");
      QFont font(families.first());font.setPointSize(11);
      app.setFont(font);
      const auto glyphs=QRawFont::fromFont(font).glyphIndexesForString(
        QStringLiteral("BASIC TONAL STEREO 123"));
      for(const auto glyph:glyphs)
        require(glyph!=0,"M20_WINDOWS_FONT_GLYPH_MISSING");
      for(const auto& filename:{
          QStringLiteral("C:/Windows/Fonts/segoeui.ttf"),
          QStringLiteral("C:/Windows/Fonts/consola.ttf")})
        if(QFileInfo::exists(filename))
          QFontDatabase::addApplicationFont(filename);
      QQuickView window;
      window.setResizeMode(QQuickView::SizeRootObjectToView);
      window.setSource(QUrl::fromLocalFile(QString::fromUtf8(M20_QML_SOURCE)));
      require(window.status()==QQuickView::Ready,"M20_QML_WINDOW_NOT_READY");
      window.resize(980,598);
      auto* item=window.rootObject();
      require(item!=nullptr,"M20_QML_SCREENSHOT_ROOT_MISSING");
      require(item->setProperty("presentation",to_qt_preview_candidate(presentation)),
        "M20_QML_SCREENSHOT_DATA_BINDING_FAILED");
      window.show();
      QEventLoop loop;QTimer::singleShot(750,&loop,&QEventLoop::quit);loop.exec();
      const auto capture=window.grabWindow();
      require(capture.width()==980&&capture.height()==598,
        "M20_QML_SCREENSHOT_DIMENSIONS");
      QSet<QRgb> distinct;
      for(int y=0;y<capture.height();y+=2)
        for(int x=0;x<capture.width();x+=2)
          distinct.insert(capture.pixel(x,y));
      std::cout<<"M20_SCREENSHOT_DISTINCT_COLORS="<<distinct.size()<<'\n';
      require(distinct.size()>=40,"M20_SCREENSHOT_BLANK_OR_MISSING_FONT");
      require(capture.save(screenshot,"PNG"),"M20_SCREENSHOT_SAVE_FAILED");
      std::cout<<"M20_REAL_WINDOWS_QT_ORIGINAL_QML_DEMO_PNG_SAVED\n";
    }
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
