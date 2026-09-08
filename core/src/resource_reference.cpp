#include <rgsml/core/resource_reference.hpp>

#include <utility>

namespace rgsml::core {
namespace {

[[nodiscard]] constexpr bool is_lower_ascii(char value) noexcept
{
    return value >= 'a' && value <= 'z';
}

[[nodiscard]] constexpr bool is_ascii_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool is_valid_provider_id(std::string_view providerId) noexcept
{
    if (providerId.empty() || !is_lower_ascii(providerId.front())) {
        return false;
    }

    for (const char value : providerId.substr(1)) {
        if (!is_lower_ascii(value) && !is_ascii_digit(value) && value != '.'
            && value != '_' && value != '-') {
            return false;
        }
    }
    return true;
}

}  // namespace

Result<ResourceReference> ResourceReference::create(
    std::string providerId,
    std::string locator,
    bool canRead,
    bool canWrite,
    std::string displayName)
{
    if (!is_valid_provider_id(providerId)) {
        return Result<ResourceReference>::failure(
            Error{ErrorCode::InvalidArgument, "Invalid resource provider identifier."});
    }
    if (locator.empty()) {
        return Result<ResourceReference>::failure(
            Error{ErrorCode::InvalidArgument, "Resource locator must not be empty."});
    }
    if (!canRead && !canWrite) {
        return Result<ResourceReference>::failure(
            Error{ErrorCode::InvalidArgument, "At least one resource permission is required."});
    }

    return Result<ResourceReference>::success(ResourceReference{
        std::move(providerId),
        std::move(locator),
        ResourcePermissions{canRead, canWrite},
        std::move(displayName),
    });
}

ResourceReference::ResourceReference(
    std::string providerId,
    std::string locator,
    ResourcePermissions permissions,
    std::string displayName) noexcept
    : providerId_(std::move(providerId))
    , locator_(std::move(locator))
    , permissions_(permissions)
    , displayName_(std::move(displayName))
{
}

const std::string& ResourceReference::provider_id() const noexcept
{
    return providerId_;
}

const std::string& ResourceReference::locator() const noexcept
{
    return locator_;
}

ResourcePermissions ResourceReference::permissions() const noexcept
{
    return permissions_;
}

const std::string& ResourceReference::display_name() const noexcept
{
    return displayName_;
}

bool ResourceReference::same_resource_identity(const ResourceReference& other) const noexcept
{
    return providerId_ == other.providerId_ && locator_ == other.locator_;
}

}  // namespace rgsml::core
