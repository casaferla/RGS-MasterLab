#pragma once
#include "l3_m09_native_role_graph.hpp"
#include "l3_m09_native_tonal_seed.hpp"
#include "l3_m09_native_side_seed.hpp"
#include "l3_m09_native_dynamic_seed.hpp"
#include "l3_m09_native_macro_seed.hpp"
#include "l3_m09_native_micro_seed.hpp"
#include "l3_m09_native_stereo_seed.hpp"
#include "l3_m09_native_loudness_seed.hpp"
namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeOwnedP2Seed final {
    dsp::StreamingTruePeakLimiterParameters payload;
    NativeP2CoordinatePolicy policy;
};
[[nodiscard]] inline bool valid_native_owned_p2_seed(const NativeOwnedP2Seed& seed)
{
    if(!valid_native_p2_coordinate_policy(seed.policy))return false;
    const auto& p=seed.payload;
    return p.ceiling_db_tp()>=seed.policy.ceiling.lower&&p.ceiling_db_tp()<=seed.policy.ceiling.upper&&
        p.drive_db()>=seed.policy.drive.lower&&p.drive_db()<=seed.policy.drive.upper;
}
struct NativeFamilySeedBank final {
    std::vector<NativeEqPrefix> tonal;
    std::vector<NativeDynamicPrefix> spectral;
    std::array<std::optional<NativeCompressorSeed>,2> macro;
    std::array<std::optional<dsp::TransientShaperParameters>,3> micro;
    std::vector<NativeEqPrefix> side;
    std::array<std::optional<NativeStereoSeed>,7> stereo;
    std::optional<NativeLoudnessGainSeed> gain;
    std::vector<m05m06_real::T01ConfiguredBand> exact_tonal_grid;
    std::vector<m05m06_real::T01ConfiguredBand> exact_side_grid;
    std::optional<NativeOwnedP2Seed> owned_p2{};
};
struct NativeRoleAllocation final {
    IntentRole role;
    dsp::ModuleInstanceId instance;
    core::Uuid semantic;
};
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
native_family_role_payload(const NativeFamilySeedBank& bank,
    const AbstractFamilyChoice& family,IntentRole role)
{
    switch(role) {
    case IntentRole::tonal_eq:
        if(family.tonal_eq_prefix&&family.tonal_eq_prefix<=bank.tonal.size())
            return bank.tonal[family.tonal_eq_prefix-1U].payload;
        break;
    case IntentRole::spectral_dynamic_eq:
        if(family.spectral_dynamic_eq_prefix&&family.spectral_dynamic_eq_prefix<=bank.spectral.size())
            return bank.spectral[family.spectral_dynamic_eq_prefix-1U].payload;
        break;
    case IntentRole::macro_compressor: {
        const auto i=static_cast<std::size_t>(family.macro);
        if(i&&i<=bank.macro.size()&&bank.macro[i-1U])return bank.macro[i-1U]->payload;
        break;
    }
    case IntentRole::micro_transient: {
        const auto i=static_cast<std::size_t>(family.micro);
        if(i&&i<=bank.micro.size()&&bank.micro[i-1U])return *bank.micro[i-1U];
        break;
    }
    case IntentRole::stereo_side_eq:
        if(family.stereo_side_eq_prefix&&family.stereo_side_eq_prefix<=bank.side.size())
            return bank.side[family.stereo_side_eq_prefix-1U].payload;
        break;
    case IntentRole::stereo_ms:
        // No independent Fixed-only low-lane estimate can impersonate
        // the insertion baseline after a selected SIDE EQ.
        if(!family.stereo_side_eq_prefix&&family.stereo_mode_index&&
           family.stereo_mode_index<=bank.stereo.size()&&bank.stereo[family.stereo_mode_index-1U])
            return bank.stereo[family.stereo_mode_index-1U]->payload;
        break;
    case IntentRole::loudness_gain:
        if(bank.gain)return bank.gain->payload;
        break;
    case IntentRole::terminal_limiter:
        if(bank.owned_p2&&valid_native_owned_p2_seed(*bank.owned_p2)&&
           (family.loudness==LoudnessMode::gain_with_terminal_limiter||
            family.loudness==LoudnessMode::terminal_limiter_only))return bank.owned_p2->payload;
        break;
    }
    return {};
}
[[nodiscard]] inline std::optional<NativeTypedSeed> build_native_family_inventory_seed(
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const NativeFamilySeedBank& bank,const AbstractFamilyChoice& family,
    std::span<const NativeRoleAllocation> allocations,
    std::span<const NativeRoleCoordinateRequest> coordinates,
    std::uint64_t revision,std::int64_t rate)
{
    const auto ordered=ordered_abstract_roles(family);
    if(ordered.size()!=allocations.size())return {};
    for(const auto& coordinate:coordinates) {
        if(coordinate.role==IntentRole::terminal_limiter) {
            if(!bank.owned_p2||!coordinate.p2_policy||
               *coordinate.p2_policy!=bank.owned_p2->policy)return {};
        }else if(coordinate.p2_policy)return {};
    }
    std::vector<NativeRoleSeed> seeds;
    for(std::size_t i=0;i<ordered.size();++i) {
        if(allocations[i].role!=ordered[i])return {};
        auto payload=native_family_role_payload(bank,family,ordered[i]);
        if(!payload)return {};
        seeds.push_back({ordered[i],allocations[i].instance,allocations[i].semantic,std::move(*payload)});
    }
    return assemble_native_artistic_inventory_seed(std::move(registry),family,seeds,coordinates,revision,rate);
}
[[nodiscard]] inline std::optional<NativeTypedSeed> build_native_family_seed(
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const NativeFamilySeedBank& bank,const AbstractFamilyChoice& family,
    std::span<const NativeRoleAllocation> allocations,
    std::span<const NativeRoleCoordinateRequest> coordinates,
    std::uint64_t revision,std::int64_t rate)
{
    if(coordinates.size()>m10_frozen::kMaxActiveCoordinates)return {};
    auto seed=build_native_family_inventory_seed(std::move(registry),bank,family,allocations,
        coordinates,revision,rate);
    if(!seed||!native_seed_inventory(*seed))return {};
    return seed;
}
// Prepare reusable role alternatives once, not once per combined
// family. SIDE EQ uses the separately supplied selected S03 canonical
// grid and exact eligible dictionary; absent evidence produces no prefix.
[[nodiscard]] inline NativeFamilySeedBank derive_native_family_seed_bank(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const m05m06_real::T01ConfiguredBand> tonal_grid,
    std::span<const NativeEqAtom> tonal_dictionary,
    std::span<const core::Uuid> dynamic_band_ids,
    core::SampleRate rate,std::uint32_t channels,double ceiling_db_tp,
    bool exact_event_context,bool exact_stereo_fixed_insertion,
    std::span<const m05m06_real::T01ConfiguredBand> side_grid={},
    int side_grid_denominator=12,
    std::span<const NativeEqAtom> side_dictionary={})
{
    NativeFamilySeedBank bank;
    if(channels<1U||channels>2U)return bank;
    bank.exact_tonal_grid.assign(tonal_grid.begin(),tonal_grid.end());
    bank.tonal=derive_native_tonal_prefixes(fixed,amount,requested,
        tonal_grid,tonal_dictionary,rate);
    if(fixed.measured_fixed_spectral_energy&&
       fixed.measured_fixed_spectral_energy->sample_rate==static_cast<std::uint32_t>(rate.value()))
        bank.spectral=derive_native_dynamic_prefixes(fixed,amount,requested,dynamic_band_ids,channels);
    bank.macro[0]=derive_native_macro_seed(fixed,amount,requested,MacroMode::linked_max,channels==2U);
    if(channels==2U)
        bank.macro[1]=derive_native_macro_seed(fixed,amount,requested,MacroMode::stereo_mean,true);
    const auto micro=derive_native_micro_seed(fixed,amount,requested,exact_event_context);
    for(std::size_t i=0;i<bank.micro.size();++i)
        bank.micro[i]=native_micro_variant_payload(micro,static_cast<MicroMode>(i+1U));
    if(channels==2U) {
        bank.exact_side_grid.assign(side_grid.begin(),side_grid.end());
        bank.side=derive_native_side_prefixes(fixed,amount,requested,side_grid,
            side_grid_denominator,side_dictionary,rate);
        for(std::size_t i=0;i<bank.stereo.size();++i)
            bank.stereo[i]=derive_native_stereo_seed(fixed,amount,requested,i+1U,true,exact_stereo_fixed_insertion);
    }
    bank.gain=derive_native_loudness_gain_seed(fixed,amount,requested,ceiling_db_tp);
    return bank;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
