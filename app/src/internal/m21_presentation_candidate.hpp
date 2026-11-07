#pragma once
// M21 isolated preproduction: C++ presenter of already authenticated/scoped
// feature evidence. NOT a production ViewModel or canonical FeatureStatus.
#include "m21_metrics_comparison_candidate.hpp"
#include <array>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>
namespace rgsml::app::m21_private {
inline const char* label(Quality q) {
  switch(q) {
    case Quality::present:return "CURRENT";
    case Quality::low_confidence:return "LOW SUPPORT";
    case Quality::missing:return "MISSING";
    case Quality::stale:return "STALE";
    case Quality::invalid:return "INVALID";
    case Quality::not_applicable:return "NOT APPLICABLE";
    case Quality::incompatible:return "INCOMPATIBLE";
  }
  return "INVALID";
}
inline std::string printable(const Cell& c,const char* unit,bool is_delta=false) {
  if(!usable(c)) {
    if(c.quality==Quality::present || c.quality==Quality::low_confidence)
      return "INVALID";
    return label(c.quality);
  }
  std::ostringstream os;os.imbue(std::locale::classic());
  if(is_delta && *c.unrounded>0)os<<'+';
  os<<std::fixed<<std::setprecision(2)<<*c.unrounded;
  if(unit && *unit)os<<" "<<unit;
  if(c.quality==Quality::low_confidence)os<<" · LOW SUPPORT";
  return os.str();
}
struct PresentedRow {
  std::string name;
  std::array<std::string,3> values{};
  std::string prepared_delta,processed_delta;
  std::array<std::string,3> statuses{};
};
struct PresentedView {
  bool ready=false;
  std::array<std::string,3> scopes{{"Not analyzed","Not analyzed","Not analyzed"}};
  std::array<std::string,3> states{{"MISSING","MISSING","MISSING"}};
  std::string warning="Independent GOLD analysis scope; no time alignment is implied.";
  std::vector<PresentedRow> rows;
  // Only real producer-owned band IDs; no invented frequency labels or bins.
  std::vector<std::uint32_t> gold_band_ids;
  std::vector<std::string> prepared_band_deltas,processed_band_deltas;
  std::array<std::size_t,3> spectrum_point_counts{};
};
inline PresentedView make_presentation(const Comparison& c) {
  PresentedView o;
  for(std::size_t i=0;i<3;++i) {
    if(!c.columns[i])continue;
    const auto& a=*c.columns[i];
    o.ready=true;
    o.scopes[i]=a.analysis_scope_label.empty()?"Unknown scope":a.analysis_scope_label;
    o.states[i]=a.lifecycle==Lifecycle::current?"CURRENT":
      a.lifecycle==Lifecycle::stale?"STALE":
      a.lifecycle==Lifecycle::invalid?"INVALID":"MISSING";
    if(a.lifecycle==Lifecycle::current)o.spectrum_point_counts[i]=a.average_spectrum.size();
  }
  for(std::size_t k=0;k<kMetricSpecs.size();++k){
    PresentedRow r;const auto& spec=kMetricSpecs[k];r.name=spec.name;
    for(std::size_t i=0;i<3;++i){
      Cell v{};
      if(c.columns[i])v=c.columns[i]->lifecycle==Lifecycle::current?
        c.columns[i]->metrics[k]:state_cell(c.columns[i]->lifecycle);
      r.values[i]=printable(v,spec.absolute_unit);
      r.statuses[i]=label(v.quality);
    }
    r.prepared_delta=printable(c.prepared_to_gold[k],spec.delta_unit,true);
    r.processed_delta=printable(c.processed_to_gold[k],spec.delta_unit,true);
    o.rows.push_back(std::move(r));
  }
  if(c.columns[1] && c.columns[1]->lifecycle==Lifecycle::current)
    for(const auto& band:c.columns[1]->tonal_profile){
      o.gold_band_ids.push_back(band.band_id);
      o.prepared_band_deltas.push_back(printable(tonal_delta(
        c.columns[0]?&*c.columns[0]:nullptr,&*c.columns[1],band.band_id),"dB",true));
      o.processed_band_deltas.push_back(printable(tonal_delta(
        c.columns[2]?&*c.columns[2]:nullptr,&*c.columns[1],band.band_id),"dB",true));
    }
  return o;
}
} // namespace rgsml::app::m21_private
