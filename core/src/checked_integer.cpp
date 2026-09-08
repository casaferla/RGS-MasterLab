#include <rgsml/core/checked_integer.hpp>

#include <limits>

namespace rgsml::core {
namespace {

[[nodiscard]] Result<std::int64_t> overflow(std::string_view operation)
{
    return Result<std::int64_t>::failure(
        Error{ErrorCode::IntegerOverflow, "Signed 64-bit integer overflow.", {{"operation", std::string(operation)}}});
}

[[nodiscard]] Result<std::int64_t> invalid_argument(std::string message)
{
    return Result<std::int64_t>::failure(
        Error{ErrorCode::InvalidArgument, std::move(message)});
}

}  // namespace

Result<std::int64_t> checked_add(std::int64_t left, std::int64_t right)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right)
        || (right < 0 && left < minimum - right)) {
        return overflow("add");
    }
    return Result<std::int64_t>::success(left + right);
}

Result<std::int64_t> checked_subtract(std::int64_t left, std::int64_t right)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left < minimum + right)
        || (right < 0 && left > maximum + right)) {
        return overflow("subtract");
    }
    return Result<std::int64_t>::success(left - right);
}

Result<std::int64_t> checked_multiply(std::int64_t left, std::int64_t right)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if (left == 0 || right == 0) {
        return Result<std::int64_t>::success(0);
    }

    bool exceedsRange = false;
    if (left > 0) {
        exceedsRange = right > 0 ? left > maximum / right : right < minimum / left;
    } else {
        exceedsRange = right > 0 ? left < minimum / right : left < maximum / right;
    }
    if (exceedsRange) {
        return overflow("multiply");
    }
    return Result<std::int64_t>::success(left * right);
}

Result<std::int64_t> checked_negate(std::int64_t value)
{
    if (value == std::numeric_limits<std::int64_t>::min()) {
        return overflow("negate");
    }
    return Result<std::int64_t>::success(-value);
}

Result<std::int64_t> checked_increment(std::int64_t value)
{
    return checked_add(value, 1);
}

Result<std::int64_t> floor_div_signed(std::int64_t dividend, std::int64_t divisor)
{
    if (divisor <= 0) {
        return invalid_argument("floor_div_signed requires a positive divisor.");
    }
    auto quotient = dividend / divisor;
    const auto remainder = dividend % divisor;
    if (remainder < 0) {
        --quotient;
    }
    return Result<std::int64_t>::success(quotient);
}

Result<std::int64_t> ceil_div_signed(std::int64_t dividend, std::int64_t divisor)
{
    if (divisor <= 0) {
        return invalid_argument("ceil_div_signed requires a positive divisor.");
    }
    auto quotient = dividend / divisor;
    const auto remainder = dividend % divisor;
    if (remainder > 0) {
        ++quotient;
    }
    return Result<std::int64_t>::success(quotient);
}

Result<std::int64_t> floor_mod(std::int64_t dividend, std::int64_t divisor)
{
    if (divisor <= 0) {
        return invalid_argument("floor_mod requires a positive divisor.");
    }
    auto remainder = dividend % divisor;
    if (remainder < 0) {
        remainder += divisor;
    }
    return Result<std::int64_t>::success(remainder);
}

Result<std::int64_t> next_even(std::int64_t value)
{
    if (value < 0) {
        return invalid_argument("next_even requires a non-negative value.");
    }
    if (value % 2 == 0) {
        return Result<std::int64_t>::success(value);
    }
    return checked_increment(value);
}

}  // namespace rgsml::core
