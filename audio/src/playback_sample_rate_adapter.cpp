#include <rgsml/audio/playback_sample_rate_adapter.hpp>

#include <rgsml/audio/audio_buffer.hpp>

#include "internal/playback_src_input.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <string_view>
#include <utility>

namespace rgsml::audio {
namespace {

class WavPlaybackSrcInput final : public internal::PlaybackSrcInput {
public:
    explicit WavPlaybackSrcInput(WavReader& reader) noexcept : reader_(reader) {}

    [[nodiscard]] const AudioFormat& format() const noexcept override
    {
        return reader_.info().audio_format();
    }

    [[nodiscard]] core::FrameCount frame_count() const noexcept override
    {
        return reader_.info().frame_count();
    }

    [[nodiscard]] core::Result<core::FrameCount> read_frames(
        core::FrameIndex localStart,
        MutableAudioBufferView destination) override
    {
        return reader_.read_frames(localStart, destination);
    }

private:
    WavReader& reader_;
};

constexpr std::array<std::uint64_t, 30'721> kKernel44100To48000{
#include "internal/playback_src_kernel_44100_48000.inc"
};
constexpr std::array<std::uint64_t, 28'225> kKernel48000To44100{
#include "internal/playback_src_kernel_48000_44100.inc"
};

constexpr std::string_view kKernel44100To48000Sha256 =
    "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4";
constexpr std::string_view kKernel48000To44100Sha256 =
    "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22";

struct WideUnsigned final {
    std::uint64_t high;
    std::uint64_t low;
};

struct WideDivision final {
    WideUnsigned quotient;
    std::uint32_t remainder;
};

struct ConvolutionWindow final {
    std::int64_t firstInput;
    std::int64_t lastInput;
    std::size_t firstCoefficient;
};

template <typename T>
[[nodiscard]] core::Result<T> failure(
    core::ErrorCode code,
    std::string_view message)
{
    return core::Result<T>::failure(core::Error{code, std::string(message)});
}

[[nodiscard]] WideUnsigned multiply_wide(
    std::uint64_t value,
    std::uint32_t multiplier) noexcept
{
    constexpr std::uint64_t kLowMask = UINT64_C(0xffffffff);
    const auto lowProduct = (value & kLowMask) * multiplier;
    const auto highProduct = (value >> 32U) * multiplier;
    const auto shiftedHigh = highProduct << 32U;
    const auto low = lowProduct + shiftedHigh;
    const auto carry = low < lowProduct ? UINT64_C(1) : UINT64_C(0);
    return WideUnsigned{(highProduct >> 32U) + carry, low};
}

[[nodiscard]] WideUnsigned add_wide(
    WideUnsigned value,
    std::uint64_t addend) noexcept
{
    const auto low = value.low + addend;
    return WideUnsigned{value.high + (low < value.low ? 1U : 0U), low};
}

[[nodiscard]] WideUnsigned subtract_wide(
    WideUnsigned value,
    std::uint64_t subtrahend) noexcept
{
    const auto low = value.low - subtrahend;
    return WideUnsigned{value.high - (value.low < subtrahend ? 1U : 0U), low};
}

[[nodiscard]] bool less_than_u64(
    WideUnsigned value,
    std::uint64_t other) noexcept
{
    return value.high == 0U && value.low < other;
}

[[nodiscard]] WideDivision divide_wide(
    WideUnsigned value,
    std::uint32_t divisor) noexcept
{
    std::array<std::uint32_t, 4> words{
        static_cast<std::uint32_t>(value.low),
        static_cast<std::uint32_t>(value.low >> 32U),
        static_cast<std::uint32_t>(value.high),
        static_cast<std::uint32_t>(value.high >> 32U),
    };
    std::array<std::uint32_t, 4> quotientWords{};
    std::uint64_t remainder = 0U;
    for (std::size_t reverse = words.size(); reverse > 0U; --reverse) {
        const auto index = reverse - 1U;
        const auto current = (remainder << 32U) | words[index];
        quotientWords[index] = static_cast<std::uint32_t>(current / divisor);
        remainder = current % divisor;
    }
    return WideDivision{
        WideUnsigned{
            (static_cast<std::uint64_t>(quotientWords[3]) << 32U)
                | quotientWords[2],
            (static_cast<std::uint64_t>(quotientWords[1]) << 32U)
                | quotientWords[0],
        },
        static_cast<std::uint32_t>(remainder),
    };
}

