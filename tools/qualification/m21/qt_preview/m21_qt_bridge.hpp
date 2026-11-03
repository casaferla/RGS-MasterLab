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
  QVariantList tonal, spectrum;
  for(std::size_t i=0;i<v.gold_band_ids.size();++i){
    QVariantMap b;
    b.insert("bandId",v.gold_band_ids[i]);
    b.insert("preparedDelta",QString::fromStdString(v.prepared_band_deltas[i]));
    b.insert("processedDelta",QString::fromStdString(v.processed_band_deltas[i]));
    tonal.push_back(b);
  }
  for(std::size_t i=0;i<3;++i){
    if(v.states[i]!="CURRENT")
      spectrum.push_back(QString::fromStdString(v.states[i]));
    else if(v.spectrum_point_counts[i]==0)
      spectrum.push_back(QStringLiteral("MISSING"));
    else
      spectrum.push_back(QStringLiteral("AVAILABLE · %1 canonical points")
                          .arg(static_cast<qulonglong>(v.spectrum_point_counts[i])));
  }
  m.insert("tonalBands",tonal);m.insert("spectrumStates",spectrum);
  return m;
}
}
