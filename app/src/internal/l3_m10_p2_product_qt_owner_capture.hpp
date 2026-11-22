#pragma once
// L3 M10/P2: real Qt Product owner -> immutable full-resource offline
// worker. SOURCE/PREPARED and REFERENCE are authenticated separately on
// the UI owner thread. REF_LINKED candidate is a typed, already-validated
// M09/M11 proposal, NEVER a mutation of the user's MANUAL chain.
// No M21 nine-metric scoring, M17 export, project write or result publish.
// This bridge alone does not claim the complete M05/M06 27-slot authority.
#include "l3_m10_p2_real_native_product_job.hpp"
#include "m21_product_owner_analysis.hpp"
#include "audition_source_selector.hpp"
#include "gold_selection_view_model.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <QThread>
#include <cstdint>
#include <memory>
#include <new>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
class ProductQtP2NativeAdmission final {
public:
    using Product=internal::M21ProductOwnerAnalysis;
    using Controllers=Product::Controllers;
    using Fanout=Product::Fanout;
    struct Accepted final {
        // QObject-dependent owner evidence is read only on Qt's thread.
        Product::Capture source;
        Product::GoldCapture reference;
        std::shared_ptr<const dsp::ModuleRegistry> live_registry;
        // The worker owns independent frozen chain/parameter copies
        // and may run on a different thread without touching Qt.
        M10P2RealNativeProductJob job;
        [[nodiscard]] constexpr bool authorizes_m10_m12_or_export()
            const noexcept{return false;}
    };

