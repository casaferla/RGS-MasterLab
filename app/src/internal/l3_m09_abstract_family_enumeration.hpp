#pragma once
// L3-M09 frozen abstract ordered family stream (nonrelease).
// Enumerates the M09 semantic alternatives BEFORE any availability,
// source eligibility, M11 ownership, M10 consumer caps or audio render.
// Descriptor readiness NEVER changes this static enumeration.
#include "l3_m09_p03_stereo_low_seed.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
enum class MacroMode:std::uint8_t {none,linked_max,stereo_mean};
enum class MicroMode:std::uint8_t {none,attack,sustain,balanced};
enum class LoudnessMode:std::uint8_t {
    none,gain,gain_with_terminal_limiter,terminal_limiter_only
};
enum class IntentRole:std::uint8_t {
    tonal_eq,spectral_dynamic_eq,macro_compressor,micro_transient,
    stereo_side_eq,stereo_ms,loudness_gain,terminal_limiter
};
struct AbstractFamilyChoice final {
    std::uint8_t tonal_eq_prefix{};
    std::uint8_t spectral_dynamic_eq_prefix{};
    MacroMode macro{MacroMode::none};
    MicroMode micro{MicroMode::none};
    std::uint8_t stereo_side_eq_prefix{};
    std::uint8_t stereo_mode_index{}; // 0 = absent, 1..7 = kLowBassAlternatives[index-1]
    LoudnessMode loudness{LoudnessMode::none};
    [[nodiscard]] constexpr bool executable_or_pub_ready()
        const noexcept{return false;}
};
// The role order is separate from instance IDs, MANUAL nodes and
// the TERMINAL insertion authority. No dummy EQ zero-band payload.
[[nodiscard]] inline std::vector<IntentRole> ordered_abstract_roles(
    const AbstractFamilyChoice& c) {
    std::vector<IntentRole> roles;
    if(c.tonal_eq_prefix)roles.push_back(IntentRole::tonal_eq);
    if(c.spectral_dynamic_eq_prefix)
        roles.push_back(IntentRole::spectral_dynamic_eq);
    if(c.macro!=MacroMode::none)
        roles.push_back(IntentRole::macro_compressor);
    if(c.micro!=MicroMode::none)
        roles.push_back(IntentRole::micro_transient);
    if(c.stereo_side_eq_prefix)
        roles.push_back(IntentRole::stereo_side_eq);
    if(c.stereo_mode_index!=0U)
        roles.push_back(IntentRole::stereo_ms);
    if(c.loudness==LoudnessMode::gain ||
       c.loudness==LoudnessMode::gain_with_terminal_limiter)
        roles.push_back(IntentRole::loudness_gain);
    if(c.loudness==LoudnessMode::gain_with_terminal_limiter||
       c.loudness==LoudnessMode::terminal_limiter_only)
        roles.push_back(IntentRole::terminal_limiter);
    return roles;
}
struct FamilyStream final {
    std::vector<AbstractFamilyChoice> ordered;
    // No member asserts candidate/topology/seed checksum, M09 P04 exact
    // dependencies, M10 fully-bound proxy score, measured safety, M12.
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
[[nodiscard]] inline std::optional<FamilyStream>
enumerate_frozen_abstract_families(
    std::uint8_t n_eligible_tonal_prefixes,
    std::uint8_t n_eligible_spectral_clusters,
    std::uint8_t n_eligible_stereo_side_eq_prefixes,
    bool stereo_source)
{
    if(n_eligible_tonal_prefixes>6U||
       n_eligible_spectral_clusters>3U||
       n_eligible_stereo_side_eq_prefixes>3U||
       (!stereo_source&&n_eligible_stereo_side_eq_prefixes!=0U))
        return std::nullopt;
    const auto macro_modes=stereo_source?3U:2U;
    // Preserve both NO Stereo/M-S node and present Stereo/M-S with
    // low-bass OFF; the latter still has broadband stereo behavior.
    const auto ms_modes=stereo_source?kLowBassAlternatives.size()+1U:1U;
    FamilyStream stream;
    const std::size_t count=
        (std::size_t{1}+n_eligible_tonal_prefixes)*
        (std::size_t{1}+n_eligible_spectral_clusters)*
        macro_modes*4U*
        (std::size_t{1}+n_eligible_stereo_side_eq_prefixes)*
        ms_modes*4U;
    stream.ordered.reserve(count);
    for(std::uint8_t tonal=0;tonal<=n_eligible_tonal_prefixes;++tonal)
        for(std::uint8_t spectral=0;
            spectral<=n_eligible_spectral_clusters;++spectral)
            for(std::uint8_t macro=0;macro<macro_modes;++macro)
                for(std::uint8_t micro=0;micro<4U;++micro)
                    for(std::uint8_t side=0;
                        side<=n_eligible_stereo_side_eq_prefixes;++side)
                        for(std::size_t ms=0;ms<ms_modes;++ms)
                            for(std::uint8_t loudness=0;
                                loudness<4U;++loudness)
                                stream.ordered.push_back({
                                    tonal,spectral,
                                    static_cast<MacroMode>(macro),
                                    static_cast<MicroMode>(micro),
                                    side,static_cast<std::uint8_t>(ms),
                                    static_cast<LoudnessMode>(loudness)});
    return stream;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
