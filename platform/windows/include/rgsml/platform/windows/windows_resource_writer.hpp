#pragma once

#include <rgsml/core/resource_io.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace rgsml::platform::windows {

// Create-new local-file adapter for the frozen core writer port. Publication,
// validation, and replacement policy belong to a higher-level transaction.
class WindowsResourceWriter final : public core::IResourceWriter {
public:
    [[nodiscard]] static constexpr std::string_view provider_id() noexcept
    {
        return "rgsml.windows.local-file";
    }

    [[nodiscard]] static core::Result<core::ResourceReference> make_write_reference(
        std::string_view absoluteLocalPathUtf8,
        std::string_view displayNameUtf8);

    [[nodiscard]] static core::Result<std::unique_ptr<WindowsResourceWriter>>
    open_create_new(core::ResourceReference reference);

    WindowsResourceWriter(const WindowsResourceWriter&) = delete;
    WindowsResourceWriter& operator=(const WindowsResourceWriter&) = delete;
    WindowsResourceWriter(WindowsResourceWriter&&) = delete;
    WindowsResourceWriter& operator=(WindowsResourceWriter&&) = delete;
    ~WindowsResourceWriter() noexcept override;

    [[nodiscard]] const core::ResourceReference& reference() const noexcept override;
    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override;
    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override;
    [[nodiscard]] core::Result<std::size_t> write(
        std::span<const std::byte> source) override;
    [[nodiscard]] core::Status seek_bytes(std::uint64_t absoluteOffset) override;
    [[nodiscard]] core::Status resize_bytes(std::uint64_t sizeBytes) override;
    [[nodiscard]] core::Status flush() override;
    [[nodiscard]] core::Status close() override;

private:
    struct Impl;

    WindowsResourceWriter(
        core::ResourceReference reference,
        std::unique_ptr<Impl> implementation) noexcept;

    core::ResourceReference reference_;
    std::unique_ptr<Impl> implementation_;
    bool closed_{false};
};

}  // namespace rgsml::platform::windows
