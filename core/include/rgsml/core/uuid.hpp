#pragma once

#include <rgsml/core/result.hpp>

#include <array>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace rgsml::core {

class Uuid final {
public:
    using Bytes = std::array<std::uint8_t, 16>;

    constexpr Uuid() noexcept = default;
    explicit constexpr Uuid(Bytes bytes) noexcept
        : bytes_(bytes)
    {
    }

    [[nodiscard]] static Result<Uuid> parse(std::string_view text);

    [[nodiscard]] constexpr const Bytes& bytes() const noexcept
    {
        return bytes_;
    }

    [[nodiscard]] constexpr bool is_nil() const noexcept
    {
        for (const auto value : bytes_) {
            if (value != 0U) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool operator==(const Uuid&) const = default;
    [[nodiscard]] auto operator<=>(const Uuid&) const = default;

private:
    Bytes bytes_{};
};

}  // namespace rgsml::core
