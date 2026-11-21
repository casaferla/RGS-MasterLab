#pragma once
// M19 PREPRODUCTION ONLY. Scope-relative exact temporal geometry; NOT
// a new Phase-6 temporal-grid owner or an accepted product implementation.
// The frozen calculation rounds independent rational offsets ties-to-even.
// No floating-point time multiplication, cumulative rounded hops, or padding.
#include <cstdint>
#include <limits>
#include <optional>

namespace rgsml::analysis::internal::m19 {

[[nodiscard]] constexpr std::optional<std::uint64_t>
checked_round_ties_even_ratio(std::uint64_t numeratorA,
                              std::uint64_t numeratorB,
                              std::uint64_t denominator) noexcept
{
    if (denominator == 0U) return std::nullopt;
    // Exact floor(A*B/D) while avoiding 64-bit overflow whenever a split
    // division is possible, for Fs<=UINT32_MAX and arbitrary 64-bit j.
    const std::uint64_t whole = numeratorA / denominator;
    const std::uint64_t fractionalPart = numeratorA % denominator;
    const auto max = (std::numeric_limits<std::uint64_t>::max)();
    if (numeratorB != 0U && whole > max / numeratorB) return std::nullopt;
    const std::uint64_t q = whole * numeratorB;

    // This private temporal geometry is only used with small fixed rates:
    // supports Fs<=1,000,000 and denominators 5/10; the bound makes the
    // residual product exact, without compiler-specific __int128.
    if (numeratorB > 1000000U || denominator > 10U
        || fractionalPart > max / numeratorB) return std::nullopt;
    const std::uint64_t remainderProduct = fractionalPart * numeratorB;
    const std::uint64_t extraWhole = remainderProduct / denominator;
    const std::uint64_t remainder = remainderProduct % denominator;
    if (extraWhole > max - q) return std::nullopt;
    const std::uint64_t floorValue = q + extraWhole;
    const bool roundUp = (remainder > denominator / 2U
        && (denominator % 2U == 0U || remainder * 2U > denominator))
        || ((denominator % 2U == 0U) && remainder == denominator / 2U
            && (floorValue & 1U) != 0U);
    if (roundUp && floorValue == max) return std::nullopt;
    return floorValue + (roundUp ? 1U : 0U);
}

// Source sample rate is integer, 6 authorized PRODUCT rates; diagnostic
// rates can exercise mathematical parity without qualifying a K bank.
[[nodiscard]] constexpr std::optional<std::uint64_t>
block_400ms_frames(std::uint32_t hz) noexcept
{
    if (hz == 0U) return std::nullopt;
    return checked_round_ties_even_ratio(2U, hz, 5U);
}

[[nodiscard]] constexpr std::optional<std::uint64_t>
relative_100ms_grid_start(std::uint64_t blockIndex,
                          std::uint32_t hz) noexcept
{
    if (hz == 0U) return std::nullopt;
    return checked_round_ties_even_ratio(blockIndex, hz, 10U);
}

[[nodiscard]] constexpr std::optional<std::uint64_t>
absolute_block_start(std::uint64_t blockIndex, std::uint32_t hz,
                     std::uint64_t logicalScopeStart) noexcept
{
    auto offset = relative_100ms_grid_start(blockIndex, hz);
    if (!offset ||
        logicalScopeStart >
            (std::numeric_limits<std::uint64_t>::max)() - *offset) {
        return std::nullopt;
    }
    return logicalScopeStart + *offset;
}

[[nodiscard]] constexpr bool complete_block_fits(
    std::uint64_t blockIndex, std::uint32_t hz,
    std::uint64_t scopeFrames) noexcept
{
    const auto start = relative_100ms_grid_start(blockIndex, hz);
    const auto length = block_400ms_frames(hz);
    return start && length && *start <= scopeFrames
        && *length <= scopeFrames - *start;
}

} // namespace rgsml::analysis::internal::m19
