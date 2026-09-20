#pragma once

#include <rgsml/project/project_document.hpp>
#include <rgsml/core/result.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace rgsml::project::internal {

[[nodiscard]] core::Result<ProjectDocument> parse_project_json(std::string_view bytes);
[[nodiscard]] core::Result<std::string> write_project_json(const ProjectDocument& document);
[[nodiscard]] core::Result<std::string> canonicalize_json(
    std::string_view bytes, std::size_t maxBytes);

}  // namespace rgsml::project::internal
