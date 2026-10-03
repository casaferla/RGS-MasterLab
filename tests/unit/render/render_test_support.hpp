#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_instance.hpp>

#include <QtCore/QtGlobal>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace rgsml::tests::render_support {

[[nodiscard]] inline rgsml::core::FrameCount frame_count(std::int64_t value)
{
    auto result = rgsml::core::FrameCount::create(value);
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] inline rgsml::core::FrameRange frame_range(
    std::int64_t begin,
    std::int64_t end)
{
    auto result = rgsml::core::FrameRange::create(
        rgsml::core::FrameIndex{begin}, rgsml::core::FrameIndex{end});
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] inline rgsml::dsp::ModuleInstanceId make_id(std::string_view text)
{
    auto uuid = rgsml::core::Uuid::parse(text);
    Q_ASSERT(uuid);
    auto id = rgsml::dsp::ModuleInstanceId::from_uuid(*uuid.value());
    Q_ASSERT(id);
    return *id.value();
}

[[nodiscard]] inline rgsml::audio::AudioFormat format(
    rgsml::audio::ChannelLayout layout,
    std::int64_t sample_rate = 48'000)
{
    auto rate = rgsml::core::SampleRate::create(sample_rate);
    Q_ASSERT(rate);
    auto result = rgsml::audio::AudioFormat::create(*rate.value(), layout);
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] inline rgsml::core::Result<rgsml::audio::AudioBuffer> make_buffer(
    rgsml::audio::ChannelLayout layout,
    std::int64_t absolute_start,
    std::span<const double> channel_zero,
    std::span<const double> channel_one = {})
{
    auto buffer = rgsml::audio::AudioBuffer::create(
        format(layout),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        rgsml::core::FrameIndex{absolute_start},
        frame_count(static_cast<std::int64_t>(channel_zero.size())));
    if (!buffer) {
        return buffer;
    }
    auto first = buffer.value()->mutable_view().channel(0);
    std::copy(channel_zero.begin(), channel_zero.end(), first.value()->begin());
    if (layout == rgsml::audio::ChannelLayout::STEREO_LR) {
        Q_ASSERT(channel_one.size() == channel_zero.size());
        auto second = buffer.value()->mutable_view().channel(1);
        std::copy(channel_one.begin(), channel_one.end(), second.value()->begin());
    }
    return buffer;
}

[[nodiscard]] inline std::vector<std::uint64_t> bits(
    rgsml::audio::AudioBufferView view)
{
    std::vector<std::uint64_t> result;
    for (std::size_t channel = 0; channel < view.format().channel_count(); ++channel) {
        const auto plane = *view.channel(channel).value();
        for (const auto sample : plane) {
            result.push_back(std::bit_cast<std::uint64_t>(sample));
        }
    }
    return result;
}

[[nodiscard]] inline std::string_view error_category(const rgsml::core::Error& error)
{
    for (const auto& detail : error.details()) {
        if (detail.key == "category") {
            return detail.value;
        }
    }
    return {};
}

}  // namespace rgsml::tests::render_support
