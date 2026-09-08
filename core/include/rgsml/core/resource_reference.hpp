#pragma once

#include <rgsml/core/result.hpp>

#include <string>
#include <string_view>

namespace rgsml::core {

// Access permissions are authorization facts. They are deliberately distinct
// from resource capabilities, which describe operations an opened endpoint can
// perform.
class ResourcePermissions final {
public:
    [[nodiscard]] constexpr bool can_read() const noexcept
    {
        return canRead_;
    }

    [[nodiscard]] constexpr bool can_write() const noexcept
    {
        return canWrite_;
    }

private:
    friend class ResourceReference;

    constexpr ResourcePermissions(bool canRead, bool canWrite) noexcept
        : canRead_(canRead)
        , canWrite_(canWrite)
    {
    }

    bool canRead_;
    bool canWrite_;
};

// A stable, serializable reference to a resource owned by a provider adapter.
// Provider identifiers use the ASCII grammar [a-z][a-z0-9._-]*. The locator is
// opaque and is never normalized or interpreted by core.
class ResourceReference final {
public:
    [[nodiscard]] static Result<ResourceReference> create(
        std::string providerId,
        std::string locator,
        bool canRead,
        bool canWrite,
        std::string displayName = {});

    [[nodiscard]] const std::string& provider_id() const noexcept;
    [[nodiscard]] const std::string& locator() const noexcept;
    [[nodiscard]] ResourcePermissions permissions() const noexcept;
    [[nodiscard]] const std::string& display_name() const noexcept;

    // Resource identity is exactly provider_id + locator. Display metadata and
    // permissions do not participate in identity.
    [[nodiscard]] bool same_resource_identity(
        const ResourceReference& other) const noexcept;

private:
    ResourceReference(
        std::string providerId,
        std::string locator,
        ResourcePermissions permissions,
        std::string displayName) noexcept;

    std::string providerId_;
    std::string locator_;
    ResourcePermissions permissions_;
    std::string displayName_;
};

}  // namespace rgsml::core
