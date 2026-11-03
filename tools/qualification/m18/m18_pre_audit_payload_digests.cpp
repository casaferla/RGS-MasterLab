// M18 independent pre-audit PCM24 payload SHA256 acceptance fixtures.
// This code invokes actual private native Philox + RN64 terminal codec.
// The expected SHA strings are independent 2026-10-04 pre-audit authority
// observations (section I); no dynamic baselines or generated expectations.
// This is not the installed app, checkpoint engine or M18 product Gate G.
#include "internal/m18_counter_dither_philox.hpp"
#include "internal/m18_terminal_sample_codec.hpp"
#include <rgsml/core/sha256.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace rgsml::audio::internal;
constexpr auto kSeed=UINT64_C(0x0123456789ABCDEF);

struct Fixture {
    std::uint64_t start;
    std::size_t frames;
    std::uint32_t channels;
    const char* expected_sha256;
};
constexpr std::array<Fixture,3> kFixtures{{
    {UINT64_C(0),257U,2U,
     "2adc005bceabf3de5f7186dc5084db3abf38a5f59e6e5e00f0a0322180a65d56"},
    {UINT64_C(4294967295),257U,2U,
     "4d79f5f4b7032d1134b365cf9aaa7041b37319c1a0aeae4f2e73d5c44f4d9f2c"},
    {UINT64_C(4294963199),8209U,1U,
     "25090a67926933541c0d2a502f98075b6c8fc344b9b047d09513af520131f447"}
}};

// Protect the immutable independently published digests against silent
// truncation or transcription errors in future revisions.
static_assert(std::string_view{kFixtures[0].expected_sha256}.size()==64U);
static_assert(std::string_view{kFixtures[1].expected_sha256}.size()==64U);
static_assert(std::string_view{kFixtures[2].expected_sha256}.size()==64U);

[[nodiscard]] bool encode(
    const Fixture& fixture,std::span<const std::size_t> partition,
    std::vector<std::uint8_t>& bytes)
{
    if(partition.empty())return false;
    const auto channels=fixture.channels;
    std::vector<double> samples(fixture.frames*channels);
    for(std::size_t f=0;f<fixture.frames;++f){
        const std::uint64_t absolute=fixture.start+f;
        // Exact frozen dyadic x=((absoluteFrame mod 7)-3)/(4*2^23).
        const auto numerator=static_cast<int>(absolute%UINT64_C(7))-3;
        const double x=static_cast<double>(numerator)*0x1.0p-25;
        for(std::uint32_t c=0;c<channels;++c)
            samples[f*channels+c]=x;
    }
    bytes.assign(samples.size()*3U,0xA5U);
    std::size_t offset=0U, step=0U;
    while(offset<fixture.frames){
        const auto count=std::min(partition[step%partition.size()],
                                  fixture.frames-offset);
        if(count==0U)return false;
        auto input=std::span<const double>{samples}.subspan(
            offset*channels,count*channels);
        auto output=std::span<std::uint8_t>{bytes}.subspan(
            offset*channels*3U,count*channels*3U);
        if(m18_pcm24_block(input,channels,
            fixture.start+offset,kSeed,output)!=M18CodecStatus::OK)
            return false;
        offset+=count;
        ++step;
    }
    return true;
}

[[nodiscard]] std::string sha(
    const std::vector<std::uint8_t>& bytes)
{
    return rgsml::core::sha256_hex(std::as_bytes(
        std::span<const std::uint8_t>{bytes}));
}

[[nodiscard]] int test_frozen_digest_fixtures()
{
    constexpr std::array<std::size_t,1> complete{{16384U}};
    constexpr std::array<std::size_t,12> adversarial{{
        1U,2U,3U,7U,31U,64U,127U,256U,511U,1024U,4096U,8191U}};
    for(std::size_t i=0;i<kFixtures.size();++i){
        const auto& fixture=kFixtures[i];
        std::vector<std::uint8_t> entire, ragged;
        if(!encode(fixture,complete,entire))return 20;
        if(!encode(fixture,adversarial,ragged))return 21;
        if(entire!=ragged)return 22;
        if(entire.size()!=fixture.frames*fixture.channels*3U)return 23;
        if(sha(entire)!=fixture.expected_sha256){
            std::fprintf(stderr,
                "M18_SHA_DIAGNOSTIC fixture=%zu observed=%s expected=%s firstBytes=",
                i,sha(entire).c_str(),fixture.expected_sha256);
            for(std::size_t n=0;n<std::min(std::size_t{48},entire.size());++n)
                std::fprintf(stderr,"%02X",static_cast<unsigned>(entire[n]));
            std::fputc('\n',stderr);
            return 24;
        }
        if(sha(ragged)!=fixture.expected_sha256)return 25;

        // Explicit checkpoint and resume boundary, source identity frozen.
        const std::size_t checkpoint=fixture.frames==257U?113U:4097U;
        const std::array<std::size_t,2> resumed{{
            checkpoint,fixture.frames-checkpoint}};
        std::vector<std::uint8_t> resumedOutput;
        if(!encode(fixture,resumed,resumedOutput))return 26;
        if(resumedOutput!=entire)return 27;
        std::printf("M18_SHA_ORACLE_PASS fixture=%zu frames=%zu channels=%u digest=%s\n",
            i,fixture.frames,static_cast<unsigned>(fixture.channels),
            fixture.expected_sha256);
    }
    return 0;
}

[[nodiscard]] int test_channel_metamorphism()
{
    const Fixture mono{0U,257U,1U,""};
    const Fixture stereo{0U,257U,2U,""};
    constexpr std::array<std::size_t,1> full{{257U}};
    std::vector<std::uint8_t> a,b;
    if(!encode(mono,full,a)||!encode(stereo,full,b))return 40;
    for(std::size_t frame=0;frame<257U;++frame){
        if(m18_philox4x32_10(kSeed,frame,0U)==
           m18_philox4x32_10(kSeed,frame,1U))return 41;
        for(std::size_t byte=0;byte<3U;++byte)
            if(a[frame*3U+byte]!=b[frame*6U+byte])return 42;
    }
    // Different channel address means different *random words*;
    // identical rounded integer codes at a given frame are allowed.
    return 0;
}
} // namespace

int main()
{
    if(const auto code=test_frozen_digest_fixtures();code){
        std::fprintf(stderr,"M18_SHA_DIGEST_FAIL_%d\n",code);
        return code;
    }
    if(const auto code=test_channel_metamorphism();code){
        std::fprintf(stderr,"M18_CHANNEL_METAMORPHISM_FAIL_%d\n",code);
        return code;
    }
    std::puts("M18_INDEPENDENT_PUBLISHED_SHA_AND_RESUME_PASS_NOT_GATE_G");
    return 0;
}
