#pragma once

#include <rgsml/core/result.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace rgsml::core {

// Canonical signed rational: reduced numerator and strictly positive
// denominator. All fallible arithmetic reports a categorical Error.
class Rational final {
public:
    Rational() = delete;

    [[nodiscard]] static Result<Rational> create(
        std::int64_t numerator,
        std::int64_t denominator);
    [[nodiscard]] static Result<Rational> parse(std::string_view text);

    [[nodiscard]] std::int64_t numerator() const noexcept;
    [[nodiscard]] std::int64_t denominator() const noexcept;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] Result<Rational> add(const Rational& other) const;
    [[nodiscard]] Result<Rational> subtract(const Rational& other) const;
    [[nodiscard]] Result<Rational> multiply(const Rational& other) const;
    [[nodiscard]] Result<Rational> divide(const Rational& other) const;

    [[nodiscard]] bool operator==(const Rational&) const noexcept = default;
    [[nodiscard]] bool operator<(const Rational& other) const noexcept;
    [[nodiscard]] bool operator<=(const Rational& other) const noexcept;
    [[nodiscard]] bool operator>(const Rational& other) const noexcept;
    [[nodiscard]] bool operator>=(const Rational& other) const noexcept;

private:
    constexpr Rational(std::int64_t numerator, std::int64_t denominator) noexcept
        : numerator_(numerator)
        , denominator_(denominator)
    {
    }

    std::int64_t numerator_;
    std::int64_t denominator_;
};

}  // namespace rgsml::core
