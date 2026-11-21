#pragma once
// L3 M05/M06 -> M07/M08 exact twenty-seven-slot admission boundary.
// Consumes existing actual FeatureDefinitionRegistry source records,
// preserving canonical M07 order and configured per-slot populations.
// A PARTIAL feature set never obtains complete M08/M10 authority.
#include "l3_m08_frozen_targeted_population.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m07_frozen {
enum class FullFeatureAssemblerReason:std::uint8_t {
    added,invalid_source_generation,invalid_feature_binding,
    duplicate_or_conflicting_binding,poisoned
};
struct PartialFeatureInventory final {
    std::size_t populated_count{};
    std::vector<std::string_view> missing_original_binding_ids;
    bool entire_generation_poisoned{};
    [[nodiscard]] constexpr bool authorizes_match() const noexcept {
        return false;
    }
};
class ExactTwentySevenFeatureAssembler final {
public:
    explicit ExactTwentySevenFeatureAssembler(
        std::string exact_phase6_generation_and_m07_content,
        bool owner_verified_all_source_dependencies)
        :snapshot_(std::move(exact_phase6_generation_and_m07_content)),
         authorized_(owner_verified_all_source_dependencies),
         poisoned_(snapshot_.empty()||!authorized_) {}
    [[nodiscard]] FullFeatureAssemblerReason add(
        m08_frozen::AuthenticatedPairSlot slot)
    {
        if(poisoned_)return FullFeatureAssemblerReason::poisoned;
        std::optional<std::size_t> ordinal;
        for(std::size_t i=0;i<kBindings.size();++i)
            if(kBindings[i].binding_id==slot.binding_id) {
                ordinal=i;break;
            }
        if(!ordinal) {
            poisoned_=true;
            return FullFeatureAssemblerReason::invalid_feature_binding;
        }
        if(slots_[*ordinal]) {
            // Reject duplicates, including identical records: no
            // first-wins or order-dependent silent replacement.
            poisoned_=true;
            return FullFeatureAssemblerReason::
                duplicate_or_conflicting_binding;
        }
        const auto& frozen=kBindings[*ordinal];
        const auto n=slot.configured.size();
        if(!slot.feature_authority_attested||
           slot.exact_feature_dependency.empty()||
           (slot.source_feature_id!=frozen.source_feature_id&&
            (frozen.exclusive_fallback_feature_id.empty()||
             slot.source_feature_id!=
                 frozen.exclusive_fallback_feature_id))||
           n==0||
           (frozen.component_share==ComponentShare::one&&n!=1U)||
           (frozen.component_share==ComponentShare::thirds&&n!=3U)) {
            poisoned_=true;
            return FullFeatureAssemblerReason::invalid_feature_binding;
        }
        for(const auto& v:slot.configured) {
            if(!std::isfinite(v.paired_native_support)||
               v.paired_native_support<0.0||
               v.paired_native_support>1.0||
               (v.prepared.z&&!std::isfinite(*v.prepared.z))||
               (v.reference.z&&!std::isfinite(*v.reference.z))||
               (v.current_candidate_z&&
                !std::isfinite(*v.current_candidate_z))) {
                poisoned_=true;
                return FullFeatureAssemblerReason::invalid_feature_binding;
            }
        }
        slots_[*ordinal]=std::move(slot);
        ++count_;
        return FullFeatureAssemblerReason::added;
    }
    [[nodiscard]] PartialFeatureInventory current_inventory() const {
        PartialFeatureInventory result;
        result.populated_count=count_;
        result.entire_generation_poisoned=poisoned_;
        for(std::size_t i=0;i<kBindings.size();++i)
            if(!slots_[i])
                result.missing_original_binding_ids.push_back(
                    kBindings[i].binding_id);
        return result;
    }
    [[nodiscard]] std::optional<
        std::vector<m08_frozen::AuthenticatedPairSlot>>
    complete_canonical_population() const {
        if(poisoned_||!authorized_||count_!=kBindings.size())
            return std::nullopt;
        std::vector<m08_frozen::AuthenticatedPairSlot> result;
        result.reserve(kBindings.size());
        for(const auto& slot:slots_) {
            if(!slot)return std::nullopt;
            result.push_back(*slot);
        }
        return result;
    }
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
private:
    std::string snapshot_;
    std::array<std::optional<m08_frozen::AuthenticatedPairSlot>,
               kBindings.size()> slots_;
    std::size_t count_{};
    bool authorized_{};
    bool poisoned_{};
};
} // namespace rgsml::app::l3_automatch::m07_frozen
