#pragma once
// RGS MasterLab M18 preproduction: integer-only, FP-environment-independent
// terminal PCM24 and Float32 codecs, sharing the frozen addressed TPDF.
// Not wired to the verified M17 Master, C++ output writer, UI or release.
// Exact arithmetic contract: binary64(x*2^23) (power-of-two exact),
// binary64(scaled + D) round-to-nearest/ties-even, then integer RNE.
// For reproducibility even under FE_UPWARD/DOWNWARD, binary64 addition is
// reconstructed from exact signed dyadics and rounded with integer bits.
// No silent saturation, no changed Master, no reusable RNG state.

#include "m18_counter_dither_philox.hpp"
#include "m18_pcm24_pack.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace rgsml::audio::internal {

enum class M18CodecStatus : std::uint8_t {
    OK, INVALID_LAYOUT, FRAME_ADDRESS_OVERFLOW, NONFINITE_INPUT,
    PCM24_OVERLOAD, FLOAT32_OVERFLOW
};

namespace m18_exact_detail {

// Input allowed for PCM24 cannot have |x|>=2 even with the full -1 LSB
// dither; restrict exact-dyadic work to at most 2^24 scaled amplitude.
// Smallest double subnormal * 2^23 is 2^-1051. A fixed 2^1051
// denominator therefore represents EVERY finite allowed x*2^23 and
// every 53-bit TPDF D exactly. Max magnitude uses <1078 binary digits.
struct UInt1152 {
    std::array<std::uint64_t,18> limb{};
    [[nodiscard]] static constexpr UInt1152 shifted(
        std::uint64_t v, unsigned bits) noexcept
    {
        UInt1152 out{};
        const auto i=bits/64U, r=bits%64U;
        if(i<18U)out.limb[i]=v<<r;
        if(r!=0U && i+1U<18U)out.limb[i+1U]=v>>(64U-r);
        return out;
    }
    [[nodiscard]] constexpr int compare(const UInt1152& rhs) const noexcept
    {
        for(int i=17;i>=0;--i) {
            if(limb[static_cast<std::size_t>(i)] >
                rhs.limb[static_cast<std::size_t>(i)])return 1;
            if(limb[static_cast<std::size_t>(i)] <
                rhs.limb[static_cast<std::size_t>(i)])return -1;
        }
        return 0;
    }
    [[nodiscard]] constexpr UInt1152 add(const UInt1152& rhs) const noexcept
    {
        UInt1152 out{};
        bool carry=false;
        for(std::size_t i=0;i<18;++i) {
            const auto t=limb[i]+rhs.limb[i];
            const auto sum=t+static_cast<std::uint64_t>(carry);
            carry=(t<limb[i])||(sum<t);
            out.limb[i]=sum;
        }
        return out;
    }
    // Only called when *this>=rhs; no signed arithmetic/undefined shifts.
    [[nodiscard]] constexpr UInt1152 subtract(
        const UInt1152& rhs) const noexcept
    {
        UInt1152 out{};
        bool borrow=false;
        for(std::size_t i=0;i<18;++i) {
            const auto t=limb[i]-rhs.limb[i];
            const auto diff=t-static_cast<std::uint64_t>(borrow);
            const bool next=(limb[i]<rhs.limb[i])
                ||(borrow && limb[i]==rhs.limb[i]);
            out.limb[i]=diff;
            borrow=next;
        }
        return out;
    }
    [[nodiscard]] constexpr int top_bit() const noexcept
    {
        for(int i=17;i>=0;--i) {
            const auto w=limb[static_cast<std::size_t>(i)];
            if(w!=0U) return i*64+63-static_cast<int>(std::countl_zero(w));
        }
        return -1;
    }
    [[nodiscard]] constexpr bool bit(unsigned pos) const noexcept
    {
        return pos<1152U && (limb[pos/64U]&(UINT64_C(1)<<(pos%64U)))!=0U;
    }
    [[nodiscard]] constexpr bool any_below(unsigned n) const noexcept
    {
        const auto whole=n/64U;
        for(unsigned i=0;i<whole && i<18U;++i)
            if(limb[i])return true;
        if(whole<18U && (n%64U)!=0U)
            return (limb[whole] & ((UINT64_C(1)<<(n%64U))-1U))!=0U;
        return false;
    }
    [[nodiscard]] constexpr std::uint64_t down(unsigned n) const noexcept
    {
        if(n>=1152U)return 0;
        const auto i=n/64U,r=n%64U;
        const auto lo=limb[i]>>r;
        return r!=0U && i+1U<18U
            ?lo|(limb[i+1U]<<(64U-r)):lo;
    }
};

[[nodiscard]] constexpr std::uint64_t abs_bits(
    std::uint64_t bits) noexcept
{
    return bits&UINT64_C(0x7FFFFFFFFFFFFFFF);
}

[[nodiscard]] inline std::uint64_t
round_exact_sum_to_binary64(std::uint64_t sourceBits,
                           std::int64_t signedDitherGrid53) noexcept
{
    const bool xNeg=(sourceBits>>63)!=0U;
    const auto eb=static_cast<unsigned>(
        (sourceBits>>52)&UINT64_C(0x7FF));
    const std::uint64_t frac=sourceBits&
        UINT64_C(0x000FFFFFFFFFFFFF);
    const std::uint64_t mant=eb==0U ? frac
        : frac|UINT64_C(0x0010000000000000);
    // x*2^23 = mant * 2^(eb-1052) for normal x.
    const UInt1152 x=UInt1152::shifted(mant,eb==0U ? 0U : eb-1U);
    const bool dNeg=signedDitherGrid53<0;
    const std::uint64_t dAbs=dNeg
        ?static_cast<std::uint64_t>(-signedDitherGrid53)
        :static_cast<std::uint64_t>(signedDitherGrid53);
    const UInt1152 d=UInt1152::shifted(dAbs,998U);
    UInt1152 magnitude{};
    bool negative=false;
    if(xNeg==dNeg) {
        magnitude=x.add(d);
        negative=xNeg;
    } else if(x.compare(d)>=0) {
        magnitude=x.subtract(d);
        negative=xNeg;
    } else {
        magnitude=d.subtract(x);
        negative=dNeg;
    }
    const int top=magnitude.top_bit();
    if(top<0)return 0; // PCM has no signed zero.
    const std::uint64_t sign=negative?UINT64_C(1)<<63:0;
    // Value is magnitude*2^-1051, so subnormals (top<29) are exact
    // multiples of 2^-1051 = 2^23 * smallest binary64 subnormal.
    if(top<29)
        return sign|(magnitude.down(0)<<23);
    int exponent=top-1051;
    std::uint64_t significand=0;
    if(top>=52) {
        const unsigned discard=static_cast<unsigned>(top-52);
        significand=magnitude.down(discard);
        if(discard && magnitude.bit(discard-1U)
            && (magnitude.any_below(discard-1U)||(significand&1U)))
            ++significand;
        if(significand==(UINT64_C(1)<<53)) {
            significand>>=1;
            ++exponent;
        }
    } else {
        significand=magnitude.down(0)<<static_cast<unsigned>(52-top);
    }
    return sign
        |(static_cast<std::uint64_t>(exponent+1023)<<52)
        |(significand&UINT64_C(0x000FFFFFFFFFFFFF));
}

// Integer round-to-nearest/ties-to-even from *canonical* binary64 bits.
// All callers bound |y|<2^25, so the integer result fits 32 bits.
[[nodiscard]] constexpr std::uint32_t
rounded_positive_integer_from_binary64(std::uint64_t bits) noexcept
{
    const auto eb=static_cast<unsigned>((bits>>52)&0x7FFU);
    if(eb==0U)return 0U;
    const int e=static_cast<int>(eb)-1023;
    if(e< -1)return 0U;
    const std::uint64_t sig=(UINT64_C(1)<<52)
        |(bits&UINT64_C(0x000FFFFFFFFFFFFF));
    const unsigned shift=static_cast<unsigned>(52-e);
    const std::uint64_t integer=sig>>shift;
    const std::uint64_t rest=sig&((UINT64_C(1)<<shift)-1U);
    const auto half=UINT64_C(1)<<(shift-1U);
    return static_cast<std::uint32_t>(integer
        +static_cast<std::uint64_t>(rest>half
            ||(rest==half && (integer&1U))));
}

[[nodiscard]] constexpr std::uint64_t
round_shift_right_even(std::uint64_t v,unsigned shift) noexcept
{
    if(shift>=65U)return 0;
    if(shift==64U)return 0; // v<2^64, never reaches half 2^63? handled below
    if(shift==0U)return v;
    const auto truncated=v>>shift;
    const auto rem=v&((UINT64_C(1)<<shift)-1U);
    const auto half=UINT64_C(1)<<(shift-1U);
    return truncated+static_cast<std::uint64_t>(
        rem>half||(rem==half&&(truncated&1U)));
}
} // namespace m18_exact_detail

