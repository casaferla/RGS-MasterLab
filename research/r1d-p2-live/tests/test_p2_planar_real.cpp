#include "p2_planar_host_bridge.hpp"
#include "p2_epoch_session.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>
using namespace rgsml::research::r1d_p2;
using Bridge=PlanarHostBridge<EpochSession,Configuration,EpochStatus>;
static std::uint64_t g_pcm_hash=14695981039346656037ULL;
static void hash_pcm(std::span<const Frame> frames){
    const auto* ptr=reinterpret_cast<const unsigned char*>(frames.data());
    for(std::size_t i=0;i<frames.size()*sizeof(Frame);++i){
        g_pcm_hash ^= ptr[i];g_pcm_hash *= 1099511628211ULL;
    }
}

static void require(bool b,const char* what){if(!b)throw std::runtime_error(what);}
static std::vector<Frame> input_for(int fs,int channels,std::size_t n,bool hot) {
    std::vector<Frame> s(n);
    for(std::size_t i=0;i<n;++i){
        const auto t=static_cast<double>(i)/fs;
        const double v=(hot?1.5:.11)*(.58*std::sin(2*3.141592653589793*231*t)
                                    +.42*std::sin(2*3.141592653589793*7841*t));
        s[i][0]=v;s[i][1]=channels==2?v*-.61:0.;
    }
    return s;
}
static std::vector<Frame> reference(const std::vector<Frame>& input,Configuration cfg,bool bypass){
    EpochSession e;
    require(e.begin_epoch(12,cfg,bypass)==EpochStatus::ok,"direct begin");
    std::vector<Frame> o(input.size()+e.latency_frames());
    for(std::size_t pos=0;pos<input.size();) {
        const auto len=std::min<std::size_t>(64,input.size()-pos);
        require(e.process(12,std::span(input).subspan(pos,len),
             std::span(o).subspan(pos,len))==EpochStatus::ok,"direct process");
        pos+=len;
    }
    require(e.finish(12,std::span(o).subspan(input.size()))==EpochStatus::ok,"direct finish");
    return o;
}
static std::vector<Frame> via_bridge(const std::vector<Frame>& input,Configuration cfg,
                                     bool bypass,std::size_t callback_size) {
    Bridge b;
    require(b.begin_epoch(200,cfg,bypass,100000)==HostBridgeStatus::ok,"bridge begin");
    const auto n=input.size(),delay=b.latency_frames();
    std::vector<double> li(n),ri(n),lo(n+delay),ro(n+delay);
    for(std::size_t i=0;i<n;++i){li[i]=input[i][0];ri[i]=input[i][1];}
    for(std::size_t pos=0;pos<n;) {
        const auto len=std::min(callback_size,n-pos);
        require(b.process(200,100000+static_cast<std::int64_t>(pos),
                 std::span(li).subspan(pos,len),
                 cfg.channels==2?std::span<const double>(ri).subspan(pos,len):std::span<const double>{},
                 std::span(lo).subspan(pos,len),
                 cfg.channels==2?std::span<double>(ro).subspan(pos,len):std::span<double>{},
                 pos==0,pos+len==n)==HostBridgeStatus::ok,"bridge process");
        pos+=len;
    }
    if(n==0)require(b.process(200,100000,{},{},{},{},true,true)==HostBridgeStatus::ok,"zero input");
    require(b.finish(200,100000+static_cast<std::int64_t>(n),
         std::span(lo).subspan(n),cfg.channels==2?std::span<double>(ro).subspan(n):std::span<double>{})
         ==HostBridgeStatus::ok,"bridge finish");
    std::vector<Frame> o(n+delay);
    for(std::size_t i=0;i<o.size();++i)o[i]=Frame{lo[i],cfg.channels==2?ro[i]:0.};
    return o;
}
static void test(int fs,int channels,bool bypass,bool hot,std::size_t n){
    const Configuration cfg{fs,channels,2,.8912509381337456,0.};
    const auto input=input_for(fs,channels,n,hot);
    const auto ref=reference(input,cfg,bypass);
    hash_pcm(ref);
    for(std::size_t block:{std::size_t{1},std::size_t{7},std::size_t{64},
                           std::size_t{511},std::size_t{512},std::size_t{1024}}){
        const auto actual=via_bridge(input,cfg,bypass,block);
        require(actual.size()==ref.size(),"bridge length mismatch");
        require(std::memcmp(actual.data(),ref.data(),ref.size()*sizeof(Frame))==0,
                "real EpochSession PCM changed by planar bridge");
    }
    if(!bypass){
        ::research::Meter meter;
        for(std::size_t i=ref.size()-n;i<ref.size();++i)meter.push(ref[i],channels);
        for(unsigned i=0;i<11;++i)meter.push(Frame{},channels);
        require(meter.peak<=cfg.ceiling_linear,"real P2 over ceiling in-memory");
    } else {
        require(std::memcmp(ref.data()+ref.size()-n,input.data(),n*sizeof(Frame))==0,
                "real bypass not bit-exact after latency");
    }
}
int main()try {
    for(int fs:{44100,48000,88200,96000,176400,192000})
        for(int channels:{1,2})
            for(bool bypass:{false,true}) {
                test(fs,channels,bypass,false,121);
                test(fs,channels,bypass,true,1537);
            }
    std::cout<<"R1d-P2 I1.5 REAL EPOCH + PLANAR HOST: PASS; FNV64="
             <<std::hex<<g_pcm_hash<<std::dec<<" (RESEARCH ONLY)\n";
    return 0;
} catch(const std::exception& e){std::cerr<<"R1d-P2 REAL EPOCH HOST: FAIL "<<e.what()<<"\n";return 1;}
