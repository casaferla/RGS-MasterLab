#pragma once

#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace rgsml::core {

enum class ResourceCapability {
    CanSeek,
    HasKnownSize,
    CanResize,
    CanFlush,
};

// Capabilities describe the opened endpoint, not the provider in general.
// CanResize and CanFlush apply to writer endpoints. The named enum values, not
// their underlying ordinals, define the runtime contract.
class ResourceCapabilities final {
public:
    [[nodiscard]] static constexpr ResourceCapabilities create(
        bool canSeek,
        bool hasKnownSize,
        bool canResize,
        bool canFlush) noexcept
    {
        return ResourceCapabilities{canSeek, hasKnownSize, canResize, canFlush};
    }

    [[nodiscard]] constexpr bool supports(ResourceCapability capability) const noexcept
    {
        switch (capability) {
        case ResourceCapability::CanSeek:
            return canSeek_;
        case ResourceCapability::HasKnownSize:
            return hasKnownSize_;
        case ResourceCapability::CanResize:
            return canResize_;
        case ResourceCapability::CanFlush:
            return canFlush_;
        }
        return false;
    }

private:
    constexpr ResourceCapabilities(
        bool canSeek,
        bool hasKnownSize,
        bool canResize,
        bool canFlush) noexcept
        : canSeek_(canSeek)
        , hasKnownSize_(hasKnownSize)
        , canResize_(canResize)
        , canFlush_(canFlush)
    {
    }

    bool canSeek_;
    bool hasKnownSize_;
    bool canResize_;
    bool canFlush_;
};

// Reader and writer ports own no platform policy. They are blocking,
// sequential-first, non-real-time streams intended for serialized use by one
// owner unless an adapter explicitly guarantees more. Empty transfers succeed
// with zero bytes and do not move the cursor. Short transfers are successful
// and report their exact byte count. EOF is a successful zero-byte read. If a
// read transfers bytes before a later failure, that call reports the partial
// success and the failure is reported by a subsequent call. Failed seek/resize
// operations preserve observable state. close() is idempotent; fallible
// operations after close report InvalidState.
class IResourceReader {
public:
    virtual ~IResourceReader() noexcept = default;

    [[nodiscard]] virtual const ResourceReference& reference() const noexcept = 0;
    [[nodiscard]] virtual ResourceCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual Result<std::uint64_t> size_bytes() const = 0;
    [[nodiscard]] virtual Result<std::uint64_t> position_bytes() const = 0;
    [[nodiscard]] virtual Result<std::size_t> read(std::span<std::byte> destination) = 0;
    [[nodiscard]] virtual Status seek_bytes(std::uint64_t absoluteOffset) = 0;
    [[nodiscard]] virtual Status close() = 0;
};

// A successful flush only hands available data to the next layer; neither
// flush, close, nor destruction promises durability, atomic replacement,
// validation, publication, or transaction commit.
class IResourceWriter {
public:
    virtual ~IResourceWriter() noexcept = default;

    [[nodiscard]] virtual const ResourceReference& reference() const noexcept = 0;
    [[nodiscard]] virtual ResourceCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual Result<std::uint64_t> position_bytes() const = 0;
    [[nodiscard]] virtual Result<std::size_t> write(
        std::span<const std::byte> source) = 0;
    [[nodiscard]] virtual Status seek_bytes(std::uint64_t absoluteOffset) = 0;
    [[nodiscard]] virtual Status resize_bytes(std::uint64_t sizeBytes) = 0;
    [[nodiscard]] virtual Status flush() = 0;
    [[nodiscard]] virtual Status close() = 0;
};

}  // namespace rgsml::core
