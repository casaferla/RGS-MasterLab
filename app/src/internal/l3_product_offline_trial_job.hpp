#pragma once
// L3 offline AutoMatch: an immutable, in-memory PRODUCT-candidate render.
// Construct on the Qt Product owner from true PREPARED and chain snapshots;
// execute on a worker without QObject, QML, file I/O or Product mutation.
// This value layer does not authenticate the Qt owner by itself. The owner
// adapter must supply a fresh M18/SOURCE/REFERENCE epoch stamp, recheck it
// before accepting any result, and keep the actual M21 authority separate.
#include "l3_automatch_typed_gain_trial.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_result.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
struct OfflineProductOwnerStamp final {
    std::uint64_t m18_owner_epoch{};
    std::uint64_t source_generation{};
    std::uint64_t reference_selection_revision{};
    std::string source_resource_identity;
    std::string reference_resource_identity;
    friend bool operator==(const OfflineProductOwnerStamp&,
                           const OfflineProductOwnerStamp&)=default;
    [[nodiscard]] bool populated() const noexcept {
        return m18_owner_epoch>0 &&source_generation>0
            &&reference_selection_revision>0
            &&!source_resource_identity.empty()
            &&!reference_resource_identity.empty()
            &&source_resource_identity!=reference_resource_identity;
    }
};

class OfflineProductTrialJob final {
public:
    // An accepted BoundGainTrial is the full typed-graph copy. None of
    // the PRODUCT registry, chain or bindings may be mutated in this call.
    [[nodiscard]] static core::Result<OfflineProductTrialJob> create(
        OfflineProductOwnerStamp stamp,
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const dsp::ModuleRegistry> product_registry,
        const dsp::ProcessingChain& live_chain,
        std::span<const dsp::ModuleExecutionBinding> live_bindings,
        const BoundGainTrial& trial,core::FrameCount maximum_block_frames)
    {
        using R=core::Result<OfflineProductTrialJob>;
        if(!stamp.populated() ||!prepared ||!product_registry
           ||!trial.still_matches(live_chain,live_bindings)
           ||trial.ticket_identity.empty()
           ||maximum_block_frames.value()<=0
           ||prepared->view().absolute_start_frame().value()!=0
           ||prepared->view().frame_count().value()<=0
           ||prepared->view().timebase().frame_domain_id()
                !=audio::FrameDomainId::SOURCE_PROCESSING_RATE
           ||live_chain.context()!=dsp::ProcessingChainContext{
                dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL})
            return R::failure(failed("L3_OFFLINE_PRODUCT_INVALID_SNAPSHOT"));
        const auto instances=live_chain.instances();
        if(instances.size()!=trial.candidate_bindings.size())
            return R::failure(failed("L3_OFFLINE_PRODUCT_MISSING_BINDING"));
        for(std::size_t i=0;i<instances.size();++i)
            if(!binding_matches_instance(instances[i],
                                         trial.candidate_bindings[i]))
                return R::failure(failed("L3_OFFLINE_PRODUCT_TYPED_BINDING_MISMATCH"));
        try {
            // Opt-in replaces the L1 factory-less Dynamic EQ slot. P2 and
            // all other actual Product registration identities are copied
            // as-is; there is no synthetic 5-DSP fallback registry.
            const auto already=product_registry->has_factory(
                "rgsml.dsp.dynamic-eq");
            auto opt=already
                ?core::Result<dsp::ModuleRegistry>::success(*product_registry)
                :dsp::ModuleRegistry::with_dynamic_eq_offline(*product_registry);
            if(!opt)return R::failure(*opt.error());
            auto registry=std::make_shared<const dsp::ModuleRegistry>(
                std::move(*opt.value()));
            auto copy=dsp::ProcessingChain::restore(
                *registry,live_chain.context(),live_chain.revision(),
                {instances.begin(),instances.end()});
            if(!copy)return R::failure(*copy.error());
            return R::success(OfflineProductTrialJob(
                std::move(stamp),std::move(prepared),std::move(registry),
                std::move(*copy.value()),trial.baseline_bindings,
                trial.candidate_bindings,trial.ticket_identity,
                maximum_block_frames));
        }catch(const std::bad_alloc&) {
            return R::failure(failed("L3_OFFLINE_PRODUCT_SNAPSHOT_OOM"));
        }
    }

