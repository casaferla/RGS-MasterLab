#pragma once

#include <rgsml/core/result.hpp>

#include <cstdint>

namespace rgsml::core {

[[nodiscard]] Result<std::int64_t> checked_add(std::int64_t left, std::int64_t right);
[[nodiscard]] Result<std::int64_t> checked_subtract(std::int64_t left, std::int64_t right);
[[nodiscard]] Result<std::int64_t> checked_multiply(std::int64_t left, std::int64_t right);
[[nodiscard]] Result<std::int64_t> checked_negate(std::int64_t value);
[[nodiscard]] Result<std::int64_t> checked_increment(std::int64_t value);

// The divisor must be positive. These implement mathematical floor/ceiling,
// including negative dividends, without negating INT64_MIN.
[[nodiscard]] Result<std::int64_t> floor_div_signed(std::int64_t dividend, std::int64_t divisor);
[[nodiscard]] Result<std::int64_t> ceil_div_signed(std::int64_t dividend, std::int64_t divisor);
[[nodiscard]] Result<std::int64_t> floor_mod(std::int64_t dividend, std::int64_t divisor);

// Defined only for non-negative input; odd INT64_MAX fails with overflow.
[[nodiscard]] Result<std::int64_t> next_even(std::int64_t value);

}  // namespace rgsml::core
