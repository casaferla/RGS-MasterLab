// M20 #6: actual M19 snapshot intake must NEVER guess retained intervals.
#include "internal/m20_m19_phase6_gate_port.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <vector>
using namespace rgsml::analysis;
using namespace rgsml::analysis::m20_private;
using namespace rgsml::analysis::internal::m19;
static void check_available_real_m19(){
  constexpr std::uint32_t fs=48000;
  constexpr std::uint64_t origin=93001, frames=48000;
  BasicLevelPreproduction actual_m19(fs,1,origin,frames);
  std::vector<double> pcm(frames);
  for(std::size_t i=0;i<pcm.size();++i)
    pcm[i]=0.25*std::sin(2.0*std::numbers::pi*1000.0*
                        static_cast<double>(i)/fs);
  // Same exact streaming owner: intermediate 1/7/127/4096 chunks.
  for(std::size_t off=0;off<pcm.size();){
    const auto take=std::min<std::size_t>(127U,pcm.size()-off);
    assert(actual_m19.push(std::span<const double>(pcm).subspan(off,take)));
    off+=take;
  }
  const auto snapshot=actual_m19.finalize();
  assert(snapshot&&snapshot->scope_frames==frames);
  assert(snapshot->loudness_state==LoudnessPreproductionState::available);
  assert(snapshot->integrated_lufs.has_value());
  const auto pending=bind_m19_retained_blocks_candidate(*snapshot,{});
  assert(pending.state==M19GatePortState::producer_disclosure_missing);
  assert(!pending.may_analyze()); // An LUFS number is NOT a mask.
  const auto geometry=spectral_geometry(fs);
  assert(geometry);
  const auto complete=1+(frames-geometry->window)/geometry->hop;
  std::vector<std::uint8_t> mask(static_cast<std::size_t>(complete),0);
  mask[0]=1;
  const std::array<Interval,2> blocks{{{origin+10,origin+22000},
                                      {origin+20000,origin+43000}}};
  const auto disclosure=M19RetainedDisclosureCandidate{
    true,origin,frames,fs,1,
    snapshot->k_weighting_bank_member_id,
    snapshot->k_weighting_bank_sha256,blocks,mask};
  const auto ready=bind_m19_retained_blocks_candidate(*snapshot,disclosure);
  assert(ready.state==M19GatePortState::ready&&ready.may_analyze());
  assert(ready.retained_blocks.size()==2);
  assert(ready.frame_mask.size()==complete);
  const auto inputs=ready.borrow();
  assert(inputs.spectral_gate_available&&inputs.width_gate_available);
  const auto spectrum=scope_spectrum_candidate(ScopeSpectrumInput{
    fs,origin,pcm,{},inputs.spectral_gate_available,inputs.spectral_frame_mask});
  assert(spectrum.diagnostic==Diagnostic::valid);
  assert(spectrum.selected_frames==1 && !spectrum.all_frames_fallback);
  // Mutating producer buffers cannot mutate selected owned copies.
  mask[0]=0;
  assert(ready.frame_mask[0]==1);

  auto wrong=disclosure;
  wrong.scope_frames+=1;
  assert(bind_m19_retained_blocks_candidate(*snapshot,wrong).state==
         M19GatePortState::scope_identity_mismatch);
  wrong=disclosure;wrong.sample_rate=44100;
  assert(bind_m19_retained_blocks_candidate(*snapshot,wrong).state==
         M19GatePortState::scope_identity_mismatch);
  wrong=disclosure;wrong.k_weighting_bank_sha256="forged";
  assert(bind_m19_retained_blocks_candidate(*snapshot,wrong).state==
         M19GatePortState::scope_identity_mismatch);
  wrong=disclosure;wrong.spectral_frame_mask=std::span<const std::uint8_t>{};
  assert(bind_m19_retained_blocks_candidate(*snapshot,wrong).state==
         M19GatePortState::invalid_disclosure);
  wrong=disclosure;wrong.retained_blocks={};
  assert(bind_m19_retained_blocks_candidate(*snapshot,wrong).state==
         M19GatePortState::invalid_disclosure);
  const std::array<Interval,1> beyond{{{origin,origin+frames+1}}};
  wrong=disclosure;wrong.retained_blocks=beyond;
  assert(bind_m19_retained_blocks_candidate(*snapshot,wrong).state==
         M19GatePortState::invalid_disclosure);
  const std::array<std::uint8_t,1> malformed{{2}};
  wrong=disclosure;wrong.spectral_frame_mask=malformed;
  assert(bind_m19_retained_blocks_candidate(*snapshot,wrong).state==
         M19GatePortState::invalid_disclosure);
}
static void check_missing_gate_distinctions(){
  BasicLevelPreproductionSnapshot s{};
  s.scope_begin_frame=100;
  s.scope_frames=22000;
  s.sample_rate_hz=48000;
  s.channel_count=2;
  s.k_weighting_bank_member_id="test.bank";
  s.k_weighting_bank_sha256="test.sha";
  s.loudness_state=LoudnessPreproductionState::insufficient_duration;
  const auto unavailable=bind_m19_retained_blocks_candidate(s,{});
  assert(unavailable.state==M19GatePortState::unavailable_explicit_fallback);
  assert(unavailable.may_analyze()&&!unavailable.borrow().spectral_gate_available);
  s.loudness_state=LoudnessPreproductionState::no_absolute_gated_blocks;
  const auto missingProof=bind_m19_retained_blocks_candidate(s,{});
  assert(!missingProof.may_analyze());
  const auto geom=spectral_geometry(48000);assert(geom);
  std::vector<std::uint8_t> zeroMask(
      static_cast<std::size_t>(1+(s.scope_frames-geom->window)/geom->hop),0);
  const auto d=M19RetainedDisclosureCandidate{
    true,100,22000,48000,2,"test.bank","test.sha",{},zeroMask};
  const auto availableEmpty=bind_m19_retained_blocks_candidate(s,d);
  assert(availableEmpty.state==M19GatePortState::available_empty);
  assert(availableEmpty.may_analyze());
  assert(availableEmpty.borrow().spectral_gate_available);
  assert(availableEmpty.borrow().width_gate_available);
  s.loudness_state=LoudnessPreproductionState::available;
  s.integrated_lufs=-18.0;
  const auto contradiction=bind_m19_retained_blocks_candidate(s,d);
  assert(contradiction.state==M19GatePortState::invalid_disclosure);
  s.loudness_state=LoudnessPreproductionState::not_finalized;
  assert(!bind_m19_retained_blocks_candidate(s,{}).may_analyze());
}
int main(){
  check_available_real_m19();
  check_missing_gate_distinctions();
  assert(correct_m20_phase6_feature_identity(
   "rgsml.tonal.profile.normalized.fractional_octave_1_12",
   "rgsml.stereo.width_ratio","rgsml.stereo.correlation.series",12));
  assert(correct_m20_phase6_feature_identity(
   "rgsml.tonal.profile.normalized.fractional_octave_1_3",
   "rgsml.stereo.width_ratio","rgsml.stereo.correlation.series",3));
  assert(!correct_m20_phase6_feature_identity(
   "rgsml.tonal.profile.normalized.fractional_octave_1_3",
   "rgsml.stereo.width_ratio","rgsml.stereo.correlation.series",12));
  std::cout<<"M20_ACTUAL_M19_SCOPE_AND_RETAINED_DISCLOSURE_SEAM_PASS_NOT_GATE_G\n";
}
