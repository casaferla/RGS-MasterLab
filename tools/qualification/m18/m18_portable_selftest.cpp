// Standalone M18 fast preproduction CI: C++20 standard library only.
// No Qt/M17/DEVICE/reader/writer, no release or full Gate-G claim.
// One tiny translation unit compiles under both MSVC and GCC/Clang.
#include "internal/m18_counter_dither_philox.hpp"
#include "internal/m18_pcm24_pack.hpp"
#include "internal/m18_terminal_sample_codec.hpp"
#include <array>
#include <cstdint>
#include <bit>
#include <cfenv>
#include <vector>
#include <cstdio>
#include <limits>

using rgsml::audio::internal::M18PhiloxWords;
using rgsml::audio::internal::M18PhiloxKey;
using rgsml::audio::internal::m18_philox4x32_10;
using rgsml::audio::internal::m18_philox4x32_10_core;
using rgsml::audio::internal::m18_uniform53;
using rgsml::audio::internal::m18_tpdf;
using rgsml::audio::internal::m18_pack_pcm24_checked;
using rgsml::audio::internal::m18_decode_pcm24_for_test;

static_assert(m18_philox4x32_10(
    UINT64_C(0x0123456789ABCDEF), UINT64_C(0), 0U) ==
    M18PhiloxWords{{0xA66BC7F6U,0xB58E890BU,0xFC849318U,0x2C57BBBAU}});
static_assert(m18_philox4x32_10(
    UINT64_C(0x0123456789ABCDEF), UINT64_C(1), 0U) ==
    M18PhiloxWords{{0x7A4D4CA8U,0xA88EDC40U,0x3EE95F3FU,0x7732B6AEU}});
static_assert(m18_philox4x32_10(
    UINT64_C(0x0123456789ABCDEF), UINT64_C(2), 0U) ==
    M18PhiloxWords{{0xFA533ACEU,0x7052D5B8U,0xB220CF76U,0x0AA621C3U}});
static_assert(m18_philox4x32_10(
    UINT64_C(0x0123456789ABCDEF), UINT64_C(3), 0U) ==
    M18PhiloxWords{{0x0399B5ABU,0xD5D94BD3U,0x4B58FCE8U,0xDD6CA161U}});
static_assert(m18_philox4x32_10(
    UINT64_C(0x0123456789ABCDEF), UINT64_C(4294967295), 0U) ==
    M18PhiloxWords{{0xC5C4CD7CU,0x5E7543E6U,0x6FA5F827U,0xF3F62D3FU}});
static_assert(m18_philox4x32_10(
    UINT64_C(0x0123456789ABCDEF), UINT64_C(4294967296), 0U) ==
    M18PhiloxWords{{0x2C54779BU,0x8228AEADU,0xAEE65C73U,0x1143BA5BU}});
static_assert(m18_philox4x32_10(
    UINT64_C(0x0123456789ABCDEF), UINT64_C(4294967297), 0U) ==
    M18PhiloxWords{{0xE3D1D048U,0x47CE0F7EU,0x7EAD1A00U,0x5A677ED7U}});
static_assert(m18_philox4x32_10(0,0,1) ==
    M18PhiloxWords{{0xF345C558U,0xCDC63815U,0xF37D4049U,0x9B3EBDE2U}});
static_assert(m18_philox4x32_10(0,0,UINT32_MAX) ==
    M18PhiloxWords{{0x82E9660BU,0x037C0260U,0x6DF70A23U,0xD6EF4B28U}});
static_assert(m18_philox4x32_10(UINT64_MAX,0,1) ==
    M18PhiloxWords{{0xE1DCD9FFU,0x4594FE99U,0x1C356896U,0xB888F0D2U}});
static_assert(m18_philox4x32_10(UINT64_MAX,0,UINT32_MAX) ==
    M18PhiloxWords{{0x668C3F68U,0xD4D94A4CU,0x6AB2EFBFU,0x87F980A9U}});
