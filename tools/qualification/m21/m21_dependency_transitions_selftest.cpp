// M21 PRIVATE dependency-transition and non-fabrication regression.
#include "internal/m21_dependency_transitions_candidate.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
using namespace rgsml::app::m21_private;
static Snapshot fixture(Column col,std::uint64_t generation,std::uint32_t fs,double level){
  Snapshot s;s.column=col;s.lifecycle=Lifecycle::current;
  s.generation_token=generation;s.sample_rate_hz=fs;
  s.resource_identity=col==Column::gold?"AUTH-INDEPENDENT-GOLD":
      col==Column::prepared?"AUTH-PREPARED":"AUTH-PROCESSED";
  s.analysis_scope_label=col==Column::gold?"Region":"Full Track";
  s.absolute_scope_begin=col==Column::gold?4003:0;
  s.scope_frames=fs;
  s.metrics[0]={level,Quality::present,{}};
  s.metrics[1]={-18.,Quality::present,{}};
  s.metrics[3]={{},Quality::missing,"M16 true peak ownership not bound"};
  return s;
}
static void assert_current(const Comparison& c,bool p,bool g,bool x) {
  assert(c.columns[0]&&c.columns[1]&&c.columns[2]);
  assert((c.columns[0]->lifecycle==Lifecycle::current)==p);
  assert((c.columns[1]->lifecycle==Lifecycle::current)==g);
  assert((c.columns[2]->lifecycle==Lifecycle::current)==x);
  assert(c.independent_scopes);
}
int main(){
  for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}){
    PrivateDependencyTransitions t;
    auto p=fixture(Column::prepared,1,fs,-18.);
    auto g=fixture(Column::gold,1,44100,-13.);
    auto x=fixture(Column::processed,1,fs,-14.);
    assert(t.begin(Column::prepared)&&t.begin(Column::gold)&&
           t.begin(Column::processed));
    assert(t.publish(g)&&t.publish(x)&&t.publish(p));
    auto c=t.view();assert_current(c,true,true,true);
    assert(*c.prepared_to_gold[0].unrounded==5.);
    assert(*c.processed_to_gold[0].unrounded==1.);
    // Audition Level Match, playback loop and display-only region move
    // MUST NOT change the persisted/per-resource analysis identity.
    for(auto event:{PrivateChange::audition_loop_region_only,
                    PrivateChange::audition_level_match_only,
                    PrivateChange::display_only_selection}){
      assert(t.apply(event));
      assert(t.expected(Column::gold)==1);
      assert(t.expected(Column::prepared)==1);
      assert(t.expected(Column::processed)==1);
      assert_current(t.view(),true,true,true);
    }
    // Mastering chain changed: genuine Processed no longer matches the chain.
    assert(t.apply(PrivateChange::mastering_chain_changed));
    c=t.view();assert_current(c,true,true,false);
    assert(c.processed_to_gold[0].quality==Quality::stale);
    assert(*c.prepared_to_gold[0].unrounded==5.);
    assert(!t.publish(x)); // delayed old async Processed result
    x.generation_token=2;x.metrics[0].unrounded=-12.;
    assert(t.publish(x));
    c=t.view();assert_current(c,true,true,true);
    assert(*c.processed_to_gold[0].unrounded==-1.);
    // Change active Gold: both deltas stale, source artifacts stay current.
    assert(t.apply(PrivateChange::active_gold_replaced));
    c=t.view();assert_current(c,true,false,true);
    assert(c.prepared_to_gold[0].quality==Quality::stale);
    assert(c.processed_to_gold[0].quality==Quality::stale);
    assert(!t.publish(g));
    g.generation_token=2;g.metrics[0].unrounded=-16.;
    assert(t.publish(g));
    c=t.view();assert_current(c,true,true,true);
    assert(*c.prepared_to_gold[0].unrounded==2.);
    // An ACTUAL Gold analysis region change differs from audition loop move:
    assert(t.apply(PrivateChange::gold_analysis_scope_replaced));
    c=t.view();assert_current(c,true,false,true);
    assert(!c.prepared_to_gold[0].unrounded);
    g.generation_token=3;g.absolute_scope_begin=27182;
    assert(t.publish(g));
    assert_current(t.view(),true,true,true);
    // PREPARED artifact replacement ALSO obsoletes Processed. External Gold
    // stays valid; nothing can masquerade as freshly analyzed Processed.
    assert(t.apply(PrivateChange::preparation_realization_replaced));
    c=t.view();assert_current(c,false,true,false);
    assert(c.prepared_to_gold[0].quality==Quality::stale);
    assert(c.processed_to_gold[0].quality==Quality::stale);
    assert(!t.publish(p)&&!t.publish(x));
    p.generation_token=2;p.resource_identity="NEW-AUTH-PREPARED";
    assert(t.publish(p));
    c=t.view();assert_current(c,true,true,false);
    assert(*c.prepared_to_gold[0].unrounded==2.);
    assert(!c.processed_to_gold[0].unrounded);
    x.generation_token=3;x.resource_identity="NEW-AUTH-PROCESSED";
    assert(t.publish(x));
    assert_current(t.view(),true,true,true);
    // Replacing the SOURCE invalidates both descendants, never independent G.
    assert(t.apply(PrivateChange::source_realization_replaced));
    assert_current(t.view(),false,true,false);
    assert(t.expected(Column::gold)==3);
    assert(t.expected(Column::prepared)==3);
    assert(t.expected(Column::processed)==4);
    assert(!t.publish(p)&&!t.publish(x));
    p.generation_token=3;x.generation_token=4;
    assert(t.publish(p)&&t.publish(x));
    assert_current(t.view(),true,true,true);
    assert(t.apply(PrivateChange::display_only_selection));
    assert(t.expected(Column::processed)==4);
    assert(t.view().columns[1]->resource_identity=="AUTH-INDEPENDENT-GOLD");
  }
  std::cout<<"M21_PRIVATE_DEPENDENCY_INVALIDATION_PASS_NOT_GATE_S_OR_M\n";
}
