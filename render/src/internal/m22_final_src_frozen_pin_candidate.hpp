// M22 nonrelease bridge from exact rate ratio to the 14 REAL frozen
// Phase-11 original Q0 FIR asset identities. Resource bytes are verified
// by the existing build/asset gate, NOT by this metadata-only adapter.
// Q1-Q6, Product M17 admission and final release remain entirely separate.
#pragma once
#include "m22_checked_output_clock_candidate.hpp"
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_geometry.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::m22_private {

enum class FrozenFirFailure : std::uint8_t {
    NONE, UNEXPECTED_ASSET_ON_BYPASS, UNKNOWN_FROZEN_RATIO,
    WRONG_FROZEN_ASSET_NAME, MISSING_FROZEN_SHA_OR_GEOMETRY
};
struct FrozenFirSelection final {
    bool identity_bypass{false};
    std::uint32_t L{1};
    std::uint32_t M{1};
    std::string pinned_asset_name;
    std::string pinned_sha256;
    std::uint32_t frozen_taps_per_phase{};
    std::uint32_t frozen_total_taps{};
    // Q0 identity does NOT establish Q1–Q6 or physical/Phase-6 release.
    [[nodiscard]] bool authorizes_release() const noexcept {return false;}
};

[[nodiscard]] inline std::optional<FrozenFirSelection>
bind_frozen_original_q0_identity_candidate(
    const CheckedOutputClockCandidate& clock,FrozenFirFailure& failure)
{
    failure=FrozenFirFailure::NONE;
    const auto rate=clock.reduced_rate_ratio();
    const auto& d=clock.declaration();
    if(!clock.uses_final_src()) {
        if(!d.selected_final_src_asset_id.empty()) {
            failure=FrozenFirFailure::UNEXPECTED_ASSET_ON_BYPASS;
            return std::nullopt;
        }
        return FrozenFirSelection{true,1,1,{},{},0,0};
    }
    const auto* pin=
        rgsml::audio::internal::phase11_frozen_original_fir_pin(
            rate.first,rate.second);
    const auto* shape=
        rgsml::audio::internal::phase11_frozen_shape(
            rate.first,rate.second);
    if(!pin||!shape) {
        failure=FrozenFirFailure::UNKNOWN_FROZEN_RATIO;
        return std::nullopt;
    }
    if(!pin->sha256 || !pin->sha256[0] || shape->N==0
       ||shape->T==0 ||shape->L!=rate.first ||shape->M!=rate.second) {
        failure=FrozenFirFailure::MISSING_FROZEN_SHA_OR_GEOMETRY;
        return std::nullopt;
    }
    const auto expectedName=std::string("RGSMSR1_L")
        +std::to_string(rate.first)+"_M"
        +std::to_string(rate.second)+".bin";
    if(d.selected_final_src_asset_id!=expectedName) {
        failure=FrozenFirFailure::WRONG_FROZEN_ASSET_NAME;
        return std::nullopt;
    }
    return FrozenFirSelection{
        false,rate.first,rate.second,expectedName,
        pin->sha256,shape->T,shape->N};
}
} // namespace rgsml::m22_private
