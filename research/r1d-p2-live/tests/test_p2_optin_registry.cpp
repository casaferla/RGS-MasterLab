// I2 Product-boundary gate: opt-in P2 registry preserves the frozen v1 path.
#include "research_p2_imodule.hpp"
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/streaming_true_peak_limiter_parameters.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace dsp = rgsml::dsp;
namespace core = rgsml::core;
namespace audio = rgsml::audio;
using rgsml::research::r1d_p2::ResearchP2IModule;
constexpr auto kLive = "rgsml.dsp.streaming-true-peak-limiter";
constexpr auto kFinal = "rgsml.dsp.true-peak-limiter";
void need(bool cond, const char *reason) { if(!cond) throw std::runtime_error(reason); }
dsp::ModuleDescriptor desc(std::string id, dsp::ModuleCategory category, bool terminal=false) {
    dsp::ModuleDescriptorSpec spec{
        id,id+".name",{category},{dsp::ProcessingStage::MASTER},
        {terminal?dsp::ChainSegment::TERMINAL:dsp::ChainSegment::MANUAL},
        !terminal,true,terminal?dsp::PlacementClass::TERMINAL_SLOT:dsp::PlacementClass::INLINE_CHAIN,
        terminal?std::optional<dsp::TerminalSlot>{dsp::TerminalSlot::FINAL_TRUE_PEAK_LIMITER}:std::nullopt,
        {},{}, {},{},false,terminal,std::nullopt,std::nullopt};
    auto r=dsp::ModuleDescriptor::create(std::move(spec));
    need(bool(r),"placeholder descriptor"); return std::move(*r.value());
}
dsp::ModuleInstanceId id(int v){core::Uuid::Bytes b{};b[15]=static_cast<std::uint8_t>(v);auto r=dsp::ModuleInstanceId::from_uuid(core::Uuid{b});need(bool(r),"id");return *r.value();}

core::FrameCount count(std::int64_t n) {auto r=core::FrameCount::create(n);need(bool(r),"FrameCount");return *r.value();}
core::FrameRange frameRange(std::int64_t origin,std::int64_t n){auto r=core::FrameRange::create(core::FrameIndex{origin},core::FrameIndex{origin+n});need(bool(r),"FrameRange");return *r.value();}
void test_factory_pcm(const dsp::ModuleRegistry &registry,int fs,int ch){
    using rgsml::research::r1d_p2::Frame;
    using rgsml::research::r1d_p2::Configuration;
    using rgsml::research::r1d_p2::EpochSession;
    using rgsml::research::r1d_p2::EpochStatus;
    constexpr std::size_t n=1307, block=79;
    constexpr std::int64_t origin=873;
    auto rate=core::SampleRate::create(fs);need(bool(rate),"rate");
    auto f=audio::AudioFormat::create(*rate.value(),ch==1?audio::ChannelLayout::MONO_C:audio::ChannelLayout::STEREO_LR);
    need(bool(f),"format");
    const dsp::DspProcessSpec spec{*f.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,count(block)};
    auto module=registry.create_module(kLive);need(bool(module),"factory IModule");
    auto &m=**module.value();
    auto req=m.runtime_requirements(spec);need(bool(req),"requirements");
    const auto d=req.value()->effective_tail_frames.value();need(d>0,"tail");
    need(bool(m.prepare(spec)),"prepare");
    auto in=audio::AudioBuffer::create(*f.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},count(n));
    auto out=audio::AudioBuffer::create(*f.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{origin},count(n+d));
    need(bool(in)&&bool(out),"buffers");
    std::vector<Frame> raw(n);
    for(std::size_t i=0;i<n;++i){
        const double t=static_cast<double>(i)/fs;
        raw[i][0]=1.65*(0.53*std::sin(2*3.141592653589793*71*t)+0.46*std::sin(2*3.141592653589793*7587*t));
        raw[i][1]=ch==2?-.70*raw[i][0]:0.0;
        for(int c=0;c<ch;++c){ auto lane=in.value()->mutable_view().channel(c);need(bool(lane),"in lane");(*lane.value())[i]=raw[i][c];}
    }
    for(std::size_t off=0;off<n;){
        const auto part=std::min(block,n-off);
        auto i=in.value()->view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(off)},count(part));
        auto o=out.value()->mutable_view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(off)},count(part));
        need(bool(i)&&bool(o),"process subviews");
        need(bool(m.process(*i.value(),*o.value(),{frameRange(origin+off,part),off==0,false})),"P2 process");
        off+=part;
    }
    for(std::int64_t off=0;off<d;){
        const auto part=std::min<std::int64_t>(block,d-off);
        auto o=out.value()->mutable_view().subview(core::FrameIndex{origin+static_cast<std::int64_t>(n)+off},count(part));
        need(bool(o),"tail subview");
        need(bool(m.finalize(*o.value(),{frameRange(origin+n+off,part),false,off+part==d})),"P2 finalize");
        off+=part;
    }
    Configuration cfg{fs,ch,2,0.8912509381337456,0.0};
    EpochSession direct;
    need(direct.begin_epoch(1,cfg,false)==EpochStatus::ok,"pinned begin");
    std::vector<Frame> expect(n+static_cast<std::size_t>(d));
    for(std::size_t off=0;off<n;){auto part=std::min(block,n-off);
        need(direct.process(1,{raw.data()+off,part},{expect.data()+off,part})==EpochStatus::ok,"pinned process");off+=part;}
    need(direct.finish(1,{expect.data()+n,static_cast<std::size_t>(d)})==EpochStatus::ok,"pinned finish");
    for(int c=0;c<ch;++c){ auto lane=out.value()->view().channel(c);need(bool(lane),"out lane");
        for(std::size_t i=0;i<expect.size();++i)
            need(std::memcmp(&(*lane.value())[i],&expect[i][c],sizeof(double))==0,"P2 factory PCM differs from pinned");}
    std::cout<<"I2 factory direct real-PCM PASS fs="<<fs<<" channels="<<ch<<" delay="<<d<<"\n";
}