struct M18Pcm24Sample {
    M18CodecStatus status{M18CodecStatus::PCM24_OVERLOAD};
    std::int32_t code{};
    M18Pcm24Bytes bytes{};
};

[[nodiscard]] inline M18Pcm24Sample m18_quantize_pcm24(
    double input,std::uint64_t seed,std::uint64_t absoluteOutputFrame,
    std::uint32_t channelIndex) noexcept
{
    const auto bits=std::bit_cast<std::uint64_t>(input);
    const auto abs=m18_exact_detail::abs_bits(bits);
    if(abs>=UINT64_C(0x7FF0000000000000))
        return {M18CodecStatus::NONFINITE_INPUT,0,{}};
    // At |x|>=2 even D in (-1,+1) cannot bring the integer result
    // into signed 24-bit range. Safe pre-reject, not saturation.
    if(abs>=UINT64_C(0x4000000000000000))
        return {M18CodecStatus::PCM24_OVERLOAD,0,{}};
    const auto words=m18_philox4x32_10(seed,absoluteOutputFrame,channelIndex);
    const auto z0=m18_uniform53(words[0],words[1]);
    const auto z1=m18_uniform53(words[2],words[3]);
    const auto d=static_cast<std::int64_t>(z0)
        -static_cast<std::int64_t>(z1);
    const auto rounded=m18_exact_detail::round_exact_sum_to_binary64(bits,d);
    const auto magnitude=m18_exact_detail::
        rounded_positive_integer_from_binary64(rounded&
            UINT64_C(0x7FFFFFFFFFFFFFFF));
    const bool neg=(rounded>>63)!=0U;
    const std::int64_t signedCode=neg
        ?-static_cast<std::int64_t>(magnitude)
        :static_cast<std::int64_t>(magnitude);
    const auto packed=m18_pack_pcm24_checked(signedCode);
    if(!packed)return {M18CodecStatus::PCM24_OVERLOAD,0,{}};
    return {M18CodecStatus::OK,static_cast<std::int32_t>(signedCode),*packed};
}

