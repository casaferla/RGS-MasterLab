// M18 prequalification: original synthetic audible probe, no music copyright.
// PREPRODUCTION ONLY. Actual terminal C++ exporter, not app / M17 FQ.
#include "internal/m18_offline_export.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>

using namespace rgsml::audio::internal;
namespace fs = std::filesystem;

struct SyntheticAudio final : M18MasterFrameProvider {
    std::uint32_t rate{48000};
    static constexpr std::uint32_t seconds = 5;
    static constexpr double pi = 3.1415926535897932384626433832795;

    [[nodiscard]] bool read_exact(std::uint64_t first, std::uint32_t count,
                                  std::uint32_t channels,
                                  std::span<double> output) noexcept override {
        const std::uint64_t total = static_cast<std::uint64_t>(seconds) * rate;
        if (channels != 2U || first > total || count > total - first ||
            output.size() != static_cast<std::size_t>(count) * channels)
            return false;
        for (std::uint32_t f = 0; f < count; ++f) {
            const double t = static_cast<double>(first + f) / rate;
            const double fade = std::min({1.0, t / 0.20,
                                          (seconds - t) / 1.45});
            const double amp = 1.0 + 0.08 * std::sin(2.0 * pi * 1.5 * t);
            for (std::uint32_t ch = 0; ch < channels; ++ch) {
                const double p = ch == 0 ? 0.0 : 0.17;
                const double x = 0.26 * std::sin(2.0 * pi * 110.0 * t)
                    + 0.15 * std::sin(2.0 * pi * 220.0 * t + p)
                    + 0.09 * std::sin(2.0 * pi * 330.0 * t + 0.3 * p)
                    + 0.04 * std::sin(2.0 * pi * 550.0 * t + 0.7 * p);
                // Sum stays strictly below full-scale, no limiter or clipping.
                output[static_cast<std::size_t>(f) * channels + ch] =
                    amp * fade * x;
            }
        }
        return true;
    }
};

struct ScopedDirectory {
    fs::path path;
    ~ScopedDirectory() {
        if (!path.empty()) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};

int main(int argc, char** argv) {
    const bool emit = argc == 3 && std::string(argv[1]) == "--emit-fixtures";
    if (argc != 1 && !emit) return 2;
    std::error_code ec;
    fs::path dir;
    if (emit) {
        dir = argv[2];
        if (!fs::is_directory(dir, ec) || ec) return 3;
    } else {
        dir = fs::temp_directory_path(ec) / ("m18-audible-"
            + std::to_string(std::chrono::steady_clock::now()
                                     .time_since_epoch().count()));
        if (ec || !fs::create_directory(dir, ec) || ec) return 4;
    }
    ScopedDirectory cleanup{emit ? fs::path{} : dir};
    constexpr std::array<std::uint32_t, 2> rates{{44100U, 48000U}};
    for (const auto rate : rates) {
        for (const auto encoding : {M18WavEncoding::PCM24,
                                    M18WavEncoding::IEEE_FLOAT32}) {
            SyntheticAudio provider;
            provider.rate = rate;
            const char* name = encoding == M18WavEncoding::PCM24 ? "pcm24" : "float32";
            const std::string file = std::string("m18-synthetic-audible-")
                + name + "-" + std::to_string(rate) + "-stereo.wav";
            const M18WavSpec spec{encoding, rate, 2U,
                static_cast<std::uint64_t>(rate) * SyntheticAudio::seconds,
                UINT64_C(0x0123456789ABCDEF)};
            const auto result = m18_export_offline_candidate(
                provider, dir / file, spec, 0U);
            if (result.status != M18OfflineExportStatus::OK || !result.receipt) {
                std::cerr << "M18_AUDIBLE_WRITE_FAIL " << file << "\n";
                return 7;
            }
            if (emit) {
                std::cout << "M18_LISTEN\t" << file << "\t" << name
                    << "\t" << rate << "\t2\t" << spec.frame_count
                    << "\t" << result.receipt->full_file_sha256
                    << "\t" << result.receipt->payload_sha256 << "\n";
            }
        }
    }
    std::cout << "M18_FOUR_AUDIBLE_SYNTHETIC_PREQUAL_WAV_PASS_NOT_LISTENING_GATE\n";
    return 0;
}