    // Worker-only execution: exact full-source RenderPreview, no substitute
    // DSP engine. Failure never yields a partial product candidate.
    [[nodiscard]] core::Result<render::RenderResult> execute() const {
        using R=core::Result<render::RenderResult>;
        if(!prepared_||!registry_)
            return R::failure(failed("L3_OFFLINE_PRODUCT_NO_OWNER"));
        const auto view=prepared_->view();
        auto request=render::RenderRequest::create(
            view,view.absolute_range(),chain_,candidate_bindings_,
            maximum_block_frames_);
        if(!request)return R::failure(*request.error());
        return render::render_preview(*request.value(),*registry_);
    }
    // Caller MUST obtain all current arguments from live Qt Product owner
    // when the worker result returns, never from the worker itself. This
    // rejects SOURCE, reference selection, chain topology and PARAMETER-ONLY
    // changes. It grants no permission to publish or export any PCM.
    [[nodiscard]] bool still_current(
        const OfflineProductOwnerStamp& live_stamp,
        const std::shared_ptr<const render::RenderResult>& live_prepared,
        const dsp::ProcessingChain& live_chain,
        std::span<const dsp::ModuleExecutionBinding> live_bindings) const
    {
        if(!stamp_.populated()||stamp_!=live_stamp
           ||prepared_!=live_prepared
           ||chain_.context()!=live_chain.context()
           ||chain_.revision()!=live_chain.revision()
           ||live_bindings.size()!=baseline_bindings_.size())
            return false;
        const auto current=live_chain.instances();
        const auto baseline=chain_.instances();
        return current.size()==baseline.size()
            &&std::equal(current.begin(),current.end(),baseline.begin())
            &&std::equal(live_bindings.begin(),live_bindings.end(),
                          baseline_bindings_.begin());
    }
    // Require complete real output and owner recheck before a private
    // measured trial is eligible for M19/M20/M21 analysis on a worker.
    [[nodiscard]] bool result_has_expected_scope(
        const render::RenderResult& result) const noexcept {
        return prepared_ &&result.view().absolute_range()
                ==prepared_->view().absolute_range()
            &&result.view().format()==prepared_->view().format()
            &&result.chain_revision()==chain_.revision();
    }
    [[nodiscard]] const OfflineProductOwnerStamp& stamp() const noexcept {
        return stamp_;
    }
    [[nodiscard]] const std::string& ticket_identity() const noexcept {
        return ticket_identity_;
    }
    [[nodiscard]] const dsp::ProcessingChain& chain() const noexcept {
        return chain_;
    }
    [[nodiscard]] const std::vector<dsp::ModuleExecutionBinding>&
    candidate_bindings() const noexcept{return candidate_bindings_;}

private:
    OfflineProductTrialJob(
        OfflineProductOwnerStamp stamp,
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const dsp::ModuleRegistry> registry,
        dsp::ProcessingChain chain,
        std::vector<dsp::ModuleExecutionBinding> baseline,
        std::vector<dsp::ModuleExecutionBinding> candidate,
        std::string fingerprint,core::FrameCount max_frames)
        :stamp_(std::move(stamp)),prepared_(std::move(prepared)),
         registry_(std::move(registry)),chain_(std::move(chain)),
         baseline_bindings_(std::move(baseline)),
         candidate_bindings_(std::move(candidate)),
         ticket_identity_(std::move(fingerprint)),
         maximum_block_frames_(max_frames) {}
    [[nodiscard]] static core::Error failed(const char* code) {
        return core::Error{core::ErrorCode::InvalidState,code};
    }
    OfflineProductOwnerStamp stamp_;
    std::shared_ptr<const render::RenderResult> prepared_;
    // The ProcessingChain stores a NON-owning pointer into this registry.
    // Member order guarantees that the registry outlives chain_.
    std::shared_ptr<const dsp::ModuleRegistry> registry_;
    dsp::ProcessingChain chain_;
    std::vector<dsp::ModuleExecutionBinding> baseline_bindings_;
    std::vector<dsp::ModuleExecutionBinding> candidate_bindings_;
    std::string ticket_identity_;
    core::FrameCount maximum_block_frames_;
};
} // namespace rgsml::app::l3_automatch
