#pragma once
#include "l3_m09_native_family_exact_content.hpp"
#include "l3_m09_pre_render_proxy.hpp"
#include "l3_m09_native_family_inventory.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeComponentCoverage final {
    std::string exact_binding_id;
    std::string exact_feature_dependency;
    std::vector<bool> covered;
};
// Coverage is an analytical capability statement, not an observed
// candidate value. Keep every configured component and its original
// support, including covered residual mass. Never mark covered as
// protected: that would change the M10 residual denominator.
[[nodiscard]] inline std::optional<double> native_covered_target_loss(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    std::span<const NativeComponentCoverage> coverage)
{
    const auto assessed=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    if(!assessed.valid_current||coverage.size()!=fixed.measured_fixed.size())return {};
    auto pop=m08_frozen::build_targeted_population(amount,fixed.measured_fixed,assessed.corrective);
    const auto guards=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!pop.value||!guards)return {};
    std::size_t offset=0;
    std::array<std::size_t,4> domain_offsets{};
    for(std::size_t i=0;i<coverage.size();++i) {
        const auto& c=coverage[i];const auto& slot=fixed.measured_fixed[i];
        const auto d=static_cast<std::size_t>(m07_frozen::kBindings[i].domain);
        if(c.exact_binding_id!=slot.binding_id||c.exact_feature_dependency!=slot.exact_feature_dependency||
           c.covered.size()!=slot.configured.size())return {};
        for(std::size_t k=0;k<c.covered.size();++k) {
            const auto& guard=(*guards)[offset++];
            auto& objective=pop.value->objective_input[d].configured[domain_offsets[d]++];
            objective.protected_from_correction=guard.protected_by_m11_mask;
            if(!c.covered[k])continue;
            if(!assessed.corrective[d]||!guard.required_in_frozen_k||
               !guard.exact_current||guard.protected_by_m11_mask||
               !objective.optimization_target_z)return {};
            objective.candidate_z=objective.optimization_target_z;
        }
    }
    if(offset!=guards->size())return {};
    const auto loss=m10_frozen::compute_frozen_target_cost(pop.value->objective_input);
    if(!loss.valid_measured())return {};
    return loss.j_target;
}
[[nodiscard]] inline PreRenderTopologyProxy native_covered_proxy(
    const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    const NativeTypedSeed& typed,std::span<const NativeComponentCoverage> coverage,
    const ExactBytes& topology)
{
    ProxyOperands operands;
    operands.canonical_unsigned_topology_key=topology;
    const auto inventory=native_seed_inventory(typed);
    const auto complexity=native_artistic_complexity(typed.seed);
    if(!inventory||!complexity)return make_pre_render_proxy(operands);
    operands.active_owned_modules=complexity->modules;
    operands.enabled_artistic_bands=complexity->bands;
    std::vector<m10_frozen::ActiveReferenceCoordinate> coordinates;
    for(const auto& c:typed.coordinates)
        coordinates.push_back({c.canonical.normalized_u,c.immutable_reference_u,c.reference_weight});
    operands.frozen_covered_target_loss=native_covered_target_loss(fixed,amount,requested,coverage);
    operands.m10_seed_normalized_parameter_displacement=m10_frozen::normalized_reference_distance(coordinates);
    operands.m10_topology_complexity=m10_frozen::complexity_cost(complexity->modules,complexity->bands);
    operands.exact_m05_m08_and_m10_operands_attested=true;
    return make_pre_render_proxy(operands);
}
// The first complete capability adapter covers only L01 with a
// directionally compatible uniform Gain. Neither L02 nor L03 is
// inferred from this knob; other roles need their own evidence map.
[[nodiscard]] inline std::optional<std::vector<NativeComponentCoverage>>
native_gain_coverage(const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    const NativeTypedSeed& typed)
{
    const auto assessment=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    const auto guards=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!assessment.valid_current||!guards||!native_seed_inventory(typed))return {};
    const auto nodes=typed.seed.linked.instances();
    const dsp::GainParameters* gain=nullptr;
    for(std::size_t i=0;i<nodes.size();++i) {
        if(const auto* p=std::get_if<dsp::GainParameters>(&typed.seed.bindings[i].parameters)) {
            if(gain||!nodes[i].active())return {};
            gain=p;
        } else return {}; // only the NONE / Gain topology is attested here
    }
    std::vector<NativeComponentCoverage> out;
    std::size_t offset=0;
    for(const auto& slot:fixed.measured_fixed) {
        NativeComponentCoverage c{slot.binding_id,slot.exact_feature_dependency,
            std::vector<bool>(slot.configured.size(),false)};
        if(gain&&assessment.corrective[3]&&slot.binding_id=="rgsml.reference-match.binding.loudness.l01") {
            if(slot.configured.size()!=1U)return {};
            const auto& p=slot.configured.front();
            const auto& guard=(*guards)[offset];
            if(guard.required_in_frozen_k&&!guard.protected_by_m11_mask&&guard.exact_current) {
                const auto target=m08_frozen::desired_for_component(amount,3,p.prepared,p.reference);
                if(!target.value||!p.current_candidate_z)return {};
                const double delta=target.value->constraint_adjusted_z.value_or(target.value->desired_z)-*p.current_candidate_z;
                c.covered[0]=(delta>0.0&&gain->gain_db()>0.0)||(delta<0.0&&gain->gain_db()<0.0);
            }
        }
        offset+=slot.configured.size();out.push_back(std::move(c));
    }
    return out;
}
// Analytical EQ coverage is tied to an actual selected comparison cell
// and its cumulative static response, never to nearest frequency.
[[nodiscard]] inline std::optional<double> exact_prefix_cell_response(
    const NativeEqPrefix& prefix,std::span<const m05m06_real::T01ConfiguredBand> grid,
    std::size_t cell,std::string_view selector)
{
    if(cell>=grid.size()||selector!=std::to_string(grid[cell].canonical_index))return {};
    const auto& frequencies=prefix.exact_active_frequencies_hz;
    const auto it=std::find(frequencies.begin(),frequencies.end(),grid[cell].center_hz);
    if(it==frequencies.end())return {};
    const auto index=static_cast<std::size_t>(it-frequencies.begin());
    double response=0.0;
    for(const auto& atom:prefix.greedy_order) {
        const auto& values=atom.fit.atom.exact_canonical_response_at_limited_gain;
        if(values.size()!=frequencies.size())return {};
        response+=values[index];
    }
    if(!std::isfinite(response))return {};
    return response;
}
// Bind explicit component capability only from an authenticated seed
// bank and the direction selected by M09. In particular D09/D12,
// correlation and unrelated loudness features are never side-effect coverage.
[[nodiscard]] inline std::optional<std::vector<NativeComponentCoverage>>
native_family_coverage(const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    const NativeTypedSeed& typed,const NativeFamilySeedBank& bank,
    const AbstractFamilyChoice& family)
{
    const auto assessment=m11_frozen::assess_native_fixed_targets(fixed,amount,requested);
    const auto guards=m11_frozen::bind_native_fixed_to_candidate(fixed,fixed.measured_fixed,amount);
    if(!assessment.valid_current||!guards||!native_seed_inventory(typed))return {};
    const auto roles=ordered_abstract_roles(family);
    if(roles.size()!=typed.seed.bindings.size())return {};
    for(std::size_t i=0;i<roles.size();++i) {
        const auto payload=native_family_role_payload(bank,family,roles[i]);
        if(!payload||*payload!=typed.seed.bindings[i].parameters)return {};
    }
    std::vector<NativeComponentCoverage> out;
    std::size_t offset=0;
    for(std::size_t i=0;i<fixed.measured_fixed.size();++i) {
        const auto& slot=fixed.measured_fixed[i];
        const auto& binding=m07_frozen::kBindings[i];
        const auto d=static_cast<std::size_t>(binding.domain);
        NativeComponentCoverage c{slot.binding_id,slot.exact_feature_dependency,
            std::vector<bool>(slot.configured.size(),false)};
        for(std::size_t k=0;k<slot.configured.size();++k) {
            const auto& guard=(*guards)[offset+k];
            const auto& p=slot.configured[k];
            if(!assessment.corrective[d]||!guard.required_in_frozen_k||
               !guard.exact_current||guard.protected_by_m11_mask)continue;
            const auto target=m08_frozen::desired_for_component(amount,d,p.prepared,p.reference);
            if(!target.value||!p.current_candidate_z)return {};
            const double delta=target.value->constraint_adjusted_z.value_or(target.value->desired_z)-*p.current_candidate_z;
            if(std::abs(delta)<=binding.deadband)continue;
            const auto matches=[&](double movement){return (delta>0.0&&movement>0.0)||(delta<0.0&&movement<0.0);};
            const auto name=std::string_view(slot.binding_id);
            if(name.ends_with(".t01")&&family.tonal_eq_prefix) {
                const auto response=exact_prefix_cell_response(bank.tonal[family.tonal_eq_prefix-1U],
                    bank.exact_tonal_grid,k,p.candidate_native.selector);
                if(response)c.covered[k]=matches(*response);
            }
            if(name.ends_with(".s03")&&family.stereo_side_eq_prefix&&
               p.candidate_native.endpoint!=m08_frozen::Endpoint::mono_side_muted&&
               target.value->endpoint!=m08_frozen::Endpoint::mono_side_muted) {
                const auto response=exact_prefix_cell_response(bank.side[family.stereo_side_eq_prefix-1U],
                    bank.exact_side_grid,k,p.candidate_native.selector);
                if(response)c.covered[k]=matches(*response);
            }
            if(name.ends_with(".l01")&&family.loudness==LoudnessMode::gain&&bank.gain)
                c.covered[k]=matches(bank.gain->payload.gain_db());
            if((name.ends_with(".d07")||name.ends_with(".d08"))&&family.micro!=MicroMode::none) {
                const auto& micro=bank.micro[static_cast<std::size_t>(family.micro)-1U];
                if(micro)c.covered[k]=matches(micro->attack_amount_db()-micro->sustain_amount_db());
            }
            if((name.ends_with(".d01")||name.ends_with(".d02")||name.ends_with(".d03")||
                name.ends_with(".d04")||name.ends_with(".d05")||name.ends_with(".d06"))&&
               family.macro!=MacroMode::none) {
                const auto& macro=bank.macro[static_cast<std::size_t>(family.macro)-1U];
                // D01 lower-tail movement is opposite to upper-tail compression.
                const double direction=name.ends_with(".d01")&&k==0U?1.0:-1.0;
                if(macro&&macro->payload.ratio()>1.0)c.covered[k]=matches(direction);
            }
            if(name.ends_with(".d10")&&family.spectral_dynamic_eq_prefix) {
                const auto& prefix=bank.spectral[family.spectral_dynamic_eq_prefix-1U];
                for(std::size_t j=0;j<prefix.ranked_clusters.size();++j) {
                    const auto& cluster=prefix.ranked_clusters[j];
                    if(k>=cluster.first&&k-cluster.first<cluster.count&&j<prefix.seeds.size())
                        c.covered[k]=matches(prefix.seeds[j].payload.spec().control.range_db);
                }
            }
            if(family.stereo_mode_index&&family.stereo_side_eq_prefix==0U) {
                const auto& ms=bank.stereo[family.stereo_mode_index-1U];
                if(ms&&name.ends_with(".s01"))
                    c.covered[k]=matches(ms->payload.side_muted()?-1.0:
                        ms->payload.side_gain_db()-ms->payload.mid_gain_db());
                if(ms&&name.ends_with(".s05")&&ms->payload.mono_bass_mode()!=dsp::MonoBassMode::OFF&&
                   p.candidate_native.selector==std::to_string(static_cast<int>(ms->payload.mono_bass_cutoff_hz()))&&
                   ms->payload.low_band_width_percent()<100.0)c.covered[k]=delta<0.0;
            }
        }
        offset+=slot.configured.size();out.push_back(std::move(c));
    }
    return out;
}
struct BoundNativeFamilyCandidate final {
    m10_frozen::BoundLegalCombinedCandidate legal;
    SelectedNativeFamilyRecipe recipe;
};
// Build the legal M09 entry and its exact selected recipe together.
// Inputs retain the allocated semantic identity and evidence dependency;
// there is no generated fake cost or substitution of unavailable roles.
[[nodiscard]] inline std::optional<BoundNativeFamilyCandidate>
bind_native_family_candidate(
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const NativeFamilySeedBank& bank,const AbstractFamilyChoice& family,
    std::span<const NativeRoleAllocation> allocations,
    std::span<const NativeRoleCoordinateRequest> coordinates,
    std::uint64_t revision,std::int64_t rate,
    CandidateCarrier identity,const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    m10_frozen::AuthoritativeCostInput constraints)
{
    if(!registry||identity.semantic_tuple.empty()||identity.dependency_tuple.empty()||
       identity.generation_snapshot.empty()||identity.ordered_provenance.empty())return {};
    const auto typed=build_native_family_seed(registry,bank,family,allocations,coordinates,revision,rate);
    if(!typed)return {};
    const auto inventory=native_seed_inventory(*typed);
    if(!inventory)return {};
    const auto bytes=exact_family_seed_content(typed->seed,*registry,*inventory);
    if(!bytes||(!identity.seed_exact_content.empty()&&identity.seed_exact_content!=*bytes))return {};
    identity.seed_exact_content=*bytes;
    const auto coverage=native_family_coverage(fixed,amount,requested,*typed,bank,family);
    if(!coverage)return {};
    const auto proxy=native_covered_proxy(fixed,amount,requested,*typed,*coverage,identity.semantic_tuple);
    if(!proxy.is_bound())return {};
    BoundNativeFamilyCandidate result;
    result.legal={identity,proxy,inventory->size(),true,true};
    result.recipe.exact_description=std::move(identity);
    result.recipe.family=family;
    result.recipe.allocations.assign(allocations.begin(),allocations.end());
    result.recipe.coordinates.assign(coordinates.begin(),coordinates.end());
    result.recipe.exact_inventory=*inventory;
    result.recipe.constraints=std::move(constraints);
    return result;
}
// Production binding derives the complete active inventory from actual
// role payloads. It never trims fields to force a candidate under 48.
[[nodiscard]] inline std::optional<BoundNativeFamilyCandidate>
bind_native_family_candidate_with_inventory(
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const NativeFamilySeedBank& bank,const AbstractFamilyChoice& family,
    std::span<const NativeRoleAllocation> allocations,
    std::span<const NativeRoleCoordinateAuthority> role_keys,
    std::span<const NativeBandCoordinateAuthority> band_keys,
    std::uint64_t revision,std::int64_t rate,
    CandidateCarrier identity,const m11_frozen::NativeFixedPopulation& fixed,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    m10_frozen::AuthoritativeCostInput constraints)
{
    const auto requests=derive_native_family_coordinate_requests(bank,family,role_keys,band_keys,rate);
    if(!requests||requests->size()>m10_frozen::kMaxActiveCoordinates)return {};
    return bind_native_family_candidate(std::move(registry),bank,family,allocations,*requests,
        revision,rate,std::move(identity),fixed,amount,requested,std::move(constraints));
}
struct BoundNativeSelectedBatch final {
    m10_frozen::BoundLegalCombinedResult selected;
    std::vector<SelectedNativeFamilyRecipe> recipes;
};
// Preserve the exact deduplication and top32 ordering from M10. The
// canonical selected ordinal indexes the deduplicated stream, not the
// original input recipe position.
[[nodiscard]] inline std::optional<BoundNativeSelectedBatch>
select_bound_native_family_batch(const ExactBytes& generation,
    std::span<const BoundNativeFamilyCandidate> candidates)
{
    if(generation.empty()||candidates.empty())return {};
    std::vector<m10_frozen::BoundLegalCombinedCandidate> legal;
    legal.reserve(candidates.size());
    for(const auto& c:candidates) {
        if(!same_selected_family_identity(c.legal.description,c.recipe.exact_description)||
           c.legal.description.generation_snapshot!=generation||
           c.legal.exact_active_coordinate_count!=c.recipe.exact_inventory.size())return {};
        legal.push_back(c.legal);
    }
    auto selected=m10_frozen::bind_exact_legal_combined_and_select_top32(generation,legal,true);
    if(selected.status!=m10_frozen::BoundLegalStatus::selected)return {};
    BoundNativeSelectedBatch result;
    for(const auto ordinal:selected.combined_consumer.numeric_selected_indices) {
        if(ordinal>=selected.exact_descriptions.size())return {};
        const auto& description=selected.exact_descriptions[ordinal];
        const auto it=std::find_if(candidates.begin(),candidates.end(),[&](const auto& c){
            return same_selected_family_identity(description,c.recipe.exact_description);});
        if(it==candidates.end())return {};
        auto recipe=it->recipe;
        recipe.ordinal=ordinal;
        recipe.exact_description=description;
        result.recipes.push_back(std::move(recipe));
    }
    result.selected=std::move(selected);
    return result;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
