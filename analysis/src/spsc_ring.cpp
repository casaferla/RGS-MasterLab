#include <rgsml/analysis/spsc_ring.hpp>

#include <algorithm>
#include <cstring>

namespace rgsml::analysis {

SpscFrameRing::SpscFrameRing(std::size_t frameCapacity)
{
    reset(frameCapacity);
}

void SpscFrameRing::reset(std::size_t frameCapacity)
{
    capacity_ = frameCapacity;
    bytes_per_frame_ = 8;
    byte_buffer_.assign(capacity_ * 8, std::uint8_t{0});
    remainder_len_ = 0;
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
}

void SpscFrameRing::clear() noexcept
{
    remainder_len_ = 0;
    tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
}

std::size_t SpscFrameRing::available_frames() const noexcept
{
    const auto head = head_.load(std::memory_order_acquire);
    const auto tail = tail_.load(std::memory_order_relaxed);
    return (head >= tail) ? (head - tail) : (capacity_ - (tail - head));
}

std::size_t SpscFrameRing::push_pcm_bytes(
    const void* pcmData,
    std::size_t byteCount,
    std::uint32_t sampleRateHz,
    std::uint8_t channelCount,
    SampleEncoding encoding,
    std::uint64_t streamGeneration,
    std::uint64_t analysisEpoch) noexcept
{
    if (byteCount == 0 && remainder_len_ == 0) {
        return 0;
    }

    const std::size_t sampleBytes = (encoding == SampleEncoding::IEEE_FLOAT32) ? 4 : 2;
    const std::size_t frameBytes = channelCount * sampleBytes;
    if (frameBytes == 0) {
        return 0;
    }

    bytes_per_frame_ = frameBytes;

    sample_rate_hz_.store(sampleRateHz, std::memory_order_relaxed);
    channel_count_.store(channelCount, std::memory_order_relaxed);
    encoding_.store(encoding, std::memory_order_relaxed);
    stream_generation_.store(streamGeneration, std::memory_order_relaxed);
    analysis_epoch_.store(analysisEpoch, std::memory_order_relaxed);

    const std::uint8_t* inPtr = static_cast<const std::uint8_t*>(pcmData);
    std::size_t inRem = byteCount;

    auto head = head_.load(std::memory_order_relaxed);
    auto tail = tail_.load(std::memory_order_acquire);
    std::size_t totalPushedFrames = 0;

    // First process remainder if present
    if (remainder_len_ > 0 && inRem > 0) {
        const std::size_t needBytes = frameBytes - remainder_len_;
        if (inRem >= needBytes) {
            std::memcpy(remainder_buffer_.data() + remainder_len_, inPtr, needBytes);
            inPtr += needBytes;
            inRem -= needBytes;
            remainder_len_ = 0;

            const std::size_t currentFrames = (head >= tail) ? (head - tail) : (capacity_ - (tail - head));
            if (capacity_ - 1 - currentFrames > 0) {
                std::memcpy(&byte_buffer_[head * frameBytes], remainder_buffer_.data(), frameBytes);
                head = (head + 1) % capacity_;
                ++totalPushedFrames;
            }
        } else {
            std::memcpy(remainder_buffer_.data() + remainder_len_, inPtr, inRem);
            remainder_len_ += inRem;
            return 0;
        }
    }

    const std::size_t directFrames = inRem / frameBytes;
    const std::size_t leftOverBytes = inRem % frameBytes;

    if (leftOverBytes > 0) {
        std::memcpy(remainder_buffer_.data(), inPtr + directFrames * frameBytes, leftOverBytes);
        remainder_len_ = leftOverBytes;
    } else {
        remainder_len_ = 0;
    }

    if (directFrames > 0) {
        const std::size_t currentFrames = (head >= tail) ? (head - tail) : (capacity_ - (tail - head));
        const std::size_t writableFrames = capacity_ - 1 - currentFrames;
        const std::size_t pushCount = std::min(directFrames, writableFrames);

        for (std::size_t f = 0; f < pushCount; ++f) {
            std::memcpy(&byte_buffer_[head * frameBytes], inPtr + f * frameBytes, frameBytes);
            head = (head + 1) % capacity_;
        }
        totalPushedFrames += pushCount;
    }

    head_.store(head, std::memory_order_release);
    return totalPushedFrames;
}

std::size_t SpscFrameRing::pop_frames_to_float(
    std::size_t maxFrames,
    float* outInterleavedFloat,
    IngressMeta& outMeta) noexcept
{
    const std::size_t avail = available_frames();
    const std::size_t count = std::min(maxFrames, avail);
    if (count == 0) {
        return 0;
    }

    outMeta.sample_rate_hz = sample_rate_hz_.load(std::memory_order_relaxed);
    outMeta.channel_count = channel_count_.load(std::memory_order_relaxed);
    outMeta.encoding = encoding_.load(std::memory_order_relaxed);
    outMeta.stream_generation = stream_generation_.load(std::memory_order_relaxed);
    outMeta.analysis_epoch = analysis_epoch_.load(std::memory_order_relaxed);

    const std::size_t channels = outMeta.channel_count > 0 ? outMeta.channel_count : 2;
    const std::size_t sampleBytes = (outMeta.encoding == SampleEncoding::IEEE_FLOAT32) ? 4 : 2;
    const std::size_t frameBytes = channels * sampleBytes;

    auto tail = tail_.load(std::memory_order_relaxed);

    for (std::size_t f = 0; f < count; ++f) {
        const std::uint8_t* framePtr = &byte_buffer_[tail * frameBytes];
        if (outMeta.encoding == SampleEncoding::IEEE_FLOAT32) {
            const float* srcFloat = reinterpret_cast<const float*>(framePtr);
            for (std::size_t c = 0; c < channels; ++c) {
                outInterleavedFloat[f * channels + c] = srcFloat[c];
            }
        } else {
            const std::int16_t* srcI16 = reinterpret_cast<const std::int16_t*>(framePtr);
            for (std::size_t c = 0; c < channels; ++c) {
                outInterleavedFloat[f * channels + c] = static_cast<float>(srcI16[c]) / 32768.0f;
            }
        }
        tail = (tail + 1) % capacity_;
    }

    tail_.store(tail, std::memory_order_release);
    return count;
}

}  // namespace rgsml::analysis
