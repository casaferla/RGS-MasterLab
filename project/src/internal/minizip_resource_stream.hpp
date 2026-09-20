#pragma once

#include <rgsml/core/resource_io.hpp>

extern "C" {
#include <mz_strm.h>
}

#include <cstdint>

namespace rgsml::project::internal {

// Borrowed endpoint is owned by ProjectRepository for the full ZIP lifetime.
// This bridge never interprets provider locators or opens an OS path.
struct ResourceStream final {
    mz_stream stream{};
    core::IResourceReader* reader{nullptr};
    core::IResourceWriter* writer{nullptr};
    bool open{true};
    std::int32_t lastError{0};

    [[nodiscard]] static ResourceStream reading(core::IResourceReader& endpoint);
    [[nodiscard]] static ResourceStream writing(core::IResourceWriter& endpoint);
};

}  // namespace rgsml::project::internal
