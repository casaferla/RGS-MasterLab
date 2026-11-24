#pragma once
#include "l3_m09_native_family_seed_bank.hpp"
#include "l3_m09_native_selected_search.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
struct SelectedNativeFamilyRecipe final {
    std::size_t ordinal{};
    CandidateCarrier exact_description;
    AbstractFamilyChoice family;
    std::vector<NativeRoleAllocation> allocations;
    std::vector<NativeRoleCoordinateRequest> coordinates;
    std::vector<m10_frozen::Coordinate> exact_inventory;
    m10_frozen::AuthoritativeCostInput constraints;
};
struct OwnedSelectedNativeFamily final {
    std::size_t ordinal{};
    std::vector<m10_frozen::Coordinate> inventory;
    std::optional<NativeTypedSeed> typed;
    m10_frozen::AuthoritativeCostInput constraints;
};
[[nodiscard]] inline bool same_selected_family_identity(
    const CandidateCarrier& a,const CandidateCarrier& b)
{
    return a.semantic_tuple==b.semantic_tuple&&a.seed_exact_content==b.seed_exact_content&&
        a.dependency_tuple==b.dependency_tuple&&a.generation_snapshot==b.generation_snapshot;
}
// Exact serializer must confirm that the physical seed matches the
// selected description, rather than accepting a role/ordinal alone.
// A selected unavailable recipe stays at its original ordinal.
template<class SerializeSeed>
[[nodiscard]] inline std::optional<std::vector<OwnedSelectedNativeFamily>>
bind_selected_native_families(
    const m10_frozen::BoundLegalCombinedResult& selected,
    std::span<const SelectedNativeFamilyRecipe> recipes,
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const NativeFamilySeedBank& bank,std::uint64_t revision,std::int64_t rate,
    SerializeSeed&& serialize_seed)
{
    using namespace m10_frozen;
    const auto& indices=selected.combined_consumer.numeric_selected_indices;
    if(!registry||selected.status!=BoundLegalStatus::selected||
       selected.combined_consumer.status!=ConsumerStatus::ready||
       indices.empty()||indices.size()!=recipes.size()||
       selected.combined_consumer.ledger.size()!=selected.exact_descriptions.size())return {};
    std::vector<OwnedSelectedNativeFamily> owned;
    owned.reserve(indices.size());
    for(std::size_t i=0;i<indices.size();++i) {
        const auto ordinal=indices[i];const auto& recipe=recipes[i];
        if(ordinal>=selected.exact_descriptions.size()||recipe.ordinal!=ordinal||
           !same_selected_family_identity(recipe.exact_description,selected.exact_descriptions[ordinal]))return {};
        const auto& ledger=selected.combined_consumer.ledger[ordinal];
        if(ledger.original_ordinal!=ordinal||ledger.reason!=ConsumerReason::selected)return {};
        auto inventory=m10_sort_exact_inventory(recipe.exact_inventory);
        if(!inventory)return {};
        std::optional<NativeTypedSeed> typed;
        if(!ledger.selected_but_unavailable) {
            typed=build_native_family_seed(registry,bank,recipe.family,
                recipe.allocations,recipe.coordinates,revision,rate);
            if(typed) {
                const auto actual=native_seed_inventory(*typed);
                if(!actual||actual->size()!=inventory->size())return {};
                for(std::size_t j=0;j<actual->size();++j)
                    if((*actual)[j].semantic_node_id!=(*inventory)[j].semantic_node_id||
                       (*actual)[j].stable_parameter_id!=(*inventory)[j].stable_parameter_id||
                       (*actual)[j].normalized_u!=(*inventory)[j].normalized_u)return {};
                const auto serialized=std::invoke(serialize_seed,typed->seed,*actual);
                if(!serialized||*serialized!=recipe.exact_description.seed_exact_content)return {};
            }
        }
        owned.push_back({ordinal,std::move(*inventory),std::move(typed),recipe.constraints});
    }
    return owned;
}
// The owned seeds remain alive for the entire synchronous offline search.
template<class SerializeSeed,class SerializeCurrent>
[[nodiscard]] inline NativeProductSearchResult run_selected_native_family_search(
    const m10_frozen::BoundLegalCombinedResult& selected,
    std::span<const SelectedNativeFamilyRecipe> recipes,
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    const NativeFamilySeedBank& bank,std::uint64_t revision,std::int64_t rate,
    const m10_frozen::PreservedNativeSearchEnvironment& env,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    bool independent_fixed_safety_scope_current,
    SerializeSeed&& serialize_seed,SerializeCurrent&& serialize_current,
    const std::atomic<bool>* cancellation=nullptr)
{
    if(cancellation&&cancellation->load(std::memory_order_acquire)) {
        NativeProductSearchResult cancelled;cancelled.status=NativeProductSearchStatus::cancelled;
        return cancelled;
    }
    const auto owned=bind_selected_native_families(selected,recipes,std::move(registry),
        bank,revision,rate,std::forward<SerializeSeed>(serialize_seed));
    if(!owned)return {};
    std::vector<SelectedNativeSeed> views;
    views.reserve(owned->size());
    for(const auto& x:*owned)
        views.push_back({x.ordinal,x.inventory,x.typed?&*x.typed:nullptr,x.constraints});
    return run_current_native_product_search(selected,views,env,amount,requested,
        independent_fixed_safety_scope_current,std::forward<SerializeCurrent>(serialize_current),cancellation);
}
} // namespace rgsml::app::l3_automatch::m09_frozen
