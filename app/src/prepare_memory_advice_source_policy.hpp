#pragma once
#include "final_src_candidate_policy.hpp"
#include <cstdint>

namespace rgsml::app {
// Presentation-only classification. Product Final SRC admission remains
// exclusively owned by final_src_candidate_path(), not by RAM advice.
enum class PrepareMemoryDisplayAdmission : std::uint8_t {
    SOURCE_OR_RATE_UNAVAILABLE = 0,
    CANDIDATE_ADMISSIBLE = 1,
    FINAL_SRC_UNQUALIFIED = 2
};

[[nodiscard]] constexpr PrepareMemoryDisplayAdmission
prepare_memory_display_admission(std::int64_t source_hz,
                                  std::int64_t master_hz) noexcept
{
    switch (final_src_candidate_path(source_hz,master_hz)) {
    case FinalSrcCandidatePath::IDENTITY:
    case FinalSrcCandidatePath::EXISTING_44K_48K_FIR:
        return PrepareMemoryDisplayAdmission::CANDIDATE_ADMISSIBLE;
    case FinalSrcCandidatePath::EXPERIMENTAL_UNQUALIFIED:
        return PrepareMemoryDisplayAdmission::FINAL_SRC_UNQUALIFIED;
    default:
        return PrepareMemoryDisplayAdmission::SOURCE_OR_RATE_UNAVAILABLE;
    }
}
} // namespace rgsml::app
