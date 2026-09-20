#pragma once

#include <rgsml/core/resource_io.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rgsml::project::internal {

struct RawEntry final {
    std::string path;
    std::uint64_t compressedSize{0};
    std::uint64_t uncompressedSize{0};
};

[[nodiscard]] core::Result<std::vector<RawEntry>> validate_raw_zip(
    core::IResourceReader& reader);
[[nodiscard]] bool safe_package_path(std::string_view path);

}  // namespace rgsml::project::internal
