#pragma once

#include "../render/render_test_support.hpp"

#include <QtCore/QByteArray>
#include <QtCore/QCryptographicHash>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rgsml::tests::stereo_ms_fixture {

constexpr std::size_t kFrames = 4097;
constexpr double kRate = 48000.0;
constexpr double kCutoff = 120.0;

// Independent deterministic dyadic test input, August M15 oracle lane.
// All generated values are exactly representable as IEEE-754 binary64.
[[nodiscard]] inline std::array<double, kFrames> samples(std::uint32_t seed)
{
    std::array<double, kFrames> out{};
    for (double& sample : out) {
        seed = UINT32_C(1664525) * seed + UINT32_C(1013904223);
        const std::int32_t centered =
            static_cast<std::int32_t>(seed >> 8) - INT32_C(8388608);
        sample = static_cast<double>(centered) / 8388608.0;
    }
    return out;
}

[[nodiscard]] inline QByteArray binary64_le_sha256(std::span<const double> samples)
{
    QByteArray data;
    data.reserve(static_cast<qsizetype>(samples.size() * 8));
    for (const double sample : samples) {
        const std::uint64_t word = std::bit_cast<std::uint64_t>(sample);
        for (std::size_t b = 0; b < 8; ++b) {
            data.append(static_cast<char>((word >> (8 * b)) & 0xffU));
        }
    }
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

}  // namespace rgsml::tests::stereo_ms_fixture
