#pragma once
// Explicit, immutable L3 Dynamic EQ coordinate trial over an existing
// enabled C01 band. This chooses NO solver policy, automatic target,
// new band UUID, topology or persistence. Numeric values come from a
// future frozen-coordinate planner, and are checked by the accepted
// DynamicEqParameters factories before a worker can render.
#include "l3_automatch_typed_gain_trial.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
enum class DynamicEqScalarCoordinate : std::uint8_t {
    frequency_hz,
    bell_q,
    shelf_slope,
    base_gain_db,
    range_db,
    threshold_dbfs,
    ratio,
    rms_time_constant_ms,
    attack_ms,
    release_ms
};

struct BoundDynamicEqTrial final {
    std::string ticket_identity;
    dsp::ModuleInstanceId module_id;
    core::Uuid band_id;
    DynamicEqScalarCoordinate coordinate;
    double candidate_value{};
    std::uint64_t baseline_chain_revision{};
    std::vector<dsp::ModuleInstance> baseline_instances;
    std::vector<dsp::ModuleExecutionBinding> baseline_bindings;
    std::vector<dsp::ModuleExecutionBinding> candidate_bindings;

    [[nodiscard]] bool still_matches(
        const dsp::ProcessingChain& current,
        std::span<const dsp::ModuleExecutionBinding> live) const
    {
        const auto instances=current.instances();
        return current.revision()==baseline_chain_revision
            &&instances.size()==baseline_instances.size()
            &&std::equal(instances.begin(),instances.end(),
                          baseline_instances.begin())
            &&live.size()==baseline_bindings.size()
            &&std::equal(live.begin(),live.end(),
                          baseline_bindings.begin());
    }
    [[nodiscard]] bool matches_ticket(const Ticket& ticket) const {
        return ticket.typed_dsp_fingerprint==ticket_identity;
    }
};

[[nodiscard]] inline std::optional<BoundDynamicEqTrial>
make_dynamic_eq_scalar_trial(
    const dsp::ProcessingChain& chain,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    const dsp::ModuleInstanceId& module_id,
    const core::Uuid& band_id,
    DynamicEqScalarCoordinate coordinate,
    double value,std::string ticket_identity)
{
    if(ticket_identity.empty()||ticket_identity.size()>128
       ||!std::isfinite(value)
       ||chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL})
        return std::nullopt;
    const auto instances=chain.instances();
    if(instances.empty()||instances.size()!=bindings.size())
        return std::nullopt;
    std::optional<std::size_t> module_at;
    for(std::size_t i=0;i<instances.size();++i) {
        if(!binding_matches_instance(instances[i],bindings[i]))
            return std::nullopt;
        if(instances[i].instance_id()==module_id) {
            if(module_at||!instances[i].active()
               ||instances[i].module_type_id()!="rgsml.dsp.dynamic-eq")
                return std::nullopt;
            module_at=i;
        }
    }
    if(!module_at)return std::nullopt;
    const auto* original=std::get_if<dsp::DynamicEqParameters>(
        &bindings[*module_at].parameters);
    if(!original)return std::nullopt;
    auto bands=original->bands();
    std::optional<std::size_t> band_at;
    for(std::size_t i=0;i<bands.size();++i)
        if(bands[i].spec().band_id==band_id) {
            if(band_at||!bands[i].spec().enabled)
                return std::nullopt;
            band_at=i;
        }
    if(!band_at)return std::nullopt;
    auto spec=bands[*band_at].spec();
    switch(coordinate) {
    case DynamicEqScalarCoordinate::frequency_hz:
        std::visit([value](auto& shape){shape.frequency_hz=value;},
                   spec.shape);
        break;
    case DynamicEqScalarCoordinate::bell_q:
        if(auto* bell=std::get_if<dsp::DynamicEqBellShape>(&spec.shape))
            bell->q=value;
        else return std::nullopt;
        break;
    case DynamicEqScalarCoordinate::shelf_slope:
        if(auto* shelf=std::get_if<dsp::DynamicEqShelfShape>(&spec.shape))
            shelf->shelf_slope=value;
        else return std::nullopt;
        break;
    case DynamicEqScalarCoordinate::base_gain_db:
        spec.control.base_gain_db=value;break;
    case DynamicEqScalarCoordinate::range_db:
        spec.control.range_db=value;break;
    case DynamicEqScalarCoordinate::threshold_dbfs:
        spec.control.threshold_dbfs=value;break;
    case DynamicEqScalarCoordinate::ratio:
        spec.control.ratio=value;break;
    case DynamicEqScalarCoordinate::rms_time_constant_ms:
        spec.control.rms_time_constant_ms=value;break;
    case DynamicEqScalarCoordinate::attack_ms:
        spec.control.attack_ms=value;break;
    case DynamicEqScalarCoordinate::release_ms:
        spec.control.release_ms=value;break;
    default:
        return std::nullopt;
    }
    const auto validated=dsp::DynamicEqBandParameters::create(
        std::move(spec));
    if(!validated)return std::nullopt;
    bands[*band_at]=*validated.value();
    const auto new_params=dsp::DynamicEqParameters::create(
        std::move(bands));
    if(!new_params||*new_params.value()==*original)
        return std::nullopt;
    std::vector<dsp::ModuleExecutionBinding> baseline(
        bindings.begin(),bindings.end());
    auto candidate=baseline;
    candidate[*module_at].parameters=*new_params.value();
    return BoundDynamicEqTrial{
        std::move(ticket_identity),module_id,band_id,coordinate,value,
        chain.revision(),{instances.begin(),instances.end()},
        std::move(baseline),std::move(candidate)};
}
} // namespace rgsml::app::l3_automatch