[[nodiscard]] core::Result<std::int64_t> checked_wide_to_i64(
    WideUnsigned value,
    std::string_view message)
{
    if (value.high != 0U
        || value.low > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max())) {
        return failure<std::int64_t>(core::ErrorCode::IntegerOverflow, message);
    }
    return core::Result<std::int64_t>::success(
        static_cast<std::int64_t>(value.low));
}

[[nodiscard]] core::Result<std::int64_t> ceil_multiply_divide(
    std::int64_t value,
    std::uint32_t multiplier,
    std::uint32_t divisor)
{
    if (value < 0) {
        return failure<std::int64_t>(
            core::ErrorCode::OutOfRange,
            "Playback SRC frame mapping requires a non-negative frame.");
    }
    const auto division = divide_wide(
        multiply_wide(static_cast<std::uint64_t>(value), multiplier),
        divisor);
    auto quotient = checked_wide_to_i64(
        division.quotient, "Playback SRC frame mapping overflowed.");
    if (!quotient) {
        return quotient;
    }
    if (division.remainder != 0U) {
        if (*quotient.value() == std::numeric_limits<std::int64_t>::max()) {
            return failure<std::int64_t>(
                core::ErrorCode::IntegerOverflow,
                "Playback SRC ceiling frame mapping overflowed.");
        }
        ++*quotient.value();
    }
    return quotient;
}

[[nodiscard]] core::Result<std::int64_t> floor_multiply_divide(
    std::int64_t value,
    std::uint32_t multiplier,
    std::uint32_t divisor)
{
    if (value < 0) {
        return failure<std::int64_t>(
            core::ErrorCode::OutOfRange,
            "Playback SRC cursor mapping requires a non-negative frame.");
    }
    return checked_wide_to_i64(
        divide_wide(
            multiply_wide(static_cast<std::uint64_t>(value), multiplier),
            divisor)
            .quotient,
        "Playback SRC cursor mapping overflowed.");
}

[[nodiscard]] core::Result<ConvolutionWindow> convolution_window(
    std::int64_t outputFrame,
    std::uint32_t interpolation,
    std::uint32_t decimation,
    std::size_t coefficientCount)
{
    if (outputFrame < 0) {
        return failure<ConvolutionWindow>(
            core::ErrorCode::OutOfRange,
            "Playback SRC output frame cannot be negative.");
    }
    const auto delay = static_cast<std::uint64_t>((coefficientCount - 1U) / 2U);
    const auto maximumCoefficient = static_cast<std::uint64_t>(
        coefficientCount - 1U);
    const auto q = add_wide(
        multiply_wide(
            static_cast<std::uint64_t>(outputFrame), decimation),
        delay);
    const auto highDivision = divide_wide(q, interpolation);
    auto high = checked_wide_to_i64(
        highDivision.quotient,
        "Playback SRC convolution input index overflowed.");
    if (!high) {
        return core::Result<ConvolutionWindow>::failure(*high.error());
    }

    std::int64_t lowValue = 0;
    if (less_than_u64(q, maximumCoefficient)) {
        const auto magnitude = maximumCoefficient - q.low;
        lowValue = -static_cast<std::int64_t>(magnitude / interpolation);
    } else {
        const auto lowDivision = divide_wide(
            subtract_wide(q, maximumCoefficient), interpolation);
        auto low = checked_wide_to_i64(
            lowDivision.quotient,
            "Playback SRC convolution input index overflowed.");
        if (!low) {
            return core::Result<ConvolutionWindow>::failure(*low.error());
        }
        lowValue = *low.value();
        if (lowDivision.remainder != 0U) {
            if (lowValue == std::numeric_limits<std::int64_t>::max()) {
                return failure<ConvolutionWindow>(
                    core::ErrorCode::IntegerOverflow,
                    "Playback SRC convolution ceiling overflowed.");
            }
            ++lowValue;
        }
    }

    const auto distance = static_cast<std::uint64_t>(*high.value() - lowValue);
    const auto firstCoefficient = static_cast<std::uint64_t>(
        highDivision.remainder)
        + distance * interpolation;
    if (firstCoefficient >= coefficientCount) {
        return failure<ConvolutionWindow>(
            core::ErrorCode::InvalidState,
            "Playback SRC phase selected an invalid coefficient.");
    }
    return core::Result<ConvolutionWindow>::success(ConvolutionWindow{
        lowValue,
        *high.value(),
        static_cast<std::size_t>(firstCoefficient),
    });
}

