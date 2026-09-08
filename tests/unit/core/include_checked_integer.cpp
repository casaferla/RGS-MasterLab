#include <rgsml/core/checked_integer.hpp>

#include <type_traits>

static_assert(std::is_class_v<rgsml::core::Result<std::int64_t>>);
