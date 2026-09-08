#pragma once

#include <rgsml/core/result.hpp>

#include <string_view>

namespace rgsml::core {

enum class ProcessingState {
    RAW,
    PREPARED,
    PROCESSED,
    GOLD
};

[[nodiscard]] std::string_view processing_state_token(ProcessingState state) noexcept;
[[nodiscard]] Result<ProcessingState> parse_processing_state(std::string_view token);

}  // namespace rgsml::core
