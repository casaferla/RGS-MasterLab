#pragma once

#include <rgsml/core/resource_io.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace rgsml::platform::windows {

// Read-only local-file adapter. Qt and native handles are intentionally hidden
// from this public boundary.
class WindowsResourceReader final : public core::IResourceReader {
public:
    [[nodiscard]] static constexpr std::string_view provider_id() noexcept
    {
        return "rgsml.windows.local-file";
    }

    [[nodiscard]] static core::Result<core::ResourceReference> make_read_reference(
        std::string_view absoluteLocalPathUtf8,
        std::string_view displayNameUtf8);

    [[nodiscard]] static core::Result<std::unique_ptr<WindowsResourceReader>>
    open_read_only(core::ResourceReference reference);

    WindowsResourceReader(const WindowsResourceReader&) = delete;
    WindowsResourceReader& operator=(const WindowsResourceReader&) = delete;
    WindowsResourceReader(WindowsResourceReader&&) = delete;
    WindowsResourceReader& operator=(WindowsResourceReader&&) = delete;
    ~WindowsResourceReader() noexcept override;

    [[nodiscard]] const core::ResourceReference& reference() const noexcept override;
    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override;
    [[nodiscard]] core::Result<std::uint64_t> size_bytes() const override;
    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override;
    [[nodiscard]] core::Result<std::size_t> read(
        std::span<std::byte> destination) override;
    [[nodiscard]] core::Status seek_bytes(std::uint64_t absoluteOffset) override;
    [[nodiscard]] core::Status close() override;

private:
    struct Impl;

    WindowsResourceReader(
        core::ResourceReference reference,
        std::unique_ptr<Impl> implementation) noexcept;

    core::ResourceReference reference_;
    std::unique_ptr<Impl> implementation_;
    bool closed_{false};
};

}  // namespace rgsml::platform::windows
