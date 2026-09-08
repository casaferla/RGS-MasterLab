#include <rgsml/core/strong_id.hpp>

#include <type_traits>

namespace {
struct HeaderTag final {
};
}

static_assert(std::is_class_v<rgsml::core::StrongId<HeaderTag>>);
