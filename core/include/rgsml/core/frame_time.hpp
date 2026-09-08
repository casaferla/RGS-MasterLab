#pragma once

#include <rgsml/core/rational.hpp>

#include <compare>
#include <cstdint>

namespace rgsml::core {

class SampleRate final {
public:
    SampleRate() = delete;
    [[nodiscard]] static Result<SampleRate> create(std::int64_t value);
    [[nodiscard]] std::int64_t value() const noexcept;
    [[nodiscard]] bool operator==(const SampleRate&) const = default;

private:
    explicit constexpr SampleRate(std::int64_t value) noexcept
        : value_(value)
    {
    }
    std::int64_t value_;
};

class FrameIndex final {
public:
    FrameIndex() = delete;
    explicit constexpr FrameIndex(std::int64_t value) noexcept
        : value_(value)
    {
    }
    [[nodiscard]] constexpr std::int64_t value() const noexcept
    {
        return value_;
    }
    [[nodiscard]] bool operator==(const FrameIndex&) const = default;
    [[nodiscard]] auto operator<=>(const FrameIndex&) const = default;

private:
    std::int64_t value_;
};

class FrameCount final {
public:
    FrameCount() = delete;
    [[nodiscard]] static Result<FrameCount> create(std::int64_t value);
    [[nodiscard]] std::int64_t value() const noexcept;
    [[nodiscard]] bool operator==(const FrameCount&) const = default;
    [[nodiscard]] auto operator<=>(const FrameCount&) const = default;

private:
    explicit constexpr FrameCount(std::int64_t value) noexcept
        : value_(value)
    {
    }
    std::int64_t value_;
};

class FrameRange final {
public:
    FrameRange() = delete;
    [[nodiscard]] static Result<FrameRange> create(FrameIndex begin, FrameIndex end);
    [[nodiscard]] FrameIndex begin() const noexcept;
    [[nodiscard]] FrameIndex end() const noexcept;
    [[nodiscard]] Result<FrameCount> length() const;
    [[nodiscard]] bool operator==(const FrameRange&) const = default;

private:
    constexpr FrameRange(FrameIndex begin, FrameIndex end) noexcept
        : begin_(begin)
        , end_(end)
    {
    }
    FrameIndex begin_;
    FrameIndex end_;
};

class RationalTime final {
public:
    RationalTime() = delete;
    explicit constexpr RationalTime(Rational seconds) noexcept
        : seconds_(seconds)
    {
    }
    [[nodiscard]] constexpr const Rational& seconds() const noexcept
    {
        return seconds_;
    }
    [[nodiscard]] bool operator==(const RationalTime&) const = default;

private:
    Rational seconds_;
};

[[nodiscard]] Result<RationalTime> frame_to_time(FrameIndex frame, SampleRate rate);
[[nodiscard]] Result<RationalTime> frame_count_to_duration(FrameCount count, SampleRate rate);

}  // namespace rgsml::core
