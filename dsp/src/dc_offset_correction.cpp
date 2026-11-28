#include <rgsml/dsp/dc_offset_correction.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace rgsml::dsp {
namespace {
using Output = core::Result<audio::AudioBuffer>;

[[nodiscard]] Output fail(core::ErrorCode code, const char* message)
{
    return Output::failure(core::Error{code, message});
}
}  // namespace

core::Result<audio::AudioBuffer> correct_dc_offset(
    audio::AudioBufferView complete_source,
    core::FrameRange authorized_scope,
    std::span<const double> stored_offsets)
{
    const auto channels = complete_source.format().channel_count();
    if (complete_source.timebase().frame_domain_id()
            != audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || complete_source.absolute_start_frame().value() != 0
        || authorized_scope.begin() < complete_source.absolute_start_frame()
        || authorized_scope.end() > complete_source.absolute_end_frame()
        || stored_offsets.size() != channels) {
        return fail(core::ErrorCode::InvalidArgument,
            "DC correction requires source-rate full-origin PCM, contained scope and native-channel offsets.");
    }
    for (const double offset : stored_offsets) {
        if (!std::isfinite(offset)) {
            return fail(core::ErrorCode::InvalidArgument, "DC offset must be finite.");
        }
        if (offset < -0.25 || offset > 0.25) {
            return fail(core::ErrorCode::OutOfRange, "DC offset must be within +/-0.25 FS.");
        }
    }
    // Validate every channel and every supplied frame, not merely the scope.
    for (std::size_t channel = 0; channel < channels; ++channel) {
        const auto plane = *complete_source.channel(channel).value();
        for (const double sample : plane) {
            if (!std::isfinite(sample)) {
                return fail(core::ErrorCode::InvalidAudioSample, "Nonfinite source PCM.");
            }
        }
    }
    auto output = audio::AudioBuffer::create(
        complete_source.format(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        complete_source.absolute_start_frame(), complete_source.frame_count());
    if (!output) {
        return Output::failure(*output.error());
    }
    const auto begin = static_cast<std::size_t>(authorized_scope.begin().value());
    const auto end = static_cast<std::size_t>(authorized_scope.end().value());
    auto view = output.value()->mutable_view();
    for (std::size_t channel = 0; channel < channels; ++channel) {
        const auto source = *complete_source.channel(channel).value();
        auto destination = *view.channel(channel).value();
        if (!source.empty()) {
            std::copy(source.begin(), source.end(), destination.begin());
        }
        for (std::size_t frame = begin; frame < end; ++frame) {
            const double sample = source[frame] - stored_offsets[channel];
            if (!std::isfinite(sample)) {
                return fail(core::ErrorCode::InvalidAudioSample, "Nonfinite corrected PCM.");
            }
            destination[frame] = sample;
        }
    }
    return Output::success(std::move(*output.value()));
}

}  // namespace rgsml::dsp
