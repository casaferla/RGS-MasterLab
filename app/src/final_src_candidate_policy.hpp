#pragma once
#include <cstdint>
namespace rgsml::app {
// Product M17 *candidate admission*, NOT Phase-11 Final SRC release qualification.
enum class FinalSrcCandidatePath : std::uint8_t {
    UNAVAILABLE=0, IDENTITY=1, EXISTING_44K_48K_FIR=2,
    EXPERIMENTAL_UNQUALIFIED=3
};
[[nodiscard]] constexpr bool m17_six_rate(std::int64_t hz) noexcept {
    return hz==44100||hz==48000||hz==88200||hz==96000||hz==176400||hz==192000;
}
[[nodiscard]] constexpr FinalSrcCandidatePath final_src_candidate_path(
    std::int64_t source,std::int64_t target) noexcept {
    if(!m17_six_rate(source)||!m17_six_rate(target))
        return FinalSrcCandidatePath::UNAVAILABLE;
    if(source==target)return FinalSrcCandidatePath::IDENTITY;
    if((source==44100&&target==48000)||(source==48000&&target==44100))
        return FinalSrcCandidatePath::EXISTING_44K_48K_FIR;
    return FinalSrcCandidatePath::EXPERIMENTAL_UNQUALIFIED;
}
[[nodiscard]] constexpr bool m17_final_src_candidate_admissible(
    std::int64_t source,std::int64_t target) noexcept {
    const auto path=final_src_candidate_path(source,target);
    return path==FinalSrcCandidatePath::IDENTITY
        ||path==FinalSrcCandidatePath::EXISTING_44K_48K_FIR;
}
} // namespace rgsml::app
