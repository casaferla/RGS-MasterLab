#pragma once

#include <cstdint>
#include <optional>

namespace rgsml::core {

struct RealizationId final {
    std::uint64_t value{0};

    [[nodiscard]] bool operator==(const RealizationId&) const = default;
    [[nodiscard]] auto operator<=>(const RealizationId&) const = default;
};

enum class AudibleHandoffPhase {
    UNAVAILABLE,
    OLD,
    TRANSITION,
    NEW,
};

struct AudibleRealizationState final {
    AudibleHandoffPhase phase{AudibleHandoffPhase::UNAVAILABLE};
    std::optional<RealizationId> realizationId;

    [[nodiscard]] bool operator==(const AudibleRealizationState&) const = default;
};

}  // namespace rgsml::core
