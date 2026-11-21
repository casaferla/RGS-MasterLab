#ifndef RGSML_M20_PRIVATE_M19_PHASE6_GATE_PORT_HPP
#define RGSML_M20_PRIVATE_M19_PHASE6_GATE_PORT_HPP
// M20 parallel preproduction. Do NOT treat this as canonical Phase-6 registry,
// proof of M19 Gate G or a new loudness retained-block source. M19's current
// snapshot exposes integrated LUFS + counts, NOT retained block identities.
// A separate producer-owned typed disclosure is mandatory for a valid gate.
#include "m20_audio_scope_adapter_candidate.hpp"
#include "m20_normalized_profile_candidate.hpp"
#include "m19_basic_level_candidate.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rgsml::analysis::m20_private {

enum class M19GatePortState : std::uint8_t {
  ready,
  unavailable_explicit_fallback,
  available_empty,
  producer_disclosure_missing,
  scope_identity_mismatch,
  invalid_disclosure
};
struct M19RetainedDisclosureCandidate {
  // Private preproduction evidence from an eventual QUALIFIED M19 producer.
  // Never synthesize these indices/frames in M20 from loudness LUFS/counts.
  bool index_authority_available = false;
  std::uint64_t absolute_scope_begin = 0;
  std::uint64_t scope_frames = 0;
  std::uint32_t sample_rate = 0;
  std::uint32_t channels = 0;
  std::string_view k_weighting_bank_member_id{};
  std::string_view k_weighting_bank_sha256{};
  std::span<const Interval> retained_blocks{};
  std::span<const std::uint8_t> spectral_frame_mask{};
};

struct M19Phase6GatePortCandidate {
  M19GatePortState state = M19GatePortState::invalid_disclosure;
  // Values are copies, never spans into temporary producer-owned storage.
  std::vector<Interval> retained_blocks;
  std::vector<std::uint8_t> frame_mask;
  [[nodiscard]] ExternallyOwnedGateInputs borrow() const noexcept {
    if(state==M19GatePortState::ready || state==M19GatePortState::available_empty)
      return {true,frame_mask,true,retained_blocks};
    return {false,{},false,{}}; // Only the EXPLICIT unavailable state may use this!
  }
  [[nodiscard]] bool may_analyze() const noexcept {
    return state==M19GatePortState::ready ||
      state==M19GatePortState::available_empty ||
      state==M19GatePortState::unavailable_explicit_fallback;
  }
};

[[nodiscard]] inline M19Phase6GatePortCandidate bind_m19_retained_blocks_candidate(
    const internal::m19::BasicLevelPreproductionSnapshot& m19,
    const M19RetainedDisclosureCandidate& from_producer)
{
  using LS=internal::m19::LoudnessPreproductionState;
  M19Phase6GatePortCandidate out;
  if(m19.scope_frames==0 || m19.sample_rate_hz==0 ||
     !supported_rate(m19.sample_rate_hz) ||
     (m19.channel_count!=1 && m19.channel_count!=2) ||
     m19.scope_begin_frame>std::numeric_limits<std::uint64_t>::max()-m19.scope_frames){
    out.state=M19GatePortState::scope_identity_mismatch;
    return out;
  }
  if(m19.loudness_state==LS::not_finalized){
    out.state=M19GatePortState::producer_disclosure_missing;
    return out;
  }
  const bool gate_result_exists =
    m19.loudness_state==LS::available ||
    m19.loudness_state==LS::no_absolute_gated_blocks;

  if(!from_producer.index_authority_available) {
    // Do NOT turn an available M19 integrated LUFS into an ALL_FRAMES fallback.
    if(gate_result_exists) out.state=M19GatePortState::producer_disclosure_missing;
    else out.state=M19GatePortState::unavailable_explicit_fallback;
    return out;
  }
  if(from_producer.absolute_scope_begin!=m19.scope_begin_frame ||
     from_producer.scope_frames!=m19.scope_frames ||
     from_producer.sample_rate!=m19.sample_rate_hz ||
     from_producer.channels!=m19.channel_count ||
     from_producer.k_weighting_bank_member_id!=m19.k_weighting_bank_member_id ||
     from_producer.k_weighting_bank_sha256!=m19.k_weighting_bank_sha256 ||
     m19.k_weighting_bank_sha256.empty() ||
     m19.k_weighting_bank_member_id.empty()){
    out.state=M19GatePortState::scope_identity_mismatch;
    return out;
  }

  // No AVAILABLE mask may be manufactured for an unsupported or incomplete M19
  // loudness calculation: accepting its purported retained blocks is unsafe.
  if(!gate_result_exists) {
    out.state=M19GatePortState::invalid_disclosure;
    return out;
  }
  const auto g=spectral_geometry(m19.sample_rate_hz);
  if(!g) return out;
  const std::uint64_t expected=m19.scope_frames<g->window?0:
    1+(m19.scope_frames-g->window)/g->hop;
  if(from_producer.spectral_frame_mask.size()!=expected) return out;
  for(auto x:from_producer.spectral_frame_mask)
    if(x!=std::uint8_t{0}&&x!=std::uint8_t{1}) return out;

  const auto absolute_end=m19.scope_begin_frame+m19.scope_frames;
  for(auto x:from_producer.retained_blocks){
    if(x.begin>=x.end || x.begin<m19.scope_begin_frame || x.end>absolute_end)
      return out;
  }
  if(m19.loudness_state==LS::no_absolute_gated_blocks){
    if(!from_producer.retained_blocks.empty() ||
       std::any_of(from_producer.spectral_frame_mask.begin(),
                   from_producer.spectral_frame_mask.end(),
                   [](std::uint8_t v){return v!=0;}))
      return out;
  } else {
    if(!m19.integrated_lufs || !std::isfinite(*m19.integrated_lufs) ||
       from_producer.retained_blocks.empty()) return out;
  }

  out.retained_blocks.assign(from_producer.retained_blocks.begin(),
                             from_producer.retained_blocks.end());
  out.frame_mask.assign(from_producer.spectral_frame_mask.begin(),
                        from_producer.spectral_frame_mask.end());
  out.state=m19.loudness_state==LS::available ?
      M19GatePortState::ready : M19GatePortState::available_empty;
  return out;
}

[[nodiscard]] inline bool correct_m20_phase6_feature_identity(
    std::string_view profile, std::string_view width, std::string_view rho,
    int denominator) noexcept
{
  return (denominator==12
        ? profile=="rgsml.tonal.profile.normalized.fractional_octave_1_12"
        : denominator==3
          ? profile=="rgsml.tonal.profile.normalized.fractional_octave_1_3"
          : false) &&
      width=="rgsml.stereo.width_ratio" &&
      rho=="rgsml.stereo.correlation.series";
}
} // namespace rgsml::analysis::m20_private
#endif
