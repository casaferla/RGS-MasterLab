#include <rgsml/core/result.hpp>

#include <type_traits>

static_assert(std::is_class_v<rgsml::core::Result<int>>);
