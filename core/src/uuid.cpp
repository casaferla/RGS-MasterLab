#include <rgsml/core/uuid.hpp>

#include <cstddef>

namespace rgsml::core {
namespace {

[[nodiscard]] int hex_value(char value) noexcept
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

}  // namespace

Result<Uuid> Uuid::parse(std::string_view text)
{
    constexpr std::size_t canonicalLength = 36;
    if (text.size() != canonicalLength || text[8] != '-' || text[13] != '-'
        || text[18] != '-' || text[23] != '-') {
        return Result<Uuid>::failure(
            Error{ErrorCode::InvalidUuid, "UUID text is not canonical hyphenated form."});
    }

    Bytes bytes{};
    std::size_t byteIndex = 0;
    for (std::size_t index = 0; index < text.size();) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            ++index;
            continue;
        }

        const int high = hex_value(text[index]);
        const int low = hex_value(text[index + 1]);
        if (high < 0 || low < 0) {
            return Result<Uuid>::failure(
                Error{ErrorCode::InvalidUuid, "UUID text contains a non-hex digit."});
        }
        bytes[byteIndex] = static_cast<std::uint8_t>((high << 4) | low);
        ++byteIndex;
        index += 2;
    }

    return Result<Uuid>::success(Uuid{bytes});
}

std::string Uuid::to_string() const
{
    constexpr char digits[] = "0123456789abcdef";
    std::string output(36, '-');
    std::size_t outputIndex = 0;
    for (const auto value : bytes_) {
        if (outputIndex == 8 || outputIndex == 13 || outputIndex == 18 || outputIndex == 23) {
            ++outputIndex;
        }
        output[outputIndex] = digits[value >> 4U];
        output[outputIndex + 1] = digits[value & 0x0fU];
        outputIndex += 2;
    }
    return output;
}

}  // namespace rgsml::core
