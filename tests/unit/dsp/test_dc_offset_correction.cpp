#include <rgsml/dsp/dc_offset_correction.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <source_location>
#include <utility>

namespace {
using namespace rgsml;
using audio::ChannelLayout;
using audio::FrameDomainId;
using core::FrameIndex;

void require(bool condition,
    std::source_location where = std::source_location::current())
{
    if (!condition) {
        std::cerr << "DC correction test failure at line " << where.line() << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template <typename T>
T take(core::Result<T> result)
{
    require(result.has_value());
    return std::move(*result.value());
}

core::FrameRange range(std::int64_t begin, std::int64_t end)
{
    return take(core::FrameRange::create(FrameIndex{begin}, FrameIndex{end}));
}

audio::AudioBuffer make(std::int64_t rate, ChannelLayout layout,
    std::int64_t start = 0, std::int64_t count = 5,
    FrameDomainId domain = FrameDomainId::SOURCE_PROCESSING_RATE)
{
    const auto format = take(audio::AudioFormat::create(
        take(core::SampleRate::create(rate)), layout));
    return take(audio::AudioBuffer::create(format, domain, FrameIndex{start},
        take(core::FrameCount::create(count))));
}

std::uint64_t bits(double sample)
{
    return std::bit_cast<std::uint64_t>(sample);
}

void check_rate(std::int64_t rate)
{
    const std::array left{-0.0, 0.5, -0.25, 1.25, -0.0};
    const std::array right{0.0, -0.5, 0.25, -1.25, 0.0};
    auto raw = make(rate, ChannelLayout::STEREO_LR);
    auto mutable_raw = raw.mutable_view();
    std::copy(left.begin(), left.end(), mutable_raw.channel(0).value()->begin());
    std::copy(right.begin(), right.end(), mutable_raw.channel(1).value()->begin());
    const std::array offsets{0.25, -0.25};
    auto corrected = take(dsp::correct_dc_offset(raw.view(), range(1, 4), offsets));
    const std::array expected_left{-0.0, 0.25, -0.5, 1.0, -0.0};
    const std::array expected_right{0.0, -0.25, 0.5, -1.0, 0.0};
    require(corrected.view().format() == raw.view().format());
    require(corrected.view().absolute_range() == raw.view().absolute_range());
    require(corrected.view().timebase() == raw.view().timebase());
    for (std::size_t i = 0; i < left.size(); ++i) {
        require(bits((*corrected.view().channel(0).value())[i]) == bits(expected_left[i]));
        require(bits((*corrected.view().channel(1).value())[i]) == bits(expected_right[i]));
        require(bits((*raw.view().channel(0).value())[i]) == bits(left[i]));
        require(bits((*raw.view().channel(1).value())[i]) == bits(right[i]));
    }
    auto identity = take(dsp::correct_dc_offset(raw.view(), range(2, 2), offsets));
    for (std::size_t i = 0; i < left.size(); ++i) {
        require(bits((*identity.view().channel(0).value())[i]) == bits(left[i]));
        require(bits((*identity.view().channel(1).value())[i]) == bits(right[i]));
    }
    // No clipping even when subtraction produces a value outside unity.
    auto mono = make(rate, ChannelLayout::MONO_C);
    auto samples = *mono.mutable_view().channel(0).value();
    samples[0] = 1.0;
    samples[1] = -1.25;
    samples[2] = std::numeric_limits<double>::denorm_min();
    samples[3] = -0.0;
    const std::array mono_offset{-0.25};
    auto mono_out = take(dsp::correct_dc_offset(mono.view(), range(0, 2), mono_offset));
    const auto plane = *mono_out.view().channel(0).value();
    require(plane[0] == 1.25);
    require(plane[1] == -1.0);
    require(bits(plane[2]) == bits(samples[2]));
    require(bits(plane[3]) == bits(samples[3]));
    auto empty = make(rate, ChannelLayout::MONO_C, 0, 0);
    require(dsp::correct_dc_offset(empty.view(), range(0, 0), mono_offset).has_value());

    // Scope cannot hide invalid input in the other channel.
    mutable_raw.channel(1).value()->back() = std::numeric_limits<double>::infinity();
    auto rejected = dsp::correct_dc_offset(raw.view(), range(1, 2), offsets);
    require(!rejected && rejected.error()->code() == core::ErrorCode::InvalidAudioSample);
    mutable_raw.channel(1).value()->back() = std::numeric_limits<double>::quiet_NaN();
    require(!dsp::correct_dc_offset(raw.view(), range(0, 0), offsets));
    mutable_raw.channel(1).value()->back() = 0.0;
    const std::array too_large{std::nextafter(0.25, 1.0), 0.0};
    const std::array too_small{0.0, std::nextafter(-0.25, -1.0)};
    const std::array invalid{0.0, std::numeric_limits<double>::infinity()};
    require(!dsp::correct_dc_offset(raw.view(), range(0, 5), too_large));
    require(!dsp::correct_dc_offset(raw.view(), range(0, 5), too_small));
    require(!dsp::correct_dc_offset(raw.view(), range(0, 5), invalid));
    require(!dsp::correct_dc_offset(raw.view(), range(0, 5), mono_offset));
    require(!dsp::correct_dc_offset(mono.view(), range(0, 5), offsets));
    require(!dsp::correct_dc_offset(raw.view(), range(0, 6), offsets));
    auto shifted = make(rate, ChannelLayout::MONO_C, 1);
    require(!dsp::correct_dc_offset(shifted.view(), range(1, 5), mono_offset));
    auto output_domain = make(rate, ChannelLayout::MONO_C, 0, 5, FrameDomainId::OUTPUT_RATE);
    require(!dsp::correct_dc_offset(output_domain.view(), range(0, 5), mono_offset));
    std::cout << "PASS DC native rate " << rate << '\n';
}

// Independent integer-lattice oracle: exact representable inputs and offsets,
// expected difference formed in integer arithmetic before conversion to PCM.
void check_dyadic_oracle(std::int64_t rate)
{
    constexpr std::int64_t count = 257;
    constexpr double scale = 1024.0;
    auto raw = make(rate, ChannelLayout::STEREO_LR, 0, count);
    auto raw_view = raw.mutable_view();
    auto left = *raw_view.channel(0).value();
    auto right = *raw_view.channel(1).value();
    for (std::int64_t n = 0; n < count; ++n) {
        const auto i = static_cast<std::size_t>(n);
        left[i] = static_cast<double>(n * 13 - 1664) / scale;
        right[i] = static_cast<double>(1024 - n * 9) / scale;
    }
    for (const std::int64_t offset : {-256, -127, -1, 0, 1, 127, 256}) {
        const std::array offsets{static_cast<double>(offset) / scale,
            static_cast<double>(-offset) / scale};
        auto out = take(dsp::correct_dc_offset(raw.view(), range(31, 223), offsets));
        const auto l = *out.view().channel(0).value();
        const auto r = *out.view().channel(1).value();
        for (std::int64_t n = 0; n < count; ++n) {
            const auto i = static_cast<std::size_t>(n);
            const auto active = (31 <= n && n < 223) ? offset : 0;
            require(l[i] == static_cast<double>(n * 13 - 1664 - active) / scale);
            require(r[i] == static_cast<double>(1024 - n * 9 + active) / scale);
            require(left[i] == static_cast<double>(n * 13 - 1664) / scale);
            require(right[i] == static_cast<double>(1024 - n * 9) / scale);
        }
    }
}
}  // namespace

int main()
{
    for (const std::int64_t rate : {44100, 48000, 88200, 96000, 176400, 192000}) {
        check_rate(rate);
        check_dyadic_oracle(rate);
    }
    return EXIT_SUCCESS;
}
