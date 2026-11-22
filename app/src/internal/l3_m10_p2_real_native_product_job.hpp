#pragma once
// L3-M10 authentic in-memory full-candidate worker for streaming P2.
// Executes exact PREPARED -> protected MANUAL prefix (without P2)
// -> candidate REF_LINKED artistic segment -> final genuine P2.
// No GUI, writer mutation, proxy PCM, WAV Export M17, M12 publication,
// forged 27-feature cost or final M13 verification. Each candidate's
// P2 Ceiling/Drive comes from the current exact M10 typed binding.
#include "l3_m10_p2_canonical_coordinate_port.hpp"
#include "l3_product_offline_trial_job.hpp"
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
// Owned, nonpersistent, exact whole-resource native PCM at the INPUT
// to P2, after both protected MANUAL-fixed and REF_LINKED artistic DSP.
// It can be reused only by a job whose entire upstream graph and owner
// are byte/instance-identical. It cannot authorize partial scoring.
class M10P2ExactPrefix final {
    friend class M10P2RealNativeProductJob;
public:
    [[nodiscard]] constexpr bool authorizes_m10_score() const noexcept {
        return false;
    }
private:
    M10P2ExactPrefix(
        OfflineProductOwnerStamp owner,
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const dsp::ModuleRegistry> registry,
        std::shared_ptr<const render::RenderResult> rendered,
        std::uint64_t manual_revision,
        std::vector<dsp::ModuleInstance> manual_nodes,
        std::vector<dsp::ModuleExecutionBinding> manual_bindings,
        std::uint64_t linked_revision,
        std::vector<dsp::ModuleInstance> linked_nodes,
        std::vector<dsp::ModuleExecutionBinding> linked_bindings,
        core::FrameCount block)
        : owner_(std::move(owner)),prepared_(std::move(prepared)),
          registry_(std::move(registry)),rendered_(std::move(rendered)),
          manual_revision_(manual_revision),
          manual_nodes_(std::move(manual_nodes)),
          manual_bindings_(std::move(manual_bindings)),
          linked_revision_(linked_revision),
          linked_nodes_(std::move(linked_nodes)),
          linked_bindings_(std::move(linked_bindings)),block_(block) {}
    OfflineProductOwnerStamp owner_;
    std::shared_ptr<const render::RenderResult> prepared_;
    std::shared_ptr<const dsp::ModuleRegistry> registry_;
    std::shared_ptr<const render::RenderResult> rendered_;
    std::uint64_t manual_revision_{};
    std::vector<dsp::ModuleInstance> manual_nodes_;
    std::vector<dsp::ModuleExecutionBinding> manual_bindings_;
    std::uint64_t linked_revision_{};
    std::vector<dsp::ModuleInstance> linked_nodes_;
    std::vector<dsp::ModuleExecutionBinding> linked_bindings_;
    core::FrameCount block_;
};
class M10P2RealNativeProductJob final {
public:
    [[nodiscard]] static core::Result<M10P2RealNativeProductJob> create(
        OfflineProductOwnerStamp owner,
        std::shared_ptr<const render::RenderResult> current_prepared,
        std::shared_ptr<const dsp::ModuleRegistry> current_registry,
        const dsp::ProcessingChain& current_manual,
        std::span<const dsp::ModuleExecutionBinding> current_manual_bindings,
        const dsp::ProcessingChain& exact_candidate_linked,
        std::span<const dsp::ModuleExecutionBinding> linked_bindings,
        const m10_frozen::BoundM10P2Candidate& selected_p2,
        core::FrameCount max_block_frames,
        bool current_m09_m10_m11_candidate_graph_attested)
    {
        using R=core::Result<M10P2RealNativeProductJob>;
        if(!current_m09_m10_m11_candidate_graph_attested||
           !owner.populated()||!current_prepared||!current_registry||
           max_block_frames.value()<=0||
           !selected_p2.typed.still_matches(
               current_manual,current_manual_bindings)||
           selected_p2.typed.base.ticket_identity.empty()||
           current_manual.context()!=dsp::ProcessingChainContext{
               dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
           exact_candidate_linked.context()!=dsp::ProcessingChainContext{
               dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
           exact_candidate_linked.instances().size()!=linked_bindings.size()||
           current_prepared->view().absolute_start_frame().value()!=0||
           current_prepared->view().frame_count().value()<=0||
           current_prepared->view().timebase().frame_domain_id()!=
               audio::FrameDomainId::SOURCE_PROCESSING_RATE)
            return R::failure(error("L3_M10_P2_PRODUCT_SNAPSHOT_INVALID"));
        const auto* effective_p2=
            selected_p2.typed.chosen_p2_parameters();
        const auto manual_nodes=current_manual.instances();
        if(!effective_p2||manual_nodes.empty()||
           manual_nodes.back().instance_id()!=
               selected_p2.typed.base.p2_id||
           !manual_nodes.back().active()||
           manual_nodes.back().module_type_id()!=
               "rgsml.dsp.streaming-true-peak-limiter")
            return R::failure(error("L3_M10_P2_FINAL_MODULE_INVALID"));
        for(std::size_t i=0;i<manual_nodes.size();++i)
            if(!binding_matches_instance(
                  manual_nodes[i],current_manual_bindings[i]))
                return R::failure(error("L3_M10_P2_MANUAL_BINDINGS_INVALID"));
        const auto linked_nodes=exact_candidate_linked.instances();
        for(std::size_t i=0;i<linked_nodes.size();++i) {
            if(!binding_matches_instance(linked_nodes[i],linked_bindings[i])||
               linked_nodes[i].provenance()!=
                   dsp::ModuleProvenance::REFERENCE_MATCH||
               linked_nodes[i].owner()!=
                   dsp::ModuleOwner::REFERENCE_MATCH||
               linked_nodes[i].link_state()!=
                   dsp::ModuleLinkState::REF_LINKED||
               linked_nodes[i].module_type_id()==
                   "rgsml.dsp.streaming-true-peak-limiter"||
               linked_nodes[i].module_type_id()==
                   "rgsml.dsp.true-peak-limiter")
                return R::failure(error("L3_M10_P2_LINKED_BINDINGS_INVALID"));
            for(const auto& fixed:manual_nodes)
                if(fixed.instance_id()==linked_nodes[i].instance_id())
                    return R::failure(error("L3_M10_P2_MODULE_ID_COLLISION"));
        }
        try {
            // Extend ONLY required native opt-in factories. Reuse the
            // same Product registry snapshot for candidate evaluation.
            auto registry=std::move(current_registry);
            const auto requires_type=[&](std::string_view name) {
                for(const auto& n:manual_nodes)
                    if(n.module_type_id()==name)return true;
                for(const auto& n:linked_nodes)
                    if(n.module_type_id()==name)return true;
                return false;
            };
            if(!registry->has_factory(
                 "rgsml.dsp.streaming-true-peak-limiter")) {
                auto opt=dsp::ModuleRegistry::with_streaming_true_peak_preview(
                    *registry);
                if(!opt)return R::failure(*opt.error());
                registry=std::make_shared<const dsp::ModuleRegistry>(
                    std::move(*opt.value()));
            }
            if(requires_type("rgsml.dsp.dynamic-eq")&&
               !registry->has_factory("rgsml.dsp.dynamic-eq")) {
                auto opt=dsp::ModuleRegistry::with_dynamic_eq_offline(
                    *registry);
                if(!opt)return R::failure(*opt.error());
                registry=std::make_shared<const dsp::ModuleRegistry>(
                    std::move(*opt.value()));
            }
            if(requires_type("rgsml.dsp.transient-shaper")&&
               !registry->has_factory("rgsml.dsp.transient-shaper")) {
                auto opt=dsp::ModuleRegistry::with_transient_shaper_offline(
                    *registry);
                if(!opt)return R::failure(*opt.error());
                registry=std::make_shared<const dsp::ModuleRegistry>(
                    std::move(*opt.value()));
            }
            auto fixed=dsp::ProcessingChain::restore(
                *registry,current_manual.context(),current_manual.revision(),
                {manual_nodes.begin(),manual_nodes.end()-1});
            if(!fixed)return R::failure(*fixed.error());
            auto linked=dsp::ProcessingChain::restore(
                *registry,exact_candidate_linked.context(),
                exact_candidate_linked.revision(),
                {linked_nodes.begin(),linked_nodes.end()});
            if(!linked)return R::failure(*linked.error());
            auto p2=dsp::ProcessingChain::restore(
                *registry,current_manual.context(),current_manual.revision(),
                {manual_nodes.back()});
            if(!p2)return R::failure(*p2.error());
            std::vector<dsp::ModuleExecutionBinding> fixed_bindings{
                current_manual_bindings.begin(),current_manual_bindings.end()-1};
            std::vector<dsp::ModuleExecutionBinding> linked_values{
                linked_bindings.begin(),linked_bindings.end()};
            std::vector<dsp::ModuleExecutionBinding> p2_binding{{
                manual_nodes.back().instance_id(),*effective_p2}};
            return R::success(M10P2RealNativeProductJob{
                std::move(owner),std::move(current_prepared),
                std::move(registry),std::move(*fixed.value()),
                std::move(*linked.value()),std::move(*p2.value()),
                std::move(fixed_bindings),std::move(linked_values),
                std::move(p2_binding),selected_p2.typed.base.ticket_identity,
                max_block_frames,current_manual.revision(),
                {manual_nodes.begin(),manual_nodes.end()},
                {current_manual_bindings.begin(),current_manual_bindings.end()}});
        }catch(const std::bad_alloc&) {
            return R::failure(error("L3_M10_P2_PRODUCT_SNAPSHOT_OOM"));
        }
    }

    // A 5-minute 48k stereo double PCM occupies ~219.7 MiB.
    // Allow ONE explicit exact prefix, bounded by 256 MiB by default.
    // Oversized 96k/192k songs are rendered COLD, never silently
    // truncated or admitted to unbounded cache memory.
    [[nodiscard]] core::Result<M10P2ExactPrefix>
    prepare_exact_pre_p2_prefix(
        std::size_t max_cache_bytes=256U*1024U*1024U) const
    {
        using R=core::Result<M10P2ExactPrefix>;
        if(!prepared_||!registry_)
            return R::failure(error("L3_M10_P2_MISSING_IMMUTABLE_OWNER"));
        const auto v=prepared_->view();
        const auto frames=v.frame_count().value();
        const auto channels=v.format().channel_count();
        if(frames<=0||channels<1U||channels>2U||max_cache_bytes==0U||
           static_cast<std::uint64_t>(frames)>
               std::numeric_limits<std::uint64_t>::max()/
               (static_cast<std::uint64_t>(channels)*sizeof(double))||
           static_cast<std::uint64_t>(frames)*
               static_cast<std::uint64_t>(channels)*sizeof(double)>
               static_cast<std::uint64_t>(max_cache_bytes))
            return R::failure(error("L3_M10_P2_EXACT_PREFIX_RAM_BUDGET"));
        auto linked=render_pre_p2();
        if(!linked)return R::failure(*linked.error());
        try {
            auto immutable=std::make_shared<const render::RenderResult>(
                std::move(*linked.value()));
            return R::success(M10P2ExactPrefix{
                owner_,prepared_,registry_,std::move(immutable),
                baseline_manual_revision_,baseline_manual_nodes_,
                baseline_manual_bindings_,linked_.revision(),
                {linked_.instances().begin(),linked_.instances().end()},
                linked_bindings_,max_block_frames_});
        }catch(const std::bad_alloc&) {
            return R::failure(error("L3_M10_P2_PREFIX_SNAPSHOT_OOM"));
        }
    }
    [[nodiscard]] core::Result<render::RenderResult>
    execute_with_exact_pre_p2_prefix(const M10P2ExactPrefix& prefix) const
    {
        using R=core::Result<render::RenderResult>;
        if(!prefix.rendered_||prefix.owner_!=owner_||
           prefix.prepared_!=prepared_||prefix.registry_!=registry_||
           prefix.block_!=max_block_frames_||
           prefix.manual_revision_!=baseline_manual_revision_||
           prefix.manual_nodes_!=baseline_manual_nodes_||
           prefix.manual_bindings_!=baseline_manual_bindings_||
           prefix.linked_revision_!=linked_.revision()||
           prefix.linked_bindings_!=linked_bindings_||
           prefix.linked_nodes_.size()!=linked_.instances().size()||
           !std::equal(prefix.linked_nodes_.begin(),
                       prefix.linked_nodes_.end(),
                       linked_.instances().begin())||
           prefix.rendered_->view().absolute_range()!=
               prepared_->view().absolute_range()||
           prefix.rendered_->view().format()!=prepared_->view().format())
            return R::failure(error("L3_M10_P2_PREFIX_UPSTREAM_STALE"));
        auto limited=render_segment(
            prefix.rendered_->view(),final_p2_,p2_bindings_);
        if(!limited)return R::failure(*limited.error());
        if(!result_has_expected_scope(*limited.value()))
            return R::failure(error("L3_M10_P2_FULL_RENDER_SCOPE_INVALID"));
        return limited;
    }
    [[nodiscard]] core::Result<render::RenderResult> execute() const
    {
        using R=core::Result<render::RenderResult>;
        // Always available: a cold full real Product rendering does not
        // depend on whether an optional bounded prefix fits RAM.
        auto upstream=render_pre_p2();
        if(!upstream)return R::failure(*upstream.error());
        auto limited=render_segment(
            upstream.value()->view(),final_p2_,p2_bindings_);
        if(!limited)return R::failure(*limited.error());
        if(!result_has_expected_scope(*limited.value()))
            return R::failure(error("L3_M10_P2_FULL_RENDER_SCOPE_INVALID"));
        return limited;
    }
    [[nodiscard]] bool result_has_expected_scope(
        const render::RenderResult& out) const noexcept
    {
        if(!prepared_)return false;
        const auto& signatures=out.signatures();
        return out.view().absolute_range()==
                  prepared_->view().absolute_range()&&
               out.view().format()==prepared_->view().format()&&
               out.view().timebase().frame_domain_id()==
                   audio::FrameDomainId::SOURCE_PROCESSING_RATE&&
               !signatures.empty()&&
               signatures.back().instance_id==p2_bindings_.front().instance_id&&
               signatures.back().type_id==
                   "rgsml.dsp.streaming-true-peak-limiter";
    }
    [[nodiscard]] bool still_current(
        const OfflineProductOwnerStamp& current_owner,
        const std::shared_ptr<const render::RenderResult>& current_prepared,
        const dsp::ProcessingChain& manual,
        std::span<const dsp::ModuleExecutionBinding> manual_bindings,
        const dsp::ProcessingChain& artistic,
        std::span<const dsp::ModuleExecutionBinding> artistic_bindings)
        const noexcept
    {
        if(!owner_.populated()||current_owner!=owner_||
           current_prepared!=prepared_||
           manual.context()!=dsp::ProcessingChainContext{
               dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
           manual.revision()!=baseline_manual_revision_||
           manual_bindings.size()!=baseline_manual_bindings_.size()||
           !std::equal(manual_bindings.begin(),manual_bindings.end(),
                       baseline_manual_bindings_.begin())||
           manual.instances().size()!=baseline_manual_nodes_.size()||
           !std::equal(manual.instances().begin(),manual.instances().end(),
                       baseline_manual_nodes_.begin())||
           artistic.context()!=linked_.context()||
           artistic.revision()!=linked_.revision()||
           artistic_bindings.size()!=linked_bindings_.size()||
           !std::equal(artistic_bindings.begin(),artistic_bindings.end(),
                       linked_bindings_.begin()))
            return false;
        const auto nodes=artistic.instances();
        const auto saved=linked_.instances();
        return nodes.size()==saved.size()&&
            std::equal(nodes.begin(),nodes.end(),saved.begin());
    }
    [[nodiscard]] const std::string& ticket_identity() const noexcept {
        return ticket_identity_;
    }
    [[nodiscard]] const OfflineProductOwnerStamp& owner_stamp()
        const noexcept{return owner_;}
    // Expose only the non-effective manual baseline for testing freshness.
    [[nodiscard]] bool identical_upstream_to(
        const M10P2RealNativeProductJob& other) const noexcept
    {
        return owner_==other.owner_&&prepared_==other.prepared_&&
            registry_==other.registry_&&
            baseline_manual_revision_==other.baseline_manual_revision_&&
            baseline_manual_nodes_==other.baseline_manual_nodes_&&
            baseline_manual_bindings_==other.baseline_manual_bindings_&&
            linked_.revision()==other.linked_.revision()&&
            linked_bindings_==other.linked_bindings_&&
            max_block_frames_==other.max_block_frames_&&
            linked_.instances().size()==other.linked_.instances().size()&&
            std::equal(linked_.instances().begin(),
                       linked_.instances().end(),
                       other.linked_.instances().begin());
    }
    [[nodiscard]] constexpr bool authorizes_publication()const noexcept {
        return false;
    }
private:
    M10P2RealNativeProductJob(
        OfflineProductOwnerStamp owner,
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const dsp::ModuleRegistry> registry,
        dsp::ProcessingChain fixed,dsp::ProcessingChain linked,
        dsp::ProcessingChain final_p2,
        std::vector<dsp::ModuleExecutionBinding> fixed_bindings,
        std::vector<dsp::ModuleExecutionBinding> linked_bindings,
        std::vector<dsp::ModuleExecutionBinding> p2_bindings,
        std::string ticket,core::FrameCount max_block,
        std::uint64_t revision,
        std::vector<dsp::ModuleInstance> manual_nodes,
        std::vector<dsp::ModuleExecutionBinding> manual_bindings)
        : owner_(std::move(owner)),prepared_(std::move(prepared)),
          registry_(std::move(registry)),
          fixed_(std::move(fixed)),linked_(std::move(linked)),
          final_p2_(std::move(final_p2)),
          fixed_bindings_(std::move(fixed_bindings)),
          linked_bindings_(std::move(linked_bindings)),
          p2_bindings_(std::move(p2_bindings)),
          ticket_identity_(std::move(ticket)),max_block_frames_(max_block),
          baseline_manual_revision_(revision),
          baseline_manual_nodes_(std::move(manual_nodes)),
          baseline_manual_bindings_(std::move(manual_bindings)) {}
    [[nodiscard]] core::Result<render::RenderResult> render_pre_p2()
        const
    {
        using R=core::Result<render::RenderResult>;
        if(!prepared_||!registry_)
            return R::failure(error("L3_M10_P2_MISSING_IMMUTABLE_OWNER"));
        auto fixed=render_segment(
            prepared_->view(),fixed_,fixed_bindings_);
        if(!fixed)return R::failure(*fixed.error());
        return render_segment(
            fixed.value()->view(),linked_,linked_bindings_);
    }
    [[nodiscard]] core::Result<render::RenderResult> render_segment(
        const audio::AudioBufferView& source,
        const dsp::ProcessingChain& chain,
        const std::vector<dsp::ModuleExecutionBinding>& bindings) const
    {
        using R=core::Result<render::RenderResult>;
        const auto request=render::RenderRequest::create(
            source,source.absolute_range(),chain,bindings,max_block_frames_);
        if(!request)return R::failure(*request.error());
        auto rendered=render::render_preview(*request.value(),*registry_);
        if(!rendered)return rendered;
        if(rendered.value()->view().absolute_range()!=
              prepared_->view().absolute_range()||
           rendered.value()->view().format()!=prepared_->view().format())
            return R::failure(error("L3_M10_P2_SEGMENT_GEOMETRY_CHANGED"));
        return rendered;
    }
    [[nodiscard]] static core::Error error(const char* code) {
        return {core::ErrorCode::InvalidState,code};
    }
    OfflineProductOwnerStamp owner_;
    std::shared_ptr<const render::RenderResult> prepared_;
    // Each ProcessingChain stores a non-owning pointer into registry_.
    // Member declaration order keeps the actual registry alive first.
    std::shared_ptr<const dsp::ModuleRegistry> registry_;
    dsp::ProcessingChain fixed_;
    dsp::ProcessingChain linked_;
    dsp::ProcessingChain final_p2_;
    std::vector<dsp::ModuleExecutionBinding> fixed_bindings_;
    std::vector<dsp::ModuleExecutionBinding> linked_bindings_;
    std::vector<dsp::ModuleExecutionBinding> p2_bindings_;
    std::string ticket_identity_;
    core::FrameCount max_block_frames_;
    std::uint64_t baseline_manual_revision_{};
    std::vector<dsp::ModuleInstance> baseline_manual_nodes_;
    std::vector<dsp::ModuleExecutionBinding> baseline_manual_bindings_;
};
} // namespace rgsml::app::l3_automatch
