// B2b qualification-only adapter. It calls the UNMODIFIED production
// full-resource estimator with authenticated PCM64; no product Gate G claim.
#include "internal/true_peak_full_resource.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#endif

namespace {
using namespace rgsml;
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8);

[[nodiscard]] bool read_decimal(std::string_view s, std::uint64_t& output)
{
    const auto [end, error] =
        std::from_chars(s.data(), s.data() + s.size(), output);
    return error == std::errc{} && end == s.data() + s.size();
}

[[nodiscard]] double binary64_le(const unsigned char* bytes) noexcept
{
    std::uint64_t bits = 0;
    for (unsigned j = 0; j < 8; ++j)
        bits |= std::uint64_t{bytes[j]} << (8U * j);
    return std::bit_cast<double>(bits);
}

void json_number(std::optional<double> number)
{
    if (number && std::isfinite(*number))
        std::cout << *number;
    else
        std::cout << "null";
}

[[nodiscard]] bool evaluate(std::uint64_t rateHz, std::uint64_t frameCount,
                            bool useStdin)
{
    if (frameCount == 0 || frameCount > 4000000
        || rateHz > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()))
        return false;
    auto rate = core::SampleRate::create(static_cast<std::int64_t>(rateHz));
    if (!rate) return false;
    auto format = audio::AudioFormat::create(
        *rate.value(), audio::ChannelLayout::STEREO_LR);
    if (!format) return false;
    auto count = core::FrameCount::create(
        static_cast<std::int64_t>(frameCount));
    if (!count) return false;
    auto buffer = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, *count.value());
    if (!buffer) return false;
    auto view = buffer.value()->mutable_view();
    auto l = view.channel(0);
    auto r = view.channel(1);
    if (!l || !r) return false;

    // Framed little-endian binary64, L then R, no audio written to disk.
    // Authentic WAV/PCM64 hashes MUST be verified separately before use.
    for (std::size_t i = 0; i < static_cast<std::size_t>(frameCount); ++i) {
        if (useStdin) {
            std::array<unsigned char, 16> bytes{};
            std::cin.read(reinterpret_cast<char*>(bytes.data()),
                          static_cast<std::streamsize>(bytes.size()));
            if (std::cin.gcount() != static_cast<std::streamsize>(bytes.size()))
                return false;
            (*l.value())[i] = binary64_le(bytes.data());
            (*r.value())[i] = binary64_le(bytes.data() + 8);
        } else {
            (*l.value())[i] = i == 0 ? 1.0 : 0.0;
            (*r.value())[i] = 0.0;
        }
    }
    if (useStdin && std::cin.peek() != std::char_traits<char>::eof())
        return false;

    auto result = analysis::internal::measure_unqualified_true_peak_full_resource(
        *buffer.value());
    if (!result) return false;
    const auto& v = *result.value();
    if (v.channel_count != 2
        || v.sample_rate_hz != static_cast<std::int64_t>(rateHz)
        || !std::isfinite(v.global_linear_peak))
        return false;

    if (!useStdin) {
        if (v.per_channel_linear_peak[0] != 7964.0 / 8192.0
            || v.per_channel_linear_peak[1] != 0.0
            || v.global_linear_peak != 7964.0 / 8192.0
            || v.is_exact_silence)
            return false;
        std::cout << "{\"synthetic_selftest\":\"PASS\","
                     "\"production_gate_g\":\"NOT_EXECUTED\"}\n";
        return true;
    }

    std::cout << std::setprecision(17);
    std::cout << "{\"sample_rate_hz\":" << v.sample_rate_hz
              << ",\"frames\":" << frameCount
              << ",\"channel_count\":" << v.channel_count
              << ",\"left_linear_peak\":" << v.per_channel_linear_peak[0]
              << ",\"right_linear_peak\":" << v.per_channel_linear_peak[1]
              << ",\"global_linear_peak\":" << v.global_linear_peak
              << ",\"left_dbtp\":";
    json_number(v.per_channel_db_tp[0]);
    std::cout << ",\"right_dbtp\":";
    json_number(v.per_channel_db_tp[1]);
    std::cout << ",\"global_dbtp\":";
    json_number(v.global_db_tp);
    std::cout << ",\"is_exact_silence\":"
              << (v.is_exact_silence ? "true" : "false")
              << ",\"production_gate_g\":\"NOT_EXECUTED\"}\n";
    return static_cast<bool>(std::cout);
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view{argv[1]} == "--selftest") {
        if (evaluate(48000, 1, false)) return 0;
        std::cerr << "B2B_SYNTHETIC_SELFTEST_FAILED\n";
        return 1;
    }
    // MSVC CRT defaults stdin to TEXT. That converts CRLF and treats 0x1A
    // as EOF, silently corrupting binary64 PCM piped from the private verifier.
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        std::cerr << "B2B_STDIN_BINARY_MODE_FAILED\n";
        return 1;
    }
#endif
    std::uint64_t rate = 0, frames = 0;
    if (argc != 3 || !read_decimal(argv[1], rate)
        || !read_decimal(argv[2], frames)) {
        std::cerr << "Usage: rgsml_true_peak_b2b_probe <sample-rate> <frames>"
                     " < private-stereo-pcm64-stdin\n";
        return 2;
    }
    if (evaluate(rate, frames, true)) return 0;
    std::cerr << "B2B_PRODUCTION_PROBE_FAILED; Gate G not passed\n";
    return 1;
}
