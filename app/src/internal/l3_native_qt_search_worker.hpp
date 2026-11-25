#pragma once
#include "l3_native_qt_search_capture.hpp"
#include "l3_m05m06_product_qt_original_pair_worker.hpp"
#include "l3_native_automatch_worker.hpp"

namespace rgsml::app::l3_automatch {
struct NativeQtSearchPolicy final {
    m10_frozen::NativeDefinitionPolicy definitions;
    std::string exact_generation;
    m09_frozen::ExactBytes generation_and_policy;
    m09_frozen::ExactBytes accepted_dependencies;
    m08_frozen::AmountConfig amount;
    std::array<bool,4> requested{};
    double independent_ceiling_db_tp{};
    bool exact_current_endpoint_safety_attested{};
    bool file_based_lra_policy_attested{};
    bool exact_event_context{};
    m10_frozen::AuthoritativeCostInput constraints;
};
struct NativeQtSearchWorkerResult final {
    std::shared_ptr<const OriginalPreparedReferenceNativeFacts> originals;
    NativeAutoMatchWorkerResult result;
    [[nodiscard]] constexpr bool authorizes_product_publication()const noexcept{return false;}
};
// Worker reads only immutable capture/policy/UUID allocator. The
// original Qt owner must revalidate the capture on return before M12.
[[nodiscard]] inline NativeQtSearchWorkerResult run_native_qt_search_worker(
    const NativeQtSearchCapture& captured,const NativeQtSearchPolicy& policy,
    m09_frozen::NativeOwnerAllocations::AllocateUuid allocate_uuid,
    std::span<const core::Uuid> protected_ids,
    const std::atomic<bool>* cancellation=nullptr)
{
    NativeQtSearchWorkerResult out;
    const auto cancelled=[&](){return cancellation&&cancellation->load(std::memory_order_acquire);};
    if(cancelled()){out.result.phase=NativeAutoMatchWorkerPhase::cancelled;return out;}
    if(!captured.execution_registry||!captured.current.job.fixed_only()||
       !m10_frozen::valid_native_definition_policy(policy.definitions)||
       policy.exact_generation.empty()||policy.generation_and_policy.empty()||
       policy.accepted_dependencies.empty())return out;
    auto pair=measure_product_qt_original_pair_native_worker(captured.current,
        captured.common_spec.prepared,captured.common_spec.reference,captured.common_spec.common,
        policy.definitions.exact_density_floor,policy.file_based_lra_policy_attested,
        policy.file_based_lra_policy_attested,cancellation);
    if(cancelled()){out.result.phase=NativeAutoMatchWorkerPhase::cancelled;return out;}
    if(!pair){out.result.phase=NativeAutoMatchWorkerPhase::unavailable_fixed;return out;}
    out.originals=std::make_shared<const OriginalPreparedReferenceNativeFacts>(std::move(*pair.value()));
    const auto owners=captured.native_worker_scope();
    // A live active final P2 is AFTER the insertion point. Its output
    // cannot attest an exact pre-terminal Stereo/M-S insertion width.
    const auto manual=owners.frozen_manual.instances();
    const bool exact_fixed_stereo_insertion=std::none_of(manual.begin(),manual.end(),[](const auto& n){
        return n.active()&&n.module_type_id()=="rgsml.dsp.streaming-true-peak-limiter";
    });
    NativeAutoMatchWorkerInputs input{owners,*out.originals,captured.common_spec.prepared,
        policy.exact_generation,policy.generation_and_policy,captured.current.job.ticket_identity(),
        captured.linked_revision,policy.amount,policy.requested,policy.independent_ceiling_db_tp,
        policy.exact_current_endpoint_safety_attested,policy.file_based_lra_policy_attested,
        policy.exact_event_context,exact_fixed_stereo_insertion,{},policy.constraints};
    out.result=run_native_automatch_with_owner_allocations(std::move(input),policy.definitions,
        std::move(allocate_uuid),protected_ids,policy.accepted_dependencies,cancellation);
    return out;
}
} // namespace rgsml::app::l3_automatch
