// M22 isolated integration seam: uses the REAL M18 private codec, no substitute dither.
// Input here is a deterministic prepared terminal-fixture, NOT M17 real audio.
#include "m22_request_plan_candidate.hpp"
#include "m18_terminal_sample_codec.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rgsml::m22_private;
using namespace rgsml::audio::internal;
#define CHECK(x) do {if(!(x)) throw std::runtime_error(std::string("M22_M18_FAIL_LINE_")+std::to_string(__LINE__));}while(0)

static Intent settings(std::uint32_t hz,std::uint8_t channels,Format format) {
    Intent r;
    r.source_identity="source";r.source_revision="source-rev";
    r.prepared_identity="prepared";r.prepared_revision="prepared-rev";
    r.chain_identity="chain";r.chain_revision=3;
    r.source_frames=5000; r.window={0,5000};r.mode=Mode::FULL;
    r.processing_rate_hz=hz;r.target_rate_hz=hz;r.channels=channels;
    r.terminal_limiter_signature="M17-candidate";r.terminal_limiter_parameters="ceiling";
    r.execution_policy_signature="binary64-original";
    r.format=format;
    if(format==Format::PCM24) r.dither_seed=UINT64_C(0x7711221133554466);
    return r;
}
static std::vector<double> fixture(std::size_t frames, std::uint8_t channels) {
    std::vector<double> x(frames*channels);
    for(std::size_t f=0;f<frames;++f)
        for(std::size_t c=0;c<channels;++c) {
            // Rational, representable binary fractions, plus a short state/transient-like
            // waveform pattern; NO substitute mastering algorithm is executed here.
            const auto signedNumber=static_cast<int>((f*37U+c*197U)%1529U)-764;
            x[f*channels+c]=static_cast<double>(signedNumber)/1024.0;
        }
    x[0]=-0.0;
    if(x.size()>1)x[1]=0x1p-149;
    return x;
}
static std::vector<std::uint8_t> encode(
    const Intent& in,std::span<const double> whole,std::size_t chunk) {
    Failure failure{};
    auto resolved=ResolvedRenderRequest::freeze(in,failure);
    CHECK(resolved && failure==Failure::NONE);
    auto plan=RenderExecutionPlan::preflight(*resolved,failure);
    CHECK(plan && failure==Failure::NONE);
    const std::size_t channels=in.channels;
    const std::size_t bytesPerSample=in.format==Format::PCM24 ? 3U:4U;
    const auto frames=static_cast<std::size_t>(plan->logical_output_frames());
    std::vector<std::uint8_t> out(frames*channels*bytesPerSample,0xCA);
    CHECK(whole.size()==static_cast<std::size_t>(in.source_frames)*channels);
    for(std::size_t pos=0;pos<frames;) {
        const auto count=std::min(chunk,frames-pos);
        const auto absolute=plan->absolute_dither_first_frame()+pos;
        const auto samples=whole.subspan((static_cast<std::size_t>(in.window.first)+pos)*channels,
                                         count*channels);
        auto bytes=std::span<std::uint8_t>{out}.subspan(pos*channels*bytesPerSample,
                                                        count*channels*bytesPerSample);
        auto status=in.format==Format::PCM24
            ?m18_pcm24_block(samples,channels,absolute,*in.dither_seed,bytes)
            :m18_float32_block(samples,channels,absolute,bytes);
        CHECK(status==M18CodecStatus::OK);
        pos+=count;
    }
    return out;
}
int main() {
 try {
    constexpr std::array<std::uint32_t,6> rates{
      44100,48000,88200,96000,176400,192000
    };
    for(auto hz:rates)for(auto ch:{std::uint8_t{1},std::uint8_t{2}})
    for(auto fmt:{Format::PCM24,Format::FLOAT32}) {
       auto in=settings(hz,ch,fmt);
       const auto src=fixture(5000,ch);
       const auto full=encode(in,src,4096);
       CHECK(full==encode(in,src,1));
       CHECK(full==encode(in,src,73));
       in.mode=Mode::PREVIEW;
       const auto bytesPer=fmt==Format::PCM24 ? 3U:4U;
       for(auto range:{Window{0,144},Window{743,4179},Window{4711,5000}}) {
          in.window=range;
          const auto part=encode(in,src,57);
          CHECK(part==encode(in,src,1));
          CHECK(part==encode(in,src,4096));
          auto first=static_cast<std::size_t>(range.first)*ch*bytesPer;
          auto len=static_cast<std::size_t>(range.end-range.first)*ch*bytesPer;
          CHECK(std::equal(part.begin(),part.end(),full.begin()+static_cast<std::ptrdiff_t>(first)));
          CHECK(part.size()==len);
          if(fmt==Format::PCM24 && range.first>0) {
              // Counterexample: incorrect local reseed must diverge from REAL M18 absolute dither.
              std::vector<std::uint8_t> wrong(len);
              auto samples=std::span<const double>{src}.subspan(
                  static_cast<std::size_t>(range.first)*ch,
                  static_cast<std::size_t>(range.end-range.first)*ch);
              CHECK(m18_pcm24_block(samples,ch,0,*in.dither_seed,wrong)==M18CodecStatus::OK);
              CHECK(wrong!=part);
          }
       }
       if(fmt==Format::PCM24) {
          auto bad=src;
          bad[250]=2.0;  // categorical M18 overload, not saturation
          std::vector<std::uint8_t> protectedBytes(bad.size()*3U,0xAE);
          auto status=m18_pcm24_block(bad,ch,0,*in.dither_seed,protectedBytes);
          CHECK(status==M18CodecStatus::PCM24_OVERLOAD);
          CHECK(std::all_of(protectedBytes.begin(),protectedBytes.end(),
                     [](std::uint8_t v){return v==0xAE;}));
       } else {
          auto negative=m18_convert_float32(-0.0);
          CHECK(negative.status==M18CodecStatus::OK && negative.bits==0x80000000U);
          auto large=m18_convert_float32(1.125);
          CHECK(large.status==M18CodecStatus::OK);
          auto bad=src;
          bad[250]=std::numeric_limits<double>::infinity();
          std::vector<std::uint8_t> protectedBytes(bad.size()*4U,0xAE);
          auto status=m18_float32_block(bad,ch,0,protectedBytes);
          CHECK(status==M18CodecStatus::NONFINITE_INPUT);
          CHECK(std::all_of(protectedBytes.begin(),protectedBytes.end(),
                     [](std::uint8_t v){return v==0xAE;}));
       }
    }
    std::cout<<"M22_PRIVATE_REAL_M18_ABSOLUTE_CROP_ALL_SIX_RATES_PASS_NOT_GATE_I_G_L\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