[[nodiscard]] core::Result<std::int64_t> reflect_frame(
    std::int64_t logical,
    std::int64_t inputFrames)
{
    if (inputFrames <= 0) {
        return failure<std::int64_t>(
            core::ErrorCode::OutOfRange,
            "Playback SRC cannot reflect an empty Source.");
    }
    if (inputFrames == 1) {
        return core::Result<std::int64_t>::success(0);
    }
    if (inputFrames > std::numeric_limits<std::int64_t>::max() / 2) {
        return failure<std::int64_t>(
            core::ErrorCode::IntegerOverflow,
            "Playback SRC reflection period overflowed.");
    }
    const auto period = inputFrames * 2;
    std::int64_t remainder = logical % period;
    if (remainder < 0) {
        remainder += period;
    }
    return core::Result<std::int64_t>::success(
        remainder < inputFrames ? remainder : period - 1 - remainder);
}

}  // namespace

core::Result<PlaybackSampleRateAdapter> PlaybackSampleRateAdapter::create(
    const PlaybackRateSpec& spec)
{
    const auto inputRate = spec.inputRate.value();
    const auto outputRate = spec.outputRate.value();
    const std::uint64_t* coefficients = nullptr;
    std::size_t coefficientCount = 0U;
    std::string_view checksum;
    std::int64_t interpolation = 0;
    std::int64_t decimation = 0;

    if (inputRate == 44'100 && outputRate == 48'000) {
        interpolation = 160;
        decimation = 147;
        coefficients = kKernel44100To48000.data();
        coefficientCount = kKernel44100To48000.size();
        checksum = kKernel44100To48000Sha256;
    } else if (inputRate == 48'000 && outputRate == 44'100) {
        interpolation = 147;
        decimation = 160;
        coefficients = kKernel48000To44100.data();
        coefficientCount = kKernel48000To44100.size();
        checksum = kKernel48000To44100Sha256;
    } else {
        return failure<PlaybackSampleRateAdapter>(
            core::ErrorCode::UnsupportedOperation,
            "PLAYBACK_RATE_PAIR_UNSUPPORTED_V1");
    }
    if (spec.channelLayout != ChannelLayout::MONO_C
        && spec.channelLayout != ChannelLayout::STEREO_LR) {
        return failure<PlaybackSampleRateAdapter>(
            core::ErrorCode::UnsupportedAudioLayout,
            "Playback SRC requires the canonical mono or stereo layout.");
    }
    if (spec.totalInputFrames.value()
        > std::numeric_limits<std::int64_t>::max() / 2) {
        return failure<PlaybackSampleRateAdapter>(
            core::ErrorCode::IntegerOverflow,
            "Playback SRC whole-track reflection period overflowed.");
    }
    auto outputFrameValue = ceil_multiply_divide(
        spec.totalInputFrames.value(),
        static_cast<std::uint32_t>(interpolation),
        static_cast<std::uint32_t>(decimation));
    if (!outputFrameValue) {
        return core::Result<PlaybackSampleRateAdapter>::failure(
            *outputFrameValue.error());
    }
    auto outputFrames = core::FrameCount::create(*outputFrameValue.value());
    if (!outputFrames) {
        return core::Result<PlaybackSampleRateAdapter>::failure(
            *outputFrames.error());
    }
    return core::Result<PlaybackSampleRateAdapter>::success(
        PlaybackSampleRateAdapter{
            spec,
            interpolation,
            decimation,
            coefficients,
            coefficientCount,
            checksum,
            *outputFrames.value(),
        });
}

