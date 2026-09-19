#include <rgsml/core/sha256.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace rgsml::core {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

[[nodiscard]] constexpr std::uint32_t choose(
    std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
{
    return (x & y) ^ (~x & z);
}

[[nodiscard]] constexpr std::uint32_t majority(
    std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
{
    return (x & y) ^ (x & z) ^ (y & z);
}

[[nodiscard]] constexpr std::uint32_t big_sigma0(std::uint32_t x) noexcept
{
    return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
}

[[nodiscard]] constexpr std::uint32_t big_sigma1(std::uint32_t x) noexcept
{
    return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
}

[[nodiscard]] constexpr std::uint32_t small_sigma0(std::uint32_t x) noexcept
{
    return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3U);
}

[[nodiscard]] constexpr std::uint32_t small_sigma1(std::uint32_t x) noexcept
{
    return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10U);
}

}  // namespace

Sha256::Sha256() noexcept
    : state_{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U}
{
}

void Sha256::transform(const std::byte* block) noexcept
{
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16U; ++index) {
        const auto offset = index * 4U;
        words[index] = (static_cast<std::uint32_t>(block[offset]) << 24U)
            | (static_cast<std::uint32_t>(block[offset + 1U]) << 16U)
            | (static_cast<std::uint32_t>(block[offset + 2U]) << 8U)
            | static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
        words[index] = small_sigma1(words[index - 2U]) + words[index - 7U]
            + small_sigma0(words[index - 15U]) + words[index - 16U];
    }

    auto a = state_[0];
    auto b = state_[1];
    auto c = state_[2];
    auto d = state_[3];
    auto e = state_[4];
    auto f = state_[5];
    auto g = state_[6];
    auto h = state_[7];
    for (std::size_t index = 0; index < words.size(); ++index) {
        const auto t1 = h + big_sigma1(e) + choose(e, f, g)
            + kRoundConstants[index] + words[index];
        const auto t2 = big_sigma0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(std::span<const std::byte> bytes) noexcept
{
    if (finalized_ || bytes.empty()) {
        return;
    }
    total_bytes_ += static_cast<std::uint64_t>(bytes.size());
    std::size_t offset = 0;
    if (buffered_ != 0U) {
        const auto copied = std::min(bytes.size(), buffer_.size() - buffered_);
        std::copy_n(bytes.data(), copied, buffer_.data() + buffered_);
        buffered_ += copied;
        offset += copied;
        if (buffered_ == buffer_.size()) {
            transform(buffer_.data());
            buffered_ = 0U;
        }
    }
    while (bytes.size() - offset >= buffer_.size()) {
        transform(bytes.data() + offset);
        offset += buffer_.size();
    }
    const auto remaining = bytes.size() - offset;
    if (remaining != 0U) {
        std::copy_n(bytes.data() + offset, remaining, buffer_.data());
        buffered_ = remaining;
    }
}

Sha256Digest Sha256::finalize() noexcept
{
    if (!finalized_) {
        const auto bitLength = total_bytes_ * 8U;
        buffer_[buffered_++] = std::byte{0x80};
        if (buffered_ > 56U) {
            std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_), buffer_.end(), std::byte{0});
            transform(buffer_.data());
            buffered_ = 0U;
        }
        std::fill(
            buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_),
            buffer_.begin() + 56,
            std::byte{0});
        for (std::size_t index = 0; index < 8U; ++index) {
            buffer_[56U + index] = static_cast<std::byte>(
                (bitLength >> ((7U - index) * 8U)) & 0xffU);
        }
        transform(buffer_.data());
        finalized_ = true;
    }
    Sha256Digest digest{};
    for (std::size_t word = 0; word < state_.size(); ++word) {
        for (std::size_t byte = 0; byte < 4U; ++byte) {
            digest[word * 4U + byte] = static_cast<std::byte>(
                (state_[word] >> ((3U - byte) * 8U)) & 0xffU);
        }
    }
    return digest;
}

std::string sha256_hex(const Sha256Digest& digest)
{
    constexpr char kHex[] = "0123456789abcdef";
    std::string text;
    text.resize(digest.size() * 2U);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        const auto value = static_cast<unsigned char>(digest[index]);
        text[index * 2U] = kHex[value >> 4U];
        text[index * 2U + 1U] = kHex[value & 0x0fU];
    }
    return text;
}

std::string sha256_hex(std::span<const std::byte> bytes)
{
    Sha256 sha;
    sha.update(bytes);
    return sha256_hex(sha.finalize());
}

}  // namespace rgsml::core
