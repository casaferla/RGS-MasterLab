#include "p2_planar_host_bridge.hpp"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

using namespace rgsml::research::r1d_p2;
namespace {
using Frame=std::array<double,2>;
enum class MockCode {ok, bad_config, stale};
struct MockConfig { int sample_rate=48000;int channels=2;};
// A model of EpochSession's public call contract. P2 DSP itself is NOT mocked
// as a quality test: only planar mapping, epoch gates and timing are asserted.
struct MockEpoch {
    static constexpr std::size_t D=3;
    std::uint64_t id=0;
    std::size_t consumed=0,emitted=0;
    std::array<Frame,D+1> queue{};
    std::size_t head=0,tail=0,count=0;
    bool active=false,ended=false,bypass=false;
    [[nodiscard]] MockCode begin_epoch(std::uint64_t token,MockConfig cfg,bool bp) {
        if(token<=id || (cfg.channels!=1 && cfg.channels!=2) ||
           (cfg.sample_rate!=44100 && cfg.sample_rate!=48000 && cfg.sample_rate!=88200 &&
            cfg.sample_rate!=96000 && cfg.sample_rate!=176400 && cfg.sample_rate!=192000))
            return MockCode::bad_config;
        id=token;consumed=emitted=head=tail=count=0;
        active=true;ended=false;bypass=bp;return MockCode::ok;
    }
    [[nodiscard]] std::size_t latency_frames() const noexcept { return D; }
    [[nodiscard]] MockCode process(std::uint64_t token,std::span<const Frame> in,
                                   std::span<Frame> out) noexcept {
        if(!active || token!=id || in.size()!=out.size())return MockCode::stale;
        for(std::size_t i=0;i<in.size();++i) {
            const auto scale=bypass?1.0:0.5;
            queue[tail]=Frame{in[i][0]*scale,in[i][1]*scale};
            tail=(tail+1)%queue.size();++count;
            if(emitted<D)out[i]={};
            else {
                out[i]=queue[head];head=(head+1)%queue.size();--count;
            }
            ++consumed;++emitted;
        }
        return MockCode::ok;
    }
    [[nodiscard]] MockCode finish(std::uint64_t token,std::span<Frame> out) noexcept {
        if(token!=id || !active || ended || out.size()!=D)return MockCode::stale;
        for(std::size_t i=0;i<D;++i) {
            if(emitted<D)out[i]={};
            else {
                if(count==0)return MockCode::stale;
                out[i]=queue[head];head=(head+1)%queue.size();--count;
            }
            ++emitted;
        }
        ended=true;return MockCode::ok;
    }
};
using Bridge=PlanarHostBridge<MockEpoch,MockConfig,MockCode>;
void expect(bool x,const char* msg){if(!x)throw std::runtime_error(msg);}

void test_layout(int fs,int channels,bool bypass,std::size_t block,std::size_t n) {
    Bridge b;
    MockConfig cfg{fs,channels};
    expect(b.begin_epoch(42,cfg,bypass,1111)==HostBridgeStatus::ok,"begin");
    expect(b.latency_frames()==3,"latency");
    std::vector<double> left(n),right(n),out_l(n),out_r(n);
    for(std::size_t i=0;i<n;++i){left[i]=double(i+1)/1000.;right[i]=double(i+17)/-900.;}
    std::size_t pos=0;
    bool first=true;
    while(pos<n){
        const auto len=std::min(block,n-pos);
        auto il=std::span(left).subspan(pos,len), ol=std::span(out_l).subspan(pos,len);
        auto ir=channels==2?std::span(right).subspan(pos,len):std::span<double>{};
        auto orr=channels==2?std::span(out_r).subspan(pos,len):std::span<double>{};
        expect(b.process(42,1111+static_cast<std::int64_t>(pos),il,ir,ol,orr,
                         first,pos+len==n)==HostBridgeStatus::ok,"valid callback");
        first=false;pos+=len;
    }
    if(n==0) expect(b.process(42,1111,{},{},{},{},true,true)==HostBridgeStatus::ok,"empty EOS");
    std::array<double,3> tail_l{},tail_r{};
    auto t_right=channels==2?std::span<double>(tail_r):std::span<double>{};
    expect(b.finish(42,1111+static_cast<std::int64_t>(n),tail_l,t_right)==HostBridgeStatus::ok,"EOS");
    std::vector<double> all_l=out_l,all_r=out_r;
    all_l.insert(all_l.end(),tail_l.begin(),tail_l.end());
    if(channels==2)all_r.insert(all_r.end(),tail_r.begin(),tail_r.end());
    const auto factor=bypass?1.0:.5;
    for(std::size_t i=0;i<3;++i){expect(all_l[i]==0.,"zero prefill left");if(channels==2)expect(all_r[i]==0.,"zero prefill right");}
    for(std::size_t i=0;i<n;++i){
        expect(all_l[i+3]==left[i]*factor,"planar left differs");
        if(channels==2)expect(all_r[i+3]==right[i]*factor,"planar right differs");
    }
    tail_l.fill(123);
    expect(b.finish(42,1111+static_cast<std::int64_t>(n),tail_l,t_right)==HostBridgeStatus::wrong_lifecycle,"double EOS");
    for(auto v:tail_l)expect(v==0.,"double EOS not silent");
}
void test_block_boundary() {
    Bridge b; MockConfig cfg{};
    expect(b.begin_epoch(101,cfg,true,0)==HostBridgeStatus::ok,"maximum callback begin");
    std::vector<double> left(Bridge::kMaxBlock+1,.2),right(Bridge::kMaxBlock+1,-.3);
    std::vector<double> out_l(Bridge::kMaxBlock+1,7.),out_r(Bridge::kMaxBlock+1,7.);
    expect(b.process(101,0,std::span(left).first(Bridge::kMaxBlock),
             std::span(right).first(Bridge::kMaxBlock),
             std::span(out_l).first(Bridge::kMaxBlock),
             std::span(out_r).first(Bridge::kMaxBlock),true,true)==HostBridgeStatus::ok,
           "4096-frame callback rejected");
    expect(b.begin_epoch(102,cfg,true,0)==HostBridgeStatus::ok,"oversized callback rearm");
    expect(b.process(102,0,left,right,out_l,out_r,true,true)==HostBridgeStatus::invalid_context,
           "4097-frame callback accepted");
    for(const auto x:out_l)expect(x==0.,"oversized output not silenced");
}
void test_faults() {
    Bridge b;MockConfig cfg{};
    std::array<double,3> left{{.1,.2,.3}},right{{.4,.5,.6}},outl{{8,8,8}},outr{{8,8,8}};
    expect(b.process(1,0,left,right,outl,outr,true,true)==HostBridgeStatus::not_prepared,"preprepare");
    expect(b.begin_epoch(7,cfg,false,555)==HostBridgeStatus::ok,"begin fault test");
    expect(b.process(6,555,left,right,outl,outr,true,false)==HostBridgeStatus::stale_epoch,"stale callback");
    for(auto v:outl)expect(v==0.,"stale not silent");
    expect(b.next_absolute_frame()==555,"stale advanced host cursor");
    auto bad=cfg;bad.sample_rate=123;
    expect(b.begin_epoch(8,bad,false,0)==HostBridgeStatus::invalid_configuration,"bad config accepted");
    expect(b.epoch()==7,"bad config overwritten live epoch");
    expect(b.process(7,555,left,right,outl,outr,true,false)==HostBridgeStatus::ok,"valid after failed begin");
    expect(b.next_absolute_frame()==558,"cursor not advanced");
    expect(b.process(7,558,left,right,outl,outr,true,false)==HostBridgeStatus::invalid_context,"double start accepted");
    for(auto v:outl)expect(v==0.,"context failure not silent");
    expect(b.begin_epoch(9,cfg,false,22)==HostBridgeStatus::ok,"rearm after fault");
    left[1]=std::numeric_limits<double>::quiet_NaN();
    expect(b.process(9,22,left,right,outl,outr,true,true)==HostBridgeStatus::invalid_pcm,"NaN accepted");
    expect(b.next_absolute_frame()==22,"invalid PCM advanced ledger");
    expect(b.begin_epoch(10,cfg,true,0)==HostBridgeStatus::ok,"rearm NaN");
    left[1]=.2;
    expect(b.process(10,0,left,right,outl,outr,true,true)==HostBridgeStatus::ok,"new epoch");
    std::array<double,3> tail{},tailr{};
    expect(b.finish(10,4,tail,tailr)==HostBridgeStatus::invalid_context,"wrong tail frame accepted");
    expect(b.begin_epoch(11,cfg,true,0)==HostBridgeStatus::ok,"rearm after bad finalize");
    expect(b.process(11,0,left,right,outl,outr,true,true)==HostBridgeStatus::ok,"process rearm");
    expect(b.finish(11,3,tail,tailr)==HostBridgeStatus::ok,"good tail");
    expect(b.begin_epoch(12,cfg,true,0)==HostBridgeStatus::ok,"alias rearm");
    expect(b.process(12,0,left,right,left,outr,true,true)==HostBridgeStatus::invalid_context,"inplace alias accepted");
    expect(b.begin_epoch(13,cfg,true,0)==HostBridgeStatus::ok,"alias rearm2");
    expect(b.process(13,0,left,right,outl,outl,true,true)==HostBridgeStatus::invalid_context,"output alias accepted");
}
}
int main(){try{
 for(int fs:{44100,48000,88200,96000,176400,192000})
  for(int ch:{1,2})for(bool bp:{false,true})
   for(std::size_t len:{std::size_t{0},std::size_t{1},std::size_t{2},std::size_t{3},std::size_t{129},std::size_t{1025}})
    for(std::size_t block:{std::size_t{1},std::size_t{7},std::size_t{64},std::size_t{511},std::size_t{512},std::size_t{1024}})
     test_layout(fs,ch,bp,block,len);
 test_block_boundary();test_faults();std::cout<<"R1d-P2 I1.5 PLANAR HOST BRIDGE CONTRACT: PASS (MOCK EPOCH ONLY)\n";
 return 0;
} catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
