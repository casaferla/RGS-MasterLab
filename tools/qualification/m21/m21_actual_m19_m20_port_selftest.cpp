// REAL (but UNQUALIFIED) M19 and M20 numerical producer objects -> M21.
#include "internal/m21_actual_m19_m20_port_candidate.hpp"
#include "internal/m20_scope_spectrum_candidate.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>
using namespace rgsml::app::m21_private;
static PortResult actual(std::uint32_t fs,double level,Column col) {
  const std::uint64_t origin=12345;
  std::vector<double> mono(static_cast<std::size_t>(fs),level);
  m19::BasicLevelPreproduction l(fs,1U,origin,fs);
  assert(l.push(std::span<const double>{mono}));
  const auto level_result=l.finalize();assert(level_result);
  m20::ScopeSpectrumInput sp{fs,origin,std::span<const double>{mono},{},false,{}};
  m20::M20ScopeCandidate s{};
  s.absolute_scope_start=origin;s.scope_frames=fs;
  s.spectrum=m20::scope_spectrum_candidate(sp);
  const m20::StereoScope mono_scope{fs,origin,std::span<const double>{mono},{},false,{}};
  s.width=m20::width_ratio(mono_scope);
  s.correlation=m20::correlation_summary(mono_scope);
  assert(s.width.diagnostic==m20::Diagnostic::not_applicable);
  return import_actual_private_m19_m20(*level_result,s,
    {col,1ULL,col==Column::gold?"GOLD-FIXTURE":"PREP-FIXTURE",
     col==Column::gold?"Region":"Full Track",true});
}
int main(){
  for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}){
    auto p=actual(fs,0.125,Column::prepared);
    auto g=actual(fs,0.25,Column::gold);
    assert(p.problem==PortProblem::none && p.snapshot);
    assert(g.problem==PortProblem::none && g.snapshot);
    assert(p.snapshot->metrics[0].quality==Quality::present);
    assert(g.snapshot->metrics[0].quality==Quality::present);
    assert(p.snapshot->metrics[1].quality==Quality::present);
    assert(p.snapshot->metrics[2].quality==Quality::present);
    assert(p.snapshot->metrics[3].quality==Quality::missing);
    assert(!p.snapshot->metrics[3].unrounded);
    assert(p.snapshot->metrics[5].quality==Quality::not_applicable);
    assert(!p.snapshot->average_spectrum.empty());
    assert(p.snapshot->profile_denominator==0);
    auto c=compose({*p.snapshot,*g.snapshot,std::nullopt});
    assert(c.prepared_to_gold[0].quality==Quality::present);
    assert(*c.prepared_to_gold[0].unrounded>5.0);
    assert(*c.prepared_to_gold[0].unrounded<7.0);
    assert(c.processed_to_gold[0].quality==Quality::missing);
    auto data=*p.snapshot;
    data.column=Column::gold; // wrong semantic label cannot masquerade as Prep
    assert(compose({data,*g.snapshot,std::nullopt})
      .prepared_to_gold[0].quality==Quality::invalid);
  }
  // Distinguish a valid numerical M19/M20 candidate from a canonical
  // producer owner: missing independently supplied owner MUST refuse.
  m19::BasicLevelPreproductionSnapshot l{};
  m20::M20ScopeCandidate s{};
  assert(import_actual_private_m19_m20(l,s,{}).problem==
         PortProblem::missing_owner_authority);
  l.scope_begin_frame=100;l.scope_frames=48000;l.sample_rate_hz=48000;
  l.channel_count=1;l.loudness_state=m19::LoudnessPreproductionState::available;
  l.true_peak_db_tp=-1.0;s.absolute_scope_start=100;s.scope_frames=48000;
  s.spectrum.geometry.fs=48000;
  assert(import_actual_private_m19_m20(l,s,
      {Column::gold,5,"owner","Region",true}).problem==
      PortProblem::forbidden_unqualified_tp);
  std::cout<<"M21_REAL_PRIVATE_M19_M20_TYPED_PORT_PASS_NOT_GATE_S\n";
}
