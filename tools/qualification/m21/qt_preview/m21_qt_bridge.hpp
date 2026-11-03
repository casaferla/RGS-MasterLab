#pragma once
#include "internal/m21_presentation_candidate.hpp"
#include <QVariantMap>
#include <QVariantList>
#include <QString>
namespace rgsml::app::m21_private {
inline QVariantMap to_qt_preview(const PresentedView& v){
  QVariantMap m;
  m.insert("ready",v.ready);
  m.insert("scopeWarning",QString::fromStdString(v.warning));
  QVariantList scopes,states,rows;
  for(const auto& s:v.scopes)scopes.push_back(QString::fromStdString(s));
  for(const auto& s:v.states)states.push_back(QString::fromStdString(s));
  for(const auto& r:v.rows) {
    QVariantMap x;x.insert("name",QString::fromStdString(r.name));
    x.insert("prepared",QString::fromStdString(r.values[0]));
    x.insert("gold",QString::fromStdString(r.values[1]));
    x.insert("processed",QString::fromStdString(r.values[2]));
    x.insert("preparedDelta",QString::fromStdString(r.prepared_delta));
    x.insert("processedDelta",QString::fromStdString(r.processed_delta));
    rows.push_back(x);
  }
  m.insert("scopes",scopes);m.insert("states",states);m.insert("rows",rows);
  return m;
}
}
