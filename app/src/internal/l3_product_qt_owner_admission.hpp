#pragma once
// Read-only Qt Product admission for one real L3 offline candidate.
// No UI state, project write, preview publication, exporter authorization
// or M17 terminal modification. This is intentionally an owner-thread
// adapter to the existing M18 fanout and authentic M21 PublicationInbox.
#include "l3_product_offline_trial_job.hpp"
#include "m21_product_owner_analysis.hpp"
#include "m21_product_publication_inbox.hpp"
#include "l2_automatch_evidence_bridge.hpp"
#include <QThread>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <new>
#include <span>
#include <vector>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch {
class ProductQtOfflineAdmission final {
public:
    using Product=internal::M21ProductOwnerAnalysis;
    using Controllers=Product::Controllers;
    using Fanout=Product::Fanout;
    using Inbox=internal::M21ProductPublicationInbox;
    struct Accepted final {
        Product::Capture render;
        Product::GoldCapture gold;
        l2_minimal::AutoMatchEvidencePair starting_evidence;
        m21_private::Snapshot authenticated_reference;
        OfflineProductTrialJob job;
        // The job may be passed to a numerical worker. Accepted and its
        // mutable-QObject freshness methods remain on the Qt owner thread.
    };

    [[nodiscard]] static core::Result<Accepted> capture_gain(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const dsp::ModuleInstanceId& gain_id,double gain_db,
        std::string ticket_identity,core::FrameCount max_block)
    {
        using R=core::Result<Accepted>;
        if(QThread::currentThread()!=product.session.thread()
           ||QThread::currentThread()!=selector.thread()
           ||QThread::currentThread()!=gold.thread()
           ||!Product::Binding::installed(product))
            return R::failure(fail("L3_OWNER_ADMISSION_REQUIRES_QT_OWNER"));
        auto owner=Product::capture(product,fanout,selector);
        if(!owner)return R::failure(*owner.error());
        auto reference=Product::capture_gold(product,fanout,selector,gold);
        if(!reference)return R::failure(*reference.error());
        if(owner.value()->owner.epoch!=reference.value()->owner.epoch
           ||owner.value()->owner.identity!=reference.value()->owner.identity
           ||owner.value()->source.same_resource_identity(
               reference.value()->gold)
           ||!owner.value()->prepared
           ||owner.value()->prepared->view().absolute_start_frame().value()!=0
           ||owner.value()->prepared->view().frame_count().value()
               !=owner.value()->source_full_frames)
            return R::failure(fail("L3_OWNER_ADMISSION_SOURCE_OR_REFERENCE_STALE"));
        // The real current M21 comparison is the only baseline score.
        // Do not manufacture a GOLD waveform or rate-match Gold to Source.
        const auto view=inbox.view_current(
            product,fanout,selector,gold);
        auto evidence=l2_minimal::make_evidence_pair(
            view,m21_private::Column::processed);
        if(!evidence.evidence)
            return R::failure(fail("L3_OWNER_ADMISSION_M21_NOT_READY"));
        // Copy the AUTHENTIC current independent GOLD analysis exactly
        // once. The worker may reuse this immutable reference across
        // candidates without touching the picker, Qt or doing reference
        // FFT again. It cannot turn this copy into live Product authority.
        const auto& reference_column=
            view.columns[static_cast<std::size_t>(m21_private::Column::gold)];
        if(!reference_column
           ||reference_column->lifecycle!=m21_private::Lifecycle::current
           ||reference_column->generation_token!=
               reference.value()->owner.epoch
           ||reference_column->resource_identity
                !=evidence.evidence->reference_identity)
            return R::failure(
                fail("L3_OWNER_ADMISSION_REFERENCE_ANALYSIS_STALE"));
        const auto& chain=product.upstream.chain.chain();
        const auto bindings=product.upstream.chain.execution_bindings();
        if(bindings!=owner.value()->manual_bindings
           ||chain.revision()!=owner.value()->owner.identity.chain_revision)
            return R::failure(fail("L3_OWNER_ADMISSION_CHAIN_MISMATCH"));
        auto gain=make_gain_trial(chain,bindings,gain_id,gain_db,
                                  std::move(ticket_identity));
        if(!gain)return R::failure(
            fail("L3_OWNER_ADMISSION_INVALID_GAIN_PROPOSAL"));
        const OfflineProductOwnerStamp stamp={
            owner.value()->owner.epoch,
            owner.value()->owner.identity.source_generation,
            reference.value()->selection_revision,
            owner.value()->source.provider_id(),
            owner.value()->source.locator(),
            reference.value()->gold.provider_id(),
            reference.value()->gold.locator()};
        auto job=OfflineProductTrialJob::create(
            stamp,owner.value()->prepared,
            product.upstream.chain.registry_snapshot(),
            chain,bindings,*gain,max_block);
        if(!job)return R::failure(*job.error());
        // Recheck every owner in the same owner-thread operation before
        // lending immutable bytes to a worker. Nothing is published yet.
        if(!Product::still_current(product,fanout,selector,*owner.value())
           ||!Product::gold_still_current(
               product,fanout,selector,gold,*reference.value())
           ||!l2_minimal::matches_current(
               *evidence.evidence,inbox.view_current(
                   product,fanout,selector,gold))
           ||product.upstream.chain.execution_bindings()!=bindings)
            return R::failure(fail("L3_OWNER_ADMISSION_CHANGED_DURING_CAPTURE"));
        return R::success(Accepted{std::move(*owner.value()),
             std::move(*reference.value()),std::move(*evidence.evidence),
             *reference_column,std::move(*job.value())});
    }

