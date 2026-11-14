// I2.6 research gate: genuine Product render_preview.cpp, actual P2 PCM,
// pinned independent P2 oracle; no Qt, no startup, no M17 execution.
#include "p2_epoch_session.hpp"
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_result.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/streaming_true_peak_limiter_parameters.hpp>
#include <rgsml/core/uuid.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace rgsml;
using rgsml::research::r1d_p2::Frame;
using rgsml::research::r1d_p2::Configuration;
using rgsml::research::r1d_p2::EpochSession;
using rgsml::research::r1d_p2::EpochStatus;
constexpr auto kP2="rgsml.dsp.streaming-true-peak-limiter";
constexpr std::size_t kN=4096;
constexpr std::int64_t kOrigin=137;
void need(bool x,const char* why){if(!x)throw std::runtime_error(why);}
core::FrameCount fc(std::int64_t n) {
    auto x=core::FrameCount::create(n);need(bool(x),"frame count");return *x.value();
}
core::FrameRange fr(std::int64_t b,std::int64_t e) {
    auto x=core::FrameRange::create(core::FrameIndex{b},core::FrameIndex{e});
    need(bool(x),"frame range");return *x.value();
}
std::uint64_t bits(double x){return std::bit_cast<std::uint64_t>(x);}
std::vector<std::uint64_t> pcm(audio::AudioBufferView view) {
    std::vector<std::uint64_t> raw;
    for(int c=0;c<2;++c) {
        auto lane=view.channel(c);need(bool(lane),"channel");
        for(double x : *lane.value()) raw.push_back(bits(x));
    }
    return raw;
}
int main() {
  try {
    auto baseline=dsp::ModuleRegistry::create_dsp_package_v1();
    need(bool(baseline),"product registry");
    need(!baseline.value()->find_descriptor(kP2),"P2 leaked to default registry");
    auto registry=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        std::move(*baseline.value()));
    need(bool(registry),"opt-in P2");
    need(bool(registry.value()->find_descriptor("rgsml.dsp.true-peak-limiter")),
         "M17 descriptor missing");
    auto chain=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    need(bool(chain),"manual chain");
    core::Uuid::Bytes bytes{};bytes[15]=13;
    auto id=dsp::ModuleInstanceId::from_uuid(core::Uuid{bytes});
    need(bool(id),"instance id");
    need(bool(chain.value()->add(*id.value(),kP2,0)),"add P2");
    auto rate=core::SampleRate::create(48000);need(bool(rate),"rate");
    auto fmt=audio::AudioFormat::create(*rate.value(),audio::ChannelLayout::STEREO_LR);
    need(bool(fmt),"format");
    auto src=audio::AudioBuffer::create(*fmt.value(),
         audio::FrameDomainId::SOURCE_PROCESSING_RATE,
         core::FrameIndex{kOrigin},fc(kN));
    need(bool(src),"audio source");
    std::vector<Frame> frames(kN);
    for(std::size_t i=0;i<kN;++i) {
      const double t=static_cast<double>(i)/48000.0;
      frames[i][0]=0.65*std::sin(2*3.141592653589793*71*t)
                  +0.45*std::sin(2*3.141592653589793*7587*t);
      frames[i][1]=-0.72*frames[i][0]+0.18*std::cos(2*3.141592653589793*2711*t);
      for(int c=0;c<2;++c) {
        auto lane=src.value()->mutable_view().channel(c);
        need(bool(lane),"fill PCM");
        (*lane.value())[i]=frames[i][c];
      }
    }
    const auto untouched=pcm(src.value()->view());
    auto p=dsp::StreamingTruePeakLimiterParameters::create(-1.0,5.0,2);
    need(bool(p),"P2 typed controls");
    auto render=[&](std::int64_t b,std::int64_t e,int block,bool bypass) {
      need(bool(chain.value()->set_user_bypass(*id.value(),bypass)),"bypass toggle");
      auto req=render::RenderRequest::create(
          src.value()->view(),fr(b,e),*chain.value(),
          {dsp::ModuleExecutionBinding{*id.value(),*p.value()}},fc(block));
      need(bool(req),"RenderRequest typed");
      auto out=render::render_preview(*req.value(),*registry.value());
      need(bool(out),"genuine render_preview");
      const auto& s=out.value()->signatures();
      need(s.size()==1&&s[0].type_id==kP2&&s[0].instance_id==*id.value(),
           "P2 execution identity");
      need(s[0].disposition==(bypass
          ? render::ModuleExecutionDisposition::BYPASS_IDENTITY
          : render::ModuleExecutionDisposition::PROCESSED),"P2 disposition");
      const auto* ps=std::get_if<
          render::StreamingTruePeakLimiterExecutionSignaturePayload>(&s[0].payload);
      need(ps&&ps->ceiling_db_tp==-1.0&&ps->drive_db==5.0&&ps->margin_percent==2,
           "P2 signature params");
      return out;
    };
    auto full=render(kOrigin,kOrigin+kN,79,false);
    const auto fullPcm=pcm(full.value()->view());
    need(fullPcm!=untouched,"active P2 has no effect");
    for(int b : {17,512}) {
      auto other=render(kOrigin,kOrigin+kN,b,false);
      need(pcm(other.value()->view())==fullPcm,"block partition changed P2 PCM");
    }
    EpochSession pinned;
    const Configuration config{48000,2,2,0.8912509381337456,5.0};
    need(pinned.begin_epoch(1,config,false)==EpochStatus::ok,"pinned begin");
    const std::size_t delay=pinned.latency_frames();
    need(delay>0,"pinned latency");
    std::vector<Frame> expected(kN+delay);
    for(std::size_t i=0;i<kN;) {
      const std::size_t n=std::min(std::size_t{79},kN-i);
      need(pinned.process(1,{frames.data()+i,n},{expected.data()+i,n})
           ==EpochStatus::ok,"pinned process");
      i+=n;
    }
    need(pinned.finish(1,{expected.data()+kN,delay})==EpochStatus::ok,
         "pinned finalize");
    for(int c=0;c<2;++c) {
      const auto lane=full.value()->view().channel(c);
      need(bool(lane),"render lane");
      for(std::size_t i=0;i<kN;++i)
        need(bits((*lane.value())[i])==bits(expected[i+delay][c]),
             "audible PCM diverged from independently delayed pinned P2");
    }
    auto mid=render(kOrigin+601,kOrigin+3801,79,false);
    for(int c=0;c<2;++c) {
      auto a=full.value()->view().channel(c);
      auto b=mid.value()->view().channel(c);
      need(bool(a)&&bool(b),"mid channels");
      for(std::size_t i=0;i<3200;++i)
        need(bits((*a.value())[i+601])==bits((*b.value())[i]),
             "midwindow preroll mismatch");
    }
    auto bypass=render(kOrigin,kOrigin+kN,79,true);
    need(pcm(bypass.value()->view())==untouched,"P2 bypass is not bit-exact");
    need(pcm(src.value()->view())==untouched,"source modified by rendering");
    std::cout<<"I2 Product RenderPreview PCM PASS: pinned oracle, EOS, latency, "
                "blocks, mid-window, bypass and signatures\n";
    return 0;
  }catch(const std::exception& e) {
    std::cerr<<"FAIL "<<e.what()<<"\n";return 1;
  }
}
