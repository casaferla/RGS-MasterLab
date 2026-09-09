#include <rgsml/audio/audio_buffer.hpp>

#include <rgsml/core/checked_integer.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

namespace rgsml::audio {
namespace {

constexpr std::size_t kPlaneAlignment = 64U;

[[nodiscard]] core::Error range_error()
{
    return core::Error{core::ErrorCode::OutOfRange, "Audio view range is out of bounds."};
}

[[nodiscard]] core::Result<core::FrameRange> checked_subrange(
    core::FrameRange parent,
    core::FrameIndex start,
    core::FrameCount count)
{
    if (start.value() < parent.begin().value()) {
        return core::Result<core::FrameRange>::failure(range_error());
    }
    auto endValue = core::checked_add(start.value(), count.value());
    if (!endValue || *endValue.value() > parent.end().value()) {
        return core::Result<core::FrameRange>::failure(range_error());
    }
    return core::FrameRange::create(start, core::FrameIndex{*endValue.value()});
}

[[nodiscard]] core::Result<std::size_t> checked_local_offset(
    core::FrameRange parent,
    core::FrameIndex start)
{
    auto difference = core::checked_subtract(start.value(), parent.begin().value());
    if (!difference || *difference.value() < 0
        || static_cast<std::uint64_t>(*difference.value())
            > std::numeric_limits<std::size_t>::max()) {
        return core::Result<std::size_t>::failure(range_error());
    }
    return core::Result<std::size_t>::success(
        static_cast<std::size_t>(*difference.value()));
}

}  // namespace

static_assert(std::numeric_limits<double>::is_iec559);
static_assert(sizeof(double) == 8U);

void AudioBuffer::AlignedPlaneDelete::operator()(double* pointer) const noexcept
{
    ::operator delete[](pointer, std::align_val_t{kPlaneAlignment});
}

core::Result<AudioBuffer> AudioBuffer::create(
    AudioFormat format,
    FrameDomainId domain,
    core::FrameIndex absoluteStart,
    core::FrameCount frameCount)
{
    if (absoluteStart.value() < 0) {
        return core::Result<AudioBuffer>::failure(
            core::Error{core::ErrorCode::OutOfRange, "Absolute audio frame cannot be negative."});
    }

    auto absoluteEnd = core::checked_add(absoluteStart.value(), frameCount.value());
    if (!absoluteEnd) {
        return core::Result<AudioBuffer>::failure(*absoluteEnd.error());
    }
    auto absoluteRange = core::FrameRange::create(
        absoluteStart, core::FrameIndex{*absoluteEnd.value()});
    if (!absoluteRange) {
        return core::Result<AudioBuffer>::failure(*absoluteRange.error());
    }
    auto timebase = AudioTimebase::create(format.sample_rate(), domain);
    if (!timebase) {
        return core::Result<AudioBuffer>::failure(*timebase.error());
    }

    auto sampleCount = core::checked_multiply(
        frameCount.value(), static_cast<std::int64_t>(format.channel_count()));
    if (!sampleCount) {
        return core::Result<AudioBuffer>::failure(*sampleCount.error());
    }
    auto byteCount = core::checked_multiply(
        *sampleCount.value(), static_cast<std::int64_t>(sizeof(double)));
    if (!byteCount) {
        return core::Result<AudioBuffer>::failure(*byteCount.error());
    }
    static_cast<void>(byteCount);

    std::array<AlignedPlane, 2> planes{};
    if (frameCount.value() > 0) {
        const auto frames = static_cast<std::size_t>(frameCount.value());
        const auto planeBytes = frames * sizeof(double);
        try {
            for (std::size_t channel = 0; channel < format.channel_count(); ++channel) {
                auto* memory = static_cast<double*>(
                    ::operator new[](planeBytes, std::align_val_t{kPlaneAlignment}));
                planes[channel].reset(memory);
                std::fill_n(memory, frames, 0.0);
            }
        } catch (const std::bad_alloc&) {
            return core::Result<AudioBuffer>::failure(
                core::Error{core::ErrorCode::IoFailure, "Unable to allocate canonical audio storage."});
        }
    }

    return core::Result<AudioBuffer>::success(AudioBuffer{
        format,
        *timebase.value(),
        *absoluteRange.value(),
        frameCount,
        std::move(planes),
    });
}

AudioBuffer::AudioBuffer(
    AudioFormat format,
    AudioTimebase timebase,
    core::FrameRange absoluteRange,
    core::FrameCount frameCount,
    std::array<AlignedPlane, 2> planes) noexcept
    : format_(format)
    , timebase_(timebase)
    , absoluteRange_(absoluteRange)
    , frameCount_(frameCount)
    , planes_(std::move(planes))
{
}

AudioBufferView AudioBuffer::view() const noexcept
{
    const auto frames = static_cast<std::size_t>(frameCount_.value());
    std::array<std::span<const double>, 2> channels{};
    for (std::size_t channel = 0; channel < format_.channel_count(); ++channel) {
        channels[channel] = std::span<const double>{planes_[channel].get(), frames};
    }
    return AudioBufferView{format_, timebase_, absoluteRange_, frameCount_, channels};
}

MutableAudioBufferView AudioBuffer::mutable_view() noexcept
{
    const auto frames = static_cast<std::size_t>(frameCount_.value());
    std::array<std::span<double>, 2> channels{};
    for (std::size_t channel = 0; channel < format_.channel_count(); ++channel) {
        channels[channel] = std::span<double>{planes_[channel].get(), frames};
    }
    return MutableAudioBufferView{format_, timebase_, absoluteRange_, frameCount_, channels};
}

AudioBufferView::AudioBufferView(
    AudioFormat format,
    AudioTimebase timebase,
    core::FrameRange absoluteRange,
    core::FrameCount frameCount,
    std::array<std::span<const double>, 2> channels) noexcept
    : format_(format)
    , timebase_(timebase)
    , absoluteRange_(absoluteRange)
    , frameCount_(frameCount)
    , channels_(channels)
{
}

const AudioFormat& AudioBufferView::format() const noexcept { return format_; }
const AudioTimebase& AudioBufferView::timebase() const noexcept { return timebase_; }
core::FrameCount AudioBufferView::frame_count() const noexcept { return frameCount_; }
core::FrameIndex AudioBufferView::absolute_start_frame() const noexcept
{
    return absoluteRange_.begin();
}
core::FrameIndex AudioBufferView::absolute_end_frame() const noexcept
{
    return absoluteRange_.end();
}
core::FrameRange AudioBufferView::absolute_range() const noexcept { return absoluteRange_; }

core::Result<core::FrameIndex> AudioBufferView::absolute_frame(
    std::size_t localFrame) const
{
    if (localFrame >= static_cast<std::size_t>(frameCount_.value())) {
        return core::Result<core::FrameIndex>::failure(range_error());
    }
    auto absolute = core::checked_add(
        absoluteRange_.begin().value(), static_cast<std::int64_t>(localFrame));
    if (!absolute) {
        return core::Result<core::FrameIndex>::failure(*absolute.error());
    }
    return core::Result<core::FrameIndex>::success(core::FrameIndex{*absolute.value()});
}

core::Result<std::span<const double>> AudioBufferView::channel(
    std::size_t channelIndex) const
{
    if (channelIndex >= format_.channel_count()) {
        return core::Result<std::span<const double>>::failure(
            core::Error{core::ErrorCode::OutOfRange, "Audio channel index is out of range."});
    }
    return core::Result<std::span<const double>>::success(channels_[channelIndex]);
}

core::Result<AudioBufferView> AudioBufferView::subview(
    core::FrameIndex absoluteStart,
    core::FrameCount frameCount) const
{
    auto range = checked_subrange(absoluteRange_, absoluteStart, frameCount);
    if (!range) {
        return core::Result<AudioBufferView>::failure(*range.error());
    }
    auto offset = checked_local_offset(absoluteRange_, absoluteStart);
    if (!offset) {
        return core::Result<AudioBufferView>::failure(*offset.error());
    }
    const auto count = static_cast<std::size_t>(frameCount.value());
    std::array<std::span<const double>, 2> channels{};
    for (std::size_t channelIndex = 0;
         channelIndex < format_.channel_count();
         ++channelIndex) {
        channels[channelIndex] = channels_[channelIndex].subspan(*offset.value(), count);
    }
    return core::Result<AudioBufferView>::success(AudioBufferView{
        format_, timebase_, *range.value(), frameCount, channels});
}

MutableAudioBufferView::MutableAudioBufferView(
    AudioFormat format,
    AudioTimebase timebase,
    core::FrameRange absoluteRange,
    core::FrameCount frameCount,
    std::array<std::span<double>, 2> channels) noexcept
    : format_(format)
    , timebase_(timebase)
    , absoluteRange_(absoluteRange)
    , frameCount_(frameCount)
    , channels_(channels)
{
}

const AudioFormat& MutableAudioBufferView::format() const noexcept { return format_; }
const AudioTimebase& MutableAudioBufferView::timebase() const noexcept { return timebase_; }
core::FrameCount MutableAudioBufferView::frame_count() const noexcept { return frameCount_; }
core::FrameIndex MutableAudioBufferView::absolute_start_frame() const noexcept
{
    return absoluteRange_.begin();
}
core::FrameIndex MutableAudioBufferView::absolute_end_frame() const noexcept
{
    return absoluteRange_.end();
}
core::FrameRange MutableAudioBufferView::absolute_range() const noexcept
{
    return absoluteRange_;
}

core::Result<core::FrameIndex> MutableAudioBufferView::absolute_frame(
    std::size_t localFrame) const
{
    return as_const().absolute_frame(localFrame);
}

core::Result<std::span<double>> MutableAudioBufferView::channel(std::size_t channelIndex)
{
    if (channelIndex >= format_.channel_count()) {
        return core::Result<std::span<double>>::failure(
            core::Error{core::ErrorCode::OutOfRange, "Audio channel index is out of range."});
    }
    return core::Result<std::span<double>>::success(channels_[channelIndex]);
}

core::Result<std::span<const double>> MutableAudioBufferView::channel(
    std::size_t channelIndex) const
{
    if (channelIndex >= format_.channel_count()) {
        return core::Result<std::span<const double>>::failure(
            core::Error{core::ErrorCode::OutOfRange, "Audio channel index is out of range."});
    }
    return core::Result<std::span<const double>>::success(channels_[channelIndex]);
}

core::Result<MutableAudioBufferView> MutableAudioBufferView::subview(
    core::FrameIndex absoluteStart,
    core::FrameCount frameCount)
{
    auto range = checked_subrange(absoluteRange_, absoluteStart, frameCount);
    if (!range) {
        return core::Result<MutableAudioBufferView>::failure(*range.error());
    }
    auto offset = checked_local_offset(absoluteRange_, absoluteStart);
    if (!offset) {
        return core::Result<MutableAudioBufferView>::failure(*offset.error());
    }
    const auto count = static_cast<std::size_t>(frameCount.value());
    std::array<std::span<double>, 2> channels{};
    for (std::size_t channelIndex = 0;
         channelIndex < format_.channel_count();
         ++channelIndex) {
        channels[channelIndex] = channels_[channelIndex].subspan(*offset.value(), count);
    }
    return core::Result<MutableAudioBufferView>::success(MutableAudioBufferView{
        format_, timebase_, *range.value(), frameCount, channels});
}

AudioBufferView MutableAudioBufferView::as_const() const noexcept
{
    std::array<std::span<const double>, 2> channels{};
    for (std::size_t channelIndex = 0;
         channelIndex < format_.channel_count();
         ++channelIndex) {
        channels[channelIndex] = channels_[channelIndex];
    }
    return AudioBufferView{format_, timebase_, absoluteRange_, frameCount_, channels};
}

}  // namespace rgsml::audio
