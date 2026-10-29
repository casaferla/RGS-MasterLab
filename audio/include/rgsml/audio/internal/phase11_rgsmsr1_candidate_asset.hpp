#pragma once

// Phase-11 RGSMSR1 binary candidate parser, TEST/QUALIFICATION ONLY.
// This verifies structural format, length, finite symmetric IEEE binary64
// tap bits and caller-declared rational geometry. It intentionally performs
// NO SHA-256 authentication, coefficient-proof, DTFT Q1-Q6, kernel release
// admission or runtime discovery. NEVER use it to enable product Final SRC.
#include <rgsml/core/result.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::audio::internal {
namespace phase11_candidate_detail {
[[nodiscard]] inline std::uint16_t read16(
    std::span<const std::uint8_t> bytes,std::size_t pos) noexcept
{
    return static_cast<std::uint16_t>(bytes[pos])
        |static_cast<std::uint16_t>(bytes[pos+1])<<8;
}
[[nodiscard]] inline std::uint32_t read32(
    std::span<const std::uint8_t> bytes,std::size_t pos) noexcept
{
    std::uint32_t v=0;
    for(std::size_t i=0;i<4;++i)
        v|=static_cast<std::uint32_t>(bytes[pos+i])<<(8*i);
    return v;
}
[[nodiscard]] inline std::uint64_t read64(
    std::span<const std::uint8_t> bytes,std::size_t pos) noexcept
{
    std::uint64_t v=0;
    for(std::size_t i=0;i<8;++i)
        v|=static_cast<std::uint64_t>(bytes[pos+i])<<(8*i);
    return v;
}
[[nodiscard]] inline core::Error invalid(const char* reason)
{
    return core::Error{core::ErrorCode::InvalidArgument,reason,
        {{"category","PHASE11_TEST_ASSET_PARSER_ONLY"}}};
}
} // namespace phase11_candidate_detail

[[nodiscard]] inline core::Result<std::vector<std::uint64_t>>
parse_phase11_rgsmsr1_candidate_bits(
    std::span<const std::uint8_t> bytes,
    std::uint32_t expectedL,std::uint32_t expectedM,
    std::uint32_t expectedT)
{
    using namespace phase11_candidate_detail;
    using Result=core::Result<std::vector<std::uint64_t>>;
    const auto reject=[](const char* reason)->Result {
        return Result::failure(invalid(reason));
    };
    constexpr std::size_t headerSize=52;
    if(expectedL==0||expectedM==0||std::gcd(expectedL,expectedM)!=1
       ||(expectedL==1&&expectedM==1)
       ||expectedT<512||expectedT%2!=0
       ||expectedL>(std::numeric_limits<std::uint32_t>::max()/expectedT))
        return reject("Phase11 invalid nonidentity rational/kernel geometry");
    const auto count=static_cast<std::uint64_t>(expectedL)*expectedT+1;
    if(count>static_cast<std::uint64_t>(
        (std::numeric_limits<std::size_t>::max()-headerSize)/8))
        return reject("Phase11 candidate asset size overflows");
    const auto expectedBytes=headerSize+8*static_cast<std::size_t>(count);
    if(bytes.size()!=expectedBytes)
        return reject("Phase11 candidate asset exact length mismatch");
    constexpr std::uint8_t magic[8]={
        'R','G','S','M','S','R','1',0};
    for(std::size_t i=0;i<8;++i)
        if(bytes[i]!=magic[i])
            return reject("Phase11 candidate asset magic mismatch");
    if(read16(bytes,8)!=1||read16(bytes,10)!=1
       ||read32(bytes,12)!=expectedL
       ||read32(bytes,16)!=expectedM
       ||read32(bytes,20)!=expectedT
       ||read32(bytes,24)!=0
       ||read64(bytes,28)!=count
       ||read64(bytes,36)!=count
       ||read64(bytes,44)!=8*count)
        return reject("Phase11 candidate asset frozen header mismatch");
    std::vector<std::uint64_t> taps;
    taps.reserve(static_cast<std::size_t>(count));
    for(std::size_t j=0;j<static_cast<std::size_t>(count);++j) {
        const auto bitPattern=read64(bytes,headerSize+8*j);
        if(!std::isfinite(std::bit_cast<double>(bitPattern)))
            return reject("Phase11 candidate asset has nonfinite tap");
        taps.push_back(bitPattern);
    }
    for(std::size_t j=0;j<taps.size()/2;++j)
        if(taps[j]!=taps[taps.size()-1-j])
            return reject("Phase11 candidate asset is not bit symmetric");
    return Result::success(std::move(taps));
}
} // namespace rgsml::audio::internal
