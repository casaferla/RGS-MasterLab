#include <rgsml/core/frame_time.hpp>

#include <rgsml/core/checked_integer.hpp>

namespace rgsml::core {

Result<SampleRate> SampleRate::create(std::int64_t value)
{
    if (value <= 0) {
        return Result<SampleRate>::failure(
            Error{ErrorCode::InvalidArgument, "SampleRate must be positive."});
    }
    return Result<SampleRate>::success(SampleRate{value});
}

std::int64_t SampleRate::value() const noexcept
{
    return value_;
}

Result<FrameCount> FrameCount::create(std::int64_t value)
{
    if (value < 0) {
        return Result<FrameCount>::failure(
            Error{ErrorCode::OutOfRange, "FrameCount cannot be negative."});
    }
    return Result<FrameCount>::success(FrameCount{value});
}

std::int64_t FrameCount::value() const noexcept
{
    return value_;
}

Result<FrameRange> FrameRange::create(FrameIndex begin, FrameIndex end)
{
    if (end < begin) {
        return Result<FrameRange>::failure(
            Error{ErrorCode::InvalidFrameRange, "FrameRange end must not precede begin."});
    }
    return Result<FrameRange>::success(FrameRange{begin, end});
}

FrameIndex FrameRange::begin() const noexcept
{
    return begin_;
}

FrameIndex FrameRange::end() const noexcept
{
    return end_;
}

Result<FrameCount> FrameRange::length() const
{
    const auto difference = checked_subtract(end_.value(), begin_.value());
    if (!difference) {
        return Result<FrameCount>::failure(*difference.error());
    }
    return FrameCount::create(*difference.value());
}

Result<RationalTime> frame_to_time(FrameIndex frame, SampleRate rate)
{
    auto seconds = Rational::create(frame.value(), rate.value());
    if (!seconds) {
        return Result<RationalTime>::failure(*seconds.error());
    }
    return Result<RationalTime>::success(RationalTime{*seconds.value()});
}

Result<RationalTime> frame_count_to_duration(FrameCount count, SampleRate rate)
{
    auto seconds = Rational::create(count.value(), rate.value());
    if (!seconds) {
        return Result<RationalTime>::failure(*seconds.error());
    }
    return Result<RationalTime>::success(RationalTime{*seconds.value()});
}

}  // namespace rgsml::core