struct M18Float32Sample {
    M18CodecStatus status{M18CodecStatus::FLOAT32_OVERFLOW};
    std::uint32_t bits{};
    std::array<std::uint8_t,4> bytes{};
};

// All operations are uint64 integer/bit-level; this explicitly preserves
// signed zero, RNE halfway/subnormal, finite out-of-unity values, and is
// independent of ambient FE_* rounding modes and x87/FTZ/DAZ policies.
[[nodiscard]] inline M18Float32Sample m18_convert_float32(
    double input) noexcept
{
    const auto raw=std::bit_cast<std::uint64_t>(input);
    const auto abs=m18_exact_detail::abs_bits(raw);
    if(abs>=UINT64_C(0x7FF0000000000000))
        return {M18CodecStatus::NONFINITE_INPUT,0,{}};
    const std::uint32_t sign=(raw>>63)!=0U ? UINT32_C(0x80000000):0U;
    const auto eb=static_cast<unsigned>((raw>>52)&0x7FFU);
    const auto frac=raw&UINT64_C(0x000FFFFFFFFFFFFF);
    std::uint32_t bits=sign;
    if(eb==0U) {
        // Every binary64 subnormal is far below binary32 half-minimum.
        bits=sign;
    } else {
        int e=static_cast<int>(eb)-1023;
        const std::uint64_t sig=(UINT64_C(1)<<52)|frac;
        if(e>127)
            return {M18CodecStatus::FLOAT32_OVERFLOW,0,{}};
        if(e< -126) {
            const unsigned shift=static_cast<unsigned>(-e-97);
            const auto q=m18_exact_detail::round_shift_right_even(sig,shift);
            if(q>=UINT64_C(0x800000))
                bits=sign|UINT32_C(0x00800000); // rounded to min normal
            else bits=sign|static_cast<std::uint32_t>(q);
        } else {
            auto q=m18_exact_detail::round_shift_right_even(sig,29U);
            if(q==(UINT64_C(1)<<24)){
                q>>=1;
                ++e;
            }
            if(e>127)return {M18CodecStatus::FLOAT32_OVERFLOW,0,{}};
            bits=sign|(static_cast<std::uint32_t>(e+127)<<23)
                |(static_cast<std::uint32_t>(q)&UINT32_C(0x007FFFFF));
        }
    }
    return {M18CodecStatus::OK,bits,{{
        static_cast<std::uint8_t>(bits),
        static_cast<std::uint8_t>(bits>>8),
        static_cast<std::uint8_t>(bits>>16),
        static_cast<std::uint8_t>(bits>>24)
    }}};
}

