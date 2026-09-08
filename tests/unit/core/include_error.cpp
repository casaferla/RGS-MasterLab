#include <rgsml/core/error.hpp>

#include <type_traits>

static_assert(std::is_class_v<rgsml::core::Error>);
static_assert(sizeof(decltype(rgsml::core::parse_error_code("invalid_argument"))) > 0);
