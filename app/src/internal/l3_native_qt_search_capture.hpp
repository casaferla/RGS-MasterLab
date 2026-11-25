#pragma once
#include "l3_m10_p2_product_qt_owner_capture.hpp"
#include "l3_native_execution_registry.hpp"
#include "l3_native_common_analysis_spec.hpp"

namespace rgsml::app::l3_automatch {
// A single accepted capture retains the original live registry for
// freshness checks and the extended immutable registry for all trials.
// The MANUAL chain's non-owning registry pointer remains alive.
struct NativeQtSearchCapture final {
    ProductQtP2NativeAdmission::Accepted current;
    std::shared_ptr<const dsp::ModuleRegistry> execution_registry;
    NativeCommonAnalysisSpec common_spec;
    std::uint64_t linked_revision{};
    [[nodiscard]] m10_frozen::P2NativeProductWorkerScope native_worker_scope()const noexcept {
        return {current.job.owner_stamp(),current.source.prepared,execution_registry,
            current.frozen_manual_chain,current.source.manual_bindings,
            current.frozen_max_block_frames,true};
    }
};
[[nodiscard]] inline core::Result<NativeQtSearchCapture>
capture_native_qt_search_on_owner(
    const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,
    const AuditionSourceSelector& selector,const GoldSelectionViewModel& gold,
    const std::string& fixed_ticket,std::uint64_t linked_revision,
    core::FrameCount block,bool exact_current_scope_attested)
{
    using R=core::Result<NativeQtSearchCapture>;
    auto captured=ProductQtP2NativeAdmission::capture_fixed_on_owner(product,fanout,selector,gold,
        fixed_ticket,linked_revision,block,exact_current_scope_attested);
    if(!captured)return R::failure(*captured.error());
    if(!captured.value()->job.fixed_only())
        return R::failure(core::Error{core::ErrorCode::InvalidState,"L3_QT_SEARCH_NOT_FIXED_ONLY"});
    const auto registry=freeze_native_automatch_execution_registry(captured.value()->live_registry);
    const auto spec=build_current_native_common_analysis_spec(
        static_cast<std::uint32_t>(captured.value()->source.prepared->view().format().sample_rate().value()),
        captured.value()->reference.sample_rate_hz,exact_current_scope_attested);
    if(!registry||!spec)
        return R::failure(core::Error{core::ErrorCode::InvalidState,"L3_QT_SEARCH_NATIVE_POLICY_UNAVAILABLE"});
    return R::success(NativeQtSearchCapture{std::move(*captured.value()),*registry,*spec,linked_revision});
}
[[nodiscard]] inline bool native_qt_search_still_current_on_owner(
    const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,
    const AuditionSourceSelector& selector,const GoldSelectionViewModel& gold,
    const NativeQtSearchCapture& captured)
{
    if(!captured.current.live_registry||!captured.execution_registry||
       !captured.current.job.fixed_only()||!captured.linked_revision)return false;
    auto empty=dsp::ProcessingChain::restore(*captured.current.live_registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},captured.linked_revision,{});
    return empty&&ProductQtP2NativeAdmission::still_current_on_owner(product,fanout,selector,gold,
        captured.current,*empty.value(),{});
}
} // namespace rgsml::app::l3_automatch
