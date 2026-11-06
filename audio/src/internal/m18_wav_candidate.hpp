#pragma once
// RGS MasterLab M18 PREPRODUCTION — portable, BOUNDED WAV candidate assembly.
// Zero modification to M17, old F64 WavWriter, the DAC or published files.
// This is a disposable memory-only candidate, NOT transactional final export.
// The final Windows filesystem writer/reader and SHA-256 acceptance remain
// separate product integration and physical qualification gates.
#include "m18_terminal_sample_codec.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::audio::internal {

enum class M18WavEncoding : std::uint8_t { PCM24, IEEE_FLOAT32 };
enum class M18WavStatus : std::uint8_t {
    OK, INVALID_SPEC, SIZE_OVERFLOW, MEMORY_LIMIT,
    CODEC_REJECTED, ALLOCATION_FAILED
};
struct M18WavSpec {
    M18WavEncoding encoding;
    std::uint32_t sample_rate;
    std::uint32_t channels; // 1 or 2, frame-major L/R
    std::uint64_t frame_count;
    std::uint64_t dither_seed; // authoritative for PCM24, irrelevant to Float32
};
struct M18WavPlan {
    M18WavSpec spec;
    bool rf64{};
    std::uint16_t block_align{};
    std::uint32_t byte_rate{};
    std::uint64_t data_bytes{};
    std::uint64_t file_bytes{};
    std::uint32_t header_bytes{};
};
struct M18WavPlanResult {
    M18WavStatus status{M18WavStatus::INVALID_SPEC};
    M18WavPlan plan{};
};

// RIFF or RF64 exactly as the existing frozen IEEE_F64 writer semantics:
// no WAVEFORMATEXTENSIBLE needed for mono/stereo PCM24/IEEE_FLOAT32.
// PCM24 RIFF 44 / RF64 80 bytes; Float32 RIFF 58 / RF64 94 bytes.
// Float32 gets mandatory fact chunk; PCM24 is ordinary WAVE_FORMAT_PCM.
[[nodiscard]] constexpr M18WavPlanResult m18_plan_wav(
    M18WavSpec spec) noexcept
{
    constexpr std::array<std::uint32_t,6> rates{{
        44100U,48000U,88200U,96000U,176400U,192000U}};
    if((spec.encoding!=M18WavEncoding::PCM24 &&
        spec.encoding!=M18WavEncoding::IEEE_FLOAT32) ||
        (spec.channels!=1U && spec.channels!=2U) ||
        std::find(rates.begin(),rates.end(),spec.sample_rate)==rates.end() ||
        spec.frame_count>static_cast<std::uint64_t>(INT64_MAX))
        return {};
    const std::uint64_t bytes_per_sample =
        spec.encoding==M18WavEncoding::PCM24 ? 3U:4U;
    const std::uint64_t align=bytes_per_sample*spec.channels;
    const std::uint64_t bytes_per_sec=align*spec.sample_rate;
    if(align>UINT16_MAX || bytes_per_sec>UINT32_MAX)
        return {M18WavStatus::SIZE_OVERFLOW,{}};
    if(spec.frame_count>UINT64_MAX/align)
        return {M18WavStatus::SIZE_OVERFLOW,{}};
    const std::uint64_t data_bytes=spec.frame_count*align;
    // RIFF chunks are WORD aligned. The data chunk reports the exact
    // unpadded sample-byte count; a trailing zero byte belongs only to
    // the enclosing RIFF/RF64 file size (and full-file SHA), never PCM.
    const std::uint64_t data_pad=data_bytes&UINT64_C(1);
    const std::uint64_t riff_overhead=
        spec.encoding==M18WavEncoding::PCM24 ? 44U:58U;
    const std::uint64_t rf64_overhead=
        spec.encoding==M18WavEncoding::PCM24 ? 80U:94U;
    if(data_bytes>UINT64_MAX-rf64_overhead-data_pad)
        return {M18WavStatus::SIZE_OVERFLOW,{}};
    const bool use_rf64=data_bytes>UINT32_C(0xFFFFFFFE)
        || data_bytes+data_pad>UINT64_C(0xFFFFFFFE)-(riff_overhead-8U);
    const auto overhead=use_rf64?rf64_overhead:riff_overhead;
    return {M18WavStatus::OK,M18WavPlan{
        spec,use_rf64,static_cast<std::uint16_t>(align),
        static_cast<std::uint32_t>(bytes_per_sec),data_bytes,
        data_bytes+data_pad+overhead,static_cast<std::uint32_t>(overhead)}};
}

namespace m18_wav_detail {
inline void append_u16(std::vector<std::uint8_t>& o,
                       std::uint16_t n)
{
    o.push_back(static_cast<std::uint8_t>(n));
    o.push_back(static_cast<std::uint8_t>(n>>8));
}
inline void append_u32(std::vector<std::uint8_t>& o,
                       std::uint32_t n)
{
    for(unsigned i=0;i<4;++i)o.push_back(
        static_cast<std::uint8_t>(n>>(8U*i)));
}
inline void append_u64(std::vector<std::uint8_t>& o,
                       std::uint64_t n)
{
    for(unsigned i=0;i<8;++i)o.push_back(
        static_cast<std::uint8_t>(n>>(8U*i)));
}
inline void fourcc(std::vector<std::uint8_t>& o,
                   const char (&tag)[5])
{
    for(unsigned i=0;i<4;++i)
        o.push_back(static_cast<std::uint8_t>(tag[i]));
}
} // namespace m18_wav_detail