int main(){
    try {
        const auto other = std::string("rgsml.dsp.stereo-ms");
        std::vector<dsp::ModuleRegistration> registrations;
        registrations.push_back({desc(other,dsp::ModuleCategory::SPATIAL),{}});
        registrations.push_back({desc(kFinal,dsp::ModuleCategory::OUTPUT,true),{}});
        auto original=dsp::ModuleRegistry::create(std::move(registrations));
        need(bool(original),"original registry");
        need(!original.value()->find_descriptor(kLive),"P2 leaked into original registry");
        auto preview=dsp::ModuleRegistry::with_streaming_true_peak_preview(std::move(*original.value()));
        need(bool(preview),"P2 preview augmentation");
        need(preview.value()->descriptors().size()==3,"count different from original +1");
        need(preview.value()->has_factory(kLive),"real P2 factory missing");
        need(!preview.value()->has_factory(kFinal),"M17 terminal factory changed");
        const auto d=preview.value()->find_descriptor(kLive);
        need(bool(d),"P2 descriptor missing");
        need(d.value()->get().must_be_last(),"P2 not last");
        need(!d.value()->get().duplicable(),"P2 duplicate allowed");
        need(d.value()->get().allowed_segments().size()==1 &&
             d.value()->get().allowed_segments()[0]==dsp::ChainSegment::MANUAL,
             "P2 not manual only");
        auto chain=dsp::ProcessingChain::create(*preview.value(),{dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        need(bool(chain),"manual chain");
        need(bool(chain.value()->add(id(1),other,0)),"add Stereo");
        need(bool(chain.value()->add(id(2),kLive,1)),"add P2");
        const auto rev=chain.value()->revision();
        need(!chain.value()->move(id(2),0),"P2 moved before Stereo");
        need(!chain.value()->add(id(3),other,2),"other DSP allowed after P2");
        need(!chain.value()->add(id(4),kFinal,2),"M17 appended manual");
        need(chain.value()->revision()==rev,"failed transaction changed revision");
        auto defaultParams=dsp::StreamingTruePeakLimiterParameters::create_default();
        need(bool(defaultParams),"P2 typed default");
        need(defaultParams.value()->margin_percent()==2 &&
             defaultParams.value()->ceiling_db_tp()==-1.0,"P2 wrong default");
        auto hotParams=dsp::StreamingTruePeakLimiterParameters::create(-3.0,5.0,2);
        need(bool(hotParams),"P2 typed hot parameters");
        auto hot=preview.value()->create_module(
            kLive,dsp::ModuleParameterPayload{*hotParams.value()});
        need(bool(hot),"P2 typed factory");
        auto json=dsp::encode_streaming_true_peak_limiter_parameters_json(*hotParams.value());
        need(bool(json),"P2 typed JSON encoder");
        auto decoded=dsp::decode_streaming_true_peak_limiter_parameters_json(*json.value());
        need(bool(decoded) && *decoded.value()==*hotParams.value(),"P2 JSON roundtrip");
        auto generic=dsp::decode_module_parameters_json(
            "rgsml.dsp.streaming-true-peak-limiter.parameters/0.1.0",*json.value());
        need(bool(generic) && std::holds_alternative<dsp::StreamingTruePeakLimiterParameters>(
             *generic.value()),"P2 generic typed decode");
        need(!dsp::decode_streaming_true_peak_limiter_parameters_json(
            R"({"ceilingDbTp":-1,"driveDb":0,"marginPercent":2,"marginPercent":5})"),
            "P2 duplicate JSON key accepted");
        need(!dsp::decode_streaming_true_peak_limiter_parameters_json(
            R"({"ceilingDbTp":-1,"driveDb":0,"marginPercent":3})"),
            "P2 bad JSON margin accepted");
        need(!dsp::decode_module_parameters_json(
            "rgsml.dsp.true-peak-limiter.parameters/1.0.0",*json.value()),
            "P2 payload accepted as M17 schema");
        auto terminal=dsp::TruePeakLimiterParameters::create_default();
        need(bool(terminal),"M17 control create");
        auto wrong=preview.value()->create_module(
            kLive,dsp::ModuleParameterPayload{*terminal.value()});
        need(!wrong,"P2 wrongly accepted M17 payload");
        need(!dsp::StreamingTruePeakLimiterParameters::create(-13.0,0.0,2),"invalid ceiling");
        need(!dsp::StreamingTruePeakLimiterParameters::create(-1.0,25.0,2),"invalid drive");
        need(!dsp::StreamingTruePeakLimiterParameters::create(-1.0,0.0,3),"invalid margin");
        std::cout<<"I2 typed P2 distinct parameters PASS"<<std::endl;
        auto actual=preview.value()->create_module(kLive);
        need(bool(actual),"P2 factory execution");
        need((*actual.value())->descriptor().type_id()==kLive,"factory wrong ID");
        // Execute through the actual production-ABI virtual interface rather than a shell.
        const auto rate=core::SampleRate::create(48000);
        need(bool(rate),"sample rate");
        const auto format=audio::AudioFormat::create(*rate.value(),audio::ChannelLayout::STEREO_LR);
        need(bool(format),"format");
        const auto maxBlock=core::FrameCount::create(512);
        need(bool(maxBlock),"blockcount");
        const dsp::DspProcessSpec proc{*format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,*maxBlock.value()};
        auto requirements=(*actual.value())->runtime_requirements(proc);
        need(bool(requirements),"P2 requirements");
        need(requirements.value()->algorithmic_latency_frames.value()==382,"unexpected P2 latency");
        need(bool((*actual.value())->prepare(proc)),"P2 prepare");
        for(int fs : {48000,192000}) for(int ch : {1,2}) test_factory_pcm(*preview.value(),fs,ch);
        std::cout << "I2 opt-in registry/chain/create/prepare PASS latency="
                  << requirements.value()->algorithmic_latency_frames.value() << "\n";
        // A non-registered category must not silently enter this package.
        auto second=dsp::ModuleRegistry::with_streaming_true_peak_preview(std::move(*preview.value()));
        need(!second,"duplicate P2 registry accepted");
        std::cout << "I2 frozen v1 not mutated; P2 requires explicit augmentation PASS\n";
        return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
