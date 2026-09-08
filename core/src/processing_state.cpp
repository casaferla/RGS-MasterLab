#include <rgsml/core/processing_state.hpp>

namespace rgsml::core {

std::string_view processing_state_token(ProcessingState state) noexcept
{
    switch (state) {
    case ProcessingState::RAW:
        return "RAW";
    case ProcessingState::PREPARED:
        return "PREPARED";
    case ProcessingState::PROCESSED:
        return "PROCESSED";
    case ProcessingState::GOLD:
        return "GOLD";
    }
    return {};
}

Result<ProcessingState> parse_processing_state(std::string_view token)
{
    if (token == "RAW") {
        return Result<ProcessingState>::success(ProcessingState::RAW);
    }
    if (token == "PREPARED") {
        return Result<ProcessingState>::success(ProcessingState::PREPARED);
    }
    if (token == "PROCESSED") {
        return Result<ProcessingState>::success(ProcessingState::PROCESSED);
    }
    if (token == "GOLD") {
        return Result<ProcessingState>::success(ProcessingState::GOLD);
    }
    return Result<ProcessingState>::failure(
        Error{ErrorCode::ParseFailure, "Unknown ProcessingState token."});
}

}  // namespace rgsml::core
