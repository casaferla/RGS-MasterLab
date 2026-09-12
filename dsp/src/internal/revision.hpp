#pragma once

#include <rgsml/core/result.hpp>

#include <cstdint>

namespace rgsml::dsp::internal {

[[nodiscard]] rgsml::core::Result<std::uint64_t>
next_revision(std::uint64_t current);

}  // namespace rgsml::dsp::internal
