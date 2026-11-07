#pragma once
// M19 PREPRODUCTION ONLY: read-only adapter to the EXISTING canonical
// AudioBufferView/subview. This is not a new analysis-scope authority, not
// the Phase-6 feature registry, and not a production analysis service.
// Region True Peak remains unavailable: M16 is full-owning-resource ONLY.
#include "m19_basic_level_candidate.hpp"

#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rgsml::analysis::internal::m19 {

// The expected domain is supplied by the *existing* call-site authority.
// This private adapter refuses to reinterpret SOURCE frames as OUTPUT frames.
[[nodiscard]] inline core::Result<BasicLevelPreproductionSnapshot>
measure_selected_scope_candidate(
    const audio::AudioBufferView& resource,
    core::FrameIndex begin,
    core::FrameCount frames,
    audio::FrameDomainId expectedDomain)
{
    if (resource.timebase().frame_domain_id() != expectedDomain) {
        return core::Result<BasicLevelPreproductionSnapshot>::failure(
            core::Error{core::ErrorCode::InvalidFrameRange,
                        "M19 scope frame domain is not the actual resource domain."});
    }
    if (begin.value() < 0 || frames.value() <= 0) {
        return core::Result<BasicLevelPreproductionSnapshot>::failure(
            core::Error{core::ErrorCode::InvalidFrameRange,
                        "M19 scope origin and frame count must be valid."});
    }
    auto subview = resource.subview(begin, frames);
    if (!subview) {
        return core::Result<BasicLevelPreproductionSnapshot>::failure(
            *subview.error());
    }
    const auto& format = subview.value()->format();
    const auto sampleRate = format.sample_rate().value();
    const auto count = format.channel_count();
    if (count != 1U && count != 2U) {
        return core::Result<BasicLevelPreproductionSnapshot>::failure(
            core::Error{core::ErrorCode::UnsupportedAudioLayout,
                        "M19 candidate requires canonical mono or stereo."});
    }
    const bool supportedClock = sampleRate == 44100 || sampleRate == 48000
        || sampleRate == 88200 || sampleRate == 96000
        || sampleRate == 176400 || sampleRate == 192000;
    if (!supportedClock) {
        return core::Result<BasicLevelPreproductionSnapshot>::failure(
            core::Error{core::ErrorCode::UnsupportedOperation,
                        "M19 candidate sample rate is outside the six-rate scope."});
    }
    auto left = subview.value()->channel(0U);
    if (!left) {
        return core::Result<BasicLevelPreproductionSnapshot>::failure(
            *left.error());
    }
    std::span<const double> right;
    if (count == 2U) {
        auto r = subview.value()->channel(1U);
        if (!r) {
            return core::Result<BasicLevelPreproductionSnapshot>::failure(
                *r.error());
        }
        right = *r.value();
    }

    const std::size_t length = static_cast<std::size_t>(frames.value());
    BasicLevelPreproduction calculator(
        static_cast<std::uint32_t>(sampleRate),
        static_cast<std::uint32_t>(count),
        static_cast<std::uint64_t>(begin.value()),
        static_cast<std::uint64_t>(frames.value()));
    constexpr std::size_t kChunkFrames = 4096U;
    for (std::size_t offset = 0; offset < length;) {
        const std::size_t take = std::min(kChunkFrames,length-offset);
        auto lpart = left.value()->subspan(offset,take);
        auto rpart = count == 2U ? right.subspan(offset,take)
                                 : std::span<const double>{};
        if (!calculator.push(lpart,rpart)) {
            return core::Result<BasicLevelPreproductionSnapshot>::failure(
                core::Error{core::ErrorCode::InvalidAudioSample,
                            "M19 selected-scope PCM contains invalid data or invalid math."});
        }
        offset += take;
    }
    auto result = calculator.finalize();
    if (!result) {
        return core::Result<BasicLevelPreproductionSnapshot>::failure(
            core::Error{core::ErrorCode::InvalidState,
                        "M19 candidate scope could not finalize."});
    }
    return core::Result<BasicLevelPreproductionSnapshot>::success(*result);
}

} // namespace rgsml::analysis::internal::m19