static_assert(m18_philox4x32_10_core(
    M18PhiloxWords{{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}},
    M18PhiloxKey{{UINT32_MAX,UINT32_MAX}}) ==
    M18PhiloxWords{{0x408F276DU,0x41C83B0EU,0xA20BC7C6U,0x6D5451FDU}});
static_assert(m18_philox4x32_10_core(
    M18PhiloxWords{{0x243F6A88U,0x85A308D3U,0x13198A2EU,0x03707344U}},
    M18PhiloxKey{{0xA4093822U,0x299F31D0U}}) ==
    M18PhiloxWords{{0xD16CFE09U,0x94FDCCEBU,0x5001E420U,0x24126EA1U}});
static_assert(m18_uniform53(0,0x800U)==1U);
static_assert(m18_uniform53(UINT32_MAX,UINT32_MAX)
    ==UINT64_C(9007199254740991));


static int m18_test_pcm24_numerics()
{
    using namespace rgsml::audio::internal;
    // The expected integers were independently reconstructed from exact
    // Fraction arithmetic using original binary64 input bits and PHILOX
    // counter addresses; they are not generated from the codec under test.
    struct Row {std::uint64_t input;std::int32_t q;};
    constexpr std::array<Row,16> vectors{{
        {UINT64_C(0x0000000000000000),0},
        {UINT64_C(0x8000000000000000),0},
        {UINT64_C(0x3e70000000000000),1},
        {UINT64_C(0xbe70000000000000),-1},
        {UINT64_C(0x3e80000000000000),1},
        {UINT64_C(0xbe80000000000000),-2},
        {UINT64_C(0x3e60000000000000),0},
        {UINT64_C(0xbe60000000000000),-1},
        {UINT64_C(0x3e70000000000000),1},
        {UINT64_C(0xbe70000000000000),-1},
        {UINT64_C(0x3fefffffc0000000),8388607},
        {UINT64_C(0xbff0000000000000),-8388608},
        {UINT64_C(0x3fd0000000000000),2097152},
        {UINT64_C(0xbfd0000000000000),-2097152},
        {UINT64_C(0x3fefffffffffffff),8388607},
        {UINT64_C(0xbfefffffffffffff),-8388609}
    }};
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    for(std::size_t i=0;i<vectors.size();++i) {
        const auto x=std::bit_cast<double>(vectors[i].input);
        const auto out=m18_quantize_pcm24(x,seed,i,0);
        if(vectors[i].q<-8388608 || vectors[i].q>8388607) {
            if(out.status!=M18CodecStatus::PCM24_OVERLOAD)return 20;
        } else {
            if(out.status!=M18CodecStatus::OK
                ||out.code!=vectors[i].q
                ||m18_decode_pcm24_for_test(out.bytes)!=vectors[i].q)
                return 21;
        }
    }
    if(m18_quantize_pcm24(
        std::bit_cast<double>(UINT64_C(0x7FF0000000000000)),
        seed,0,0).status!=M18CodecStatus::NONFINITE_INPUT)return 22;
    if(m18_quantize_pcm24(
        std::bit_cast<double>(UINT64_C(0x7FF8000000000000)),
        seed,0,0).status!=M18CodecStatus::NONFINITE_INPUT)return 23;
    if(m18_quantize_pcm24(2.0,seed,0,0).status
        !=M18CodecStatus::PCM24_OVERLOAD)return 24;
    return 0;
}

