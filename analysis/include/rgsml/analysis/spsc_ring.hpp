#ifndef RGSML_ANALYSIS_SPSC_RING_HPP
#define RGSML_ANALYSIS_SPSC_RING_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rgsml::analysis {

enum class SampleEncoding : std::uint8_t {
    IEEE_FLOAT32 = 0,
    PCM16_LE = 1
};

class SpscFrameRing final {
public:
    explicit SpscFrameRing(std::size_t frameCapacity = 32768);

    void reset(std::size_t frameCapacity);

    // Lock-free, zero allocation on hot audio callback
    std::size_t push_pcm_bytes(
        const void* pcmData,
        std::size_t byteCount,
        std::uint32_t sampleRateHz,
        std::uint8_t channelCount,
        SampleEncoding encoding,
        std::uint64_t streamGeneration,
        std::uint64_t analysisEpoch) noexcept;

    struct IngressMeta {
        std::uint32_t sample_rate_hz{0};
        std::uint8_t channel_count{0};
        SampleEncoding encoding{SampleEncoding::IEEE_FLOAT32};
        std::uint64_t stream_generation{0};
        std::uint64_t analysis_epoch{0};
    };

    std::size_t pop_frames_to_float(
        std::size_t maxFrames,
        float* outInterleavedFloat,
        IngressMeta& outMeta) noexcept;

    [[nodiscard]] std::size_t available_frames() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    void clear() noexcept;

private:
    std::size_t capacity_{0};
    std::vector<std::uint8_t> byte_buffer_;
    std::size_t bytes_per_frame_{8};

    std::array<std::uint8_t, 64> remainder_buffer_{};
    std::size_t remainder_len_{0};

    std::atomic<std::uint32_t> sample_rate_hz_{44100};
    std::atomic<std::uint8_t> channel_count_{2};
    std::atomic<SampleEncoding> encoding_{SampleEncoding::IEEE_FLOAT32};
    std::atomic<std::uint64_t> stream_generation_{0};
    std::atomic<std::uint64_t> analysis_epoch_{0};

    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

}  // namespace rgsml::analysis

#endif  // RGSML_ANALYSIS_SPSC_RING_HPP
