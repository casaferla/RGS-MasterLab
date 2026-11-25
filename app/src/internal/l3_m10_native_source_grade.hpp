#pragma once
#include "l3_m05m06_real_analysis_port.hpp"

namespace rgsml::app::l3_automatch::m05m06_real {
// Phase6 section 12 reference-grade duration gates. The native feature
// binders still enforce descriptor-specific measured support, valid
// counts, energy floors, endpoint evidence and current source ownership.
struct NativeDurationGrade final {
    bool integrated_and_macro{};
    bool spectrum{};
    bool stereo{};
    bool transient{};
};
[[nodiscard]] inline std::optional<NativeDurationGrade>
native_full_source_duration_grade(const SourceReadings& source,std::uint64_t active_frames)
{
    if(!source.owner.full_track_owner_confirmed||source.owner.resource_identity.empty()||
       !source.owner.generation||source.sample_rate_hz<=0||source.scope_frame_count<=0||
       active_frames>static_cast<std::uint64_t>(source.scope_frame_count)||
       source.channels<1U||source.channels>2U)return {};
    const auto rate=static_cast<std::uint64_t>(source.sample_rate_hz);
    // Integer division avoids duration rounding and multiplication overflow.
    const auto seconds=active_frames/rate;
    return NativeDurationGrade{seconds>=10U,seconds>=2U,
        source.channels==2U&&seconds>=1U,seconds>=5U};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
