#pragma once
// L3-M12 immutable bridge to the EXISTING Product PreviewJob contract.
// This is a nonpublishing, read-only preflight: ProjectSession must
// acquire a real owner transaction before future live installation.
// The worker never chooses a source, ref, DSP, P2 value or M17 ceiling.
#include "l3_m12_restored_native_render.hpp"
#include "l3_m12_project_transaction_draft.hpp"
#include "mastering_preview_controller.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
struct M12ImmutableProductPreviewTicket final {
    std::shared_ptr<const RestoredM12NativeReplayPlan> replay;
    std::shared_ptr<const render::RenderResult> exact_prepared_owner;
    // Structural revisions can advance with identical node parameters.
    std::uint64_t exact_manual_chain_revision{};
    std::vector<dsp::ModuleInstance> exact_manual_nodes;
    std::vector<dsp::ModuleExecutionBinding> exact_manual_bindings;
    [[nodiscard]] constexpr bool authorizes_product_installation() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool authorizes_m12_receipt() const noexcept {
        return false;
    }
    [[nodiscard]] constexpr bool authorizes_m13_verified() const noexcept {
        return false;
    }
};
[[nodiscard]] inline core::Result<M12ImmutableProductPreviewTicket>
prepare_m12_immutable_product_preview_ticket(
    std::shared_ptr<const RestoredM12NativeReplayPlan> replay,
    std::shared_ptr<const render::RenderResult> exact_current_prepared,
    const dsp::ProcessingChain& current_manual,
    std::span<const dsp::ModuleExecutionBinding> current_manual_bindings)
{
    using R=core::Result<M12ImmutableProductPreviewTicket>;
    const auto fail=[](const char* reason){
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!replay||!replay->registry_owner||!exact_current_prepared||
       current_manual.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       // A new Product structural revision cannot borrow an old
       // materialization even when the modules and values match exactly.
       replay->manual_prefix.revision()!=current_manual.revision()||
       (replay->terminal_p2 &&
        replay->terminal_p2->revision()!=current_manual.revision())||
       current_manual.instances().size()!=current_manual_bindings.size())
        return fail("L3_M12_WORKER_NO_EXACT_PRODUCT_OWNERS");
    const auto nodes=current_manual.instances();
    const auto prefix=replay->manual_prefix.instances();
    const auto has_terminal=replay->terminal_p2.has_value();
    if(nodes.size()!=prefix.size()+(has_terminal?1U:0U)||
       replay->manual_bindings.size()!=prefix.size()||
       replay->terminal_p2_bindings.size()!=(has_terminal?1U:0U)||
       !std::equal(nodes.begin(),nodes.begin()+prefix.size(),prefix.begin())||
       !std::equal(current_manual_bindings.begin(),
                   current_manual_bindings.begin()+prefix.size(),
                   replay->manual_bindings.begin()))
        return fail("L3_M12_WORKER_PROTECTED_FIXED_CHANGED");
    if(has_terminal&&
       (replay->terminal_p2->instances().size()!=1U||
        nodes.back()!=replay->terminal_p2->instances().front()||
        current_manual_bindings.back()!=replay->terminal_p2_bindings.front()))
        return fail("L3_M12_WORKER_TERMINAL_CHANGED");
    const auto prepared=exact_current_prepared->view();
    if(prepared.absolute_start_frame().value()!=0||
       prepared.frame_count().value()<=0||
       prepared.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE)
        return fail("L3_M12_WORKER_PREPARED_INVALID");
    return R::success(M12ImmutableProductPreviewTicket{
        std::move(replay),std::move(exact_current_prepared),
        current_manual.revision(),{nodes.begin(),nodes.end()},
        {current_manual_bindings.begin(),current_manual_bindings.end()}});
}
// Prepublication Product seam: bind one accepted immutable Project
// after-state to its exact live precommit snapshot and the one PREPARED
// PCM owner. No second chain, implicit DSP re-selection or adoption.
// ProductSession must still acquire its exclusive Qt mutation lease,
// recheck the before-state, install the whole project atomically and
// issue a real receipt. This function intentionally cannot do that.
[[nodiscard]] inline core::Result<M12ImmutableProductPreviewTicket>
prepare_m12_product_prepublication_preview_ticket(
    const project::ProjectSnapshot& current_project,
    const M12PreparedProjectTransaction& uncommitted,
    std::shared_ptr<const dsp::ModuleRegistry> registry,
    core::Uuid current_manual_chain_id,
    const dsp::ProcessingChain& current_manual,
    std::span<const dsp::ModuleExecutionBinding> current_manual_bindings,
    std::shared_ptr<const render::RenderResult> current_prepared,
    bool exact_product_owner_epoch_and_accepted_diff_attested)
{
    using R=core::Result<M12ImmutableProductPreviewTicket>;
    const auto fail=[](const char* reason) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,reason});
    };
    if(!exact_product_owner_epoch_and_accepted_diff_attested||
       !uncommitted.matches_exact_precommit(current_project)||
       !registry||!current_prepared||
       uncommitted.plan_id.is_nil()||
       uncommitted.controller_id.is_nil()||
       uncommitted.ref_linked_chain_id.is_nil()||
       uncommitted.accepted_plan_checksum.empty()||
       uncommitted.pre_commit_project_revision_dependency.empty())
        return fail("L3_M12_PRODUCT_PRECOMMIT_OWNER_STALE");
    const auto& before=current_project.document();
    const auto& candidate=uncommitted.candidate_not_yet_committed.document();
    if(candidate.projectId!=before.projectId||
       candidate.sourceResourceId!=before.sourceResourceId||
       candidate.references.activeReferenceId!=
           before.references.activeReferenceId||
       candidate.pipeline.masteringChainId!=
           before.pipeline.masteringChainId||
       !candidate.pipeline.masteringChainId||
       *candidate.pipeline.masteringChainId!=current_manual_chain_id)
        return fail("L3_M12_PRODUCT_PRECOMMIT_PROJECT_CHANGED");
    // A prepared C++ carrier is not a Product mutation lease. It cannot
    // use REF_LINKED as camouflage for changing output choices, history,
    // ordinary MANUAL/other chains, resource locators or attachments.
    // Remove ONLY the exact controller-owned MASTER REF segment for a
    // whole-document comparison; the linked segment is checked below by
    // the original strict decoder. Other REF segments are never stripped.
    auto beforeUnowned=before;
    auto afterUnowned=candidate;
    const auto remove_owned=[](project::ProjectDocument& doc) {
        std::erase_if(doc.chains,[](const project::Chain& chain) {
            return chain.stage=="MASTER"&&chain.segment=="REF_LINKED";
        });
    };
    remove_owned(beforeUnowned);
    remove_owned(afterUnowned);
    const auto beforeMeta=QJsonDocument::fromJson(
        QByteArray::fromStdString(before.referenceMatch.canonical_utf8()));
    const auto afterMeta=QJsonDocument::fromJson(
        QByteArray::fromStdString(candidate.referenceMatch.canonical_utf8()));
    if(!beforeMeta.isObject()||!afterMeta.isObject())
        return fail("L3_M12_PRODUCT_PRECOMMIT_METADATA_CORRUPT");
    auto beforeOther=beforeMeta.object();
    auto afterOther=afterMeta.object();
    // An earlier materialization cannot be erased or usurped just by
    // making the non-REF Project envelope match. Validate its persisted
    // controller/plan with the SAME canonical decoder before replacing it.
    const bool existing_ref=std::any_of(
        before.chains.begin(),before.chains.end(),
        [](const project::Chain& chain) {
            return chain.stage=="MASTER"&&chain.segment=="REF_LINKED";
        });
    if(existing_ref) {
        const auto prior=decode_exact_m12_ref_linked_product_state(
            before,registry);
        if(!prior||prior.value()->controller_id!=uncommitted.controller_id||
           prior.value()->persisted_linked_chain_id!=
               uncommitted.ref_linked_chain_id)
            return fail("L3_M12_PRODUCT_PRECOMMIT_PRIOR_OWNER_CONFLICT");
    } else if(beforeOther.contains(QStringLiteral("materialization"))) {
        return fail("L3_M12_PRODUCT_PRECOMMIT_ORPHANED_PRIOR_PLAN");
    }
    beforeOther.remove(QStringLiteral("materialization"));
    afterOther.remove(QStringLiteral("materialization"));
    if(beforeOther!=afterOther)
        return fail("L3_M12_PRODUCT_PRECOMMIT_FOREIGN_META_CHANGED");
    beforeUnowned.referenceMatch=project::OpaqueJsonValue::empty_object();
    afterUnowned.referenceMatch=project::OpaqueJsonValue::empty_object();
    const auto safeBefore=project::ProjectSnapshot::create(
        std::move(beforeUnowned),
        {current_project.optional_entries().begin(),
         current_project.optional_entries().end()});
    const auto safeAfter=project::ProjectSnapshot::create(
        std::move(afterUnowned),
        {uncommitted.candidate_not_yet_committed.optional_entries().begin(),
         uncommitted.candidate_not_yet_committed.optional_entries().end()});
    if(!safeBefore||!safeAfter||
       !m12_exact_project_snapshot_equal(
           *safeBefore.value(),*safeAfter.value()))
        return fail("L3_M12_PRODUCT_PRECOMMIT_FIXED_PROJECT_CHANGED");
    // Decode again from the ACTUAL persistent Project carrier, not from
    // caller-supplied typed nodes. Missing factories or mismatched
    // algorithm/parameter schemas must fail, never silently drop a DSP.
    auto restored=decode_exact_m12_ref_linked_product_state(
        candidate,registry);
    if(!restored)return R::failure(*restored.error());
    if(restored.value()->plan_id!=uncommitted.plan_id||
       restored.value()->controller_id!=uncommitted.controller_id||
       restored.value()->persisted_linked_chain_id!=
           uncommitted.ref_linked_chain_id||
       restored.value()->exact_plan_checksum!=
           uncommitted.accepted_plan_checksum||
       restored.value()->exact_source_reference_scope_revision!=
           uncommitted.pre_commit_project_revision_dependency)
        return fail("L3_M12_PRODUCT_PRECOMMIT_PLAN_SUBSTITUTED");
    auto replay=prepare_restored_m12_native_replay_plan(
        candidate,*restored.value(),current_manual_chain_id,
        current_manual,current_manual_bindings,true);
    if(!replay)return R::failure(*replay.error());
    return prepare_m12_immutable_product_preview_ticket(
        std::make_shared<const RestoredM12NativeReplayPlan>(
            std::move(*replay.value())),
        std::move(current_prepared),current_manual,
        current_manual_bindings);
}
[[nodiscard]] inline core::Result<render::RenderResult>
render_m12_exact_product_preview_job(
    const M12ImmutableProductPreviewTicket& ticket,
    const MasteringPreviewController::PreviewJob& job,
    core::FrameCount maximum_block_frames)
{
    using R=core::Result<render::RenderResult>;
    if(!ticket.replay||!job.generation||
       !job.registry||!job.preparedSnapshot||
       job.registry!=ticket.replay->registry_owner||
       job.preparedSnapshot!=ticket.exact_prepared_owner||
       job.chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       job.chain.revision()!=ticket.exact_manual_chain_revision||
       job.chain.instances().size()!=ticket.exact_manual_nodes.size()||
       !std::equal(job.chain.instances().begin(),
                   job.chain.instances().end(),
                   ticket.exact_manual_nodes.begin())||
       job.bindings!=ticket.exact_manual_bindings)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M12_WORKER_PREVIEW_OWNER_OR_MANUAL_STALE"});
    std::unique_ptr<render::linked_activity::LinkedActivityRenderCapture> activity;
    auto result=render_restored_m12_full_native_pcm_once(
        *ticket.replay,*ticket.exact_prepared_owner,maximum_block_frames,
        nullptr,nullptr,&activity);
    if(result&&activity)
        static_cast<void>(result.value()->adopt_linked_activity(
            std::move(activity),ticket.replay->exact_plan_checksum));
    return result;
}
} // namespace rgsml::app::l3_automatch::m12_frozen
