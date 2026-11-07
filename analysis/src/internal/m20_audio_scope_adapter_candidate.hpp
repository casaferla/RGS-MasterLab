#ifndef RGSML_M20_PRIVATE_AUDIO_SCOPE_ADAPTER_CANDIDATE_HPP
#define RGSML_M20_PRIVATE_AUDIO_SCOPE_ADAPTER_CANDIDATE_HPP
// M20 PREPRODUCTION ONLY. Read-only bridge to actual AudioBufferView/subview.
// This is NOT a new Phase-6 AnalysisScope carrier or public feature API.
// Retained loudness blocks and gated STFT mask MUST come from qualified owners.
#include "m20_scope_spectrum_candidate.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <span>
#include <utility>
namespace rgsml::analysis::m20_private {

struct ExternallyOwnedGateInputs {
  bool spectral_gate_available = false;
  std::span<const std::uint8_t> spectral_frame_mask{};
  bool width_gate_available = false;
  std::span<const Interval> retained_width_blocks{};
};

struct M20ScopeCandidate {
  std::uint64_t absolute_scope_start = 0;
  std::uint64_t scope_frames = 0;
  audio::FrameDomainId frame_domain = audio::FrameDomainId::SOURCE_PROCESSING_RATE;
  ScopeSpectrumResult spectrum{};
  WidthResult width{};
  CorrelationResult correlation{};
};

[[nodiscard]] inline core::Result<M20ScopeCandidate> analyze_audio_buffer_scope_candidate(
    const audio::AudioBufferView& owner,
    core::FrameIndex absolute_begin,
    core::FrameCount frames,
    audio::FrameDomainId expected_domain,
    ExternallyOwnedGateInputs gates = {})
{
  using Snapshot = M20ScopeCandidate;
  auto fail = [](core::ErrorCode code, const char* message) {
    return core::Result<Snapshot>::failure(core::Error{code,message});
  };
  if(owner.timebase().frame_domain_id()!=expected_domain)
    return fail(core::ErrorCode::InvalidFrameRange,
      "M20 source/output frame-domain ownership mismatch.");
  if(absolute_begin.value()<0||frames.value()<=0)
    return fail(core::ErrorCode::InvalidFrameRange,
      "M20 requested absolute scope range must be nonempty and nonnegative.");

  auto selected=owner.subview(absolute_begin,frames);
  if(!selected) return core::Result<Snapshot>::failure(*selected.error());
  const auto& fmt=selected.value()->format();
  const auto layout=fmt.channel_count();
  if(layout!=1U&&layout!=2U)
    return fail(core::ErrorCode::UnsupportedAudioLayout,"M20 only accepts mono/stereo.");
  const auto rate=fmt.sample_rate().value();
  if(rate<=0||!supported_rate(static_cast<std::uint32_t>(rate)))
    return fail(core::ErrorCode::UnsupportedOperation,"M20 six-rate contract unsupported.");

  const auto lr=selected.value()->channel(0U);
  if(!lr) return core::Result<Snapshot>::failure(*lr.error());
  std::span<const double> rr;
  if(layout==2U) {
    auto got=selected.value()->channel(1U);
    if(!got) return core::Result<Snapshot>::failure(*got.error());
    rr=*got.value();
  }
  const StereoScope stereo{
    static_cast<std::uint32_t>(rate),
    static_cast<std::uint64_t>(absolute_begin.value()),
    *lr.value(),rr,
    gates.width_gate_available,
    gates.retained_width_blocks
  };
  if(!valid_scope(stereo))
    return fail(core::ErrorCode::InvalidAudioSample,
      "M20 selected-scope audio contains a non-finite sample or is invalid.");

  Snapshot result;
  result.absolute_scope_start=static_cast<std::uint64_t>(absolute_begin.value());
  result.scope_frames=static_cast<std::uint64_t>(frames.value());
  result.frame_domain=expected_domain;
  result.spectrum=scope_spectrum_candidate(ScopeSpectrumInput{
    static_cast<std::uint32_t>(rate),result.absolute_scope_start,
    *lr.value(),rr,gates.spectral_gate_available,gates.spectral_frame_mask});
  if(result.spectrum.diagnostic==Diagnostic::invalid_input)
    return fail(core::ErrorCode::InvalidState,
      "M20 canonical spectral gate mask/geometry unqualified or inconsistent.");
  result.width=width_ratio(stereo);
  if(result.width.diagnostic==Diagnostic::invalid_input)
    return fail(core::ErrorCode::InvalidState,
      "M20 broadband support/width private candidate invalid.");
  result.correlation=correlation_summary(stereo);
  if(result.correlation.diagnostic==Diagnostic::invalid_input)
    return fail(core::ErrorCode::InvalidState,
      "M20 correlation private candidate invalid.");
  return core::Result<Snapshot>::success(std::move(result));
}
} // namespace rgsml::analysis::m20_private
#endif