PlaybackSampleRateAdapter::PlaybackSampleRateAdapter(
    PlaybackRateSpec spec,
    std::int64_t interpolation,
    std::int64_t decimation,
    const std::uint64_t* coefficientBits,
    std::size_t coefficientCount,
    std::string_view kernelSha256,
    core::FrameCount outputFrames) noexcept
    : spec_(spec)
    , interpolation_(interpolation)
    , decimation_(decimation)
    , coefficientBits_(coefficientBits)
    , coefficientCount_(coefficientCount)
    , kernelSha256_(kernelSha256)
    , outputFrames_(outputFrames)
{
}

core::SampleRate PlaybackSampleRateAdapter::input_rate() const noexcept
{
    return spec_.inputRate;
}

core::SampleRate PlaybackSampleRateAdapter::output_rate() const noexcept
{
    return spec_.outputRate;
}

ChannelLayout PlaybackSampleRateAdapter::channel_layout() const noexcept
{
    return spec_.channelLayout;
}

core::FrameCount PlaybackSampleRateAdapter::input_frame_count() const noexcept
{
    return spec_.totalInputFrames;
}

core::FrameCount PlaybackSampleRateAdapter::output_frame_count() const noexcept
{
    return outputFrames_;
}

std::int64_t PlaybackSampleRateAdapter::interpolation_factor() const noexcept
{
    return interpolation_;
}

std::int64_t PlaybackSampleRateAdapter::decimation_factor() const noexcept
{
    return decimation_;
}

std::size_t PlaybackSampleRateAdapter::kernel_size() const noexcept
{
    return coefficientCount_;
}

std::int64_t
PlaybackSampleRateAdapter::group_delay_high_rate_frames() const noexcept
{
    return static_cast<std::int64_t>((coefficientCount_ - 1U) / 2U);
}

std::string_view PlaybackSampleRateAdapter::kernel_sha256() const noexcept
{
    return kernelSha256_;
}

std::uint64_t PlaybackSampleRateAdapter::coefficient_bits(
    std::size_t index) const noexcept
{
    return index < coefficientCount_ ? coefficientBits_[index] : 0U;
}

core::Result<core::FrameIndex>
PlaybackSampleRateAdapter::map_input_frame_to_output(
    core::FrameIndex inputFrame) const
{
    if (inputFrame.value() > spec_.totalInputFrames.value()) {
        return failure<core::FrameIndex>(
            core::ErrorCode::OutOfRange,
            "Playback SRC Source frame is outside the prepared track.");
    }
    auto mapped = ceil_multiply_divide(
        inputFrame.value(),
        static_cast<std::uint32_t>(interpolation_),
        static_cast<std::uint32_t>(decimation_));
    if (!mapped) {
        return core::Result<core::FrameIndex>::failure(*mapped.error());
    }
    return core::Result<core::FrameIndex>::success(
        core::FrameIndex{*mapped.value()});
}

core::Result<core::FrameRange>
PlaybackSampleRateAdapter::map_input_range_to_output(
    core::FrameRange inputRange) const
{
    if (inputRange.begin().value() < 0
        || inputRange.end().value() > spec_.totalInputFrames.value()) {
        return failure<core::FrameRange>(
            core::ErrorCode::OutOfRange,
            "Playback SRC Source interval is outside the prepared track.");
    }
    auto begin = map_input_frame_to_output(inputRange.begin());
    auto end = map_input_frame_to_output(inputRange.end());
    if (!begin) {
        return core::Result<core::FrameRange>::failure(*begin.error());
    }
    if (!end) {
        return core::Result<core::FrameRange>::failure(*end.error());
    }
    return core::FrameRange::create(*begin.value(), *end.value());
}

