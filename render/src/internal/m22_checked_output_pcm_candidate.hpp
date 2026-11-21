// M22 PRIVATE typed witness for an already-materialized full OUTPUT_RATE PCM.
// It validates the actual AudioBufferView, not external count strings alone.
// Neither a buffer view nor this metadata probe grants M17/FinalSRC release.
#pragma once
#include "m22_checked_output_clock_candidate.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <cstddef>
#include <cstdint>

namespace rgsml::m22_private {

enum class PcmClockFailure : std::uint8_t {
    NONE, INVALID_OUTPUT_DOMAIN, WRONG_OUTPUT_RATE, NOT_WHOLE_PROGRAMME,
    WRONG_CHANNEL_LAYOUT, UNREADABLE_CHANNELS
};

// The immutable full-master owner MUST be retained separately by the
// canonical M17/M18 Product epoch gate. This is a read-only consistency
// check, never a license to publish export/Preview from arbitrary PCM.
[[nodiscard]] inline PcmClockFailure
check_real_full_output_pcm_candidate(
    const CheckedOutputClockCandidate& expected,
    const rgsml::audio::AudioBufferView& actual) noexcept
{
    using namespace rgsml;
    if(actual.timebase().frame_domain_id()
           !=audio::FrameDomainId::OUTPUT_RATE)
        return PcmClockFailure::INVALID_OUTPUT_DOMAIN;
    const auto& declared=expected.declaration();
    if(actual.timebase().sample_rate().value()
           !=static_cast<std::int64_t>(declared.output_hz)
       ||actual.format().sample_rate().value()
           !=static_cast<std::int64_t>(declared.output_hz))
        return PcmClockFailure::WRONG_OUTPUT_RATE;
    if(actual.absolute_start_frame().value()!=0
       ||actual.frame_count().value()<1
       ||static_cast<std::uint64_t>(actual.frame_count().value())
           !=declared.declared_full_output_frames
       ||actual.absolute_end_frame().value()
           !=actual.frame_count().value())
        return PcmClockFailure::NOT_WHOLE_PROGRAMME;
    const auto count=actual.format().channel_count();
    if(count<1||count>2)
        return PcmClockFailure::WRONG_CHANNEL_LAYOUT;
    for(std::size_t ch=0;ch<count;++ch) {
        const auto plane=actual.channel(ch);
        if(!plane||plane.value()->size()
               !=static_cast<std::size_t>(actual.frame_count().value()))
            return PcmClockFailure::UNREADABLE_CHANNELS;
    }
    return PcmClockFailure::NONE;
}
} // namespace rgsml::m22_private
