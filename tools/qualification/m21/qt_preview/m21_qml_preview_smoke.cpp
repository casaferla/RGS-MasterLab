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
  auto g=make(Column::gold,-13.);
  auto x=make(Column::processed,-14.);
  auto displayed=to_qt_preview(make_presentation(compose({p,g,x})));
  check(root->setProperty("presentation",displayed),"M21_QML_SET_DATA");
  QCoreApplication::processEvents();
  check(!none->property("visible").toBool()&&table->property("visible").toBool(),
        "M21_QML_PRESENTATION_NOT_VISIBLE");
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
  check(displayed.value("rows").toList()[0].toMap().value("preparedDelta").toString()
        ==QStringLiteral("STALE"),"M21_QML_STALE_NOT_VISIBLE");
  check(root->setProperty("presentation",QVariantMap{{"ready",false}}),"M21_QML_CLEAR");
  QCoreApplication::processEvents();
  check(none->property("visible").toBool()&&!table->property("visible").toBool(),
        "M21_QML_CLEAR_STALE");
  std::cout<<"M21_REAL_QT_PREVIEW_PASS_NOT_GATE_M\n";
}
