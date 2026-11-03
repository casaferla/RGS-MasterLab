#pragma once
#include <rgsml/core/result.hpp>
#include <rgsml/project/opaque_json_value.hpp>
#include <cstdint>
#include <optional>

namespace rgsml::project {
// Project/.rgsml output field schema bridge, no Qt or DSP dependencies.
// This is a saved user PREFERENCE, not proof of actual Final SRC or M17 PCM.
class MasterOutputRatePreference final {
public:
    [[nodiscard]] static bool supported(std::int64_t rate) noexcept;
    [[nodiscard]] static core::Result<std::optional<std::int64_t>>
    read(const OpaqueJsonValue& output);
    [[nodiscard]] static core::Result<OpaqueJsonValue>
    with_target(const OpaqueJsonValue& existing, std::int64_t target);
    // Only canonical known output keys count as implemented semantics;
    // any unknown field remains future/degraded authority when opened.
    [[nodiscard]] static bool only_known_fields(const OpaqueJsonValue& output);
};
} // namespace rgsml::project