static int m18_test_float32_numerics()
{
    using namespace rgsml::audio::internal;
    struct Row {std::uint64_t source;std::uint32_t expected;bool fail;};
    // Signed zero, subnormal tie/just-above, normal transition,
    // largest finite and one-ULP-before halfway-overflow.
    constexpr std::array<Row,15> vectors{{
        {UINT64_C(0x0000000000000000),0U,false},
        {UINT64_C(0x8000000000000000),0x80000000U,false},
        {UINT64_C(0x3FF0000000000000),0x3F800000U,false},
        {UINT64_C(0xBFF0000000000000),0xBF800000U,false},
        {UINT64_C(0x36A0000000000000),0x00000001U,false},
        {UINT64_C(0x3690000000000000),0x00000000U,false},
        {UINT64_C(0x3690000000000001),0x00000001U,false},
        {UINT64_C(0x3810000000000000),0x00800000U,false},
        {UINT64_C(0x380FFFFFFFFFFFFF),0x00800000U,false},
        {UINT64_C(0x47EFFFFFE0000000),0x7F7FFFFFU,false},
        {UINT64_C(0x47EFFFFFF0000000),0U,true},
        {UINT64_C(0x47EFFFFFEFFFFFFF),0x7F7FFFFFU,false},
        {UINT64_C(0x3FF8000000000000),0x3FC00000U,false},
        {UINT64_C(0xB6A0000000000000),0x80000001U,false},
        {UINT64_C(0x7FF8000000000000),0U,true}
    }};
    for(const auto& row:vectors) {
        const auto got=m18_convert_float32(std::bit_cast<double>(row.source));
        if(row.fail) {
            if(got.status!=M18CodecStatus::NONFINITE_INPUT
               &&got.status!=M18CodecStatus::FLOAT32_OVERFLOW)return 30;
        } else if(got.status!=M18CodecStatus::OK
            ||got.bits!=row.expected
            ||got.bytes[0]!=static_cast<std::uint8_t>(row.expected)
            ||got.bytes[3]!=static_cast<std::uint8_t>(row.expected>>24))
            return 31;
    }
    return 0;
}

static int m18_test_fail_atomic_and_chunks()
{
    using namespace rgsml::audio::internal;
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    // Two-pass prevalidation: a bad final sample leaves all caller bytes
    // EXACTLY untouched and never publishes a partial terminal candidate.
    std::array<double,6> input{{0.0,0.25,-0.25,0.0,0.125,2.0}};
    std::array<std::uint8_t,18> output{};
    output.fill(0xA5U);
    if(m18_pcm24_block(input,2U,17,seed,output)
        !=M18CodecStatus::PCM24_OVERLOAD)return 40;
    for(const auto v:output)if(v!=0xA5U)return 41;
    input.back()=0.0;
    if(m18_pcm24_block(input,2U,17,seed,output)
        !=M18CodecStatus::OK)return 42;
    // Verify exact frame address continuity across arbitrary chunking
    // and per-channel independence; mono and stereo are different layouts.
    std::array<std::uint8_t,18> chunked{};
    if(m18_pcm24_block(std::span<const double>(input).first(2),2U,
        17,seed,std::span<std::uint8_t>(chunked).first(6))
            !=M18CodecStatus::OK)return 43;
    if(m18_pcm24_block(std::span<const double>(input).subspan(2),2U,
        18,seed,std::span<std::uint8_t>(chunked).subspan(6))
            !=M18CodecStatus::OK)return 44;
    if(chunked!=output)return 45;
    if(m18_pcm24_block(input,2U,UINT64_MAX,seed,output)
        !=M18CodecStatus::FRAME_ADDRESS_OVERFLOW)return 46;
    std::array<double,2> oneFrame{{0.0,0.0}};
    std::array<std::uint8_t,6> oneBytes{};
    if(m18_pcm24_block(oneFrame,2U,UINT64_MAX,seed,oneBytes)
        !=M18CodecStatus::OK)return 47;
    if(m18_pcm24_block(input,3U,0,seed,output)
        !=M18CodecStatus::INVALID_LAYOUT)return 48;
    std::array<std::uint8_t,24> floatBytes{};
    floatBytes.fill(0xA5U);
    input.back()=std::bit_cast<double>(UINT64_C(0x7FF0000000000000));
    if(m18_float32_block(input,2U,17,floatBytes)
        !=M18CodecStatus::NONFINITE_INPUT)return 49;
    for(const auto v:floatBytes)if(v!=0xA5U)return 50;
    input.back()=0.0;
    if(m18_float32_block(input,2U,17,floatBytes)
        !=M18CodecStatus::OK)return 51;
    return 0;
}

