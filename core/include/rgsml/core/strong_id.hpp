#pragma once

#include <rgsml/core/uuid.hpp>

#include <compare>
#include <string>
#include <utility>

namespace rgsml::core {

// Tag exists only at compile time; persisted identity is always the underlying
// UUID bytes/text. Nil represents absence and is rejected by this factory.
template <typename Tag>
class StrongId final {
public:
    StrongId() = delete;

    [[nodiscard]] static Result<StrongId> from_uuid(Uuid uuid)
    {
        if (uuid.is_nil()) {
            return Result<StrongId>::failure(
                Error{ErrorCode::InvalidUuid, "A typed ID cannot contain the nil UUID."});
        }
        return Result<StrongId>::success(StrongId(std::move(uuid)));
    }

    [[nodiscard]] const Uuid& uuid() const noexcept
    {
        return uuid_;
    }

    [[nodiscard]] std::string to_string() const
    {
        return uuid_.to_string();
    }

    [[nodiscard]] bool operator==(const StrongId&) const = default;
    [[nodiscard]] auto operator<=>(const StrongId&) const = default;

private:
    explicit StrongId(Uuid uuid) noexcept
        : uuid_(std::move(uuid))
    {
    }

    Uuid uuid_;
};

}  // namespace rgsml::core
