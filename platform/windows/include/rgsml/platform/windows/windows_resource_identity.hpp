#pragma once

#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>

namespace rgsml::platform::windows {

// Read-only, fail-closed comparison of the underlying local Windows files.
// Equal volume serial and file index identify hardlink aliases as one file.
[[nodiscard]] core::Result<bool> same_underlying_local_file(
    const core::ResourceReference& first,
    const core::ResourceReference& second);

}  // namespace rgsml::platform::windows
