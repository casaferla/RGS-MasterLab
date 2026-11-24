#pragma once
#include "l3_m09_native_eq_dictionary.hpp"
#include "l3_m09_native_family_seed_bank.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
// No caller-provided arbitrary filter inventory. The owner supplies
// allocation authority and the selected source grids; M09 supplies
// eligible dictionary identities and actual analytical values.
template<class AllocateBand>
[[nodiscard]] inline std::optional<NativeFamilySeedBank>
derive_native_automatic_seed_bank(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const m05m06_real::T01ConfiguredBand> tonal_grid,
    std::span<const m05m06_real::T01ConfiguredBand> side_grid,int side_denominator,
    double common_low,double common_high,core::SampleRate rate,std::uint32_t channels,
    double ceiling_db_tp,bool exact_event_context,bool exact_stereo_fixed_insertion,
    std::span<const core::Uuid> dynamic_band_ids,AllocateBand&& allocate)
{
    if(channels<1U||channels>2U||!std::isfinite(common_low)||!std::isfinite(common_high)||
       common_low<20.0||common_high<=common_low||
       common_high>std::min(20000.0,0.45*static_cast<double>(rate.value())))return {};
    std::vector<NativeEqAtom> tonal_dictionary,side_dictionary;
    if(const auto pop=native_tonal_fit_population(fixed,amount,requested,tonal_grid)) {
        auto dictionary=derive_native_eq_dictionary(tonal_grid,*pop,common_low,common_high,
            rate,false,[&](const auto& id){return std::invoke(allocate,IntentRole::tonal_eq,id);});
        if(!dictionary)return {};
        tonal_dictionary=std::move(*dictionary);
    }
    if(channels==2U) {
        if(const auto pop=native_side_fit_population(fixed,amount,requested,side_grid,side_denominator)) {
            auto dictionary=derive_native_eq_dictionary(side_grid,*pop,common_low,common_high,
                rate,true,[&](const auto& id){return std::invoke(allocate,IntentRole::stereo_side_eq,id);});
            if(!dictionary)return {};
            side_dictionary=std::move(*dictionary);
        }
    }
    return derive_native_family_seed_bank(fixed,amount,requested,tonal_grid,tonal_dictionary,
        dynamic_band_ids,rate,channels,ceiling_db_tp,exact_event_context,
        exact_stereo_fixed_insertion,side_grid,side_denominator,side_dictionary);
}
} // namespace rgsml::app::l3_automatch::m09_frozen