    // Call after the worker has completed and BEFORE any M21 candidate
    // admission. This requires the authentic immutable Product and GOLD
    // owners to remain current, including parameter-only DSP changes.
    [[nodiscard]] static bool still_current(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Accepted& accepted)
    {
        if(QThread::currentThread()!=product.session.thread()
           ||QThread::currentThread()!=selector.thread()
           ||QThread::currentThread()!=gold.thread()
           ||!Product::still_current(
               product,fanout,selector,accepted.render)
           ||!Product::gold_still_current(
               product,fanout,selector,gold,accepted.gold)
           ||!l2_minimal::matches_current(
               accepted.starting_evidence,
               inbox.view_current(product,fanout,selector,gold)))
            return false;
        const auto& current=product.upstream.chain.chain();
        const auto bindings=product.upstream.chain.execution_bindings();
        const OfflineProductOwnerStamp stamp={
            accepted.render.owner.epoch,
            accepted.render.owner.identity.source_generation,
            gold.selection_revision(),
            accepted.render.source.provider_id(),
            accepted.render.source.locator(),
            accepted.gold.gold.provider_id(),
            accepted.gold.gold.locator()};
        return accepted.job.still_current(
            stamp,selector.prepared_realization_snapshot(),current,bindings);
    }

    struct GainProposal final {
        double db{};
        std::string ticket_identity;
    };

