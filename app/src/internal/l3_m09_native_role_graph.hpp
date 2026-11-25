#pragma once
#include "l3_m09_abstract_family_enumeration.hpp"
#include "l3_m09_exact_candidate_dedup.hpp"
#include "l3_m09_native_automatic_coordinate.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeRoleSeed final {
    IntentRole role;
    dsp::ModuleInstanceId instance;
    core::Uuid semantic;
    dsp::ModuleParameterPayload payload;
};
[[nodiscard]] inline std::optional<std::string_view>
native_artistic_role_type(IntentRole role) noexcept {
    switch(role) {
    case IntentRole::tonal_eq: case IntentRole::stereo_side_eq:return "rgsml.dsp.parametric-eq";
    case IntentRole::spectral_dynamic_eq:return "rgsml.dsp.dynamic-eq";
    case IntentRole::macro_compressor:return "rgsml.dsp.compressor";
    case IntentRole::micro_transient:return "rgsml.dsp.transient-shaper";
    case IntentRole::stereo_ms:return "rgsml.dsp.stereo-ms";
    case IntentRole::loudness_gain:return "rgsml.dsp.gain";
    case IntentRole::terminal_limiter:return "rgsml.dsp.streaming-true-peak-limiter";
    }
    return std::nullopt;
}
// Preserve the frozen discrete role and band-count choices. Factory
// acceptance alone does not attest automatic-use topology.
[[nodiscard]] inline bool native_role_payload_matches(
    const AbstractFamilyChoice& family,const NativeRoleSeed& seed)
{
    const auto& payload=seed.payload;
    switch(seed.role) {
    case IntentRole::tonal_eq:
    case IntentRole::stereo_side_eq: {
        const auto* eq=std::get_if<dsp::ParametricEqParameters>(&payload);
        const bool side=seed.role==IntentRole::stereo_side_eq;
        const auto count=side?family.stereo_side_eq_prefix:family.tonal_eq_prefix;
        if(!eq||count==0U||eq->bands().size()!=count)return false;
        for(const auto& band:eq->bands()) {
            if(!band.enabled()||band.routing()!=(side?dsp::EqRouting::SIDE:dsp::EqRouting::STEREO))
                return false;
            if(const auto* bell=std::get_if<dsp::BellPayload>(&band.payload())) {
                if(std::abs(bell->gain_db)>(side?1.5:3.0)||
                   bell->q<0.1||bell->q>(side?1.0:1.5))return false;
            } else if(const auto* shelf=std::get_if<dsp::ShelfPayload>(&band.payload())) {
                if(std::abs(shelf->gain_db)>(side?1.5:3.0)||
                   shelf->shelf_slope<0.1||shelf->shelf_slope>1.0)return false;
            } else return false;
        }
        return true;
    }
    case IntentRole::spectral_dynamic_eq: {
        const auto* eq=std::get_if<dsp::DynamicEqParameters>(&payload);
        if(!eq||family.spectral_dynamic_eq_prefix==0U||
           eq->bands().size()!=family.spectral_dynamic_eq_prefix)return false;
        for(const auto& band:eq->bands())
            if(!band.spec().enabled||band.spec().routing!=dsp::EqRouting::STEREO||
               band.spec().control.base_gain_db!=0.0||
               band.spec().control.range_db==0.0||
               std::abs(band.spec().control.range_db)>2.0)return false;
        return true;
    }
    case IntentRole::macro_compressor: {
        const auto* compressor=std::get_if<dsp::CompressorParameters>(&payload);
        if(!compressor||family.macro==MacroMode::none)return false;
        const auto link=family.macro==MacroMode::linked_max?
            dsp::CompressorChannelLink::LINKED_MAX:dsp::CompressorChannelLink::LINKED_MEAN;
        return compressor->detector_mode()==dsp::CompressorDetectorMode::RMS&&
            compressor->channel_link()==link&&compressor->makeup_gain_db()==0.0&&
            compressor->ratio()<=2.0;
    }
    case IntentRole::micro_transient: {
        const auto* transient=std::get_if<dsp::TransientShaperParameters>(&payload);
        if(!transient||family.micro==MicroMode::none)return false;
        return std::abs(transient->attack_amount_db())<=3.0&&
            std::abs(transient->sustain_amount_db())<=3.0&&
            (family.micro!=MicroMode::attack||transient->sustain_amount_db()==0.0)&&
            (family.micro!=MicroMode::sustain||transient->attack_amount_db()==0.0);
    }
    case IntentRole::stereo_ms: {
        const auto* ms=std::get_if<dsp::StereoMsParameters>(&payload);
        if(!ms||family.stereo_mode_index==0U||
           family.stereo_mode_index>kLowBassAlternatives.size())return false;
        const auto& low=kLowBassAlternatives[family.stereo_mode_index-1U];
        const auto mode=low.mode==LowBassMode::off?dsp::MonoBassMode::OFF:
            low.mode==LowBassMode::lr12?dsp::MonoBassMode::LR12:dsp::MonoBassMode::LR24;
        return ms->mono_bass_mode()==mode&&ms->mono_bass_cutoff_hz()==low.cutoff_hz;
    }
    case IntentRole::loudness_gain: {
        const auto* gain=std::get_if<dsp::GainParameters>(&payload);
        return gain&&std::abs(gain->gain_db())<=6.0;
    }
    case IntentRole::terminal_limiter:
        return (family.loudness==LoudnessMode::gain_with_terminal_limiter||
                family.loudness==LoudnessMode::terminal_limiter_only)&&
            std::holds_alternative<dsp::StreamingTruePeakLimiterParameters>(payload);
    }
    return false;
}
// Assemble the ordered REF_LINKED musical segment. An explicitly
// supplied owned P2 is last; WAV Export remains outside this graph.
// The Product worker separately rejects a protected MANUAL P2 conflict.
[[nodiscard]] inline std::optional<m10_frozen::PreservedNativeGraph>
assemble_native_artistic_role_graph(
    const dsp::ModuleRegistry& registry,const AbstractFamilyChoice& family,
    std::span<const NativeRoleSeed> ordered_seeds,std::uint64_t revision)
{
    const auto roles=ordered_abstract_roles(family);
    if(roles.size()!=ordered_seeds.size())return std::nullopt;
    std::vector<dsp::ModuleInstance> nodes;
    std::vector<dsp::ModuleExecutionBinding> bindings;
    nodes.reserve(roles.size());bindings.reserve(roles.size());
    for(std::size_t i=0;i<roles.size();++i) {
        const auto& seed=ordered_seeds[i];
        const auto type=native_artistic_role_type(roles[i]);
        if(!type||seed.role!=roles[i]||seed.semantic.is_nil()||
           !native_role_payload_matches(family,seed))return std::nullopt;
        for(std::size_t j=0;j<i;++j)
            if(ordered_seeds[j].instance==seed.instance||
               ordered_seeds[j].semantic==seed.semantic)return std::nullopt;
        if(!registry.has_factory(*type)||!registry.create_module(*type,seed.payload))
            return std::nullopt;
        auto node=dsp::ModuleInstance::create({seed.instance,std::string(*type),
            true,false,false,false,dsp::ModuleProvenance::REFERENCE_MATCH,
            dsp::ModuleOwner::REFERENCE_MATCH,dsp::ModuleLinkState::REF_LINKED,
            seed.semantic,{}});
        if(!node)return std::nullopt;
        nodes.push_back(std::move(*node.value()));
        bindings.push_back({seed.instance,seed.payload});
    }
    auto chain=dsp::ProcessingChain::restore(registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        revision,std::move(nodes));
    if(!chain)return std::nullopt;
    return m10_frozen::PreservedNativeGraph{std::move(*chain.value()),std::move(bindings)};
}
struct NativeRoleCoordinateRequest final {
    IntentRole role;
    ExactBytes semantic_key;
    ExactBytes stable_parameter_key;
    NativeField field;
    std::optional<core::Uuid> band_id;
    std::optional<NativeFrequencyBounds> frequency_bounds;
    std::optional<NativeP2CoordinatePolicy> p2_policy{};
};
// M09/M12 provide the allocated semantic identities and complete
// coordinate descriptors. This builds the actual immutable seed and
// verifies exact round-trip through the existing M10 inverse transform.
[[nodiscard]] inline std::optional<NativeTypedSeed>
assemble_native_artistic_inventory_seed(
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const AbstractFamilyChoice& family,std::span<const NativeRoleSeed> roles,
    std::span<const NativeRoleCoordinateRequest> requests,
    std::uint64_t revision,std::int64_t sample_rate)
{
    if(!registry)return std::nullopt;
    if(sample_rate!=44100&&sample_rate!=48000&&sample_rate!=88200&&
       sample_rate!=96000&&sample_rate!=176400&&sample_rate!=192000)
        return std::nullopt;
    const double maximum_frequency=std::min(20000.0,0.45*static_cast<double>(sample_rate));
    for(const auto& role:roles) {
        if(const auto* eq=std::get_if<dsp::ParametricEqParameters>(&role.payload))
            for(const auto& band:eq->bands()) {
                const auto frequency=std::visit([](const auto& p){return p.frequency_hz;},band.payload());
                if(!std::isfinite(frequency)||frequency<20.0||frequency>maximum_frequency)
                    return {};
            }
        if(const auto* dynamic=std::get_if<dsp::DynamicEqParameters>(&role.payload))
            if(!dynamic->validate_for_audio(static_cast<std::uint32_t>(sample_rate),2U))return {};
    }
    auto graph=assemble_native_artistic_role_graph(*registry,family,roles,revision);
    if(!graph)return std::nullopt;
    const auto nodes=graph->linked.instances();
    const bool has_p2=std::any_of(roles.begin(),roles.end(),[](const auto& r){
        return r.role==IntentRole::terminal_limiter;});
    const NativeP2CoordinatePolicy* exact_p2_policy=nullptr;
    for(const auto& request:requests) {
        if(request.role!=IntentRole::terminal_limiter) {
            if(request.p2_policy)return {};
            continue;
        }
        if(!has_p2||!request.p2_policy||
           !valid_native_p2_coordinate_policy(*request.p2_policy)||
           request.band_id||request.frequency_bounds)return {};
        if(exact_p2_policy&&*exact_p2_policy!=*request.p2_policy)return {};
        exact_p2_policy=&*request.p2_policy;
    }
    if(has_p2) {
        const auto count=[&](NativeField field){return std::count_if(requests.begin(),requests.end(),
            [&](const auto& r){return r.role==IntentRole::terminal_limiter&&r.field==field&&r.p2_policy.has_value();});};
        if(count(NativeField::p2_ceiling_db_tp)!=1||count(NativeField::p2_drive_db)!=1)return {};
    }
    std::vector<NativeSeedCoordinate> coordinates;
    coordinates.reserve(requests.size());
    for(const auto& request:requests) {
        const auto role=std::find_if(roles.begin(),roles.end(),[&](const auto& seed){
            return seed.role==request.role;
        });
        if(role==roles.end())return std::nullopt;
        const auto index=static_cast<std::size_t>(role-roles.begin());
        auto coordinate=make_native_seed_coordinate(nodes[index],graph->bindings[index],
            request.semantic_key,request.stable_parameter_key,request.field,
            request.band_id,request.frequency_bounds,sample_rate,
            request.p2_policy?&*request.p2_policy:nullptr);
        if(!coordinate)return std::nullopt;
        coordinates.push_back(std::move(*coordinate));
    }
    NativeTypedSeed seed{std::move(registry),std::move(*graph),std::move(coordinates)};
    return seed;
}
// Execution remains capped at the frozen 48 coordinates. M09 can
// describe the exact over-budget graph without making a trial executable.
[[nodiscard]] inline std::optional<NativeTypedSeed>
assemble_native_artistic_typed_seed(
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const AbstractFamilyChoice& family,std::span<const NativeRoleSeed> roles,
    std::span<const NativeRoleCoordinateRequest> requests,
    std::uint64_t revision,std::int64_t sample_rate)
{
    if(requests.size()>m10_frozen::kMaxActiveCoordinates)return {};
    auto seed=assemble_native_artistic_inventory_seed(std::move(registry),family,roles,
        requests,revision,sample_rate);
    if(!seed||!native_seed_inventory(*seed))return {};
    return seed;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
