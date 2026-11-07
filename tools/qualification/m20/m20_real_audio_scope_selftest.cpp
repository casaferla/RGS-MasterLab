// M20 real AudioBufferView scoped reads, frame-domain guards and OUTSIDE poison.
// This checks the existing canonical buffer API, not a new AnalysisScope schema.
#include "internal/m20_audio_scope_adapter_candidate.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>

using namespace rgsml;
using namespace rgsml::analysis::m20_private;
static audio::AudioBuffer make_buffer(std::uint32_t fs,bool stereo,
    audio::FrameDomainId domain,std::uint64_t scope_begin,std::size_t length,
    bool invalid_inside=false)
{
  auto sr=core::SampleRate::create(static_cast<std::int64_t>(fs));
  assert(sr);
  auto f=audio::AudioFormat::create(*sr.value(),
    stereo?audio::ChannelLayout::STEREO_LR:audio::ChannelLayout::MONO_C);
  assert(f);
  constexpr std::uint64_t resource_origin=1000;
  const auto frames=core::FrameCount::create(
      static_cast<std::int64_t>((scope_begin-resource_origin)+length+11));
  assert(frames);
  auto result=audio::AudioBuffer::create(*f.value(),domain,
    core::FrameIndex{static_cast<std::int64_t>(resource_origin)},
    *frames.value());
  assert(result);
  auto editable=result.value()->mutable_view();
  const auto offset=static_cast<std::size_t>(scope_begin-resource_origin);
  const auto poison=(std::numeric_limits<double>::quiet_NaN)();
  for(std::size_t ch=0;ch<(stereo?2U:1U);++ch){
    auto plane=editable.channel(ch);
    assert(plane);
    std::fill(plane.value()->begin(),plane.value()->end(),poison);
    for(std::size_t i=0;i<length;++i){
      const double phase=2*std::numbers::pi*static_cast<double>((i%257U))/257.0;
      (*plane.value())[offset+i]=
        ch==0?0.25*std::sin(phase):0.125*std::cos(phase);
    }
    if(invalid_inside) (*plane.value())[offset+17]=poison;
  }
  return std::move(*result.value());
}
int main(){
  constexpr std::uint64_t origin=1033;
  for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}) {
    const auto geometry=spectral_geometry(fs); assert(geometry);
    const auto count=round_even_div(2ULL*fs,5)+round_even_div(fs,10);
    assert(count>geometry->window);
    const auto fc=core::FrameCount::create(static_cast<std::int64_t>(count));
    assert(fc);
    const auto domain=fs==44100?audio::FrameDomainId::OUTPUT_RATE:
      audio::FrameDomainId::SOURCE_PROCESSING_RATE;
    for(const bool stereo:{false,true}){
      auto resource=make_buffer(fs,stereo,domain,origin,static_cast<std::size_t>(count));
      const auto unchanged=resource.view();
      auto left=unchanged.channel(0);assert(left);
      assert(std::isnan((*left.value())[32]));
      assert(std::isnan((*left.value())[33+count]));
      auto base=analyze_audio_buffer_scope_candidate(resource.view(),
        core::FrameIndex{static_cast<std::int64_t>(origin)},*fc.value(),domain);
      assert(base);
      assert(base.value()->absolute_scope_start==origin);
      assert(base.value()->scope_frames==count);
      assert(base.value()->frame_domain==domain);
      assert(base.value()->spectrum.diagnostic==Diagnostic::valid);
      assert(base.value()->spectrum.all_frames_fallback);
      assert(base.value()->spectrum.selected_absolute_starts[0]==origin);
      if(stereo){
        assert(base.value()->width.diagnostic==Diagnostic::valid);
        assert(base.value()->correlation.diagnostic==Diagnostic::valid);
      }else{
        assert(base.value()->width.diagnostic==Diagnostic::not_applicable);
        assert(base.value()->correlation.diagnostic==Diagnostic::not_applicable);
      }
      const auto windows=base.value()->spectrum.complete_frames;
      std::vector<std::uint8_t> mask(static_cast<std::size_t>(windows),0);
      if(windows>0) mask[0]=1;
      const ExternallyOwnedGateInputs gated{true,mask,false,{}};
      auto selected=analyze_audio_buffer_scope_candidate(resource.view(),
        core::FrameIndex{static_cast<std::int64_t>(origin)},*fc.value(),domain,gated);
      assert(selected&&selected.value()->spectrum.selected_frames==1);
      std::fill(mask.begin(),mask.end(),std::uint8_t{0}); // MSVC /W4 /WX exact element type
      const auto empty=analyze_audio_buffer_scope_candidate(resource.view(),
        core::FrameIndex{static_cast<std::int64_t>(origin)},*fc.value(),domain,gated);
      assert(empty&&empty.value()->spectrum.diagnostic==Diagnostic::insufficient_content);
      assert(!empty.value()->spectrum.all_frames_fallback);
      if(stereo){
        const std::array<Interval,1> outside{{{0,1}}};
        const ExternallyOwnedGateInputs emptyWidth{false,{},true,outside};
        auto zero=analyze_audio_buffer_scope_candidate(resource.view(),
          core::FrameIndex{static_cast<std::int64_t>(origin)},*fc.value(),domain,emptyWidth);
        assert(zero&&zero.value()->width.diagnostic==Diagnostic::insufficient_content);
        assert(!zero.value()->width.used_all_scope_fallback);
      }
      // Deliberately bad mask needs a NEW span after its vector storage grows.
      // Reusing "gated" after mask.push_back would leave a dangling old span
      // and the wrong (old) length: undefined behavior across MSVC/Linux.
      auto mismatchedMask=mask;
      mismatchedMask.push_back(0);
      const ExternallyOwnedGateInputs badGate{true,mismatchedMask,false,{}};
      const auto bad=analyze_audio_buffer_scope_candidate(resource.view(),
        core::FrameIndex{static_cast<std::int64_t>(origin)},*fc.value(),domain,badGate);
      assert(!bad&&bad.error()->code()==core::ErrorCode::InvalidState);
      auto wrongDomain=analyze_audio_buffer_scope_candidate(resource.view(),
        core::FrameIndex{static_cast<std::int64_t>(origin)},*fc.value(),
        domain==audio::FrameDomainId::OUTPUT_RATE?
          audio::FrameDomainId::SOURCE_PROCESSING_RATE:
          audio::FrameDomainId::OUTPUT_RATE);
      assert(!wrongDomain&&wrongDomain.error()->code()==core::ErrorCode::InvalidFrameRange);
      const auto beyond=core::FrameCount::create(static_cast<std::int64_t>(count+12));
      assert(beyond);
      auto out=analyze_audio_buffer_scope_candidate(resource.view(),
        core::FrameIndex{static_cast<std::int64_t>(origin)},*beyond.value(),domain);
      assert(!out);
      auto badAudio=make_buffer(fs,stereo,domain,origin,static_cast<std::size_t>(count),true);
      auto invalid=analyze_audio_buffer_scope_candidate(badAudio.view(),
        core::FrameIndex{static_cast<std::int64_t>(origin)},*fc.value(),domain);
      assert(!invalid&&invalid.error()->code()==core::ErrorCode::InvalidAudioSample);
      // Out-of-scope poison survives all operations untouched.
      auto after=resource.view().channel(0);assert(after);
      assert(std::isnan((*after.value())[32]));
      assert(std::isnan((*after.value())[33+count]));
    }
  }
  std::cout<<"M20_REAL_AUDIOBUFFER_SCOPE_DOMAIN_POISON_PASS_NOT_GATE_G\n";
}
