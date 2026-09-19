#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace rgsml::core {

using Sha256Digest = std::array<std::byte, 32>;

class Sha256 final {
public:
    Sha256() noexcept;

    void update(std::span<const std::byte> bytes) noexcept;
    [[nodiscard]] Sha256Digest finalize() noexcept;

private:
    void transform(const std::byte* block) noexcept;

    std::array<std::uint32_t, 8> state_{};
    std::array<std::byte, 64> buffer_{};
    std::uint64_t total_bytes_{0};
    std::size_t buffered_{0};
    bool finalized_{false};
};

[[nodiscard]] std::string sha256_hex(const Sha256Digest& digest);
[[nodiscard]] std::string sha256_hex(std::span<const std::byte> bytes);

}  // namespace rgsml::core