core::Result<core::FrameIndex>
PlaybackSampleRateAdapter::map_output_frame_to_input_cursor(
    core::FrameIndex outputFrame) const
{
    if (outputFrame.value() < 0
        || outputFrame.value() > outputFrames_.value()) {
        return failure<core::FrameIndex>(
            core::ErrorCode::OutOfRange,
            "Playback SRC output cursor is outside the prepared track.");
    }
    auto mapped = floor_multiply_divide(
        outputFrame.value(),
        static_cast<std::uint32_t>(decimation_),
        static_cast<std::uint32_t>(interpolation_));
    if (!mapped) {
        return core::Result<core::FrameIndex>::failure(*mapped.error());
    }
    return core::Result<core::FrameIndex>::success(core::FrameIndex{
        std::min(*mapped.value(), spec_.totalInputFrames.value())});
}

core::Result<core::FrameCount> PlaybackSampleRateAdapter::read_frames(
    WavReader& source,
    core::FrameIndex absoluteOutputStart,
    MutableAudioBufferView destination) const
{
    WavPlaybackSrcInput input{source};
    return internal::read_playback_src_frames(
        *this, input, absoluteOutputStart, destination);
}

core::Result<core::FrameCount> internal::read_playback_src_frames(
    const PlaybackSampleRateAdapter& adapter,
    PlaybackSrcInput& source,
    core::FrameIndex absoluteOutputStart,
    MutableAudioBufferView destination)
{
    const auto requested = destination.frame_count().value();
    if (absoluteOutputStart.value() < 0
        || requested < 0
        || requested > PlaybackSampleRateAdapter::kMaximumOutputBlockFrames
        || absoluteOutputStart != destination.absolute_start_frame()
        || destination.format().sample_rate() != adapter.output_rate()
        || destination.format().channel_layout() != adapter.channel_layout()
        || destination.timebase().frame_domain_id() != FrameDomainId::OUTPUT_RATE
        || source.format().sample_rate() != adapter.input_rate()
        || source.format().channel_layout() != adapter.channel_layout()
        || source.frame_count() != adapter.input_frame_count()) {
        return failure<core::FrameCount>(
            core::ErrorCode::InvalidArgument,
            "Playback SRC reader or destination metadata is incoherent.");
    }
    if (absoluteOutputStart.value() > adapter.output_frame_count().value()
        || requested > adapter.output_frame_count().value()
            - absoluteOutputStart.value()) {
        return failure<core::FrameCount>(
            core::ErrorCode::OutOfRange,
            "Playback SRC output range is outside the prepared track.");
    }
    if (requested == 0) {
        return core::Result<core::FrameCount>::success(destination.frame_count());
    }
    if (adapter.input_frame_count().value() == 0) {
        return failure<core::FrameCount>(
            core::ErrorCode::OutOfRange,
            "Playback SRC cannot produce frames from an empty Source.");
    }

    auto firstWindow = convolution_window(
        absoluteOutputStart.value(),
        static_cast<std::uint32_t>(adapter.interpolation_factor()),
        static_cast<std::uint32_t>(adapter.decimation_factor()),
        adapter.kernel_size());
    auto lastWindow = convolution_window(
        absoluteOutputStart.value() + requested - 1,
        static_cast<std::uint32_t>(adapter.interpolation_factor()),
        static_cast<std::uint32_t>(adapter.decimation_factor()),
        adapter.kernel_size());
    if (!firstWindow) {
        return core::Result<core::FrameCount>::failure(*firstWindow.error());
    }
    if (!lastWindow) {
        return core::Result<core::FrameCount>::failure(*lastWindow.error());
    }

    std::int64_t minimumSource = adapter.input_frame_count().value();
    std::int64_t maximumSource = 0;
    for (std::int64_t logical = firstWindow.value()->firstInput;
         logical <= lastWindow.value()->lastInput;
         ++logical) {
        auto reflected = reflect_frame(logical, adapter.input_frame_count().value());
        if (!reflected) {
            return core::Result<core::FrameCount>::failure(*reflected.error());
        }
        minimumSource = std::min(minimumSource, *reflected.value());
        maximumSource = std::max(maximumSource, *reflected.value());
    }
    const auto sourceWindowFrames = maximumSource - minimumSource + 1;
    constexpr auto kMaximumSourceWindowFrames =
        PlaybackSampleRateAdapter::kMaximumOutputBlockFrames
        + (PlaybackSampleRateAdapter::kTapsPerPhase * 2) + 4;
    if (sourceWindowFrames > kMaximumSourceWindowFrames) {
        return failure<core::FrameCount>(
            core::ErrorCode::InvalidState,
            "Playback SRC exceeded its bounded decode window.");
    }

    auto sourceFrameCount = core::FrameCount::create(sourceWindowFrames);
    if (!sourceFrameCount) {
        return core::Result<core::FrameCount>::failure(*sourceFrameCount.error());
    }
    auto sourceWindow = AudioBuffer::create(
        source.format(),
        FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{minimumSource},
        *sourceFrameCount.value());
    if (!sourceWindow) {
        return core::Result<core::FrameCount>::failure(*sourceWindow.error());
    }
    auto decoded = source.read_frames(
        core::FrameIndex{minimumSource}, sourceWindow.value()->mutable_view());
    if (!decoded) {
        return core::Result<core::FrameCount>::failure(*decoded.error());
    }
    if (decoded.value()->value() != sourceWindowFrames) {
        return failure<core::FrameCount>(
            core::ErrorCode::TruncatedAudioData,
            "Playback SRC decoder returned an unexpected short context window.");
    }

    std::array<std::span<const double>, 2> sourcePlanes{};
    std::array<std::span<double>, 2> destinationPlanes{};
    for (std::size_t channel = 0;
         channel < source.format().channel_count();
         ++channel) {
        auto sourcePlane = sourceWindow.value()->view().channel(channel);
        auto destinationPlane = destination.channel(channel);
        if (!sourcePlane) {
            return core::Result<core::FrameCount>::failure(*sourcePlane.error());
        }
        if (!destinationPlane) {
            return core::Result<core::FrameCount>::failure(*destinationPlane.error());
        }
        sourcePlanes[channel] = *sourcePlane.value();
        destinationPlanes[channel] = *destinationPlane.value();
    }

    for (std::int64_t localOutput = 0; localOutput < requested; ++localOutput) {
        auto window = convolution_window(
            absoluteOutputStart.value() + localOutput,
            static_cast<std::uint32_t>(adapter.interpolation_factor()),
            static_cast<std::uint32_t>(adapter.decimation_factor()),
            adapter.kernel_size());
        if (!window) {
            return core::Result<core::FrameCount>::failure(*window.error());
        }
        for (std::size_t channel = 0;
             channel < source.format().channel_count();
             ++channel) {
            double accumulator = 0.0;
            auto coefficient = window.value()->firstCoefficient;
            for (auto logical = window.value()->firstInput;
                 logical <= window.value()->lastInput;
                 ++logical) {
                auto reflected = reflect_frame(
                    logical, adapter.input_frame_count().value());
                if (!reflected) {
                    return core::Result<core::FrameCount>::failure(
                        *reflected.error());
                }
                const auto sourceOffset = static_cast<std::size_t>(
                    *reflected.value() - minimumSource);
                const auto sample = sourcePlanes[channel][sourceOffset];
                if (!std::isfinite(sample)) {
                    return failure<core::FrameCount>(
                        core::ErrorCode::InvalidAudioSample,
                        "Non-finite canonical sample reached playback SRC.");
                }
                const auto kernel = std::bit_cast<double>(
                    adapter.coefficient_bits(coefficient));
                const auto product = sample * kernel;
                accumulator = accumulator + product;
                if (logical != window.value()->lastInput) {
                    coefficient -= static_cast<std::size_t>(
                        adapter.interpolation_factor());
                }
            }
            if (!std::isfinite(accumulator)) {
                return failure<core::FrameCount>(
                    core::ErrorCode::InvalidAudioSample,
                    "Playback SRC produced a non-finite sample.");
            }
            destinationPlanes[channel][static_cast<std::size_t>(localOutput)] =
                accumulator;
        }
    }
    return core::Result<core::FrameCount>::success(destination.frame_count());
}

}  // namespace rgsml::audio
