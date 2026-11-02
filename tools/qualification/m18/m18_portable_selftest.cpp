// Standalone M18 fast preproduction CI: C++20 standard library only.
// No Qt/M17/DEVICE/reader/writer, no release or full Gate-G claim.
// One tiny translation unit compiles under both MSVC and GCC/Clang.
#include "internal/m18_counter_dither_philox.hpp"
#include "internal/m18_pcm24_pack.hpp"
#include <array>
#include <cstdint>
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

    std::puts("M18_PREPRODUCTION_PORTABLE_PASS: corrected Philox, TPDF, frame carry, chunk identity, PCM24 strict packing");
    return 0;
}
