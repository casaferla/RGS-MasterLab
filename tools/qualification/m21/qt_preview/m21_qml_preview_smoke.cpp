#include "m21_qt_bridge.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickView>
#include <QQuickItem>
#include <QEventLoop>
#include <QTimer>
#include <QImage>
#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QSet>
#include <QFileInfo>
#include <QString>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace rgsml::app::m21_private;
static void check(bool x,const char* msg){if(!x)throw std::runtime_error(msg);}
static Snapshot make(Column column,double lufs) {
  Snapshot x;x.column=column;x.lifecycle=Lifecycle::current;
  x.resource_identity="private-fixture";x.analysis_scope_label="Full Track";
  x.generation_token=1;x.sample_rate_hz=48000;x.scope_frames=48000;
  x.metrics[0]={lufs,Quality::present,{}};
  for(std::size_t i=5;i<9;++i)x.metrics[i]={{},Quality::not_applicable,"mono"};
  return x;
}
int main(int argc,char** argv){
  QGuiApplication app(argc,argv);
  // Font atlas guard is only for the actual Windows screenshot lane.
  // Linux offscreen Qt does not require Microsoft's Arial to test QML.
  if(!qEnvironmentVariable("M21_SCREENSHOT_PATH").isEmpty()){
    // Offscreen Windows Qt may not auto-enumerate system font families.
    // Register a concrete installed system font into the Qt font database.
    const QString arialPath=QStringLiteral("C:/Windows/Fonts/arial.ttf");
    std::cerr<<"M21_FONT_DIAG arial-file-exists="<<QFileInfo::exists(arialPath)<<'\n';
    const auto fontId=QFontDatabase::addApplicationFont(arialPath);
    const auto fromFile=fontId>=0 ?
        QFontDatabase::applicationFontFamilies(fontId) : QStringList{};
    const auto available=QFontDatabase::families();
    std::cerr<<"M21_FONT_DIAG loaded-id="<<fontId
             <<" loaded-count="<<fromFile.size()
             <<" system-count="<<available.size()<<'\n';
    const QString family=!fromFile.isEmpty()?fromFile.front() :
        available.contains(QStringLiteral("Arial")) ?
        QStringLiteral("Arial") :
        available.contains(QStringLiteral("Segoe UI")) ?
        QStringLiteral("Segoe UI") : QString{};
    check(!family.isEmpty(),"M21_WINDOWS_FONT_LOAD_FAILED");
    std::cerr<<"M21_FONT_DIAG family="<<family.toStdString()<<'\n';
    QFont actualFont(family);
    actualFont.setPointSize(10);
    app.setFont(actualFont);
    const auto rawFont=QRawFont::fromFont(actualFont);
    check(rawFont.isValid(),"M21_FONT_RAW_INVALID");
    const auto glyphs=rawFont.glyphIndexesForString(QStringLiteral("MASTER 123"));
    for(const auto glyph:glyphs)
      check(glyph!=0,"M21_FONT_MISSING_ASCII_GLYPHS");
  }
  QQmlEngine engine;
  QQmlComponent component(&engine,QUrl::fromLocalFile(QString::fromUtf8(M21_QML_SOURCE)));
  if(!component.isReady()) {
    for(const auto& e:component.errors())std::cerr<<e.toString().toStdString()<<'\n';
    return 2;
  }
  std::unique_ptr<QObject> root(component.create());
  check(root!=nullptr,"M21_QML_CREATE_FAILED");
  auto* none=root->findChild<QObject*>("m21NoData");
  auto* table=root->findChild<QObject*>("m21MetricsTable");
  auto* warning=root->findChild<QObject*>("m21ScopeWarning");
  check(none&&table&&warning,"M21_QML_COMPONENTS_MISSING");
  check(none->property("visible").toBool()&&!table->property("visible").toBool(),
        "M21_QML_INITIAL_MISSING");
  auto p=make(Column::prepared,-18.);
  auto g=make(Column::gold,-13.);g.analysis_scope_label="Region";
  auto x=make(Column::processed,-14.);
  // Private canonical-shape fixture: never production auditory data.
  p.profile_denominator=12;g.profile_denominator=12;x.profile_denominator=12;
  p.common_comparison_support_id="UI-TEST-SUPPORT";
  g.common_comparison_support_id="UI-TEST-SUPPORT";
  x.common_comparison_support_id="UI-TEST-SUPPORT";
  p.tonal_profile={{4,true,{-3.0,Quality::present,{}}}};
  g.tonal_profile={{4,true,{-1.0,Quality::present,{}}}};
  x.tonal_profile={{4,true,{-2.0,Quality::present,{}}}};
  auto displayed=to_qt_preview(make_presentation(compose({p,g,x})));
  check(root->setProperty("presentation",displayed),"M21_QML_SET_DATA");
  QCoreApplication::processEvents();
  check(!none->property("visible").toBool()&&table->property("visible").toBool(),
        "M21_QML_PRESENTATION_NOT_VISIBLE");
  auto* prepScope=root->findChild<QObject*>("m21PreparedScope");
  auto* goldScope=root->findChild<QObject*>("m21GoldScope");
  auto* processedScope=root->findChild<QObject*>("m21ProcessedScope");
  auto* tonal=root->findChild<QObject*>("m21TonalList");
  auto* spectrum=root->findChild<QObject*>("m21SpectrumAvailability");
  check(prepScope&&goldScope&&processedScope&&tonal&&spectrum,
        "M21_QML_SCOPE_TONAL_SPECTRUM_COMPONENTS_MISSING");
  check(prepScope->property("text").toString().contains("Full Track"),
        "M21_QML_PREPARED_SCOPE");
  check(goldScope->property("text").toString().contains("Region"),
        "M21_QML_EXTERNAL_GOLD_SCOPE");
  check(goldScope->property("text").toString().startsWith("REFERENCE"),
        "M21_QML_VISIBLE_REFERENCE_SCOPE_EXPECTED");
  check(!goldScope->property("text").toString().contains("GOLD"),
        "M21_QML_INTERNAL_GOLD_LEAK_IN_PRESENTATION");
  check(processedScope->property("text").toString().contains("CURRENT"),
        "M21_QML_PROCESSED_STATE");
  check(tonal->property("count").toInt()==1,"M21_QML_TONAL_BAND_LIST");
  const auto bands=displayed.value("tonalBands").toList();
  check(bands.size()==1 && bands[0].toMap().value("preparedDelta").toString()
       ==QStringLiteral("+2.00 dB"),"M21_QML_CPP_TONAL_DELTA");
  check(spectrum->property("text").toString().contains("MISSING"),
        "M21_QML_SPECTRUM_MISSING_NOT_ZERO");
  // GENUINE Qt-rendered screenshot only when the runner provides a path.
  // All visualized numbers are explicit PRIVATE DEMONSTRATION fixtures.
  const QString screenshotPath=qEnvironmentVariable("M21_SCREENSHOT_PATH");
  if(!screenshotPath.isEmpty()){
    auto demoP=p;auto demoG=g;auto demoX=x;
    for(auto* item:{&demoP,&demoG,&demoX}){
      item->metrics[1]={-18.8,Quality::present,{}};
      item->metrics[2]={-1.7,Quality::present,{}};
      item->metrics[3]={{},Quality::missing,"M16 authoritative result not bound"};
      item->metrics[4]={17.1,Quality::present,{}};
      item->metrics[5]={1.04,Quality::present,{}};
      item->metrics[6]={0.18,Quality::present,{}};
      item->metrics[7]={0.64,Quality::present,{}};
      item->metrics[8]={0.92,Quality::present,{}};
    }
    demoP.metrics[1].unrounded=-20.2;
    demoG.metrics[1].unrounded=-14.9;
    demoG.metrics[2].unrounded=-0.9;
    demoX.metrics[1].unrounded=-15.7;
    demoX.metrics[2].unrounded=-1.1;
    demoP.average_spectrum={{100.,{1e-6,Quality::present,{}}}};
    demoG.average_spectrum={{100.,{2e-6,Quality::present,{}}}};
    demoX.average_spectrum={{100.,{1.5e-6,Quality::present,{}}}};
    for(std::uint32_t id:{5U,6U,7U,8U}){
      demoP.tonal_profile.push_back({id,true,{-4.0,Quality::present,{}}});
      demoG.tonal_profile.push_back({id,true,{-1.1,Quality::present,{}}});
      demoX.tonal_profile.push_back({id,true,{-2.3,Quality::present,{}}});
    }
    const auto demo=to_qt_preview(make_presentation(compose({demoP,demoG,demoX})));
    QQuickView screenshotView;
    screenshotView.setResizeMode(QQuickView::SizeRootObjectToView);
    screenshotView.setSource(QUrl::fromLocalFile(QString::fromUtf8(M21_QML_SOURCE)));
    check(screenshotView.status()==QQuickView::Ready,"M21_SCREENSHOT_QML_NOT_READY");
    screenshotView.resize(1100,765);
    check(screenshotView.rootObject()!=nullptr,"M21_SCREENSHOT_ROOT_ABSENT");
    check(screenshotView.rootObject()->setProperty("presentation",demo),
          "M21_SCREENSHOT_BINDING_FAILED");
    screenshotView.show();
    QEventLoop loop;
    QTimer::singleShot(350,&loop,&QEventLoop::quit);
    loop.exec();
    const auto capture=screenshotView.grabWindow();
    check(!capture.isNull(),"M21_SCREENSHOT_GRAB_NULL");
    check(capture.width()>=1000 && capture.height()>=700,
          "M21_SCREENSHOT_WRONG_SIZE");
    // Font-atlas sanity: a rendered 1100x765 screenshot with only a handful
    // of RGB colors was a genuine CI failure: every text glyph was a box.
    // Sample actual pixels, not just QML labels or test harness strings.
    QSet<QRgb> distinctPixels;
    for(int yy=0;yy<capture.height();yy+=2)
      for(int xx=0;xx<capture.width();xx+=2)
        distinctPixels.insert(capture.pixel(xx,yy));
    check(distinctPixels.size()>=30,
          "M21_SCREENSHOT_FONT_ATLAS_BOX_GLYPHS_OR_BLANK_UI");
    check(capture.save(screenshotPath,"PNG"),"M21_SCREENSHOT_PNG_WRITE_FAILED");
    std::cout<<"M21_REAL_QT_RENDERED_SCREENSHOT_PRIVATE_FIXTURE_PNG_SAVED\n";
  }
  auto rows=displayed.value("rows").toList();
  check(rows.size()==9,"M21_QML_NINE_METRICS");
  check(rows[0].toMap().value("preparedDelta").toString()==QStringLiteral("+5.00 LU"),
        "M21_QML_CPP_DELTA");
  check(rows[5].toMap().value("prepared").toString()==QStringLiteral("NOT APPLICABLE"),
        "M21_QML_MONO_STATUS");
  g.lifecycle=Lifecycle::stale;
  displayed=to_qt_preview(make_presentation(compose({p,g,x})));
  check(root->setProperty("presentation",displayed),"M21_QML_SET_STALE");
  QCoreApplication::processEvents();
  check(goldScope->property("text").toString().contains("STALE"),
        "M21_QML_GOLD_SCOPE_STALE_NOT_VISIBLE");
  check(tonal->property("count").toInt()==0,
        "M21_QML_TONAL_STALE_NOT_CLEARED");
  check(displayed.value("rows").toList()[0].toMap().value("preparedDelta").toString()
        ==QStringLiteral("STALE"),"M21_QML_STALE_NOT_VISIBLE");
  check(root->setProperty("presentation",QVariantMap{{"ready",false}}),"M21_QML_CLEAR");
  QCoreApplication::processEvents();
  check(none->property("visible").toBool()&&!table->property("visible").toBool(),
        "M21_QML_CLEAR_STALE");
  std::cout<<"M21_REAL_QT_PREVIEW_PASS_NOT_GATE_M\n";
}