// Caller supplies an already-validated plan. For a plan derived through
// m18_plan_wav this yields a constant-size canonical byte sequence.
// No file I/O or publication occurs.
[[nodiscard]] inline std::vector<std::uint8_t> m18_wav_header(
    const M18WavPlan& p)
{
    using namespace m18_wav_detail;
    std::vector<std::uint8_t> out;
    out.reserve(p.header_bytes);
    if(p.rf64)fourcc(out,"RF64");
    else fourcc(out,"RIFF");
    append_u32(out,p.rf64?UINT32_MAX:
        static_cast<std::uint32_t>(p.file_bytes-8U));
    fourcc(out,"WAVE");
    if(p.rf64){
        fourcc(out,"ds64");
        append_u32(out,28U);
        append_u64(out,p.file_bytes-8U);
        append_u64(out,p.data_bytes);
        append_u64(out,p.spec.frame_count);
        append_u32(out,0U);
    }
    fourcc(out,"fmt ");
    const bool is_float=p.spec.encoding==M18WavEncoding::IEEE_FLOAT32;
    append_u32(out,is_float?18U:16U);
    append_u16(out,is_float?3U:1U); // IEEE_FLOAT vs PCM
    append_u16(out,static_cast<std::uint16_t>(p.spec.channels));
    append_u32(out,p.spec.sample_rate);
    append_u32(out,p.byte_rate);
    append_u16(out,p.block_align);
    append_u16(out,is_float?32U:24U);
    if(is_float){
        append_u16(out,0U); // WAVEFORMATEX cbSize
        fourcc(out,"fact");
        append_u32(out,4U);
        append_u32(out,p.spec.frame_count>UINT32_MAX
            ?UINT32_MAX:static_cast<std::uint32_t>(p.spec.frame_count));
    }
    fourcc(out,"data");
    append_u32(out,p.rf64?UINT32_MAX:
        static_cast<std::uint32_t>(p.data_bytes));
    return out;
}

struct M18WavCandidate {
    M18WavStatus status{M18WavStatus::INVALID_SPEC};
    M18CodecStatus codec_status{M18CodecStatus::OK};
    std::optional<M18WavPlan> plan{};
    std::vector<std::uint8_t> bytes{};
};

// Bounded in-memory oracle and integration fixture, not large-program
// exporter. Only COMPLETE candidates are returned; errors return no bytes.
// Caller supplies the final-MASTER binary64 samples in frame-major order.
// Mutually isolated channel addressing and absolute SOURCE/M17 rate domains
// are the responsibility of the future immutable Master owner.
[[nodiscard]] inline M18WavCandidate m18_make_bounded_wav_candidate(
    M18WavSpec spec,std::span<const double> interleaved,
    std::uint64_t first_absolute_output_frame=0U) noexcept
{
    const auto p=m18_plan_wav(spec);
    if(p.status!=M18WavStatus::OK)return {p.status};
    if(spec.frame_count!=0U && spec.frame_count-1U>
        UINT64_MAX-first_absolute_output_frame)
        return {M18WavStatus::SIZE_OVERFLOW};
    if(spec.frame_count!=interleaved.size()/spec.channels
       ||interleaved.size()%spec.channels!=0U)
        return {M18WavStatus::INVALID_SPEC};
    constexpr std::uint64_t max_candidate_bytes=UINT64_C(16)*1024*1024;
    if(p.plan.file_bytes>max_candidate_bytes)
        return {M18WavStatus::MEMORY_LIMIT};
    try {
        auto header=m18_wav_header(p.plan);
        if(header.size()!=p.plan.header_bytes)
            return {M18WavStatus::INVALID_SPEC};
        std::vector<std::uint8_t> result(
            static_cast<std::size_t>(p.plan.file_bytes),0U);
        std::copy(header.begin(),header.end(),result.begin());
        const auto payload=std::span<std::uint8_t>{result}.subspan(
            p.plan.header_bytes);
        const auto c=spec.encoding==M18WavEncoding::PCM24
            ?m18_pcm24_block(interleaved,spec.channels,
                first_absolute_output_frame,spec.dither_seed,payload)
            :m18_float32_block(interleaved,spec.channels,
                first_absolute_output_frame,payload);
        if(c!=M18CodecStatus::OK)
            return {M18WavStatus::CODEC_REJECTED,c};
        return {M18WavStatus::OK,M18CodecStatus::OK,p.plan,
            std::move(result)};
    } catch(...) {
        return {M18WavStatus::ALLOCATION_FAILED};
    }
}
} // namespace rgsml::audio::internal
