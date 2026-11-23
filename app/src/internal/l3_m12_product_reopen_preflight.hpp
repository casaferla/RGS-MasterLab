#pragma once
// L3-M12 Project Open preflight. Rehydrate the exact ordinary DSP nodes
// from a persisted REF_LINKED Project before any Source/Gold/MASTER
// replacement. This is NONPUBLISHING and does not mint a new receipt.
// Caller must acquire its real exclusive Product Open epoch first.
#include "l3_m12_ref_linked_native_replay_plan.hpp"
#include "mastering_chain_state.hpp"
#include <rgsml/core/result.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/project/project_document.hpp>
#include <memory>
#include <string_view>
#include <utility>

namespace rgsml::app::l3_automatch::m12_frozen {
struct M12ProductReopenPreflight final {
    std::shared_ptr<const RestoredM12NativeReplayPlan> replay;
    [[nodiscard]] constexpr bool is_new_m12_materialization_receipt()
        const noexcept {return false;}
    [[nodiscard]] constexpr bool authorizes_m13_verified()
        const noexcept {return false;}
};
// Never auto-choose another algorithm or omit an unsupported linked DSP.
// The MANUAL candidate retains its exact independent Product registry;
// only the immutable native REF worker gets extra offline factories.
[[nodiscard]] inline core::Result<M12ProductReopenPreflight>
prepare_m12_product_reopen_preflight(
    const project::ProjectDocument& persisted,
    const MasteringChainState& exact_restored_manual,
    bool actual_exclusive_qt_product_open_epoch_attested)
{
    using R=core::Result<M12ProductReopenPreflight>;
    const auto fail=[](const char* code){
        return R::failure(core::Error{core::ErrorCode::InvalidState,code});
    };
    const auto manualRegistry=exact_restored_manual.registry_snapshot();
    if(!actual_exclusive_qt_product_open_epoch_attested||
       !manualRegistry||!persisted.pipeline.masteringChainId||
       *persisted.pipeline.masteringChainId!=
           exact_restored_manual.chain_id()||
       !persisted.references.activeReferenceId)
        return fail("L3_M12_REOPEN_NO_EXCLUSIVE_MANUAL_PRODUCT_OWNER");
    auto expanded=*manualRegistry;
    const auto requires=[](const project::ProjectDocument& doc,
                           std::string_view type) {
        for(const auto& chain:doc.chains)
            if(chain.stage=="MASTER"&&chain.segment=="REF_LINKED")
                for(const auto& node:chain.modules)
                    if(node.typeId==type)return true;
        return false;
    };
    if(requires(persisted,"rgsml.dsp.dynamic-eq")&&
       !expanded.has_factory("rgsml.dsp.dynamic-eq")) {
        auto added=dsp::ModuleRegistry::with_dynamic_eq_offline(
            std::move(expanded));
        if(!added)return R::failure(*added.error());
        expanded=std::move(*added.value());
    }
    if(requires(persisted,"rgsml.dsp.transient-shaper")&&
       !expanded.has_factory("rgsml.dsp.transient-shaper")) {
        auto added=dsp::ModuleRegistry::with_transient_shaper_offline(
            std::move(expanded));
        if(!added)return R::failure(*added.error());
        expanded=std::move(*added.value());
    }
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(expanded));
    const auto restored=decode_exact_m12_ref_linked_product_state(
        persisted,registry);
    if(!restored)return R::failure(*restored.error());
    const auto manualBindings=exact_restored_manual.execution_bindings();
    auto replay=prepare_restored_m12_native_replay_plan(
        persisted,*restored.value(),exact_restored_manual.chain_id(),
        exact_restored_manual.chain(),manualBindings,true);
    if(!replay)return R::failure(*replay.error());
    return R::success(M12ProductReopenPreflight{
        std::make_shared<const RestoredM12NativeReplayPlan>(
            std::move(*replay.value()))});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