static int m18_test_fp_environment_independence()
{
    using namespace rgsml::audio::internal;
    const int previous=std::fegetround();
    if(previous==-1)return 0; // platform may not expose host fenv
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    const double source=std::bit_cast<double>(
        UINT64_C(0x3e70000000000000));
    const auto pcm=m18_quantize_pcm24(source,seed,12,1);
    const auto f32=m18_convert_float32(
        std::bit_cast<double>(UINT64_C(0x3690000000000001)));
    bool matched=true;
    for(const auto mode:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
        if(std::fesetround(mode)!=0)continue;
        const auto a=m18_quantize_pcm24(source,seed,12,1);
        const auto b=m18_convert_float32(
            std::bit_cast<double>(UINT64_C(0x3690000000000001)));
        if(a.status!=pcm.status||a.bytes!=pcm.bytes
            ||b.status!=f32.status||b.bits!=f32.bits)matched=false;
    }
    if(std::fesetround(previous)!=0)return 61;
    return matched?0:60;
}

int main()
{
    // Runtime checks mirror independently anchored compile-time vectors
    // and prove deterministic output across asynchronous chunk boundaries.
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    constexpr auto start=UINT64_C(0xFFFFFFFB);
    std::array<M18PhiloxWords,17> flat{};
    std::array<M18PhiloxWords,17> ragged{};
    for(std::size_t i=0;i<flat.size();++i)
        flat[i]=m18_philox4x32_10(seed,start+i,0);
    std::size_t at=0;
    for(std::size_t chunk:{1U,2U,3U,7U,4U}) {
        for(std::size_t i=0;i<chunk;++i)
            ragged[at+i]=m18_philox4x32_10(seed,start+at+i,0);
        at+=chunk;
    }
    if(at!=flat.size()||flat!=ragged) return 1;

    for(std::size_t i=0;i<flat.size();++i) {
        if(flat[i]==m18_philox4x32_10(seed,start+i,1))return 2;
        for(std::uint32_t channel:{0U,1U,31U}) {
            const auto d=m18_tpdf(seed,start+i,channel);
            if(!(d.u0>=0.0 && d.u0<1.0 &&
                 d.u1>=0.0 && d.u1<1.0 &&
                 d.dither_lsb()>-1.0 && d.dither_lsb()<1.0))
                return 3;
        }
    }
    constexpr std::array<std::int32_t,10> boundary{{
        -8388608,-8388607,-65536,-257,-1,0,1,257,65535,8388607}};
    for(const auto q:boundary){
        const auto b=m18_pack_pcm24_checked(q);
        if(!b||m18_decode_pcm24_for_test(*b)!=q)return 4;
    }
    if(m18_pack_pcm24_checked(INT64_C(8388608))
       ||m18_pack_pcm24_checked(-INT64_C(8388609))
       ||m18_pack_pcm24_checked(INT64_MAX)
       ||m18_pack_pcm24_checked(INT64_MIN))return 5;
    // Structural byte order: frame-major interleaving, no hidden RNG state.
    const auto left=m18_pack_pcm24_checked(8388607);
    const auto right=m18_pack_pcm24_checked(-8388608);
    if(!left||!right||(*left)[0]!=0xFFU||(*left)[2]!=0x7FU
       ||(*right)[0]!=0x00U||(*right)[2]!=0x80U)return 6;

    if(const auto s=m18_test_pcm24_numerics();s) return s;
    if(const auto s=m18_test_float32_numerics();s) return s;
    if(const auto s=m18_test_fail_atomic_and_chunks();s) return s;
    if(const auto s=m18_test_fp_environment_independence();s) return s;
    std::puts("M18_PREPRODUCTION_PORTABLE_PASS: exact RN64 PCM24, Float32 RNE, fail-atomic blocks, corrected Philox, FP-mode independence");
    return 0;
}
