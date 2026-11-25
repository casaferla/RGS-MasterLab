#pragma once
// Connect the already selected M09 combined top32 to the actual typed
// native seed/refinement path. No availability-driven backfill, no
// reordering and no additional solver or render budget.
#include "l3_m09_native_automatic_coordinate.hpp"
#include "l3_m10_p04_exact_combined_binding.hpp"
#include "l3_m10_native_fixed_search_gate.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
struct SelectedNativeSeed final {
    std::size_t exact_combined_ordinal{};
    std::vector<m10_frozen::Coordinate> exact_m09_inventory;
    const NativeTypedSeed* available_typed_seed{};
    m10_frozen::AuthoritativeCostInput frozen_constraints;
};
template<class SerializeCanonicalCurrent,class ResolveOriginalTriple>
[[nodiscard]] inline m10_frozen::RefinementBatchResult
run_selected_native_typed_search(
    const m10_frozen::BoundLegalCombinedResult& selected,
    std::span<const SelectedNativeSeed> selected_seeds,
    std::uint32_t attempts_before_seeds,
    const m10_frozen::PreservedNativeSearchEnvironment& env,
    SerializeCanonicalCurrent&& serialize_current,
    ResolveOriginalTriple&& resolve_original,
    const std::atomic<bool>* cancellation=nullptr)
{
    using namespace m10_frozen;
    RefinementBatchResult invalid;
    invalid.reason=RefinementBatchReason::invalid_inventory;
    invalid.total_attempted=attempts_before_seeds;
    const auto& ordinals=selected.combined_consumer.numeric_selected_indices;
    if(selected.status!=BoundLegalStatus::selected||
       selected.combined_consumer.status!=ConsumerStatus::ready||
       ordinals.empty()||ordinals.size()!=selected_seeds.size()||
       selected.combined_consumer.ledger.size()!=selected.exact_descriptions.size())
        return invalid;
    std::vector<std::vector<Coordinate>> inventories;
    inventories.reserve(selected_seeds.size());
    for(std::size_t i=0;i<selected_seeds.size();++i) {
        const auto& seed=selected_seeds[i];
        const auto ordinal=ordinals[i];
        if(seed.exact_combined_ordinal!=ordinal||
           ordinal>=selected.exact_descriptions.size())return invalid;
        const auto& ledger=selected.combined_consumer.ledger[ordinal];
        if(ledger.original_ordinal!=ordinal||ledger.reason!=ConsumerReason::selected)
            return invalid;
        auto sorted=m10_sort_exact_inventory(seed.exact_m09_inventory);
        if(!sorted)return invalid;
        if(seed.available_typed_seed&&!ledger.selected_but_unavailable) {
            const auto actual=native_seed_inventory(*seed.available_typed_seed);
            if(!actual||actual->size()!=sorted->size())return invalid;
            for(std::size_t j=0;j<actual->size();++j)
                if((*actual)[j].semantic_node_id!=(*sorted)[j].semantic_node_id||
                   (*actual)[j].stable_parameter_id!=(*sorted)[j].stable_parameter_id||
                   (*actual)[j].normalized_u!=(*sorted)[j].normalized_u)
                    return invalid;
        }
        inventories.push_back(std::move(*sorted));
    }
    return run_preserved_native_seed_and_refinement(
        ordinals,inventories,attempts_before_seeds,env,
        [&](std::size_t ordinal,const auto& inventory,const auto& u)
            ->std::optional<PreservedNativeTrial>{
            const auto found=std::find(ordinals.begin(),ordinals.end(),ordinal);
            if(found==ordinals.end())return std::nullopt;
            const auto& seed=selected_seeds[static_cast<std::size_t>(found-ordinals.begin())];
            if(!seed.available_typed_seed||
               selected.combined_consumer.ledger[ordinal].selected_but_unavailable)
                return std::nullopt;
            const auto& description=selected.exact_descriptions[ordinal];
            return build_native_typed_trial(*seed.available_typed_seed,inventory,u,
                seed.frozen_constraints,[&](const auto& graph,const auto& coords,const auto& values){
                    return std::invoke(serialize_current,description,graph,coords,values);
                });
        },std::forward<ResolveOriginalTriple>(resolve_original),cancellation);
}
enum class NativeProductSearchStatus:std::uint8_t {
    unavailable,no_correction_required,measured_candidate,
    no_measured_candidate,cancelled,measured_none_candidate
};
struct NativeMeasuredWinnerGraph final {
    // The graph retains the registry owner after the search's seed bank
    // and typed trial objects have been destroyed.
    std::shared_ptr<const dsp::ModuleRegistry> registry;
    m10_frozen::PreservedNativeGraph graph;
    std::size_t exact_combined_ordinal{};
    std::string exact_ticket;
    std::vector<m10_frozen::Coordinate> immutable_seed_inventory;
    std::vector<double> winning_values;
};
template<class SerializeCanonicalCurrent>
[[nodiscard]] inline std::optional<NativeMeasuredWinnerGraph>
reconstruct_selected_native_winner(
    const m10_frozen::BoundLegalCombinedResult& selected,
    std::span<const SelectedNativeSeed> seeds,
    const m10_frozen::RefinementBatchResult& result,
    SerializeCanonicalCurrent&& serialize_current)
{
    using namespace m10_frozen;
    if((result.reason!=RefinementBatchReason::complete&&
        result.reason!=RefinementBatchReason::budget_exhausted)||
       !result.winning_refinement_index||
       *result.winning_refinement_index>=result.candidate_refinements.size())return {};
    const auto& refined=result.candidate_refinements[*result.winning_refinement_index];
    const auto ordinal=refined.exact_combined_ordinal;
    if(ordinal>=selected.exact_descriptions.size()||
       ordinal>=selected.combined_consumer.ledger.size()||
       selected.combined_consumer.ledger[ordinal].reason!=ConsumerReason::selected||
       selected.combined_consumer.ledger[ordinal].selected_but_unavailable)return {};
    const auto found=std::find_if(seeds.begin(),seeds.end(),[&](const auto& s){
        return s.exact_combined_ordinal==ordinal;});
    if(found==seeds.end()||!found->available_typed_seed||
       std::count_if(seeds.begin(),seeds.end(),[&](const auto& s){
           return s.exact_combined_ordinal==ordinal;})!=1)return {};
    const auto& typed=*found->available_typed_seed;
    const auto inventory=m10_sort_exact_inventory(found->exact_m09_inventory);
    const auto& current=refined.numerical.coordinates;
    const auto& winner=refined.numerical.winner;
    if(!inventory||inventory->size()!=current.size()||!valid_key(winner)||
       !winner.native_prediction||refined.numerical.end==End::cancelled)return {};
    std::vector<double> u;
    for(std::size_t i=0;i<current.size();++i) {
        if(current[i].semantic_node_id!=(*inventory)[i].semantic_node_id||
           current[i].stable_parameter_id!=(*inventory)[i].stable_parameter_id)return {};
        u.push_back(current[i].normalized_u);
    }
    const auto ticket=m10_exact_native_trial_ticket(ordinal,*inventory,u);
    if(!ticket||*ticket!=winner.native_prediction->exact_candidate_ticket)return {};
    // Important: decode with immutable seed coordinates, NOT the
    // refined inventory whose normalized_u has changed.
    auto trial=build_native_typed_trial(typed,*inventory,u,found->frozen_constraints,
        [&](const auto& graph,const auto& coords,const auto& values){
            return std::invoke(serialize_current,selected.exact_descriptions[ordinal],graph,coords,values);
        });
    if(!trial||trial->key.canonical_current_tuple!=winner.canonical_current_tuple||
       trial->key.topology_evidence_identity!=winner.topology_evidence_identity||
       exact_native_graph_content(trial->graph.linked,trial->graph.bindings,*typed.registry)!=
          std::optional<ExactCacheBytes>{winner.native_prediction->exact_linked_graph_content})return {};
    return NativeMeasuredWinnerGraph{typed.registry,std::move(trial->graph),ordinal,
        *ticket,*inventory,std::move(u)};
}
struct NativeProductSearchResult final {
    NativeProductSearchStatus status{NativeProductSearchStatus::unavailable};
    m10_frozen::NativeFixedSearchGate fixed_gate;
    std::optional<m10_frozen::RefinementBatchResult> refinement;
    std::optional<NativeMeasuredWinnerGraph> winning_graph;
    [[nodiscard]] constexpr bool authorizes_product_publication()const noexcept{return false;}
};
template<class SerializeCanonicalCurrent>
[[nodiscard]] inline NativeProductSearchResult
run_current_native_product_search(
    const m10_frozen::BoundLegalCombinedResult& selected,
    std::span<const SelectedNativeSeed> selected_seeds,
    const m10_frozen::PreservedNativeSearchEnvironment& env,
    const m08_frozen::AmountConfig& exact_amount,
    const std::array<bool,4>& exact_requested_domains,
    bool independent_fixed_safety_scope_current,
    SerializeCanonicalCurrent&& serialize_current,
    const std::atomic<bool>* cancellation=nullptr)
{
    using namespace m10_frozen;
    NativeProductSearchResult out;
    if(cancellation&&cancellation->load(std::memory_order_acquire)) {
        out.status=NativeProductSearchStatus::cancelled;return out;
    }
    out.fixed_gate=assess_native_fixed_search_gate(env,exact_amount,
        exact_requested_domains,independent_fixed_safety_scope_current);
    if(out.fixed_gate.status==NativeFixedSearchStatus::no_correction_required) {
        out.status=NativeProductSearchStatus::no_correction_required;return out;
    }
    if(out.fixed_gate.status!=NativeFixedSearchStatus::correction_required)return out;
    // Every seed uses the same immutable owner/Amount/protection policy.
    // Only its real graph and automatic-use coordinates can vary.
    std::vector<SelectedNativeSeed> current(selected_seeds.begin(),selected_seeds.end());
    for(auto& seed:current) {
        seed.frozen_constraints.amount=exact_amount;
        seed.frozen_constraints.m08_reporting_requested_domains=exact_requested_domains;
        seed.frozen_constraints.m08_corrective_mask=out.fixed_gate.targets.corrective;
        seed.frozen_constraints.m11_no_correction_full_evidence_attested=false;
    }
    // Production evaluation always resolves original definitions from
    // Fixed; the legacy research callback must never supply them.
    out.refinement=run_selected_native_typed_search(selected,current,0U,env,
        serialize_current,
        [](std::size_t,const M10P2FullNativeEvidence&)
            ->std::optional<P2OriginalRealNativeTriple>{return std::nullopt;},
        cancellation);
    if(out.refinement->reason==RefinementBatchReason::cancelled)
        out.status=NativeProductSearchStatus::cancelled;
    else if(out.refinement->winning_refinement_index&&
       (out.refinement->reason==RefinementBatchReason::complete||
        out.refinement->reason==RefinementBatchReason::budget_exhausted)) {
        out.winning_graph=reconstruct_selected_native_winner(selected,current,*out.refinement,
            serialize_current);
        out.status=!out.winning_graph?NativeProductSearchStatus::unavailable:
            out.winning_graph->graph.linked.instances().empty()?
                NativeProductSearchStatus::measured_none_candidate:
                NativeProductSearchStatus::measured_candidate;
    }
    else out.status=NativeProductSearchStatus::no_measured_candidate;
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
