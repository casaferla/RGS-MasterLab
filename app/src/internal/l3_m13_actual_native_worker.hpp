#pragma once
// Worker-side composition of the actual independent rendering and the
// accepted native M10 prediction. The Qt session must re-admit this
// completion on the current Product owner before sealing a diagnostic.
#include "l3_m13_actual_native_assessment.hpp"
#include "l3_m13_owned_native_spec.hpp"
#include "l3_m10_exact_native_graph_content.hpp"
#include "l3_m10_original_definition_authority.hpp"
#include "l3_m10_p2_actual_m16_guard.hpp"
#include <memory>
#include <utility>

namespace rgsml::app::l3_automatch::m13_frozen {
struct M13ActualNativeWorkerInputs final {
    M13ImmutableProductFreshProbeRequest request;
    M13OwnedNativeSpec native_spec;
    std::shared_ptr<const OriginalPreparedReferenceNativeFacts> independent_pair;
    OfflineProductOwnerStamp source_reference_owner;
    m10_frozen::P2OriginalRealNativeTriple original_definitions;
    std::string exact_m07_generation;
    std::string accepted_scope_revision;
    m10_frozen::MeasuredOrderKey accepted_winner;
    std::string accepted_winner_checksum;
    std::string accepted_protection_checksum;
    bool accepted_winner_evidence_attested{};
    bool independent_source_reference_and_scopes_attested{};
    bool file_based_lra_policy_attested{};
    bool endpoint_and_independent_safety_attested{};
    MandatoryHardPredicate other_hard_safety{MandatoryHardPredicate::unavailable};
};

class M13ActualNativeWorkerCompletion final {
public:
    [[nodiscard]] const M13FreshProductMasterProbe& fresh()const noexcept{return fresh_;}
    [[nodiscard]] const M13ProvisionalAssessment& assessment()const noexcept{return assessment_;}
    [[nodiscard]] const OfflineProductOwnerStamp& source_reference_owner()const noexcept{return owner_;}
    [[nodiscard]] constexpr bool authorizes_verified()const noexcept{return false;}
    [[nodiscard]] static core::Result<M13ActualNativeWorkerCompletion> execute(
        M13ActualNativeWorkerInputs input,core::FrameCount maximum_block_frames,
        const std::atomic<bool>* cancellation=nullptr)
    {
        using R=core::Result<M13ActualNativeWorkerCompletion>;
        if(!input.independent_pair||
           !original_pair_matches_product_owner(*input.independent_pair,
                                                input.source_reference_owner)||
           !input.accepted_winner.native_prediction||
           !input.accepted_winner_evidence_attested||
           !input.independent_source_reference_and_scopes_attested)
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_MISSING_ACCEPTED_NATIVE_EVIDENCE"});
        const auto& replay=input.request.exact_materialized_preview_owner().replay;
        const auto& prediction=*input.accepted_winner.native_prediction;
        if(!m10_frozen::native_prediction_passes_current_m16(prediction,
                input.request.frozen_independent_m16_ceiling_db_tp()))
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_INDEPENDENT_M16_STALE"});
        if(!prediction.original_definition_authority||
           prediction.exact_definition_authority_content.empty()||
           m10_frozen::exact_original_native_definition_content(*prediction.original_definition_authority)!=
               prediction.exact_definition_authority_content)
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_ORIGINAL_DEFINITIONS_UNAVAILABLE"});
        const m05m06_real::SourceIdentity accepted{
            prediction.candidate_resource_identity,prediction.candidate_generation,true};
        if(!replay||!m10_frozen::valid_key(input.accepted_winner)||
           input.accepted_scope_revision.empty()||
           replay->exact_source_reference_scope_revision!=input.accepted_scope_revision||
           input.accepted_winner_checksum.empty()||
           input.accepted_winner_checksum!=replay->exact_m10_winner_checksum||
           input.accepted_protection_checksum.empty()||
           input.accepted_protection_checksum!=replay->exact_m11_protection_checksum||
           input.exact_m07_generation.empty()||
           input.exact_m07_generation!=prediction.exact_m07_generation||
           !authentic_product_candidate_identity(accepted,input.source_reference_owner,
                                                  prediction.exact_candidate_ticket))
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_ACCEPTED_PREDICTION_STALE"});
        // Reconstruct the exact executed segment identities from the
        // actual persisted M12 replay. Do not accept an unrelated winner
        // with the same user-supplied plan/checksum strings.
        if(!replay->registry_owner)
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_REPLAY_REGISTRY_MISSING"});
        const auto& registry=*replay->registry_owner;
        const auto fixed=m10_frozen::exact_native_graph_content(
            replay->manual_prefix,replay->manual_bindings,registry);
        const auto linked=m10_frozen::exact_native_graph_content(
            replay->reference_linked,replay->reference_bindings,registry);
        std::optional<m10_frozen::ExactCacheBytes> terminal;
        if(replay->terminal_p2)
            terminal=m10_frozen::exact_native_graph_content(
                *replay->terminal_p2,replay->terminal_p2_bindings,registry);
        else {
            auto empty=dsp::ProcessingChain::restore(registry,
                replay->manual_prefix.context(),replay->manual_prefix.revision(),{});
            if(empty)terminal=m10_frozen::exact_native_graph_content(
                *empty.value(),{},registry);
        }
        if(!fixed||!linked||!terminal||
           prediction.exact_linked_graph_content!=*linked||
           prediction.exact_execution_graph_content!=
               m10_frozen::exact_native_execution_segments(
                   maximum_block_frames.value(),*fixed,*linked,*terminal))
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_REPLAY_GRAPH_DIFFERS_FROM_WINNER"});
        const auto& prepared=input.request.exact_materialized_preview_owner().exact_prepared_owner;
        const auto& original=input.independent_pair->prepared.original_m19_m20;
        if(!prepared||
           prepared->view().format().sample_rate().value()!=original.sample_rate_hz||
           prepared->view().frame_count().value()!=original.scope_frame_count||
           prepared->view().format().channel_count()!=original.channels)
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_PREPARED_SCOPE_MISMATCH"});
        auto native_spec=input.native_spec.borrow();
        if(!bind_original_pair_profile_common(native_spec,*input.independent_pair))
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_PROFILE_SCOPE_MISMATCH"});
        auto fresh=run_fresh_m13_product_postmaterialization_probe(
            input.request,maximum_block_frames,std::move(native_spec),cancellation);
        if(!fresh)return R::failure(*fresh.error());
        auto diagnosis=assess_m13_actual_native_completion(
            input.request,*fresh.value(),*input.independent_pair,
            input.source_reference_owner,*prediction.original_definition_authority,
            std::move(input.exact_m07_generation),input.accepted_scope_revision,
            input.accepted_winner,input.accepted_winner_checksum,
            input.accepted_protection_checksum,input.accepted_winner_evidence_attested,
            true,input.independent_source_reference_and_scopes_attested,
            input.file_based_lra_policy_attested,
            input.endpoint_and_independent_safety_attested,input.other_hard_safety);
        if(cancellation&&cancellation->load(std::memory_order_acquire))
            return R::failure(core::Error{core::ErrorCode::InvalidState,
                "L3_M13_ACTUAL_WORKER_CANCELLED_AFTER_ASSESSMENT"});
        return R::success(M13ActualNativeWorkerCompletion{
            std::move(*fresh.value()),std::move(diagnosis),
            std::move(input.source_reference_owner)});
    }
private:
    M13ActualNativeWorkerCompletion(M13FreshProductMasterProbe fresh,
        M13ProvisionalAssessment diagnosis,OfflineProductOwnerStamp owner)
        :fresh_(std::move(fresh)),assessment_(std::move(diagnosis)),
         owner_(std::move(owner)){}
    M13FreshProductMasterProbe fresh_;
    M13ProvisionalAssessment assessment_;
    OfflineProductOwnerStamp owner_;
};
} // namespace rgsml::app::l3_automatch::m13_frozen
