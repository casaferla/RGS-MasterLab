// M18 independent portable integration: exact byte-stream identity, SHA256,
// disk staging, no-overwrite publication, sink failures and cleanup.
// Test-only. No M17 app/GUI, no final M18 release or crash-durability claim.
#include "internal/m18_streaming_wav.hpp"
#include "internal/m18_transactional_file.hpp"
#include <rgsml/core/sha256.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using namespace rgsml::audio::internal;

class MemorySink final : public M18ByteSink {
public:
    std::vector<std::uint8_t> bytes;
    bool fail_next=false;
    [[nodiscard]] bool write_all(
        std::span<const std::uint8_t> block) noexcept override
    {
        if(fail_next)return false;
        try {
            bytes.insert(bytes.end(),block.begin(),block.end());
            return true;
        }catch(...){return false;}
    }
};

[[nodiscard]] static std::string hash_bytes(
    std::span<const std::uint8_t> bytes)
{
    return rgsml::core::sha256_hex(std::as_bytes(bytes));
}

[[nodiscard]] static int streaming_matches_bounded(
    M18WavSpec spec, std::span<const double> pcm,
    std::uint64_t first, int errorBase)
{
    const auto bounded=m18_make_bounded_wav_candidate(spec,pcm,first);
    if(bounded.status!=M18WavStatus::OK)return errorBase+1;
    MemorySink sink;
    M18StreamingWavWriter writer(sink,spec,first);
    if(writer.start()!=M18StreamStatus::OK)return errorBase+2;
    constexpr std::array<std::size_t,7> steps{{1,3,2,73,2041,4097,256}};
    const std::size_t frames=pcm.size()/spec.channels;
    std::size_t at=0;
    std::size_t index=0;
    while(at<frames){
        const auto chunk=std::min(steps[index++%steps.size()],frames-at);
        const auto frameSamples=pcm.subspan(at*spec.channels,
                                            chunk*spec.channels);
        if(writer.append(frameSamples)!=M18StreamStatus::OK)
            return errorBase+3;
        at+=chunk;
    }
    const auto done=writer.finish();
    if(done.status!=M18StreamStatus::OK||!done.receipt)
        return errorBase+4;
    if(done.receipt->frames!=frames
       ||done.receipt->plan.file_bytes!=sink.bytes.size()
       ||done.receipt->full_file_sha256!=hash_bytes(sink.bytes)
       ||done.receipt->payload_sha256!=hash_bytes(
             std::span<const std::uint8_t>{sink.bytes}.subspan(
                 done.receipt->plan.header_bytes)))
        return errorBase+5;
    if(sink.bytes!=bounded.bytes)return errorBase+6;
    if(writer.append({})!=M18StreamStatus::BAD_SEQUENCE)
        return errorBase+7;
    return 0;
}

static int test_streaming()
{
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    constexpr std::size_t frames=8391; // must cross 4096-frame internal buffer
    std::vector<double> source(frames*2);
    constexpr std::array<double,8> values{{
        0.0,0.125,-0.25,0.5,-0.75,0.015625,0.625,-0.5}};
    for(std::size_t i=0;i<source.size();++i)
        source[i]=values[i%values.size()];
    for(const auto fmt:{M18WavEncoding::PCM24,
                        M18WavEncoding::IEEE_FLOAT32}){
        M18WavSpec spec{fmt,192000U,2U,frames,seed};
        if(const auto e=streaming_matches_bounded(spec,source,17,100);e)
            return e;
        spec.channels=1U;
        spec.frame_count=source.size();
        if(const auto e=streaming_matches_bounded(spec,source,0,115);e)
            return e;
    }

    // Empty stream produces header only, exact empty payload digest.
    const M18WavSpec empty{M18WavEncoding::PCM24,44100U,1U,0U,seed};
    if(const auto e=streaming_matches_bounded(empty,
        std::span<const double>{},0,130);e)return e;

    // The address UINT64_MAX is legal for one final frame, not two.
    const M18WavSpec finalFrame{
        M18WavEncoding::PCM24,48000U,1U,1U,seed};
    const std::array<double,1> one{{0.0}};
    if(const auto e=streaming_matches_bounded(
        finalFrame,one,UINT64_MAX,145);e)return e;

    MemorySink sink;
    M18WavSpec two{M18WavEncoding::PCM24,48000U,2U,2U,seed};
    M18StreamingWavWriter incomplete(sink,two);
    if(incomplete.start()!=M18StreamStatus::OK)return 160;
    if(incomplete.append(std::span{source}.first(2))
        !=M18StreamStatus::OK)return 161;
    if(incomplete.finish().status!=M18StreamStatus::INCOMPLETE)
        return 162;
    if(incomplete.finish().receipt)return 163;

    MemorySink failure;
    M18StreamingWavWriter dropped(failure,two);
    if(dropped.start()!=M18StreamStatus::OK)return 164;
    failure.fail_next=true;
    if(dropped.append(std::span{source}.first(4))
        !=M18StreamStatus::SINK_FAILED)return 165;
    if(dropped.finish().receipt)return 166;

    MemorySink badSourceSink;
    M18StreamingWavWriter invalid(badSourceSink,two);
    if(invalid.start()!=M18StreamStatus::OK)return 167;
    const std::array<double,4> bad{{0.0,0.0,0.0,2.0}};
    if(invalid.append(bad)!=M18StreamStatus::CODEC_REJECTED
       ||invalid.codec_failure()!=M18CodecStatus::PCM24_OVERLOAD)
        return 168;
    if(invalid.finish().receipt)return 169;
    return 0;
}

