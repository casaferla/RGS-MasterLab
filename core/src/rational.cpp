#include <rgsml/core/rational.hpp>

#include <rgsml/core/checked_integer.hpp>

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>

namespace rgsml::core {
namespace {

struct UInt128 final {
    std::uint64_t high{};
    std::uint64_t low{};
};

[[nodiscard]] constexpr std::uint64_t magnitude(std::int64_t value) noexcept
{
    if (value >= 0) {
        return static_cast<std::uint64_t>(value);
    }
    return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] constexpr std::uint64_t gcd_u64(std::uint64_t left, std::uint64_t right) noexcept
{
    while (right != 0U) {
        const auto remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

[[nodiscard]] constexpr UInt128 multiply_u64(std::uint64_t left, std::uint64_t right) noexcept
{
    constexpr std::uint64_t mask32 = 0xffffffffULL;
    const std::uint64_t leftLow = left & mask32;
    const std::uint64_t leftHigh = left >> 32U;
    const std::uint64_t rightLow = right & mask32;
    const std::uint64_t rightHigh = right >> 32U;

    const std::uint64_t productLow = leftLow * rightLow;
    const std::uint64_t productMiddle1 = leftHigh * rightLow;
    const std::uint64_t productMiddle2 = leftLow * rightHigh;
    const std::uint64_t productHigh = leftHigh * rightHigh;

    const std::uint64_t middle = (productLow >> 32U)
        + (productMiddle1 & mask32) + (productMiddle2 & mask32);
    return UInt128{
        productHigh + (productMiddle1 >> 32U) + (productMiddle2 >> 32U) + (middle >> 32U),
        (middle << 32U) | (productLow & mask32)};
}

[[nodiscard]] constexpr int compare_u128(UInt128 left, UInt128 right) noexcept
{
    if (left.high != right.high) {
        return left.high < right.high ? -1 : 1;
    }
    if (left.low != right.low) {
        return left.low < right.low ? -1 : 1;
    }
    return 0;
}

[[nodiscard]] constexpr UInt128 add_u128(UInt128 left, UInt128 right) noexcept
{
    const auto low = left.low + right.low;
    return UInt128{left.high + right.high + (low < left.low ? 1U : 0U), low};
}

[[nodiscard]] constexpr UInt128 subtract_u128(UInt128 left, UInt128 right) noexcept
{
    const auto low = left.low - right.low;
    return UInt128{left.high - right.high - (left.low < right.low ? 1U : 0U), low};
}

[[nodiscard]] constexpr bool is_zero(UInt128 value) noexcept
{
    return value.high == 0U && value.low == 0U;
}

[[nodiscard]] constexpr bool bit_at(UInt128 value, int index) noexcept
{
    if (index >= 64) {
        return ((value.high >> static_cast<unsigned int>(index - 64)) & 1U) != 0U;
    }
    return ((value.low >> static_cast<unsigned int>(index)) & 1U) != 0U;
}

[[nodiscard]] std::uint64_t modulo_u128_u64(UInt128 dividend, std::uint64_t divisor) noexcept
{
    std::uint64_t remainder = 0;
    for (int bit = 127; bit >= 0; --bit) {
        remainder = remainder * 2U + (bit_at(dividend, bit) ? 1U : 0U);
        if (remainder >= divisor) {
            remainder -= divisor;
        }
    }
    return remainder;
}

[[nodiscard]] UInt128 divide_u128_u64(UInt128 dividend, std::uint64_t divisor) noexcept
{
    UInt128 quotient{};
    std::uint64_t remainder = 0;
    for (int bit = 127; bit >= 0; --bit) {
        remainder = remainder * 2U + (bit_at(dividend, bit) ? 1U : 0U);
        if (remainder < divisor) {
            continue;
        }
        remainder -= divisor;
        if (bit >= 64) {
            quotient.high |= 1ULL << static_cast<unsigned int>(bit - 64);
        } else {
            quotient.low |= 1ULL << static_cast<unsigned int>(bit);
        }
    }
    return quotient;
}

struct SignedMagnitude final {
    bool negative{};
    UInt128 magnitude{};
};

[[nodiscard]] SignedMagnitude product(
    std::int64_t signedValue,
    std::uint64_t unsignedValue,
    bool invertSign = false) noexcept
{
    const auto productMagnitude = multiply_u64(magnitude(signedValue), unsignedValue);
    const bool negative = !is_zero(productMagnitude) && ((signedValue < 0) != invertSign);
    return SignedMagnitude{negative, productMagnitude};
}

[[nodiscard]] SignedMagnitude add_signed(SignedMagnitude left, SignedMagnitude right) noexcept
{
    if (left.negative == right.negative) {
        return SignedMagnitude{left.negative, add_u128(left.magnitude, right.magnitude)};
    }
    const int ordering = compare_u128(left.magnitude, right.magnitude);
    if (ordering == 0) {
        return {};
    }
    if (ordering > 0) {
        return SignedMagnitude{left.negative, subtract_u128(left.magnitude, right.magnitude)};
    }
    return SignedMagnitude{right.negative, subtract_u128(right.magnitude, left.magnitude)};
}

[[nodiscard]] Result<std::int64_t> signed_from_magnitude(SignedMagnitude value)
{
    constexpr std::uint64_t maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    constexpr std::uint64_t minimumMagnitude = maximum + 1U;
    if (value.magnitude.high != 0U) {
        return Result<std::int64_t>::failure(
            Error{ErrorCode::IntegerOverflow, "Rational numerator exceeds signed 64-bit range."});
    }
    if (!value.negative && value.magnitude.low > maximum) {
        return Result<std::int64_t>::failure(
            Error{ErrorCode::IntegerOverflow, "Rational numerator exceeds signed 64-bit range."});
    }
    if (value.negative && value.magnitude.low > minimumMagnitude) {
        return Result<std::int64_t>::failure(
            Error{ErrorCode::IntegerOverflow, "Rational numerator exceeds signed 64-bit range."});
    }
    if (value.negative && value.magnitude.low == minimumMagnitude) {
        return Result<std::int64_t>::success(std::numeric_limits<std::int64_t>::min());
    }
    const auto converted = static_cast<std::int64_t>(value.magnitude.low);
    return Result<std::int64_t>::success(value.negative ? -converted : converted);
}

[[nodiscard]] Result<Rational> arithmetic_overflow()
{
    return Result<Rational>::failure(
        Error{ErrorCode::IntegerOverflow, "Rational result exceeds signed 64-bit representation."});
}

[[nodiscard]] Result<Rational> add_or_subtract(
    const Rational& left,
    const Rational& right,
    bool subtractRight)
{
    const auto leftDenominator = static_cast<std::uint64_t>(left.denominator());
    const auto rightDenominator = static_cast<std::uint64_t>(right.denominator());
    const auto commonDivisor = gcd_u64(leftDenominator, rightDenominator);
    const auto leftScale = rightDenominator / commonDivisor;
    const auto rightScale = leftDenominator / commonDivisor;

    auto numerator = add_signed(
        product(left.numerator(), leftScale),
        product(right.numerator(), rightScale, subtractRight));

    const auto secondDivisor = gcd_u64(
        modulo_u128_u64(numerator.magnitude, commonDivisor), commonDivisor);
    numerator.magnitude = divide_u128_u64(numerator.magnitude, secondDivisor);

    auto signedNumerator = signed_from_magnitude(numerator);
    if (!signedNumerator) {
        return arithmetic_overflow();
    }

    const auto denominator = checked_multiply(
        static_cast<std::int64_t>(leftDenominator / commonDivisor),
        static_cast<std::int64_t>(rightDenominator / secondDivisor));
    if (!denominator) {
        return arithmetic_overflow();
    }
    return Rational::create(*signedNumerator.value(), *denominator.value());
}

[[nodiscard]] int compare_positive(
    std::uint64_t leftNumerator,
    std::uint64_t leftDenominator,
    std::uint64_t rightNumerator,
    std::uint64_t rightDenominator) noexcept
{
    bool reverse = false;
    while (true) {
        const auto leftQuotient = leftNumerator / leftDenominator;
        const auto rightQuotient = rightNumerator / rightDenominator;
        if (leftQuotient != rightQuotient) {
            const int result = leftQuotient < rightQuotient ? -1 : 1;
            return reverse ? -result : result;
        }

        const auto leftRemainder = leftNumerator % leftDenominator;
        const auto rightRemainder = rightNumerator % rightDenominator;
        if (leftRemainder == 0U || rightRemainder == 0U) {
            int result = 0;
            if (leftRemainder != rightRemainder) {
                result = leftRemainder == 0U ? -1 : 1;
            }
            return reverse ? -result : result;
        }

        leftNumerator = leftDenominator;
        leftDenominator = leftRemainder;
        rightNumerator = rightDenominator;
        rightDenominator = rightRemainder;
        reverse = !reverse;
    }
}

[[nodiscard]] bool is_canonical_integer_text(std::string_view text, bool allowNegative) noexcept
{
    if (text.empty() || text.front() == '+') {
        return false;
    }
    std::size_t firstDigit = 0;
    if (text.front() == '-') {
        if (!allowNegative || text.size() == 1) {
            return false;
        }
        firstDigit = 1;
    }
    if (text[firstDigit] == '0' && text.size() - firstDigit != 1) {
        return false;
    }
    if (firstDigit == 1 && text[firstDigit] == '0') {
        return false;
    }
    for (std::size_t index = firstDigit; index < text.size(); ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_i64(std::string_view text, std::int64_t& value) noexcept
{
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

}  // namespace

Result<Rational> Rational::create(std::int64_t numerator, std::int64_t denominator)
{
    if (denominator <= 0) {
        return Result<Rational>::failure(
            Error{ErrorCode::InvalidRational, "A Rational denominator must be positive."});
    }
    if (numerator == 0) {
        return Result<Rational>::success(Rational{0, 1});
    }

    const auto divisor = gcd_u64(
        magnitude(numerator), static_cast<std::uint64_t>(denominator));
    return Result<Rational>::success(Rational{
        numerator / static_cast<std::int64_t>(divisor),
        denominator / static_cast<std::int64_t>(divisor)});
}

Result<Rational> Rational::parse(std::string_view text)
{
    const auto slash = text.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 >= text.size()
        || text.find('/', slash + 1) != std::string_view::npos) {
        return Result<Rational>::failure(
            Error{ErrorCode::ParseFailure, "Rational text must contain one slash."});
    }

    const auto numeratorText = text.substr(0, slash);
    const auto denominatorText = text.substr(slash + 1);
    if (!is_canonical_integer_text(numeratorText, true)
        || !is_canonical_integer_text(denominatorText, false)) {
        return Result<Rational>::failure(
            Error{ErrorCode::ParseFailure, "Rational text is not canonical decimal form."});
    }

    std::int64_t numerator = 0;
    std::int64_t denominator = 0;
    if (!parse_i64(numeratorText, numerator) || !parse_i64(denominatorText, denominator)
        || denominator <= 0) {
        return Result<Rational>::failure(
            Error{ErrorCode::ParseFailure, "Rational text is outside its valid range."});
    }

    auto rational = create(numerator, denominator);
    if (!rational || rational.value()->to_string() != text) {
        return Result<Rational>::failure(
            Error{ErrorCode::ParseFailure, "Rational text is not in reduced canonical form."});
    }
    return rational;
}

std::int64_t Rational::numerator() const noexcept
{
    return numerator_;
}

std::int64_t Rational::denominator() const noexcept
{
    return denominator_;
}

std::string Rational::to_string() const
{
    char numeratorBuffer[32]{};
    char denominatorBuffer[32]{};
    const auto numeratorResult = std::to_chars(
        numeratorBuffer, numeratorBuffer + sizeof(numeratorBuffer), numerator_);
    const auto denominatorResult = std::to_chars(
        denominatorBuffer, denominatorBuffer + sizeof(denominatorBuffer), denominator_);
    std::string output(numeratorBuffer, numeratorResult.ptr);
    output.push_back('/');
    output.append(denominatorBuffer, denominatorResult.ptr);
    return output;
}

Result<Rational> Rational::add(const Rational& other) const
{
    return add_or_subtract(*this, other, false);
}

Result<Rational> Rational::subtract(const Rational& other) const
{
    return add_or_subtract(*this, other, true);
}

Result<Rational> Rational::multiply(const Rational& other) const
{
    if (numerator_ == 0 || other.numerator_ == 0) {
        return Result<Rational>::success(Rational{0, 1});
    }

    const auto leftMagnitude = magnitude(numerator_);
    const auto rightMagnitude = magnitude(other.numerator_);
    const auto firstDivisor = gcd_u64(
        leftMagnitude, static_cast<std::uint64_t>(other.denominator_));
    const auto secondDivisor = gcd_u64(
        rightMagnitude, static_cast<std::uint64_t>(denominator_));

    const SignedMagnitude numerator{
        (numerator_ < 0) != (other.numerator_ < 0),
        multiply_u64(leftMagnitude / firstDivisor, rightMagnitude / secondDivisor)};
    auto signedNumerator = signed_from_magnitude(numerator);
    if (!signedNumerator) {
        return arithmetic_overflow();
    }

    const auto denominator = checked_multiply(
        denominator_ / static_cast<std::int64_t>(secondDivisor),
        other.denominator_ / static_cast<std::int64_t>(firstDivisor));
    if (!denominator) {
        return arithmetic_overflow();
    }
    return Rational::create(*signedNumerator.value(), *denominator.value());
}

Result<Rational> Rational::divide(const Rational& other) const
{
    if (other.numerator_ == 0) {
        return Result<Rational>::failure(
            Error{ErrorCode::DivisionByZero, "Cannot divide by a zero Rational."});
    }
    if (numerator_ == 0) {
        return Result<Rational>::success(Rational{0, 1});
    }

    const auto leftMagnitude = magnitude(numerator_);
    const auto rightMagnitude = magnitude(other.numerator_);
    const auto firstDivisor = gcd_u64(leftMagnitude, rightMagnitude);
    const auto secondDivisor = gcd_u64(
        static_cast<std::uint64_t>(other.denominator_),
        static_cast<std::uint64_t>(denominator_));

    const SignedMagnitude numerator{
        (numerator_ < 0) != (other.numerator_ < 0),
        multiply_u64(
            leftMagnitude / firstDivisor,
            static_cast<std::uint64_t>(other.denominator_) / secondDivisor)};
    auto signedNumerator = signed_from_magnitude(numerator);
    if (!signedNumerator) {
        return arithmetic_overflow();
    }

    const auto denominatorMagnitude = multiply_u64(
        static_cast<std::uint64_t>(denominator_) / secondDivisor,
        rightMagnitude / firstDivisor);
    constexpr auto maximum = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (denominatorMagnitude.high != 0U || denominatorMagnitude.low > maximum) {
        return arithmetic_overflow();
    }
    return Rational::create(
        *signedNumerator.value(), static_cast<std::int64_t>(denominatorMagnitude.low));
}

bool Rational::operator<(const Rational& other) const noexcept
{
    const bool leftNegative = numerator_ < 0;
    const bool rightNegative = other.numerator_ < 0;
    if (leftNegative != rightNegative) {
        return leftNegative;
    }
    if (numerator_ == 0 || other.numerator_ == 0) {
        return numerator_ < other.numerator_;
    }

    const int comparison = compare_positive(
        magnitude(numerator_), static_cast<std::uint64_t>(denominator_),
        magnitude(other.numerator_), static_cast<std::uint64_t>(other.denominator_));
    return leftNegative ? comparison > 0 : comparison < 0;
}

bool Rational::operator<=(const Rational& other) const noexcept
{
    return *this == other || *this < other;
}

bool Rational::operator>(const Rational& other) const noexcept
{
    return other < *this;
}

bool Rational::operator>=(const Rational& other) const noexcept
{
    return *this == other || other < *this;
}

}  // namespace rgsml::core
