#pragma once
// M10 P01 automatic-use bounds/reference map for actual native payloads.
// Frequency bounds MUST come from the immutable M09 eligible run/cluster
// intersected with common comparison bandwidth. They are never guessed.
#include "l3_m09_native_typed_seed.hpp"
#include <cstdint>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeFrequencyBounds final {double lower{},upper{};};
struct NativeCoordinateRule final {
    m10_frozen::Transform kind;
    double lower{},upper{};
    std::optional<double> fixed_reference;
    double reference_weight{};
    friend bool operator==(const NativeCoordinateRule&,const NativeCoordinateRule&)=default;
};
// P2's two new continuous coordinates require an explicit accepted
// policy. The old export-limiter fixed-ceiling law cannot provide it.
struct NativeP2CoordinatePolicy final {
    NativeCoordinateRule ceiling;
    NativeCoordinateRule drive;
    friend bool operator==(const NativeP2CoordinatePolicy&,const NativeP2CoordinatePolicy&)=default;
};
[[nodiscard]] inline bool valid_native_p2_coordinate_policy(const NativeP2CoordinatePolicy& policy)
{
    const auto valid=[](const NativeCoordinateRule& r,double lo,double hi) {
        return r.kind==m10_frozen::Transform::additive_or_linear&&
            std::isfinite(r.lower)&&std::isfinite(r.upper)&&
            lo<=r.lower&&r.lower<r.upper&&r.upper<=hi&&
            std::isfinite(r.reference_weight)&&r.reference_weight>0.0&&
            (!r.fixed_reference||(std::isfinite(*r.fixed_reference)&&
                *r.fixed_reference>=r.lower&&*r.fixed_reference<=r.upper));
    };
    return valid(policy.ceiling,-12.0,0.0)&&valid(policy.drive,-24.0,24.0);
}
[[nodiscard]] inline std::optional<NativeCoordinateRule>
native_automatic_coordinate_rule(
    const dsp::ModuleParameterPayload& seed,NativeField field,
    const std::optional<core::Uuid>& band_id,
    std::optional<NativeFrequencyBounds> exact_frequency_bounds,
    std::int64_t sample_rate_hz,const NativeP2CoordinatePolicy* p2_policy=nullptr)
{
    using T=m10_frozen::Transform;
    const auto linear=[](double a,double b,std::optional<double> ref,double weight){
        return NativeCoordinateRule{T::additive_or_linear,a,b,ref,weight};
    };
    const auto logarithmic=[](double a,double b,std::optional<double> ref,double weight){
        return NativeCoordinateRule{T::positive_time_or_q_log,a,b,ref,weight};
    };
    if(!read_native_physical_field(seed,field,band_id))return std::nullopt;
    if(std::holds_alternative<dsp::StreamingTruePeakLimiterParameters>(seed)) {
        if(!p2_policy||!valid_native_p2_coordinate_policy(*p2_policy)||exact_frequency_bounds)return {};
        if(field==NativeField::p2_ceiling_db_tp)return p2_policy->ceiling;
        if(field==NativeField::p2_drive_db)return p2_policy->drive;
        return {};
    }
    if(field==NativeField::frequency_hz) {
        if(!exact_frequency_bounds||sample_rate_hz<=0)return std::nullopt;
        const double a=exact_frequency_bounds->lower,b=exact_frequency_bounds->upper;
        if(!std::isfinite(a)||!std::isfinite(b)||a<20.0||a>=b||
           b>std::min(20000.0,0.45*static_cast<double>(sample_rate_hz)))
            return std::nullopt;
        return NativeCoordinateRule{T::frequency_log2,a,b,{},0.25};
    }
    if(std::holds_alternative<dsp::GainParameters>(seed))
        return linear(-6.0,6.0,0.0,1.0);
    if(std::holds_alternative<dsp::TransientShaperParameters>(seed))
        return linear(-3.0,3.0,0.0,1.0);
    if(std::holds_alternative<dsp::StereoMsParameters>(seed)) {
        if(field==NativeField::mid_gain_db)return linear(-12.0,12.0,0.0,1.0);
        if(field==NativeField::side_gain_db)return linear(-3.0,1.5,0.0,1.0);
        if(field==NativeField::low_width_percent)return linear(0.0,100.0,100.0,1.0);
        return std::nullopt;
    }
    if(const auto* eq=std::get_if<dsp::ParametricEqParameters>(&seed)) {
        if(!band_id)return std::nullopt;
        for(const auto& band:eq->bands()) {
            if(band.band_id()!=*band_id||!band.enabled())continue;
            const bool side=band.routing()==dsp::EqRouting::SIDE;
            if(!side&&band.routing()!=dsp::EqRouting::STEREO)return std::nullopt;
            if(field==NativeField::gain_db)
                return linear(side?-1.5:-3.0,side?1.5:3.0,0.0,1.0);
            if(field==NativeField::q)
                return logarithmic(0.10,side?1.0:1.5,{},0.25);
            if(field==NativeField::shelf_slope)
                return logarithmic(0.10,1.0,{},0.25);
        }
        return std::nullopt;
    }
    const bool compressor=std::holds_alternative<dsp::CompressorParameters>(seed);
    const bool dynamic=std::holds_alternative<dsp::DynamicEqParameters>(seed);
    if(!compressor&&!dynamic)return std::nullopt;
    switch(field) {
    case NativeField::threshold_dbfs:return linear(-120.0,0.0,{},0.25);
    case NativeField::ratio:
        return NativeCoordinateRule{T::ratio_log_from_one,1.0,compressor?2.0:20.0,1.0,1.0};
    case NativeField::knee_db:
        if(compressor)return linear(0.0,24.0,{},0.25);
        return std::nullopt;
    case NativeField::attack_ms:return logarithmic(0.1,500.0,{},0.25);
    case NativeField::release_ms:return logarithmic(1.0,5000.0,{},0.25);
    case NativeField::rms_ms:return logarithmic(1.0,500.0,{},0.25);
    case NativeField::look_ahead_ms:
        if(compressor)return NativeCoordinateRule{T::zero_inclusive_time,0.0,20.0,{},0.10};
        return std::nullopt;
    case NativeField::mix_percent:
        if(compressor)return linear(0.0,100.0,0.0,1.0);
        return std::nullopt;
    case NativeField::q:
        if(dynamic)return logarithmic(0.35,4.0,{},0.25);
        return std::nullopt;
    case NativeField::shelf_slope:
        if(dynamic)return logarithmic(0.10,1.0,{},0.25);
        return std::nullopt;
    case NativeField::range_db: {
        if(!dynamic)return std::nullopt;
        const auto range=read_native_physical_field(seed,field,band_id);
        if(!range||*range==0.0)return std::nullopt; // no invented direction
        return *range<0.0?linear(-2.0,0.0,0.0,1.0):linear(0.0,2.0,0.0,1.0);
    }
    default:return std::nullopt;
    }
}
[[nodiscard]] inline std::optional<NativeSeedCoordinate>
make_native_seed_coordinate(
    const dsp::ModuleInstance& node,const dsp::ModuleExecutionBinding& binding,
    std::vector<std::uint8_t> exact_semantic_node_key,
    std::vector<std::uint8_t> exact_stable_parameter_key,
    NativeField field,std::optional<core::Uuid> band_id,
    std::optional<NativeFrequencyBounds> exact_frequency_bounds,
    std::int64_t sample_rate_hz,const NativeP2CoordinatePolicy* p2_policy=nullptr)
{
    const auto semantic=node.semantic_node_id();
    if(!semantic||semantic->is_nil()||!node.active()||
       node.owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
       node.link_state()!=dsp::ModuleLinkState::REF_LINKED||
       exact_semantic_node_key.empty()||exact_stable_parameter_key.empty()||
       !binding_matches_instance(node,binding))
        return std::nullopt;
    const auto rule=native_automatic_coordinate_rule(binding.parameters,field,
        band_id,exact_frequency_bounds,sample_rate_hz,p2_policy);
    const auto seed=read_native_physical_field(binding.parameters,field,band_id);
    if(!rule||!seed)return std::nullopt;
    const auto transform=m10_frozen::CoordinateTransform::create(
        rule->kind,rule->lower,rule->upper);
    if(!transform)return std::nullopt;
    const auto u=transform->to_normalized(*seed);
    const auto reference=transform->to_normalized(rule->fixed_reference.value_or(*seed));
    if(!u||!reference)return std::nullopt;
    return NativeSeedCoordinate{
        {std::move(exact_semantic_node_key),std::move(exact_stable_parameter_key),*u},
        *transform,node.instance_id(),field,band_id,*reference,rule->reference_weight,*semantic};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
