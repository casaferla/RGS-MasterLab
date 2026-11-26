#pragma once
#include "l3_m13_product_fresh_postmaterialization_probe.hpp"
#include <algorithm>
#include <array>
#include <string_view>
namespace rgsml::app::l3_automatch::m13_frozen {
enum class NativeGraphSafety {passed,failed,unavailable};
[[nodiscard]] inline NativeGraphSafety audit_m13_native_graph_safety(
    const M13ImmutableProductFreshProbeRequest& request,
    const M13FreshProductMasterProbe& fresh)
{
    const auto& replay=request.exact_materialized_preview_owner().replay;
    if(!replay||!replay->registry_owner||!fresh.matches_executed_request(request))
        return NativeGraphSafety::unavailable;
    if(!m12_frozen::m12_native_stage_witness_matches_replay_plan(*replay,fresh.stage_witness))
        return NativeGraphSafety::failed;
    // Current Product's admitted ordinary DSP implementations. None
    // implements Haas or artificial decorrelation. Unknown active DSP
    // types must not inherit that assertion from a successful render.
    constexpr std::array<std::string_view,7> known{
        "rgsml.dsp.gain","rgsml.dsp.parametric-eq","rgsml.dsp.compressor",
        "rgsml.dsp.dynamic-eq","rgsml.dsp.transient-shaper",
        "rgsml.dsp.stereo-ms","rgsml.dsp.streaming-true-peak-limiter"};
    const auto check=[&](const dsp::ProcessingChain& chain,bool linked) {
        for(const auto& node:chain.instances()) {
            if(node.module_type_id()=="rgsml.dsp.true-peak-limiter"||
               node.module_type_id()=="rgsml.dsp.dither")
                return NativeGraphSafety::failed;
            if(linked&&(node.owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
                node.link_state()!=dsp::ModuleLinkState::REF_LINKED))
                return NativeGraphSafety::failed;
            if(node.active()&&std::find(known.begin(),known.end(),node.module_type_id())==known.end())
                return NativeGraphSafety::unavailable;
        }
        return NativeGraphSafety::passed;
    };
    auto result=check(replay->manual_prefix,false);
    if(result!=NativeGraphSafety::passed)return result;
    result=check(replay->reference_linked,true);
    if(result!=NativeGraphSafety::passed)return result;
    if(replay->terminal_p2) {
        const auto nodes=replay->terminal_p2->instances();
        if(nodes.size()!=1U||nodes.front().module_type_id()!="rgsml.dsp.streaming-true-peak-limiter"||
           nodes.front().owner()!=dsp::ModuleOwner::USER||
           nodes.front().link_state()!=dsp::ModuleLinkState::UNLINKED)
            return NativeGraphSafety::failed;
        return check(*replay->terminal_p2,false);
    }
    return NativeGraphSafety::passed;
}
} // namespace rgsml::app::l3_automatch::m13_frozen
