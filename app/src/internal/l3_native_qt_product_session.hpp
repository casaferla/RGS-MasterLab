#pragma once
// First Product AutoMatch operation. Owner-thread capture and private
// worker data are deliberately separate from eventual user adoption.
#include "l3_native_qt_product_policy.hpp"
#include "l3_native_qt_search_handoff.hpp"
#include <set>

namespace rgsml::app::l3_automatch {
struct NativeQtProductSession final {
    project::ProjectSnapshot baseline;
    NativeQtSearchCapture capture;
    NativeQtSearchPolicy policy;
    m12_frozen::M12ProjectTransactionExpected expected;
    std::vector<core::Uuid> protected_ids;
    core::Uuid plan_id,controller_id,linked_chain_id;
};
[[nodiscard]] inline core::Result<NativeQtProductSession>
begin_native_qt_product_session_on_owner(
    ProjectSessionViewModel& session,const ProductQtP2NativeAdmission::Controllers& product,
    const ProductQtP2NativeAdmission::Fanout& fanout,const AuditionSourceSelector& selector,
    const GoldSelectionViewModel& gold,const m08_frozen::AmountConfig& amount,
    const std::array<bool,4>& requested,double independent_ceiling_db_tp,
    core::FrameCount block,core::Uuid plan_id,core::Uuid controller_id,core::Uuid linked_chain_id)
{
    using R=core::Result<NativeQtProductSession>;
    const auto fail=[](const char* why){return R::failure(core::Error{core::ErrorCode::InvalidState,why});};
    if(QThread::currentThread()!=session.thread()||
       &session!=&product.upstream.project||
       plan_id.is_nil()||controller_id.is_nil()||linked_chain_id.is_nil())
        return fail("L3_PRODUCT_SESSION_INVALID_OWNER");
    auto baseline=session.capture_private_m12_search_baseline();
    if(!baseline)return R::failure(*baseline.error());
    const auto& doc=baseline.value()->document();
    if(!doc.references.activeReferenceId||!doc.pipeline.masteringChainId)
        return fail("L3_PRODUCT_SESSION_MISSING_SOURCE_REFERENCE_MANUAL");
    const auto manual=std::find_if(doc.chains.begin(),doc.chains.end(),[&](const auto& c){
        return c.chainId==*doc.pipeline.masteringChainId&&c.stage=="MASTER"&&c.segment=="MANUAL";
    });
    if(manual==doc.chains.end())return fail("L3_PRODUCT_SESSION_MANUAL_MISSING");
    // Do not silently replace a previously accepted controller.
    if(std::any_of(doc.chains.begin(),doc.chains.end(),[](const auto& c){
        return c.stage=="MASTER"&&c.segment=="REF_LINKED";
    }))return fail("L3_PRODUCT_SESSION_EXISTING_LINKED_REQUIRES_REPLACEMENT");
    std::set<core::Uuid> used;
    used.insert(doc.projectId);used.insert(doc.sourceResourceId);
    for(const auto& r:doc.resources)used.insert(r.resourceId);
    for(const auto& r:doc.references.items)used.insert(r.referenceId);
    for(const auto& r:doc.timeSelections.auditionRegions)used.insert(r.regionId);
    for(const auto& c:doc.chains) {
        used.insert(c.chainId);
        for(const auto& node:c.modules) {
            used.insert(node.instanceId);
            if(node.semanticNodeId)used.insert(*node.semanticNodeId);
        }
    }
    for(const auto& id:{plan_id,controller_id,linked_chain_id})
        if(!used.insert(id).second)return fail("L3_PRODUCT_SESSION_ID_COLLISION");
    auto capture=capture_native_qt_search_on_owner(product,fanout,selector,gold,
        plan_id.to_string()+"/fixed",1U,block,true);
    if(!capture)return R::failure(*capture.error());
    auto policy=build_current_native_qt_product_policy(*capture.value(),amount,requested,independent_ceiling_db_tp);
    if(!policy)return fail("L3_PRODUCT_SESSION_POLICY_UNAVAILABLE");
    m12_frozen::M12ProjectTransactionExpected expected{doc.sourceResourceId,
        *doc.references.activeReferenceId,*doc.pipeline.masteringChainId,*manual,std::nullopt,true,true};
    std::vector<core::Uuid> protected_ids(used.begin(),used.end());
    return R::success(NativeQtProductSession{std::move(*baseline.value()),
        std::move(*capture.value()),std::move(*policy),std::move(expected),
        std::move(protected_ids),plan_id,controller_id,linked_chain_id});
}
} // namespace rgsml::app::l3_automatch
