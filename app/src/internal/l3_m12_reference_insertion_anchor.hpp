#pragma once
// L3 M12 canonical REF_LINKED insertion gap, independent of list index.
// The normal frozen P04 placement is immediately before the maximal
// canonical TERMINAL_SLOT suffix or at the legal end, never before
// Input Gain. The opt-in streaming P2 MODULE is INLINE_CHAIN but
// must_be_last; its separate Product terminal-effect suffix is also
// placed AFTER the REF_LINKED artistic segment. P2 is NOT M17.
// This is an immutable preflight/anchor artifact, not a mutation,
// rebound decision, materialization receipt or M13 verification.
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace rgsml::app::l3_automatch::m12_frozen {
enum class InsertionAnchorReason:std::uint8_t {
    exact_current,stale_or_invalid_owner,
    unsupported_or_wrong_fixed_topology,
    changed_neighbor_pair_requires_review
};
struct ReferenceMatchInsertionAnchor final {
    std::optional<dsp::ModuleInstanceId> left_neighbor_instance_id;
    std::optional<dsp::ModuleInstanceId> right_neighbor_instance_id;
    std::uint64_t anchor_revision{1U};
    std::uint64_t protected_manual_revision{};
    [[nodiscard]] constexpr bool authorizes_project_mutation()
        const noexcept{return false;}
};
struct M12InsertionAnchorAudit final {
    std::optional<ReferenceMatchInsertionAnchor> current_gap;
    InsertionAnchorReason reason{
        InsertionAnchorReason::stale_or_invalid_owner};
    [[nodiscard]] constexpr bool exact_current() const noexcept {
        return reason==InsertionAnchorReason::exact_current&&
               current_gap.has_value();
    }
    [[nodiscard]] constexpr bool authorizes_m12_commit() const noexcept {
        return false;
    }
};
[[nodiscard]] inline M12InsertionAnchorAudit
plan_canonical_m12_insertion_anchor(
    const dsp::ProcessingChain& fixed_manual,
    const dsp::ModuleRegistry& canonical_registry,
    bool exact_current_fixed_owner_and_registry_attested)
{
    const auto reject=[](InsertionAnchorReason r){
        return M12InsertionAnchorAudit{{},r};
    };
    if(!exact_current_fixed_owner_and_registry_attested||
       fixed_manual.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL})
        return reject(InsertionAnchorReason::stale_or_invalid_owner);
    const auto nodes=fixed_manual.instances();
    if(nodes.empty()||nodes.front().module_type_id()!="rgsml.dsp.gain")
        return reject(InsertionAnchorReason::
            unsupported_or_wrong_fixed_topology);
    // Classify terminal processors by registry metadata, not GUI name.
    // The distinct streaming-P2 opt-in is an INLINE must-be-last
    // processor, preserved as the unique protected final suffix.
    std::size_t suffix_begin=nodes.size();
    bool saw_p2=false;
    for(std::size_t i=0;i<nodes.size();++i) {
        const auto desc=canonical_registry.find_descriptor(
            nodes[i].module_type_id());
        if(!desc)return reject(InsertionAnchorReason::
            unsupported_or_wrong_fixed_topology);
        const auto& d=desc.value()->get();
        const bool streaming_p2=
            nodes[i].module_type_id()==
                "rgsml.dsp.streaming-true-peak-limiter";
        if(streaming_p2) {
            if(saw_p2||i!=nodes.size()-1U||!d.must_be_last()||
               d.placement_class()!=dsp::PlacementClass::INLINE_CHAIN)
                return reject(InsertionAnchorReason::
                    unsupported_or_wrong_fixed_topology);
            saw_p2=true;
            suffix_begin=std::min(suffix_begin,i);
        } else if(d.placement_class()==
                  dsp::PlacementClass::TERMINAL_SLOT) {
            suffix_begin=std::min(suffix_begin,i);
        } else if(i>=suffix_begin)
            return reject(InsertionAnchorReason::
                unsupported_or_wrong_fixed_topology);
    }
    // Input Gain is always the first protected module; the REF
    // insertion gap may never precede it.
    if(suffix_begin==0U)
        return reject(InsertionAnchorReason::
            unsupported_or_wrong_fixed_topology);
    ReferenceMatchInsertionAnchor anchor;
    anchor.protected_manual_revision=fixed_manual.revision();
    anchor.left_neighbor_instance_id=
        nodes[suffix_begin-1U].instance_id();
    if(suffix_begin<nodes.size())
        anchor.right_neighbor_instance_id=
            nodes[suffix_begin].instance_id();
    return {anchor,InsertionAnchorReason::exact_current};
}
// Freeze the exact neighbor pair before presenting an accepted diff.
// A changed node sequence or revision requires new upstream review;
// it cannot be silently "rebound" to another sound boundary.
[[nodiscard]] inline M12InsertionAnchorAudit
revalidate_exact_m12_insertion_anchor(
    const ReferenceMatchInsertionAnchor& accepted,
    const dsp::ProcessingChain& live_manual,
    const dsp::ModuleRegistry& live_registry,
    bool real_qt_fixed_owner_and_anchor_rechecked)
{
    if(!real_qt_fixed_owner_and_anchor_rechecked||
       !accepted.anchor_revision||
       accepted.protected_manual_revision!=live_manual.revision())
        return {{},InsertionAnchorReason::stale_or_invalid_owner};
    auto current=plan_canonical_m12_insertion_anchor(
        live_manual,live_registry,true);
    if(!current.exact_current())return current;
    current.current_gap->anchor_revision=accepted.anchor_revision;
    if(current.current_gap->left_neighbor_instance_id!=
           accepted.left_neighbor_instance_id||
       current.current_gap->right_neighbor_instance_id!=
           accepted.right_neighbor_instance_id)
        return {{},InsertionAnchorReason::
            changed_neighbor_pair_requires_review};
    return current;
}
} // namespace rgsml::app::l3_automatch::m12_frozen
