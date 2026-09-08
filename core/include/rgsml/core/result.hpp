#pragma once

#include <rgsml/core/error.hpp>

#include <optional>
#include <utility>
#include <variant>

namespace rgsml::core {

// Exactly one value or Error is always present. Observers return pointers so a
// normal domain failure never turns into an exception at an accessor.
template <typename T>
class [[nodiscard]] Result final {
public:
    Result() = delete;

    [[nodiscard]] static Result success(T value)
    {
        return Result(ValueState{std::move(value)});
    }

    [[nodiscard]] static Result failure(Error error)
    {
        return Result(ErrorState{std::move(error)});
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return std::holds_alternative<ValueState>(state_);
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] T* value() noexcept
    {
        auto* state = std::get_if<ValueState>(&state_);
        return state == nullptr ? nullptr : &state->value;
    }

    [[nodiscard]] const T* value() const noexcept
    {
        const auto* state = std::get_if<ValueState>(&state_);
        return state == nullptr ? nullptr : &state->value;
    }

    [[nodiscard]] Error* error() noexcept
    {
        auto* state = std::get_if<ErrorState>(&state_);
        return state == nullptr ? nullptr : &state->error;
    }

    [[nodiscard]] const Error* error() const noexcept
    {
        const auto* state = std::get_if<ErrorState>(&state_);
        return state == nullptr ? nullptr : &state->error;
    }

private:
    struct ValueState final {
        T value;
    };

    struct ErrorState final {
        Error error;
    };

    explicit Result(ValueState value)
        : state_(std::move(value))
    {
    }

    explicit Result(ErrorState error)
        : state_(std::move(error))
    {
    }

    std::variant<ValueState, ErrorState> state_;
};

template <>
class [[nodiscard]] Result<void> final {
public:
    Result() = delete;

    [[nodiscard]] static Result success()
    {
        return Result(std::nullopt);
    }

    [[nodiscard]] static Result failure(Error error)
    {
        return Result(std::move(error));
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return !error_.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] Error* error() noexcept
    {
        return error_ ? &*error_ : nullptr;
    }

    [[nodiscard]] const Error* error() const noexcept
    {
        return error_ ? &*error_ : nullptr;
    }

private:
    explicit Result(std::optional<Error> error)
        : error_(std::move(error))
    {
    }

    std::optional<Error> error_;
};

using Status = Result<void>;

}  // namespace rgsml::core
