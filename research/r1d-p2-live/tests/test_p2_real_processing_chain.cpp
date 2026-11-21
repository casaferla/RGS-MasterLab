// RGS R1d-P2 I1.7 research only: genuine ProcessingChain + genuine
// ModuleRegistry::create() with a TEST-ONLY registration set. This does not
// modify ModuleRegistry::create_dsp_package_v1() or product DSP wiring.
#include "research_p2_imodule.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

namespace dsp = rgsml::dsp;
namespace core = rgsml::core;
namespace audio = rgsml::audio;
using rgsml::research::r1d_p2::ResearchP2IModule;
using rgsml::research::r1d_p2::Configuration;
using rgsml::research::r1d_p2::Frame;
using rgsml::research::r1d_p2::EpochSession;
using rgsml::research::r1d_p2::EpochStatus;

static void need(bool value, const char* why) {
    if(!value) throw std::runtime_error(why);
}
static dsp::ModuleInstanceId instance_id(unsigned n) {
    core::Uuid::Bytes bytes{};
    bytes[0]=0x12;
    bytes[15]=static_cast<std::uint8_t>(n);
    auto id=dsp::ModuleInstanceId::from_uuid(core::Uuid{bytes});
    need(bool(id),"instance ID");
    return *id.value();
}
static dsp::ModuleDescriptor make_desc(std::string type,
        dsp::ModuleCategory category, bool last=false, bool terminal=false) {
    dsp::ModuleDescriptorSpec s{
        type, type+".display-name", {category},
        {dsp::ProcessingStage::MASTER},
        {terminal ? dsp::ChainSegment::TERMINAL : dsp::ChainSegment::MANUAL},
        !last && !terminal, true,
        terminal ? dsp::PlacementClass::TERMINAL_SLOT : dsp::PlacementClass::INLINE_CHAIN,
        terminal ? std::optional<dsp::TerminalSlot>{dsp::TerminalSlot::FINAL_TRUE_PEAK_LIMITER} : std::nullopt,
        {},{}, {},{},last, last || terminal,
        std::optional<std::string>{"0.1.0-research"},
        std::optional<std::string>{type+".params/0.1.0"}
    };
    auto d=dsp::ModuleDescriptor::create(std::move(s));
    need(bool(d),"module descriptor creation");
    return std::move(*d.value());
}
static constexpr const char* kLive="rgsml.dsp.streaming-true-peak-limiter";
static constexpr const char* kCompressor="rgsml.dsp.compressor";
static constexpr const char* kStereo="rgsml.dsp.stereo-ms";
static constexpr const char* kFinal="rgsml.dsp.true-peak-limiter";

class ResearchFactory final : public dsp::IModuleFactory {
public:
    explicit ResearchFactory(dsp::ModuleDescriptor d):descriptor_(std::move(d)){}
    std::string_view module_type_id() const noexcept override {return descriptor_.type_id();}
    core::Result<std::unique_ptr<dsp::IModule>> create() const override {
        std::unique_ptr<dsp::IModule> p=std::make_unique<ResearchP2IModule>(
            descriptor_, Configuration{48000,2,2,0.8912509381337456,0.0},false);
        return core::Result<std::unique_ptr<dsp::IModule>>::success(std::move(p));
    }
private:
    dsp::ModuleDescriptor descriptor_;
};

