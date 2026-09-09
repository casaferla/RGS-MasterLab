#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rgsml::core {

template <typename T>
class Result;

// Stable categorical identity for foundational failures. Human-readable
// messages and details never replace this code during propagation.
enum class ErrorCode {
    InvalidArgument,
    OutOfRange,
    IntegerOverflow,
    DivisionByZero,
    ParseFailure,
    InvalidUuid,
    InvalidRational,
    InvalidFrameRange,
    ResourceNotFound,
    AccessDenied,
    IoFailure,
    UnsupportedOperation,
    InvalidState,
    UnsupportedAudioEncoding,
    UnsupportedAudioLayout,
    InvalidAudioSample,
    MalformedAudioContainer,
    TruncatedAudioData,
};

struct ErrorDetail final {
    std::string key;
    std::string value;

    [[nodiscard]] bool operator==(const ErrorDetail&) const = default;
};

class Error final {
public:
    Error() = delete;
    explicit Error(
        ErrorCode code,
        std::string message = {},
        std::vector<ErrorDetail> details = {});

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;
    [[nodiscard]] const std::vector<ErrorDetail>& details() const noexcept;

    [[nodiscard]] bool operator==(const Error&) const = default;

private:
    ErrorCode code_;
    std::string message_;
    std::vector<ErrorDetail> details_;
};

[[nodiscard]] std::string_view error_code_token(ErrorCode code) noexcept;
[[nodiscard]] Result<ErrorCode> parse_error_code(std::string_view token);

}  // namespace rgsml::core

// Result depends on the error value types above. Including it last also makes
// parse_error_code immediately usable when this is the only public header
// included; result.hpp's include guard breaks the intentional dependency cycle.
#include <rgsml/core/result.hpp>
