#pragma once
// M18 preproduction: decode our OWN bounded immutable terminal candidate to
// canonical binary64 samples for FUTURE independent M16 post-delivery TP.
// NOT an M16 measurement, not a trusted unvalidated external-file decoder,
// not the production WAV reader and not final-delivery acceptance.
#include "m18_wav_candidate.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rgsml::audio::internal {

enum class M18DecodeStatus : std::uint8_t {
    OK, NO_CANDIDATE, INCONSISTENT_HEADER,
    MALFORMED_PAYLOAD, ALLOCATION_FAILED
};

struct M18DecodedCandidate {
    M18DecodeStatus status{M18DecodeStatus::NO_CANDIDATE};
    std::uint32_t sample_rate{};
    std::uint32_t channels{};
    std::vector<double> interleaved{}; // MASTER target Fs only
};

[[nodiscard]] inline M18DecodedCandidate
m18_decode_own_bounded_candidate(const M18WavCandidate& candidate) noexcept
{
    if(candidate.status!=M18WavStatus::OK || !candidate.plan)
        return {};
    const auto p=*candidate.plan;
    const auto canonical=m18_plan_wav(p.spec);
    if(canonical.status!=M18WavStatus::OK ||
       canonical.plan.rf64!=p.rf64 ||
       canonical.plan.file_bytes!=p.file_bytes ||
       canonical.plan.header_bytes!=p.header_bytes ||
       candidate.bytes.size()!=p.file_bytes)
        return {M18DecodeStatus::INCONSISTENT_HEADER};
    try {
        const auto expectedHeader=m18_wav_header(p);
        if(!std::equal(expectedHeader.begin(),expectedHeader.end(),
                       candidate.bytes.begin()))
            return {M18DecodeStatus::INCONSISTENT_HEADER};
        const auto frames=p.spec.frame_count;
        const auto channels=p.spec.channels;
        const std::uint64_t n=frames*channels;
        if(n>candidate.bytes.size())
            return {M18DecodeStatus::MALFORMED_PAYLOAD};
        std::vector<double> decoded(static_cast<std::size_t>(n),0.0);
        const auto step=p.spec.encoding==M18WavEncoding::PCM24?3U:4U;
        for(std::size_t i=0;i<decoded.size();++i) {
            const auto at=static_cast<std::size_t>(p.header_bytes)
                +i*step;
            if(at+step>candidate.bytes.size())
                return {M18DecodeStatus::MALFORMED_PAYLOAD};
            if(p.spec.encoding==M18WavEncoding::PCM24) {
                M18Pcm24Bytes bytes{{
                    candidate.bytes[at],candidate.bytes[at+1U],
                    candidate.bytes[at+2U]}};
                const auto integer=m18_decode_pcm24_for_test(bytes);
                decoded[i]=static_cast<double>(integer)*0x1.0p-23;
            } else {
                const std::uint32_t bits=
                    static_cast<std::uint32_t>(candidate.bytes[at])
                    |(static_cast<std::uint32_t>(candidate.bytes[at+1U])<<8)
                    |(static_cast<std::uint32_t>(candidate.bytes[at+2U])<<16)
                    |(static_cast<std::uint32_t>(candidate.bytes[at+3U])<<24);
                const float sample=std::bit_cast<float>(bits);
                if(!std::isfinite(sample))
                    return {M18DecodeStatus::MALFORMED_PAYLOAD};
                decoded[i]=static_cast<double>(sample);
            }
        }
        return {M18DecodeStatus::OK,p.spec.sample_rate,p.spec.channels,
                std::move(decoded)};
    } catch(...) {
        return {M18DecodeStatus::ALLOCATION_FAILED};
    }
}
} // namespace rgsml::audio::internal