    // One real M18/Product capture can feed multiple immutable worker
    // attempts. Construct value snapshots on the Qt owner, without ever
    // changing the MANUAL chain or each proposal's permitted gain range.
    // The owned registry is reused across the whole batch (not rebuilt 800x).
    [[nodiscard]] static core::Result<std::vector<OfflineProductTrialJob>>
    create_gain_batch_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Accepted& accepted,const dsp::ModuleInstanceId& gain_id,
        std::span<const GainProposal> proposals,
        core::FrameCount max_block)
    {
        using R=core::Result<std::vector<OfflineProductTrialJob>>;
        if(proposals.empty()||proposals.size()>800
           ||!still_current(product,fanout,selector,gold,inbox,accepted))
            return R::failure(
                fail("L3_OWNER_BATCH_REQUIRES_CURRENT_SOURCE_REFERENCE"));
        const auto& current=product.upstream.chain.chain();
        const auto bindings=product.upstream.chain.execution_bindings();
        try {
            std::vector<OfflineProductTrialJob> out;
            out.reserve(proposals.size());
            for(const auto& proposal:proposals) {
                auto trial=make_gain_trial(current,bindings,gain_id,
                                           proposal.db,
                                           proposal.ticket_identity);
                if(!trial)return R::failure(
                    fail("L3_OWNER_BATCH_INVALID_TYPED_GAIN_PROPOSAL"));
                auto job=OfflineProductTrialJob::create(
                    accepted.job.stamp(),accepted.job.prepared_owner(),
                    accepted.job.registry_snapshot(),current,bindings,
                    *trial,max_block);
                if(!job)return R::failure(*job.error());
                out.push_back(std::move(*job.value()));
            }
            if(!still_current(product,fanout,selector,gold,inbox,accepted))
                return R::failure(
                    fail("L3_OWNER_BATCH_CHANGED_DURING_COPY"));
            return R::success(std::move(out));
        }catch(const std::bad_alloc&) {
            return R::failure(fail("L3_OWNER_BATCH_ALLOCATION_FAILED"));
        }
    }

    struct DynamicEqProposal final {
        dsp::ModuleInstanceId module_id;
        core::Uuid band_id;
        DynamicEqScalarCoordinate coordinate{};
        double value{};
        std::string ticket_identity;
    };

    [[nodiscard]] static core::Result<std::vector<OfflineProductTrialJob>>
    create_dynamic_eq_batch_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Accepted& accepted,
        std::span<const DynamicEqProposal> proposals,
        core::FrameCount max_block)
    {
        using R=core::Result<std::vector<OfflineProductTrialJob>>;
        if(proposals.empty()||proposals.size()>800
           ||!still_current(product,fanout,selector,gold,inbox,accepted))
            return R::failure(
                fail("L3_OWNER_DYNAMIC_EQ_BATCH_REQUIRES_CURRENT_EPOCH"));
        const auto& chain=product.upstream.chain.chain();
        const auto bindings=product.upstream.chain.execution_bindings();
        try {
            std::vector<OfflineProductTrialJob> out;
            out.reserve(proposals.size());
            for(const auto& proposal:proposals) {
                auto trial=make_dynamic_eq_scalar_trial(
                    chain,bindings,proposal.module_id,proposal.band_id,
                    proposal.coordinate,proposal.value,
                    proposal.ticket_identity);
                if(!trial)return R::failure(
                    fail("L3_OWNER_DYNAMIC_EQ_BATCH_INVALID_COORDINATE"));
                auto job=OfflineProductTrialJob::create(
                    accepted.job.stamp(),accepted.job.prepared_owner(),
                    accepted.job.registry_snapshot(),chain,bindings,
                    *trial,max_block);
                if(!job)return R::failure(*job.error());
                out.push_back(std::move(*job.value()));
            }
            if(!still_current(product,fanout,selector,gold,inbox,accepted))
                return R::failure(
                    fail("L3_OWNER_DYNAMIC_EQ_BATCH_STALE_DURING_COPY"));
            return R::success(std::move(out));
        }catch(const std::bad_alloc&) {
            return R::failure(
                fail("L3_OWNER_DYNAMIC_EQ_BATCH_ALLOCATION_FAILED"));
        }
    }

    [[nodiscard]] static bool result_eligible(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Accepted& accepted,const render::RenderResult& result)
    {
        return still_current(product,fanout,selector,gold,inbox,accepted)
            &&accepted.job.result_has_expected_scope(result);
    }
private:
    [[nodiscard]] static core::Error fail(const char* message) {
        return core::Error{core::ErrorCode::InvalidState,message};
    }
};
} // namespace rgsml::app::l3_automatch