    [[nodiscard]] static core::Result<Accepted> capture_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,
        const dsp::ProcessingChain& exact_candidate_ref_linked,
        std::span<const dsp::ModuleExecutionBinding> linked_bindings,
        const m10_frozen::BoundM10P2Candidate& typed_m10_p2,
        core::FrameCount max_block_frames,
        bool exact_current_m09_m10_m11_candidate_attested)
    {
        using R=core::Result<Accepted>;
        if(QThread::currentThread()!=product.session.thread()||
           QThread::currentThread()!=selector.thread()||
           QThread::currentThread()!=gold.thread()||
           !Product::Binding::installed(product)||
           !exact_current_m09_m10_m11_candidate_attested)
            return R::failure(error("L3_P2_QT_CAPTURE_REQUIRES_CURRENT_OWNER"));
        auto source=Product::capture(product,fanout,selector);
        if(!source)return R::failure(*source.error());
        auto reference=Product::capture_gold(product,fanout,selector,gold);
        if(!reference)return R::failure(*reference.error());
        if(source.value()->owner.epoch!=reference.value()->owner.epoch||
           source.value()->owner.identity!=reference.value()->owner.identity||
           source.value()->source.same_resource_identity(
               reference.value()->gold)||
           !source.value()->prepared||
           source.value()->source_full_frames<=0||
           source.value()->prepared->view().absolute_start_frame().value()!=0||
           source.value()->prepared->view().frame_count().value()!=
               source.value()->source_full_frames)
            return R::failure(error("L3_P2_QT_CAPTURE_SOURCE_OR_GOLD_STALE"));
        const auto& manual=product.upstream.chain.chain();
        const auto bindings=product.upstream.chain.execution_bindings();
        const auto live_registry=product.upstream.chain.registry_snapshot();
        if(!live_registry||
           bindings!=source.value()->manual_bindings||
           manual.revision()!=source.value()->owner.identity.chain_revision)
            return R::failure(error("L3_P2_QT_CAPTURE_MANUAL_CHANGED"));
        const OfflineProductOwnerStamp stamp{
            source.value()->owner.epoch,
            source.value()->owner.identity.source_generation,
            reference.value()->selection_revision,
            source.value()->source.provider_id(),
            source.value()->source.locator(),
            reference.value()->gold.provider_id(),
            reference.value()->gold.locator()};
        auto job=M10P2RealNativeProductJob::create(
            stamp,source.value()->prepared,live_registry,
            manual,bindings,exact_candidate_ref_linked,linked_bindings,
            typed_m10_p2,max_block_frames,true);
        if(!job)return R::failure(*job.error());
        // Capture can only return an immutable worker after the same
        // Qt owner thread revalidates current SOURCE, GOLD and MANUAL.
        if(!Product::still_current(
             product,fanout,selector,*source.value())||
           !Product::gold_still_current(
             product,fanout,selector,gold,*reference.value())||
           product.upstream.chain.execution_bindings()!=bindings||
           product.upstream.chain.registry_snapshot()!=live_registry)
            return R::failure(
                error("L3_P2_QT_CAPTURE_CHANGED_BEFORE_DISPATCH"));
        return R::success(Accepted{
            std::move(*source.value()),std::move(*reference.value()),
            live_registry,std::move(*job.value())});
    }

    struct BatchCandidate final {
        // These are short-lived references valid only for the
        // synchronous owner-thread capture. Each returned job owns
        // immutable copies of its actual graph and typed parameters.
        const dsp::ProcessingChain* exact_linked{};
        std::span<const dsp::ModuleExecutionBinding> linked_bindings;
        const m10_frozen::BoundM10P2Candidate* selected_p2{};
    };

    // Capture a coherent M10 evaluation batch without walking QML,
    // SOURCE or REFERENCE for every candidate. Zero attempted renders
    // and zero Product mutations occur here. The frozen 800 cap is a
    // maximum, NOT a target or permission to skip native analysis.
    [[nodiscard]] static core::Result<
        std::vector<M10P2RealNativeProductJob>>
    create_batch_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,
        const Accepted& accepted,
        const dsp::ProcessingChain& original_ref_linked,
        std::span<const dsp::ModuleExecutionBinding> original_bindings,
        std::span<const BatchCandidate> candidates,
        core::FrameCount max_block_frames,
        bool exact_current_m09_m10_m11_candidate_batch_attested)
    {
        using R=core::Result<std::vector<M10P2RealNativeProductJob>>;
        if(!exact_current_m09_m10_m11_candidate_batch_attested||
           candidates.empty()||candidates.size()>800U||
           !still_current_on_owner(
               product,fanout,selector,gold,accepted,
               original_ref_linked,original_bindings))
            return R::failure(
                error("L3_P2_QT_BATCH_OWNER_OR_INVENTORY_STALE"));
        try {
            std::vector<M10P2RealNativeProductJob> jobs;
            jobs.reserve(candidates.size());
            std::set<std::string> seen_tickets;
            const auto& manual=product.upstream.chain.chain();
            const auto manual_bindings=
                product.upstream.chain.execution_bindings();
            for(const auto& item:candidates) {
                if(!item.exact_linked||!item.selected_p2||
                   item.selected_p2->typed.base.ticket_identity.empty()||
                   !seen_tickets.insert(
                       item.selected_p2->typed.base.ticket_identity).second)
                    return R::failure(
                        error("L3_P2_QT_BATCH_DUPLICATE_OR_MISSING_CANDIDATE"));
                auto job=M10P2RealNativeProductJob::create(
                    accepted.job.owner_stamp(),accepted.source.prepared,
                    accepted.live_registry,manual,manual_bindings,
                    *item.exact_linked,item.linked_bindings,
                    *item.selected_p2,max_block_frames,true);
                if(!job)return R::failure(*job.error());
                jobs.push_back(std::move(*job.value()));
            }
            if(!still_current_on_owner(
                product,fanout,selector,gold,accepted,
                original_ref_linked,original_bindings)||
               product.upstream.chain.execution_bindings()!=manual_bindings)
                return R::failure(
                    error("L3_P2_QT_BATCH_OWNER_CHANGED_DURING_CAPTURE"));
            return R::success(std::move(jobs));
        }catch(const std::bad_alloc&) {
            return R::failure(error("L3_P2_QT_BATCH_ALLOCATION_FAILED"));
        }
    }

    // MUST run on the original Qt thread after a worker returns, before
    // considering any score, plan or M12 publication. A project reopen,
    // new REFERENCE selection, manual parameter edit, or changed
    // REF_LINKED candidate invalidates the exact worker result.
    [[nodiscard]] static bool still_current_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,
        const Accepted& accepted,
        const dsp::ProcessingChain& current_ref_linked,
        std::span<const dsp::ModuleExecutionBinding> linked_bindings)
    {
        if(QThread::currentThread()!=product.session.thread()||
           QThread::currentThread()!=selector.thread()||
           QThread::currentThread()!=gold.thread()||
           !Product::still_current(
                product,fanout,selector,accepted.source)||
           !Product::gold_still_current(
                product,fanout,selector,gold,accepted.reference)||
           product.upstream.chain.registry_snapshot()!=
               accepted.live_registry)
            return false;
        const auto& manual=product.upstream.chain.chain();
        const auto bindings=product.upstream.chain.execution_bindings();
        const OfflineProductOwnerStamp now{
            accepted.source.owner.epoch,
            accepted.source.owner.identity.source_generation,
            gold.selection_revision(),
            accepted.source.source.provider_id(),
            accepted.source.source.locator(),
            accepted.reference.gold.provider_id(),
            accepted.reference.gold.locator()};
        return accepted.job.still_current(
            now,selector.prepared_realization_snapshot(),
            manual,bindings,current_ref_linked,linked_bindings);
    }

    [[nodiscard]] static bool result_eligible_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,
        const Accepted& accepted,
        const dsp::ProcessingChain& current_ref_linked,
        std::span<const dsp::ModuleExecutionBinding> linked_bindings,
        const render::RenderResult& real_result)
    {
        return still_current_on_owner(
            product,fanout,selector,gold,accepted,
            current_ref_linked,linked_bindings)&&
            accepted.job.result_has_expected_scope(real_result);
    }
private:
    [[nodiscard]] static core::Error error(const char* code) {
        return core::Error{core::ErrorCode::InvalidState,code};
    }
};
} // namespace rgsml::app::l3_automatch
