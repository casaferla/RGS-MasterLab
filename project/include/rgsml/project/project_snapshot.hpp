#pragma once

#include <rgsml/project/project_document.hpp>

#include <rgsml/core/result.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::project {

struct OptionalEntry final {
    std::string path;
    std::string mediaType{"application/octet-stream"};
    std::vector<std::byte> bytes;
};

class ProjectSnapshot final {
public:
    [[nodiscard]] static core::Result<ProjectSnapshot> create(
        ProjectDocument document, std::vector<OptionalEntry> optionalEntries = {});

    [[nodiscard]] const ProjectDocument& document() const noexcept { return document_; }
    [[nodiscard]] const std::vector<OptionalEntry>& optional_entries() const noexcept
    {
        return optionalEntries_;
    }

private:
    ProjectSnapshot(ProjectDocument document, std::vector<OptionalEntry> entries)
        : document_(std::move(document)), optionalEntries_(std::move(entries)) {}

    ProjectDocument document_;
    std::vector<OptionalEntry> optionalEntries_;
};

}  // namespace rgsml::project