// A block is entirely PREVALIDATED before ANY output byte changes.
// No allocation, deterministic absolute-frame addressing, no stochastic
// mutable state. Original M17 sample bytes remain borrowed/const.
// For nonzero frame count, starting at UINT64_MAX is valid for exactly
// one frame; no synthetic next frame may wrap.
[[nodiscard]] inline M18CodecStatus m18_pcm24_block(
    std::span<const double> interleaved,
    std::uint32_t channelCount,std::uint64_t firstAbsoluteFrame,
    std::uint64_t seed,std::span<std::uint8_t> output) noexcept
{
    if((channelCount!=1U && channelCount!=2U)
       ||(interleaved.size()%channelCount)!=0U
       ||output.size()%3U!=0U
       ||output.size()/3U!=interleaved.size())
        return M18CodecStatus::INVALID_LAYOUT;
    const auto frames=interleaved.size()/channelCount;
    if(frames>0U && frames-1U >
       UINT64_MAX-firstAbsoluteFrame)
        return M18CodecStatus::FRAME_ADDRESS_OVERFLOW;
    for(std::size_t i=0;i<interleaved.size();++i){
        const auto checked=m18_quantize_pcm24(interleaved[i],seed,
            firstAbsoluteFrame+i/channelCount,
            static_cast<std::uint32_t>(i%channelCount));
        if(checked.status!=M18CodecStatus::OK)return checked.status;
    }
    for(std::size_t i=0;i<interleaved.size();++i){
        const auto checked=m18_quantize_pcm24(interleaved[i],seed,
            firstAbsoluteFrame+i/channelCount,
            static_cast<std::uint32_t>(i%channelCount));
        const auto offset=i*3U;
        output[offset]=checked.bytes[0];
        output[offset+1U]=checked.bytes[1];
        output[offset+2U]=checked.bytes[2];
    }
    return M18CodecStatus::OK;
}

[[nodiscard]] inline M18CodecStatus m18_float32_block(
    std::span<const double> interleaved,
    std::uint32_t channelCount,std::uint64_t firstAbsoluteFrame,
    std::span<std::uint8_t> output) noexcept
{
    if((channelCount!=1U && channelCount!=2U)
       ||(interleaved.size()%channelCount)!=0U
       ||output.size()%4U!=0U
       ||output.size()/4U!=interleaved.size())
        return M18CodecStatus::INVALID_LAYOUT;
    const auto frames=interleaved.size()/channelCount;
    if(frames>0U && frames-1U >
       UINT64_MAX-firstAbsoluteFrame)
        return M18CodecStatus::FRAME_ADDRESS_OVERFLOW;
    for(const double x:interleaved){
        const auto checked=m18_convert_float32(x);
        if(checked.status!=M18CodecStatus::OK)return checked.status;
    }
    for(std::size_t i=0;i<interleaved.size();++i){
        const auto checked=m18_convert_float32(interleaved[i]);
        const auto offset=i*4U;
        for(unsigned b=0;b<4;++b)output[offset+b]=checked.bytes[b];
    }
    return M18CodecStatus::OK;
}

} // namespace rgsml::audio::internal
