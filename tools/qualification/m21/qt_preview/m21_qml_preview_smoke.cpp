#include "m21_qt_bridge.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
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
  check(processedScope->property("text").toString().contains("CURRENT"),
        "M21_QML_PROCESSED_STATE");
  check(tonal->property("count").toInt()==1,"M21_QML_TONAL_BAND_LIST");
  const auto bands=displayed.value("tonalBands").toList();
  check(bands.size()==1 && bands[0].toMap().value("preparedDelta").toString()
       ==QStringLiteral("+2.00 dB"),"M21_QML_CPP_TONAL_DELTA");
  check(spectrum->property("text").toString().contains("MISSING"),
        "M21_QML_SPECTRUM_MISSING_NOT_ZERO");
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
