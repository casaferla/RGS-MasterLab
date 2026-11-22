#pragma once
// L3 M05/M06 independently authentic native fragments -> one canonical
// M07 exact 27-slot generation. This joins ORIGINAL feature-source
// bindings; it does not derive missing Phase6 metrics, make a fake
// twenty-seven-feature dataset or expose release qualification.
#include "l3_m07_exact_27_feature_assembler.hpp"
#include "l3_m06_d01_native_binding.hpp"
#include "l3_m06_d02_lra_binding.hpp"
#include "l3_m06_d03d04_to_m07_m08.hpp"
#include "l3_m06_d07_d09_native_binding.hpp"
#include "l3_m06_d10_d12_native_binding.hpp"
#include "l3_m19_native_macro_loudness_bindings.hpp"
#include "l3_m20_native_stereo_bindings.hpp"
#include "l3_m20_tonal_scalar_bindings.hpp"
#include "l3_m20_t01_primary_binding.hpp"
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace rgsml::app::l3_automatch::m07_frozen {
struct ExistingNativeSourceFragments final {
    std::optional<m05m06_real::T01BindingProjection> tonal_primary;
    std::optional<m05m06_real::TonalBindingProjection> tonal_scalar;
    std::optional<m05m06_real::D01BindingProjection> macro_d01;
    std::optional<m05m06_real::LraBindingProjection> macro_lra;
    std::optional<m05m06_real::D03D04SlotProjection> macro_windowed;
    std::optional<m05m06_real::TransientBindingProjection> micro_events;
    std::optional<m05m06_real::SpectralSlotProjection> spectral_frame_energy;
    std::optional<m05m06_real::M19NativeSlotProjection> macro_loudness;
    std::optional<m05m06_real::M20StereoProjection> stereo_broadband;
};
enum class NativeFragmentAdmission:std::uint8_t {
    accepted_partial,accepted_complete,missing_upstream_authority,
    conflicting_or_invalid_source
};
struct NativeFragmentAdmissionResult final {
    NativeFragmentAdmission status{
        NativeFragmentAdmission::missing_upstream_authority};
    PartialFeatureInventory inventory;
    [[nodiscard]] constexpr bool authorizes_solver()
        const noexcept{return false;}
};
[[nodiscard]] inline NativeFragmentAdmissionResult
collect_existing_m05m06_native_fragments(
    ExactTwentySevenFeatureAssembler& generation,
    const ExistingNativeSourceFragments& fragments)
{
    bool invalid=false;
    // Typed append of complete individual producer projections only.
    const auto add=[&](const m08_frozen::AuthenticatedPairSlot& slot) {
        if(generation.add(slot)!=FullFeatureAssemblerReason::added)
            invalid=true;
    };
    if(fragments.tonal_primary&&fragments.tonal_primary->slot)
        add(*fragments.tonal_primary->slot);
    if(fragments.tonal_scalar&&fragments.tonal_scalar->feature_slots)
        for(const auto& s:*fragments.tonal_scalar->feature_slots)add(s);
    if(fragments.macro_d01&&fragments.macro_d01->slot)
        add(*fragments.macro_d01->slot);
    if(fragments.macro_lra&&fragments.macro_lra->slot)
        add(*fragments.macro_lra->slot);
    if(fragments.macro_windowed&&fragments.macro_windowed->feature_slots)
        for(const auto& s:*fragments.macro_windowed->feature_slots)add(s);
    if(fragments.micro_events&&fragments.micro_events->feature_slots)
        for(const auto& s:*fragments.micro_events->feature_slots)add(s);
    if(fragments.spectral_frame_energy&&
       fragments.spectral_frame_energy->feature_bindings)
        for(const auto& s:*
            fragments.spectral_frame_energy->feature_bindings)add(s);
    if(fragments.macro_loudness&&fragments.macro_loudness->feature_slots)
        for(const auto& s:*fragments.macro_loudness->feature_slots)add(s);
    if(fragments.stereo_broadband&&fragments.stereo_broadband->feature_slots)
        for(const auto& s:*fragments.stereo_broadband->feature_slots)add(s);
    const auto inventory=generation.current_inventory();
    if(invalid||inventory.entire_generation_poisoned)
        return {NativeFragmentAdmission::conflicting_or_invalid_source,
                inventory};
    if(inventory.populated_count==0U)
        return {NativeFragmentAdmission::missing_upstream_authority,
                inventory};
    if(inventory.populated_count==kBindings.size())
        return {NativeFragmentAdmission::accepted_complete,inventory};
    return {NativeFragmentAdmission::accepted_partial,inventory};
}
} // namespace rgsml::app::l3_automatch::m07_frozen
