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
    ring_buffer_.assign(capacity_, AnalysisFrame{});
    remainder_len_ = 0;
    producer_rate_hz_ = 0;
    producer_channels_ = 0;
    producer_generation_ = 0;
    producer_epoch_ = 0;
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
}

void SpscFrameRing::clear() noexcept
{
    tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
}

std::size_t SpscFrameRing::available_frames() const noexcept
{
    const auto head = head_.load(std::memory_order_acquire);
    const auto tail = tail_.load(std::memory_order_relaxed);
    return (head >= tail) ? (head - tail) : (capacity_ - (tail - head));
}

namespace {

inline void decode_sample_pair(
    const std::uint8_t* ptr,
    std::uint8_t channelCount,
    SampleEncoding encoding,
    float& outL,
    float& outR) noexcept
{
    if (encoding == SampleEncoding::IEEE_FLOAT32) {
        const float* f32 = reinterpret_cast<const float*>(ptr);
        outL = f32[0];
        outR = (channelCount > 1) ? f32[1] : outL;
    } else {
        const std::int16_t* i16 = reinterpret_cast<const std::int16_t*>(ptr);
        outL = static_cast<float>(i16[0]) / 32768.0f;
        outR = (channelCount > 1) ? static_cast<float>(i16[1]) / 32768.0f : outL;
    }
}

}  // namespace

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

    // If stream identity or format changed, discard incomplete old producer remainder!
    if (producer_rate_hz_ != sampleRateHz
        || producer_channels_ != channelCount
        || producer_encoding_ != encoding
        || producer_generation_ != streamGeneration
        || producer_epoch_ != analysisEpoch) {
        remainder_len_ = 0;
        producer_rate_hz_ = sampleRateHz;
        producer_channels_ = channelCount;
        producer_encoding_ = encoding;
        producer_generation_ = streamGeneration;
        producer_epoch_ = analysisEpoch;
    }

    const std::uint8_t* inPtr = static_cast<const std::uint8_t*>(pcmData);
    std::size_t inRem = byteCount;

    auto head = head_.load(std::memory_order_relaxed);
    auto tail = tail_.load(std::memory_order_acquire);
    std::size_t pushedFrames = 0;

    // Process remainder if present
    if (remainder_len_ > 0 && inRem > 0) {
        const std::size_t need = frameBytes - remainder_len_;
        if (inRem >= need) {
            std::memcpy(remainder_buffer_.data() + remainder_len_, inPtr, need);
            inPtr += need;
            inRem -= need;
            remainder_len_ = 0;

            const std::size_t curFrames = (head >= tail) ? (head - tail) : (capacity_ - (tail - head));
            if (capacity_ - 1 - curFrames > 0) {
                AnalysisFrame& frame = ring_buffer_[head];
                decode_sample_pair(remainder_buffer_.data(), channelCount, encoding, frame.sample_l, frame.sample_r);
                frame.sample_rate_hz = sampleRateHz;
                frame.channel_count = channelCount;
                frame.stream_generation = streamGeneration;
                frame.analysis_epoch = analysisEpoch;

                head = (head + 1) % capacity_;
                ++pushedFrames;
            }
        } else {
            std::memcpy(remainder_buffer_.data() + remainder_len_, inPtr, inRem);
            remainder_len_ += inRem;
            return 0;
        }
    }

    const std::size_t directFrames = inRem / frameBytes;
    const std::size_t leftover = inRem % frameBytes;

    if (leftover > 0) {
        std::memcpy(remainder_buffer_.data(), inPtr + directFrames * frameBytes, leftover);
        remainder_len_ = leftover;
    } else {
        remainder_len_ = 0;
    }

    if (directFrames > 0) {
        const std::size_t curFrames = (head >= tail) ? (head - tail) : (capacity_ - (tail - head));
        const std::size_t freeFrames = capacity_ - 1 - curFrames;
        const std::size_t countToPush = std::min(directFrames, freeFrames);

        if (countToPush < directFrames) {
            // Capacity overflow! Advance producer analysis epoch for discontinuity
            ++producer_epoch_;
        }

        const std::uint64_t effectiveEpoch = (countToPush < directFrames) ? producer_epoch_ : analysisEpoch;

        for (std::size_t f = 0; f < countToPush; ++f) {
            AnalysisFrame& frame = ring_buffer_[head];
            decode_sample_pair(inPtr + f * frameBytes, channelCount, encoding, frame.sample_l, frame.sample_r);
            frame.sample_rate_hz = sampleRateHz;
            frame.channel_count = channelCount;
            frame.stream_generation = streamGeneration;
            frame.analysis_epoch = effectiveEpoch;

            head = (head + 1) % capacity_;
        }
        pushedFrames += countToPush;
    }

    head_.store(head, std::memory_order_release);
    return pushedFrames;
}

std::size_t SpscFrameRing::pop_frames(
    std::size_t maxFrames,
    AnalysisFrame* outFrames) noexcept
{
    const std::size_t avail = available_frames();
    const std::size_t count = std::min(maxFrames, avail);
    if (count == 0) {
        return 0;
    }

    auto tail = tail_.load(std::memory_order_relaxed);
    for (std::size_t f = 0; f < count; ++f) {
        outFrames[f] = ring_buffer_[tail];
        tail = (tail + 1) % capacity_;
    }

    tail_.store(tail, std::memory_order_release);
    return count;
}

}  // namespace rgsml::analysis
