#pragma once
#include "l3_m09_abstract_family_enumeration.hpp"
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
    case IntentRole::terminal_limiter:return std::nullopt;
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
        for(const auto& band:eq->bands())
            if(!band.enabled()||band.routing()!=(side?dsp::EqRouting::SIDE:dsp::EqRouting::STEREO)||
               !(std::holds_alternative<dsp::BellPayload>(band.payload())||
                 std::holds_alternative<dsp::ShelfPayload>(band.payload())))return false;
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
    case IntentRole::terminal_limiter:return false;
    }
    return false;
}
// Only the artistic segment is assembled here. A terminal-limiter
// alternative requires its separate owner-authorized composition;
// never silently discard it or turn WAV Export into a chain DSP.
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
} // namespace rgsml::app::l3_automatch::m09_frozen