static dsp::ModuleRegistry registry() {
    auto compressor=make_desc(kCompressor,dsp::ModuleCategory::DYNAMICS);
    auto stereo=make_desc(kStereo,dsp::ModuleCategory::SPATIAL);
    auto live=make_desc(kLive,dsp::ModuleCategory::DYNAMICS,true);
    auto terminal=make_desc(kFinal,dsp::ModuleCategory::OUTPUT,false,true);
    std::vector<dsp::ModuleRegistration> items;
    items.push_back({std::move(compressor),{}});
    items.push_back({std::move(stereo),{}});
    auto factory=std::make_shared<ResearchFactory>(live);
    items.push_back({std::move(live),std::move(factory)});
    items.push_back({std::move(terminal),{}});
    auto r=dsp::ModuleRegistry::create(std::move(items));
    need(bool(r),"genuine ModuleRegistry::create");
    return std::move(*r.value());
}
static void metadata_tests(const dsp::ModuleRegistry& registry) {
    need(registry.has_factory(kLive),"P2 test factory not visible");
    need(!registry.has_factory(kFinal),"terminal has unwarranted factory");
    auto chain=dsp::ProcessingChain::create(registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    need(bool(chain),"create manual processing chain");
    need(chain.value()->revision()==0,"new chain revision");
    need(bool(chain.value()->add(instance_id(1),kCompressor,0)),"insert compressor");
    need(bool(chain.value()->add(instance_id(2),kStereo,1)),"insert stereo");
    need(bool(chain.value()->add(instance_id(3),kLive,2)),"append P2");
    need(chain.value()->instances().size()==3,"manual length");
    need(chain.value()->instances().back().module_type_id()==kLive,"P2 is not last DSP");
    const auto version=chain.value()->revision();
    // Atomic refusal: metadata and revision cannot change after an invalid move.
    need(!bool(chain.value()->move(instance_id(3),0)),"P2 moved in front of DSPs");
    need(chain.value()->revision()==version,"revision changed on invalid move");
    need(!bool(chain.value()->add(instance_id(4),kCompressor,3)),"DSP inserted after P2");
    need(chain.value()->revision()==version,"revision changed on invalid add");
    need(!bool(chain.value()->add(instance_id(4),kFinal,3)),"terminal M17 entered MANUAL");
    need(chain.value()->revision()==version,"revision changed on invalid terminal add");
    need(!bool(chain.value()->duplicate(instance_id(3),instance_id(4),3)),"duplicate P2 allowed");
    need(chain.value()->revision()==version,"revision changed on invalid duplicate");
    need(bool(chain.value()->set_user_bypass(instance_id(3),true)),"manual bypass P2");
    need(!chain.value()->instances().back().active(),"bypassed P2 should not be active");
    need(bool(chain.value()->set_user_bypass(instance_id(3),false)),"resume P2");
    need(chain.value()->instances().back().active(),"P2 not active after resume");
    need(bool(chain.value()->remove(instance_id(3))),"remove live P2");
    need(chain.value()->instances().size()==2,"manual removal failed");
    need(bool(chain.value()->add(instance_id(3),kLive,2)),"reinsert P2");
    // Terminal is an independent metadata chain with frozen slot discipline.
    auto terminal=dsp::ProcessingChain::create(registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
    need(bool(terminal),"create terminal chain");
    need(bool(terminal.value()->add(instance_id(4),kFinal,0)),"add frozen M17 terminal placeholder");
    need(!bool(terminal.value()->add(instance_id(5),kLive,1)),"P2 entered TERMINAL");
    need(terminal.value()->instances().size()==1,"terminal chain changed on reject");
    std::cout<<"ok genuine ProcessingChain stage/order/atomic bypass (P2 MANUAL, M17 TERMINAL)\n";
}
static core::FrameCount frames(std::int64_t n){auto v=core::FrameCount::create(n);need(bool(v),"frame count");return *v.value();}
static core::FrameRange range(std::int64_t pos,std::int64_t n){auto r=core::FrameRange::create(core::FrameIndex{pos},core::FrameIndex{pos+n});need(bool(r),"range");return *r.value();}
static audio::AudioFormat format(int fs,int chans) {
    auto s=core::SampleRate::create(fs);need(bool(s),"sample rate");
    auto r=audio::AudioFormat::create(*s.value(),chans==1?audio::ChannelLayout::MONO_C:audio::ChannelLayout::STEREO_LR);
    need(bool(r),"audio format");return *r.value();
}
static void execution_test(const dsp::ModuleRegistry& registry,int fs,int chans) {
    // Compose as a manual-chain instance before instantiating the DSP through
    // its registry factory. The Product host scheduler is not yet involved.
    auto chain=dsp::ProcessingChain::create(registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    need(bool(chain),"build research run chain");
    need(bool(chain.value()->add(instance_id(3),kLive,0)),"insert research live P2");
    const auto& selected=chain.value()->instances().front();
    need(selected.active(),"selected module not active");
    auto result=registry.create_module(selected.module_type_id());
    need(bool(result),"chain-selected registry factory created IModule");
    auto& module=**result.value();
    need(module.descriptor().type_id()==kLive,"factory wrong module type");
    constexpr std::size_t n=1009, maxBlock=73;
    constexpr std::int64_t origin=1309;
    const auto fmt=format(fs,chans);
    const dsp::DspProcessSpec spec{fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,frames(maxBlock)};
    auto req=module.runtime_requirements(spec);
    need(bool(req),"runtime requirements");
    const auto d=req.value()->algorithmic_latency_frames.value();
    need(d>0 && d==req.value()->effective_tail_frames.value(),"precise D EOS");
    need(bool(module.prepare(spec)),"real IModule prepare");
    auto input=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},frames(n));
    auto output=audio::AudioBuffer::create(fmt,audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},frames(n+d));
    need(bool(input)&&bool(output),"real audio buffers");
    std::vector<Frame> pcm(n);
    for(std::size_t i=0;i<n;++i) {
        const double t=static_cast<double>(i)/fs;
        pcm[i][0]=1.7*(.51*std::sin(2*3.141592653589793*61*t)+.41*std::sin(2*3.141592653589793*9011*t));
        pcm[i][1]=chans==2?-.68*pcm[i][0]:0.0;
        for(int c=0;c<chans;++c) (*input.value()->mutable_view().channel(c).value())[i]=pcm[i][c];
    }
    for(std::size_t off=0;off<n;) {
        const auto part=std::min(maxBlock,n-off);
        auto in=input.value()->view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(off)},frames(part));
        auto out=output.value()->mutable_view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(off)},frames(part));
        need(bool(in)&&bool(out),"real process subviews");
        const auto rc=module.process(*in.value(),*out.value(),{range(origin+off,part),off==0,false});
        need(bool(rc),"registry-dispatched P2 process");
        off+=part;
    }
    for(std::int64_t off=0;off<d;) {
        const auto part=std::min<std::int64_t>(maxBlock,d-off);
        auto out=output.value()->mutable_view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(n)+off},frames(part));
        need(bool(out),"real finalize subview");
        need(bool(module.finalize(*out.value(),{range(origin+n+off,part),false,off+part==d})),"registry-dispatched P2 finalize");
        off+=part;
    }
    Configuration cfg{fs,chans,2,.8912509381337456,0.0};
    EpochSession direct;
    need(direct.begin_epoch(1,cfg,false)==EpochStatus::ok,"direct P2 begin");
    std::vector<Frame> actual(n+static_cast<std::size_t>(d));
    for(std::size_t off=0;off<n;) {
        const auto part=std::min(maxBlock,n-off);
        need(direct.process(1,{pcm.data()+off,part},{actual.data()+off,part})==EpochStatus::ok,"direct P2 process");
        off+=part;
    }
    need(direct.finish(1,{actual.data()+n,static_cast<std::size_t>(d)})==EpochStatus::ok,"direct P2 finalize");
    for(int c=0;c<chans;++c) {
        auto lane=output.value()->view().channel(c);need(bool(lane),"actual output lane");
        for(std::size_t i=0;i<actual.size();++i)
            need(std::memcmp(&(*lane.value())[i],&actual[i][c],sizeof(double))==0,"registry P2 vs direct PCM mismatch");
    }
    std::cout<<"ok factory->IModule real P2 PCM exact fs="<<fs<<" channels="<<chans<<" delay="<<d<<"\n";
}
int main() try {
    auto reg=registry();
    metadata_tests(reg);
    for(int fs:{48000,192000}) for(int chans:{1,2}) execution_test(reg,fs,chans);
    std::cout<<"RGS R1d-P2 I1.7 REAL ProcessingChain/ModuleRegistry research: PASS\n";
    return 0;
} catch(const std::exception& e) {
    std::cerr<<"I1.7 FAIL: "<<e.what()<<"\n";return 1;
}
