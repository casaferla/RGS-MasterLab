#pragma once
#include <cstdint>
namespace rgsml::app {
// Product M17 *candidate admission*, NOT Phase-11 Final SRC release qualification.
enum class FinalSrcCandidatePath : std::uint8_t {
    UNAVAILABLE=0, IDENTITY=1, PINNED_FIR_PENDING_PRODUCT_FQ=2
};
[[nodiscard]] constexpr bool m17_six_rate(std::int64_t hz) noexcept {
    return hz==44100||hz==48000||hz==88200||hz==96000||hz==176400||hz==192000;
}
[[nodiscard]] constexpr FinalSrcCandidatePath final_src_candidate_path(
    std::int64_t source,std::int64_t target) noexcept {
    if(!m17_six_rate(source)||!m17_six_rate(target))
        return FinalSrcCandidatePath::UNAVAILABLE;
    if(source==target)return FinalSrcCandidatePath::IDENTITY;
    return FinalSrcCandidatePath::PINNED_FIR_PENDING_PRODUCT_FQ;
}
[[nodiscard]] constexpr bool m17_final_src_candidate_admissible(
    std::int64_t source,std::int64_t target,
    bool authenticated_full_original_q0_pack=false) noexcept {
    const auto path=final_src_candidate_path(source,target);
    return path==FinalSrcCandidatePath::IDENTITY
        ||(path==FinalSrcCandidatePath::PINNED_FIR_PENDING_PRODUCT_FQ
            &&authenticated_full_original_q0_pack);
}
} // namespace rgsml::app
