#pragma once

#include <rgsml/core/resource_io.hpp>
#include <rgsml/project/project_snapshot.hpp>

#include <functional>
#include <memory>

namespace rgsml::project {

class ProjectRepository final {
public:
    using ReopenReader = std::function<core::Result<std::unique_ptr<core::IResourceReader>>() >;

    [[nodiscard]] static core::Result<ProjectSnapshot> open(
        std::unique_ptr<core::IResourceReader> reader);
    [[nodiscard]] static core::Status save_create_new(
        const ProjectSnapshot& snapshot,
        std::unique_ptr<core::IResourceWriter> writer,
        ReopenReader reopen);
};

}  // namespace rgsml::project
