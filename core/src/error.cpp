#include <rgsml/core/error.hpp>

#include <rgsml/core/result.hpp>

#include <array>
#include <utility>

namespace rgsml::core {

Error::Error(ErrorCode code, std::string message, std::vector<ErrorDetail> details)
    : code_(code)
    , message_(std::move(message))
    , details_(std::move(details))
{
}

ErrorCode Error::code() const noexcept
{
    return code_;
}

const std::string& Error::message() const noexcept
{
    return message_;
}

const std::vector<ErrorDetail>& Error::details() const noexcept
{
    return details_;
}

std::string_view error_code_token(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::InvalidArgument:
        return "invalid_argument";
    case ErrorCode::OutOfRange:
        return "out_of_range";
    case ErrorCode::IntegerOverflow:
        return "integer_overflow";
    case ErrorCode::DivisionByZero:
        return "division_by_zero";
    case ErrorCode::ParseFailure:
        return "parse_failure";
    case ErrorCode::InvalidUuid:
        return "invalid_uuid";
    case ErrorCode::InvalidRational:
        return "invalid_rational";
    case ErrorCode::InvalidFrameRange:
        return "invalid_frame_range";
    }
    return {};
}

Result<ErrorCode> parse_error_code(std::string_view token)
{
    constexpr std::array<ErrorCode, 8> codes{
        ErrorCode::InvalidArgument,
        ErrorCode::OutOfRange,
        ErrorCode::IntegerOverflow,
        ErrorCode::DivisionByZero,
        ErrorCode::ParseFailure,
        ErrorCode::InvalidUuid,
        ErrorCode::InvalidRational,
        ErrorCode::InvalidFrameRange,
    };

    for (const auto code : codes) {
        if (error_code_token(code) == token) {
            return Result<ErrorCode>::success(code);
        }
    }
    return Result<ErrorCode>::failure(
        Error{ErrorCode::ParseFailure, "Unknown ErrorCode token."});
}

}  // namespace rgsml::core