struct ScopedFolder {
    std::filesystem::path path;
    ~ScopedFolder() noexcept
    {
        std::error_code error;
        std::filesystem::remove_all(path,error);
    }
};

static int test_disk_publish()
{
    namespace fs=std::filesystem;
    std::error_code ec;
    const auto parent=fs::temp_directory_path(ec);
    if(ec)return 180;
    const auto stamp=std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto folder=parent/("rgs-m18-test-"+std::to_string(stamp));
    if(!fs::create_directory(folder,ec)||ec)return 181;
    ScopedFolder cleanup{folder};
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    const std::array<double,8> source{{
        0.0,0.125,0.25,0.5,-0.125,-0.25,-0.5,0.0}};
    const M18WavSpec spec{
        M18WavEncoding::PCM24,48000U,2U,4U,seed};
    const auto expected=m18_make_bounded_wav_candidate(spec,source);
    if(expected.status!=M18WavStatus::OK)return 182;

    const auto final=folder/"master.wav";
    {
        auto staging=M18TransactionalFile::open(final);
        if(!staging)return 183;
        M18StreamingWavWriter writer(*staging,spec);
        if(writer.start()!=M18StreamStatus::OK)return 184;
        if(writer.append(source)!=M18StreamStatus::OK)return 185;
        auto receipt=writer.finish();
        if(receipt.status!=M18StreamStatus::OK||!receipt.receipt)
            return 186;
        if(staging->publish(*receipt.receipt)!=M18FileStatus::OK)
            return 187;
    }
    if(!fs::exists(final))return 188;
    std::ifstream in(final,std::ios::binary);
    if(!in)return 189;
    const std::vector<std::uint8_t> got(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    if(got!=expected.bytes)return 190;
    if(M18TransactionalFile::open(final))return 191;

    // Simulated concurrent writer races to create the FINAL name. The
    // transaction must never overwrite that competing file.
    const auto collision=folder/"collision.wav";
    {
        auto staging=M18TransactionalFile::open(collision);
        if(!staging)return 192;
        M18StreamingWavWriter writer(*staging,spec);
        if(writer.start()!=M18StreamStatus::OK
           ||writer.append(source)!=M18StreamStatus::OK)
            return 193;
        const auto receipt=writer.finish();
        if(!receipt.receipt)return 194;
        {std::ofstream competing(collision,std::ios::binary);
         competing<<"external_file_must_remain";}
        if(staging->publish(*receipt.receipt)
           !=M18FileStatus::TARGET_EXISTS)return 195;
    }
    std::ifstream competing(collision,std::ios::binary);
    if(!competing)return 196;
    std::string sentinel;
    std::getline(competing,sentinel);
    if(sentinel!="external_file_must_remain")return 197;

    // Failure after a partially staged header+PCM sample cannot publish.
    const auto aborted=folder/"aborted.wav";
    {
        auto staging=M18TransactionalFile::open(aborted);
        if(!staging)return 198;
        M18StreamingWavWriter writer(*staging,spec);
        if(writer.start()!=M18StreamStatus::OK)return 199;
        const std::array<double,8> bad{{
            0.0,0.25,0.0,0.25,0.0,0.25,0.0,2.0}};
        if(writer.append(bad)!=M18StreamStatus::CODEC_REJECTED)
            return 200;
        if(writer.finish().receipt)return 201;
    }
    if(fs::exists(aborted))return 202;
    return 0;
}

int main()
{
    if(const auto code=test_streaming();code){
        std::cerr<<"M18_STREAM_FAIL "<<code<<"\n";return code;
    }
    if(const auto code=test_disk_publish();code){
        std::cerr<<"M18_DISK_FAIL "<<code<<"\n";return code;
    }
    std::cout<<"M18_STREAM_AND_TRANSACTION_PASS exact SHA256, chunk identity,"
        <<" disk staging, nonoverwrite, error rollback\n";
    return 0;
}
