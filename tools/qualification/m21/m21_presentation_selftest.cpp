#include "internal/m21_presentation_candidate.hpp"
#include <array>
#include <cassert>
#include <iostream>
using namespace rgsml::app::m21_private;
static Snapshot make(Column c,double loud,std::uint32_t fs){
  Snapshot s;s.column=c;s.lifecycle=Lifecycle::current;
  s.resource_identity="private-test-realization";
  s.generation_token=1;s.sample_rate_hz=fs;s.scope_frames=fs*2ULL;
  s.analysis_scope_label=c==Column::gold?"Region":"Full Track";
  s.metrics[0]={loud,Quality::present,{}};
  s.metrics[1]={-20.,Quality::present,{}};
  s.metrics[2]={-4.,Quality::present,{}};
  s.metrics[3]={{},Quality::missing,"M16 unqualified"};
  s.metrics[4]={16.,Quality::present,{}};
  for(std::size_t i=5;i<9;++i)
    s.metrics[i]={{},Quality::not_applicable,"mono"};
  s.profile_denominator=12;
  s.tonal_profile={{10,true,{-8.,Quality::present,{}}},
                   {11,false,{{},Quality::missing,"unsupported"}}};
  s.average_spectrum={{101.,{1.e-8,Quality::present,{}}}};
  return s;
}
int main(){
  for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}){
    auto p=make(Column::prepared,-18.,fs);
    auto g=make(Column::gold,-13.,fs);
    auto x=make(Column::processed,-14.,fs);
    auto o=make_presentation(compose({p,g,x}));
    assert(o.ready && o.rows.size()==9);
    assert(o.rows[0].values[0]=="-18.00 LUFS");
    assert(o.rows[0].values[1]=="-13.00 LUFS");
    assert(o.rows[0].prepared_delta=="+5.00 LU");
    assert(o.rows[0].processed_delta=="+1.00 LU");
    assert(o.rows[3].values[0]=="MISSING");
    assert(o.rows[5].values[0]=="NOT APPLICABLE");
    assert(o.scopes[0]=="Full Track" && o.scopes[1]=="Region");
    assert(o.warning.find("no time alignment")!=std::string::npos);
    assert(o.spectrum_point_counts[1]==1 && o.gold_band_ids.size()==2);
    assert(o.prepared_band_deltas[0]=="0.00 dB");
    assert(o.prepared_band_deltas[1]=="INCOMPATIBLE");
    g.metrics[0].quality=Quality::low_confidence;
    o=make_presentation(compose({p,g,x}));
    assert(o.rows[0].prepared_delta.find("LOW SUPPORT")!=std::string::npos);
    g.lifecycle=Lifecycle::stale;
    o=make_presentation(compose({p,g,x}));
    assert(o.states[1]=="STALE" && o.rows[0].prepared_delta=="STALE");
    assert(o.spectrum_point_counts[1]==0);
    g.lifecycle=Lifecycle::current;x.lifecycle=Lifecycle::stale;
    o=make_presentation(compose({p,g,x}));
    assert(o.rows[0].processed_delta=="STALE" && o.rows[0].prepared_delta!=="STALE");
    o=make_presentation(compose({g,p,x})); // deliberately swapped identities
    assert(o.states[0]=="INVALID" && o.rows[0].values[0]=="INVALID");
    o=make_presentation(compose({std::nullopt,std::nullopt,std::nullopt}));
    assert(!o.ready && o.rows.size()==9);
    for(const auto& r:o.rows)assert(r.prepared_delta=="MISSING");
  }
  std::cout<<"M21_PRIVATE_DISPLAY_PASS_NOT_GATE_S_OR_M\n";
}
