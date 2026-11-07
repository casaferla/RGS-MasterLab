// M18 isolated no-authority failure matrix, actual private byte-stream writer.
// Test immutable payload identity under ragged block schedules, across 2^32,
// and fail-closed receipts for every injected header/payload sink failure.
#include "internal/m18_streaming_wav.hpp"
#include "internal/m18_wav_candidate.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>

using namespace rgsml::audio::internal;
namespace {
class FaultSink final : public M18ByteSink {
public:
    std::vector<std::uint8_t> bytes;
    std::size_t calls{};
    std::size_t faultCall{std::numeric_limits<std::size_t>::max()};
    bool partial{false};
    [[nodiscard]] bool write_all(std::span<const std::uint8_t> data) noexcept override {
        try {
            const auto ordinal=calls++;
            if(ordinal==faultCall){
                if(partial)bytes.insert(bytes.end(),data.begin(),data.begin()+
                    static_cast<std::ptrdiff_t>(data.size()/2U));
                return false;
            }
            bytes.insert(bytes.end(),data.begin(),data.end());
            return true;
        }catch(...){return false;}
    }
};
constexpr std::uint64_t kOrigin=UINT64_C(4294967295)-4096U;
constexpr std::uint64_t kSeed=UINT64_C(0x0123456789ABCDEF);
constexpr std::size_t kFrames=8209U;

[[nodiscard]] std::vector<double> make_samples(M18WavEncoding format,
                                                std::uint32_t channels) {
    std::vector<double> out(kFrames*channels);
    constexpr std::array<std::uint64_t,12> pcmBits{{
        0U,UINT64_C(0x8000000000000000),
        UINT64_C(0x3E60000000000000),UINT64_C(0xBE60000000000000),
        UINT64_C(0x3E70000000000000),UINT64_C(0xBE70000000000000),
        UINT64_C(0x3FE0000000000000),UINT64_C(0xBFE0000000000000),
        1U,UINT64_C(0x8000000000000001),
        UINT64_C(0x3FD0000000000000),UINT64_C(0xBFD0000000000000)
    }};
    constexpr std::array<std::uint64_t,12> floatBits{{
        0U,UINT64_C(0x8000000000000000),
        UINT64_C(0x3FF000000FFFFFFF),UINT64_C(0x3FF0000010000000),
        UINT64_C(0x3FF0000010000001),UINT64_C(0xBFF0000010000000),
        UINT64_C(0x36A0000000000000),UINT64_C(0xB6A0000000000000),
        UINT64_C(0x3690000000000000),UINT64_C(0xB690000000000000),
        UINT64_C(0x3810000000000000),UINT64_C(0xB810000000000000)
    }};
    for(std::size_t i=0;i<kFrames;++i)for(std::uint32_t ch=0;ch<channels;++ch){
        const auto n=format==M18WavEncoding::PCM24?pcmBits.size():floatBits.size();
        const auto at=(i+11U*ch)%n;
        const auto bits=format==M18WavEncoding::PCM24?pcmBits[at]:floatBits[at];
        out[i*channels+ch]=std::bit_cast<double>(bits);
    }
    return out;
}

[[nodiscard]] int run_case(M18WavEncoding format,std::uint32_t channels){
    const M18WavSpec spec{format,48000U,channels,kFrames,kSeed};
    const auto data=make_samples(format,channels);
    const auto expected=m18_make_bounded_wav_candidate(spec,data,kOrigin);
    if(expected.status!=M18WavStatus::OK)return 10;
    constexpr std::array<std::size_t,12> schedules{{
        1U,2U,3U,7U,31U,64U,127U,256U,511U,1024U,4096U,8191U
    }};
    for(auto limit:schedules){
        FaultSink sink;
        M18StreamingWavWriter writer(sink,spec,kOrigin);
        if(writer.start()!=M18StreamStatus::OK)return 11;
        for(std::size_t offset=0;offset<kFrames;){
            const auto take=std::min(limit,kFrames-offset);
            const auto part=std::span<const double>{data}.subspan(
                offset*channels,take*channels);
            if(writer.append(part)!=M18StreamStatus::OK)return 12;
            offset+=take;
        }
        const auto finish=writer.finish();
        if(finish.status!=M18StreamStatus::OK||!finish.receipt||
           finish.receipt->frames!=kFrames||
           sink.bytes!=expected.bytes)return 13;
        if(writer.append(data)!=M18StreamStatus::BAD_SEQUENCE||
           writer.finish().receipt.has_value())return 14;
    }
    // A single append is internally split into three (4096,4096,17)
    // writes, plus its header; each attempted write can fail *after* a
    // partial byte acceptance, and must irrevocably suppress the receipt.
    for(std::size_t at=0;at<4U;++at)for(bool partial:{false,true}){
        FaultSink sink;
        sink.faultCall=at;
        sink.partial=partial;
        M18StreamingWavWriter writer(sink,spec,kOrigin);
        if(at==0U){
            if(writer.start()!=M18StreamStatus::SINK_FAILED)return 20;
        }else{
            if(writer.start()!=M18StreamStatus::OK)return 21;
            if(writer.append(data)!=M18StreamStatus::SINK_FAILED)return 22;
        }
        if(writer.status()!=M18StreamStatus::SINK_FAILED||
           writer.finish().receipt.has_value()||
           writer.append(data)!=M18StreamStatus::SINK_FAILED)return 23;
        // The partially written PRIVATE sink MUST be discarded. A sink does
        // not know how to publish or recover, and no success receipt exists.
        if(sink.bytes==expected.bytes)return 24;
    }
    // No early receipt if frames are missing; no reuse of rejected writer.
    {
        FaultSink sink;
        M18StreamingWavWriter writer(sink,spec,kOrigin);
        if(writer.start()!=M18StreamStatus::OK)return 30;
        if(writer.append(std::span<const double>{data}.first(channels))
           !=M18StreamStatus::OK)return 31;
        if(writer.finish().status!=M18StreamStatus::INCOMPLETE||
           writer.finish().receipt.has_value()||
           writer.append(data)!=M18StreamStatus::INCOMPLETE)return 32;
    }
    // Caller cannot start twice or send too many frames.
    {
        FaultSink sink;
        M18StreamingWavWriter writer(sink,spec,kOrigin);
        if(writer.start()!=M18StreamStatus::OK||
           writer.start()!=M18StreamStatus::BAD_SEQUENCE||
           writer.finish().receipt.has_value())return 33;
    }
    {
        FaultSink sink;
        M18StreamingWavWriter writer(sink,spec,kOrigin);
        if(writer.append(data)!=M18StreamStatus::BAD_SEQUENCE||
           writer.finish().receipt.has_value())return 34;
    }
    {
        FaultSink sink;
        M18StreamingWavWriter writer(sink,spec,kOrigin);
        if(writer.start()!=M18StreamStatus::OK)return 35;
        const std::vector<double> extra(channels,0.0);
        if(writer.append(data)!=M18StreamStatus::OK||
           writer.append(extra)!=M18StreamStatus::BAD_LAYOUT||
           writer.finish().receipt.has_value())return 36;
    }
    return 0;
}
} // namespace
int main(){
    for(auto fmt:{M18WavEncoding::PCM24,M18WavEncoding::IEEE_FLOAT32})
        for(auto channels:{1U,2U}){
            const auto n=run_case(fmt,channels);
            if(n){std::fprintf(stderr,"M18_FAULT_PARTITION_FAILED:%d\n",n);return n;}
        }
    std::puts("M18_FAULT_INJECTION_96_CASES_AND_RAGGED_PARTITIONS_PASS_NOT_PRODUCT_GATE");
    return 0;
}
