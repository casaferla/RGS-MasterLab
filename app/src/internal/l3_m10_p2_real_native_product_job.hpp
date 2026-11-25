#pragma once
// L3-M10 authentic in-memory full-candidate worker for streaming P2.
// Executes exact PREPARED -> protected MANUAL prefix (without P2)
// -> candidate REF_LINKED artistic segment -> final genuine P2.
// No GUI, writer mutation, proxy PCM, WAV Export M17, M12 publication,
// forged 27-feature cost or final M13 verification. Each candidate's
// P2 Ceiling/Drive comes from the current exact M10 typed binding.
#include "l3_m10_p2_canonical_coordinate_port.hpp"
#include "l3_m10_p2_memory_budget.hpp"
#include "l3_m10_exact_native_graph_content.hpp"
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
// Exact PCM for the protected MANUAL prefix. This is already a
// complete, source-rate segment of the cold path; reusing it never
// introduces an additional cut at a stateful module boundary.
class M10ExactFixedPrefix final {
    friend class M10P2RealNativeProductJob;
public:
    [[nodiscard]] constexpr bool authorizes_m10_score() const noexcept{return false;}
private:
    M10ExactFixedPrefix(OfflineProductOwnerStamp owner,
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const dsp::ModuleRegistry> registry,
        std::shared_ptr<const render::RenderResult> rendered,
        m10_frozen::ExactCacheBytes graph)
        :owner_(std::move(owner)),prepared_(std::move(prepared)),registry_(std::move(registry)),
         rendered_(std::move(rendered)),exact_fixed_graph_(std::move(graph)){}
    OfflineProductOwnerStamp owner_;
    std::shared_ptr<const render::RenderResult> prepared_;
    std::shared_ptr<const dsp::ModuleRegistry> registry_;
    std::shared_ptr<const render::RenderResult> rendered_;
    m10_frozen::ExactCacheBytes exact_fixed_graph_;
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
        if(!selected_p2.typed.still_matches(current_manual,current_manual_bindings)||
           !selected_p2.typed.chosen_p2_parameters())
            return core::Result<M10P2RealNativeProductJob>::failure(
                error("L3_M10_P2_PRODUCT_SNAPSHOT_INVALID"));
        return create_common(std::move(owner),std::move(current_prepared),
            std::move(current_registry),current_manual,current_manual_bindings,
            exact_candidate_linked,linked_bindings,
            selected_p2.typed.base.ticket_identity,max_block_frames,
            current_m09_m10_m11_candidate_graph_attested,
            selected_p2.typed.chosen_p2_parameters());
    }

    // Product AutoMatch does not require or optimize a MANUAL P2.
    // If present, preserve its exact parameters and activation state,
    // and place it after the REF_LINKED artistic chain. A separately
    // owned P2 may be the last REF_LINKED node only when the protected
    // MANUAL graph has no P2 at all (even a bypassed one).
    [[nodiscard]] static core::Result<M10P2RealNativeProductJob>
    create_preserving_user_terminal(
        OfflineProductOwnerStamp owner,
        std::shared_ptr<const render::RenderResult> current_prepared,
        std::shared_ptr<const dsp::ModuleRegistry> current_registry,
        const dsp::ProcessingChain& current_manual,
        std::span<const dsp::ModuleExecutionBinding> current_manual_bindings,
        const dsp::ProcessingChain& exact_candidate_linked,
        std::span<const dsp::ModuleExecutionBinding> linked_bindings,
        std::string candidate_ticket,
        core::FrameCount max_block_frames,
        bool current_m09_m10_m11_candidate_graph_attested)
    {
        return create_common(std::move(owner),std::move(current_prepared),
            std::move(current_registry),current_manual,current_manual_bindings,
            exact_candidate_linked,linked_bindings,std::move(candidate_ticket),
            max_block_frames,current_m09_m10_m11_candidate_graph_attested,nullptr);
    }
private:
    [[nodiscard]] static core::Result<M10P2RealNativeProductJob> create_common(
        OfflineProductOwnerStamp owner,
        std::shared_ptr<const render::RenderResult> current_prepared,
        std::shared_ptr<const dsp::ModuleRegistry> current_registry,
        const dsp::ProcessingChain& current_manual,
        std::span<const dsp::ModuleExecutionBinding> current_manual_bindings,
        const dsp::ProcessingChain& exact_candidate_linked,
        std::span<const dsp::ModuleExecutionBinding> linked_bindings,
        std::string candidate_ticket,core::FrameCount max_block_frames,
        bool current_m09_m10_m11_candidate_graph_attested,
        const dsp::StreamingTruePeakLimiterParameters* explicitly_selected_research_p2)
    {
        using R=core::Result<M10P2RealNativeProductJob>;
        if(!current_m09_m10_m11_candidate_graph_attested||
           !owner.populated()||!current_prepared||!current_registry||
           max_block_frames.value()<=0||
           current_manual.instances().size()!=current_manual_bindings.size()||
           candidate_ticket.empty()||
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
        const auto manual_nodes=current_manual.instances();
        bool has_terminal=false;
        for(std::size_t i=0;i<manual_nodes.size();++i) {
            const auto& node=manual_nodes[i];
            if(node.module_type_id()=="rgsml.dsp.streaming-true-peak-limiter") {
                if(has_terminal||i+1U!=manual_nodes.size())
                    return R::failure(error("L3_M10_P2_FINAL_MODULE_INVALID"));
                has_terminal=true;
            }
        }
        if(explicitly_selected_research_p2&&
           (!has_terminal||!manual_nodes.back().active()))
            return R::failure(error("L3_M10_P2_FINAL_MODULE_INVALID"));
        for(std::size_t i=0;i<manual_nodes.size();++i)
            if(!binding_matches_instance(
                  manual_nodes[i],current_manual_bindings[i]))
                return R::failure(error("L3_M10_P2_MANUAL_BINDINGS_INVALID"));
        const auto linked_nodes=exact_candidate_linked.instances();
        bool linked_terminal=false;
        for(std::size_t i=0;i<linked_nodes.size();++i) {
            const bool is_p2=linked_nodes[i].module_type_id()=="rgsml.dsp.streaming-true-peak-limiter";
            if(is_p2) {
                if(has_terminal||linked_terminal||i+1U!=linked_nodes.size()||
                   !std::holds_alternative<dsp::StreamingTruePeakLimiterParameters>(linked_bindings[i].parameters))
                    return R::failure(error("L3_M10_P2_OWNED_TERMINAL_CONFLICT"));
                linked_terminal=true;
            }
            if(!binding_matches_instance(linked_nodes[i],linked_bindings[i])||
               linked_nodes[i].provenance()!=
                   dsp::ModuleProvenance::REFERENCE_MATCH||
               linked_nodes[i].owner()!=
                   dsp::ModuleOwner::REFERENCE_MATCH||
               linked_nodes[i].link_state()!=
                   dsp::ModuleLinkState::REF_LINKED||
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
            if(has_terminal&&!registry->has_factory(
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
                {manual_nodes.begin(),manual_nodes.end()-(has_terminal?1:0)});
            if(!fixed)return R::failure(*fixed.error());
            auto linked=dsp::ProcessingChain::restore(
                *registry,exact_candidate_linked.context(),
                exact_candidate_linked.revision(),
                {linked_nodes.begin(),linked_nodes.end()});
            if(!linked)return R::failure(*linked.error());
            std::vector<dsp::ModuleInstance> terminal_nodes;
            if(has_terminal)terminal_nodes.push_back(manual_nodes.back());
            auto p2=dsp::ProcessingChain::restore(
                *registry,current_manual.context(),current_manual.revision(),
                std::move(terminal_nodes));
            if(!p2)return R::failure(*p2.error());
            std::vector<dsp::ModuleExecutionBinding> fixed_bindings{
                current_manual_bindings.begin(),current_manual_bindings.end()-(has_terminal?1:0)};
            std::vector<dsp::ModuleExecutionBinding> linked_values{
                linked_bindings.begin(),linked_bindings.end()};
            std::vector<dsp::ModuleExecutionBinding> p2_binding;
            if(has_terminal) {
                p2_binding.push_back(current_manual_bindings.back());
                if(explicitly_selected_research_p2)
                    p2_binding.back().parameters=*explicitly_selected_research_p2;
            }
            return R::success(M10P2RealNativeProductJob{
                std::move(owner),std::move(current_prepared),
                std::move(registry),std::move(*fixed.value()),
                std::move(*linked.value()),std::move(*p2.value()),
                std::move(fixed_bindings),std::move(linked_values),
                std::move(p2_binding),std::move(candidate_ticket),
                max_block_frames,current_manual.revision(),
                {manual_nodes.begin(),manual_nodes.end()},
                {current_manual_bindings.begin(),current_manual_bindings.end()}});
        }catch(const std::bad_alloc&) {
            return R::failure(error("L3_M10_P2_PRODUCT_SNAPSHOT_OOM"));
        }
    }

public:
    // All ordinary trials share this upstream segment. Bound storage to
    // one immutable PCM snapshot; do not retain large telemetry sidecars.
    [[nodiscard]] core::Result<M10ExactFixedPrefix>
    prepare_exact_fixed_prefix(std::optional<std::size_t> requested_maximum=std::nullopt) const {
        using R=core::Result<M10ExactFixedPrefix>;
        if(!prepared_||!registry_||!preserves_user_terminal())
            return R::failure(error("L3_M10_FIXED_PREFIX_OWNER_INVALID"));
        const auto view=prepared_->view();
        const auto frames=view.frame_count().value();
        const auto channels=view.format().channel_count();
        const auto budget=m10_p2_memory::current_budget(requested_maximum);
        if(frames<=0||channels<1U||channels>2U||
           static_cast<std::uint64_t>(frames)>budget/(static_cast<std::uint64_t>(channels)*sizeof(double)))
            return R::failure(error("L3_M10_FIXED_PREFIX_RAM_BUDGET"));
        // Avoid building/cache-retaining stage telemetry for these
        // snapshots. Other module types can take the cold path.
        for(const auto& node:fixed_.instances())
            if(node.active()&&(node.module_type_id()=="rgsml.dsp.compressor"||
               node.module_type_id()=="rgsml.dsp.stereo-ms"))
                return R::failure(error("L3_M10_FIXED_PREFIX_TELEMETRY_UNSUPPORTED"));
        const auto graph=exact_fixed_graph_content();
        if(!graph)return R::failure(error("L3_M10_FIXED_PREFIX_GRAPH_INVALID"));
        auto rendered=render_segment(view,fixed_,fixed_bindings_);
        if(!rendered)return R::failure(*rendered.error());
        if(rendered.value()->compressor_telemetry_sidecar()||
           !rendered.value()->stereo_ms_stage_output_sidecars().empty())
            return R::failure(error("L3_M10_FIXED_PREFIX_TELEMETRY_UNSUPPORTED"));
        try {
            auto pcm=std::make_shared<const render::RenderResult>(std::move(*rendered.value()));
            return R::success(M10ExactFixedPrefix{owner_,prepared_,registry_,std::move(pcm),*graph});
        }catch(const std::bad_alloc&) {
            return R::failure(error("L3_M10_FIXED_PREFIX_OOM"));
        }
    }
    [[nodiscard]] bool matches_exact_fixed_prefix(const M10ExactFixedPrefix& prefix) const {
        if(!prepared_||!registry_||!prefix.rendered_||!preserves_user_terminal()||
           prefix.owner_!=owner_||prefix.prepared_!=prepared_||prefix.registry_!=registry_||
           prefix.rendered_->view().absolute_range()!=prepared_->view().absolute_range()||
           prefix.rendered_->view().format()!=prepared_->view().format())return false;
        const auto current=exact_fixed_graph_content();
        return current&&*current==prefix.exact_fixed_graph_;
    }
    [[nodiscard]] core::Result<render::RenderResult>
    execute_with_exact_fixed_prefix(const M10ExactFixedPrefix& prefix) const {
        using R=core::Result<render::RenderResult>;
        if(!matches_exact_fixed_prefix(prefix))return R::failure(error("L3_M10_FIXED_PREFIX_STALE"));
        auto linked=render_segment(prefix.rendered_->view(),linked_,linked_bindings_);
        if(!linked)return linked;
        if(p2_bindings_.empty()) {
            if(!result_has_expected_scope(*linked.value()))return R::failure(error("L3_M10_FIXED_PREFIX_SCOPE"));
            return linked;
        }
        auto final=render_segment(linked.value()->view(),final_p2_,p2_bindings_);
        if(!final)return final;
        if(!result_has_expected_scope(*final.value()))return R::failure(error("L3_M10_FIXED_PREFIX_SCOPE"));
        return final;
    }
    [[nodiscard]] bool owned_p2_prefix_eligible() const noexcept {
        const auto nodes=linked_.instances();
        return p2_bindings_.empty()&&!nodes.empty()&&nodes.back().active()&&
            nodes.back().module_type_id()=="rgsml.dsp.streaming-true-peak-limiter"&&
            std::all_of(nodes.begin(),nodes.end()-1,[](const auto& n){
                return n.module_type_id()=="rgsml.dsp.gain"||n.module_type_id()=="rgsml.dsp.parametric-eq";
            });
    }
    [[nodiscard]] std::optional<m10_frozen::ExactCacheBytes> exact_owned_p2_upstream_content() const {
        if(!registry_||!owned_p2_prefix_eligible())return {};
        const auto nodes=linked_.instances();
        const auto prefix=dsp::ProcessingChain::restore(*registry_,linked_.context(),
            linked_.revision(),{nodes.begin(),nodes.end()-1});
        if(!prefix)return {};
        const auto fixed=exact_fixed_graph_content();
        const auto linked=m10_frozen::exact_native_graph_content(*prefix.value(),
            std::span<const dsp::ModuleExecutionBinding>{linked_bindings_}.first(nodes.size()-1U),*registry_);
        if(!fixed||!linked)return {};
        using namespace m10_frozen::p2_cache_detail;
        m10_frozen::ExactCacheBytes out;
        append_exact_field(out,"rgsml.l3.owned-p2-upstream/1");
        append_exact_byte_field(out,*fixed);append_exact_byte_field(out,*linked);
        return out;
    }
    // Optional exact PCM prefix cache uses available physical RAM at the
    // time of creation. Never compromise entire-resource output if the
    // budget is too small: execute() takes the correct cold path.
    [[nodiscard]] core::Result<M10P2ExactPrefix>
    prepare_exact_pre_p2_prefix(
        std::optional<std::size_t> requested_maximum=std::nullopt) const
    {
        using R=core::Result<M10P2ExactPrefix>;
        const bool owned=owned_p2_prefix_eligible();
        if(!prepared_||!registry_||(p2_bindings_.empty()&&!owned))
            return R::failure(error("L3_M10_P2_MISSING_IMMUTABLE_OWNER"));
        const auto v=prepared_->view();
        const auto frames=v.frame_count().value();
        const auto channels=v.format().channel_count();
        const auto max_cache_bytes=
            m10_p2_memory::current_budget(requested_maximum);
        if(frames<=0||channels<1U||channels>2U||max_cache_bytes==0U||
           static_cast<std::uint64_t>(frames)>
               std::numeric_limits<std::uint64_t>::max()/
               (static_cast<std::uint64_t>(channels)*sizeof(double))||
           static_cast<std::uint64_t>(frames)*
               static_cast<std::uint64_t>(channels)*sizeof(double)>
               static_cast<std::uint64_t>(max_cache_bytes))
            return R::failure(error("L3_M10_P2_EXACT_PREFIX_RAM_BUDGET"));
        const auto nodes=linked_.instances();
        const auto prefix_count=nodes.size()-(owned?1U:0U);
        const auto prefix_end=nodes.begin()+static_cast<std::ptrdiff_t>(prefix_count);
        const auto binding_end=linked_bindings_.begin()+static_cast<std::ptrdiff_t>(prefix_count);
        auto linked=[&]()->core::Result<render::RenderResult>{
            if(!owned)return render_pre_p2();
            auto fixed=render_segment(prepared_->view(),fixed_,fixed_bindings_);
            if(!fixed)return fixed;
            auto prefix=dsp::ProcessingChain::restore(*registry_,linked_.context(),
                linked_.revision(),{nodes.begin(),prefix_end});
            if(!prefix)return core::Result<render::RenderResult>::failure(*prefix.error());
            return render_segment(fixed.value()->view(),*prefix.value(),
                {linked_bindings_.begin(),binding_end});
        }();
        if(!linked)return R::failure(*linked.error());
        try {
            auto immutable=std::make_shared<const render::RenderResult>(
                std::move(*linked.value()));
            return R::success(M10P2ExactPrefix{
                owner_,prepared_,registry_,std::move(immutable),
                baseline_manual_revision_,baseline_manual_nodes_,
                baseline_manual_bindings_,linked_.revision(),
                {nodes.begin(),prefix_end},
                {linked_bindings_.begin(),binding_end},max_block_frames_});
        }catch(const std::bad_alloc&) {
            return R::failure(error("L3_M10_P2_PREFIX_SNAPSHOT_OOM"));
        }
    }
    [[nodiscard]] bool matches_exact_pre_p2_prefix(const M10P2ExactPrefix& prefix) const noexcept {
        const bool owned=owned_p2_prefix_eligible();
        const auto nodes=linked_.instances();
        const auto count=nodes.size()-(owned?1U:0U);
        return prepared_&&registry_&&(!p2_bindings_.empty()||owned)&&
           prefix.rendered_&&prefix.owner_==owner_&&
           prefix.prepared_==prepared_&&prefix.registry_==registry_&&
           prefix.block_==max_block_frames_&&
           prefix.manual_revision_==baseline_manual_revision_&&
           prefix.manual_nodes_==baseline_manual_nodes_&&
           prefix.manual_bindings_==baseline_manual_bindings_&&
           prefix.linked_revision_==linked_.revision()&&
           prefix.linked_bindings_.size()==count&&prefix.linked_nodes_.size()==count&&
           std::equal(prefix.linked_bindings_.begin(),prefix.linked_bindings_.end(),linked_bindings_.begin())&&
           std::equal(prefix.linked_nodes_.begin(),prefix.linked_nodes_.end(),nodes.begin())&&
           prefix.rendered_->view().absolute_range()==prepared_->view().absolute_range()&&
           prefix.rendered_->view().format()==prepared_->view().format();
    }
    [[nodiscard]] core::Result<render::RenderResult>
    execute_with_exact_pre_p2_prefix(const M10P2ExactPrefix& prefix) const
    {
        using R=core::Result<render::RenderResult>;
        if(!matches_exact_pre_p2_prefix(prefix))
            return R::failure(error("L3_M10_P2_PREFIX_UPSTREAM_STALE"));
        auto limited=[&]()->R{
            if(!owned_p2_prefix_eligible())
                return render_segment(prefix.rendered_->view(),final_p2_,p2_bindings_);
            // The proof-bound prefix owns the exact fixed+linked PCM.
            // Its view supplies geometry to the native suffix request;
            // only the final P2 is executed by the cache renderer.
            const auto request=render::RenderRequest::create(prefix.rendered_->view(),
                prefix.rendered_->view().absolute_range(),linked_,linked_bindings_,max_block_frames_);
            if(!request)return R::failure(*request.error());
            return render::render_preview_p2_cached_prefix(*request.value(),*prefix.rendered_,*registry_);
        }();
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
        if(p2_bindings_.empty())return upstream;
        auto limited=render_segment(
            upstream.value()->view(),final_p2_,p2_bindings_);
        if(!limited)return R::failure(*limited.error());
        if(!result_has_expected_scope(*limited.value()))
            return R::failure(error("L3_M10_P2_FULL_RENDER_SCOPE_INVALID"));
        return limited;
    }
    // Read-only insertion observation before a preserved MANUAL terminal.
    // Not a complete scored candidate, not a modification of the terminal.
    [[nodiscard]] core::Result<render::RenderResult> execute_stereo_insertion_observation() const {
        using R=core::Result<render::RenderResult>;
        if(!preserves_user_terminal()||!std::all_of(linked_.instances().begin(),linked_.instances().end(),
            [](const auto& n){return n.module_type_id()=="rgsml.dsp.parametric-eq";}))
            return R::failure(error("L3_M09_INSERTION_GRAPH_INVALID"));
        auto measured=render_pre_p2();
        if(!measured)return measured;
        if(!stereo_insertion_has_expected_scope(*measured.value()))
            return R::failure(error("L3_M09_INSERTION_SCOPE_INVALID"));
        return measured;
    }
    [[nodiscard]] bool stereo_insertion_has_expected_scope(const render::RenderResult& out) const noexcept {
        return preserves_user_terminal()&&std::all_of(linked_.instances().begin(),linked_.instances().end(),
            [](const auto& n){return n.module_type_id()=="rgsml.dsp.parametric-eq";})&&
            result_matches_segment(out,linked_);
    }
    [[nodiscard]] std::optional<m10_frozen::ExactCacheBytes> exact_stereo_insertion_content() const {
        if(!registry_||!preserves_user_terminal()||!std::all_of(linked_.instances().begin(),linked_.instances().end(),
            [](const auto& n){return n.module_type_id()=="rgsml.dsp.parametric-eq";}))return {};
        const auto fixed=exact_fixed_graph_content();
        const auto linked=exact_linked_graph_content();
        if(!fixed||!linked)return {};
        using namespace m10_frozen::p2_cache_detail;
        m10_frozen::ExactCacheBytes out;
        append_exact_field(out,"rgsml.l3.pre-manual-terminal-stereo-insertion/1");
        append_exact_byte_field(out,*fixed);append_exact_byte_field(out,*linked);
        return out;
    }
    [[nodiscard]] bool result_has_expected_scope(const render::RenderResult& out) const noexcept {
        return result_matches_segment(out,p2_bindings_.empty()?linked_:final_p2_);
    }
private:
    [[nodiscard]] bool result_matches_segment(
        const render::RenderResult& out,const dsp::ProcessingChain& final_chain) const noexcept
    {
        if(!prepared_)return false;
        const auto& signatures=out.signatures();
        const auto nodes=final_chain.instances();
        if(out.view().absolute_range()!=prepared_->view().absolute_range()||
           out.view().format()!=prepared_->view().format()||
           out.view().timebase().frame_domain_id()!=audio::FrameDomainId::SOURCE_PROCESSING_RATE||
           out.chain_revision()!=final_chain.revision()||signatures.size()!=nodes.size())
            return false;
        for(std::size_t i=0;i<nodes.size();++i) {
            const auto& node=nodes[i];
            const auto& signature=signatures[i];
            if(signature.instance_id!=node.instance_id()||
               signature.type_id!=node.module_type_id()||
               signature.disposition!=(node.active()
                   ?render::ModuleExecutionDisposition::PROCESSED
                   :render::ModuleExecutionDisposition::BYPASS_IDENTITY))
                return false;
        }
        return true;
    }
public:
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
    [[nodiscard]] const std::shared_ptr<const dsp::ModuleRegistry>&
    execution_registry_snapshot() const noexcept {return registry_;}
    [[nodiscard]] std::optional<m10_frozen::ExactCacheBytes>
    exact_linked_graph_content() const {
        if(!registry_)return std::nullopt;
        return m10_frozen::exact_native_graph_content(linked_,linked_bindings_,*registry_);
    }
    [[nodiscard]] std::optional<m10_frozen::ExactCacheBytes>
    exact_fixed_graph_content() const {
        if(!registry_)return std::nullopt;
        const auto fixed=m10_frozen::exact_native_graph_content(fixed_,fixed_bindings_,*registry_);
        const auto terminal=m10_frozen::exact_native_graph_content(final_p2_,p2_bindings_,*registry_);
        if(!fixed||!terminal)return std::nullopt;
        m10_frozen::ExactCacheBytes out;
        using namespace m10_frozen::p2_cache_detail;
        append_exact_field(out,"rgsml.l3.native-fixed-graph/1");
        append_exact_field(out,std::to_string(max_block_frames_.value()));
        append_exact_byte_field(out,*fixed);
        append_exact_byte_field(out,*terminal);
        return out;
    }
    [[nodiscard]] std::optional<m10_frozen::ExactCacheBytes>
    exact_execution_graph_content() const {
        if(!registry_)return std::nullopt;
        const auto fixed=m10_frozen::exact_native_graph_content(fixed_,fixed_bindings_,*registry_);
        const auto linked=exact_linked_graph_content();
        const auto terminal=m10_frozen::exact_native_graph_content(final_p2_,p2_bindings_,*registry_);
        if(!fixed||!linked||!terminal)return std::nullopt;
        return m10_frozen::exact_native_execution_segments(
            max_block_frames_.value(),*fixed,*linked,*terminal);
    }
    [[nodiscard]] bool preserves_user_terminal() const noexcept {
        return p2_bindings_.empty()||
            (p2_bindings_.size()==1U&&!baseline_manual_bindings_.empty()&&
             p2_bindings_.front()==baseline_manual_bindings_.back());
    }
    [[nodiscard]] bool fixed_only() const noexcept {
        return linked_.instances().empty()&&preserves_user_terminal();
    }
    [[nodiscard]] bool same_fixed_baseline_as(
        const M10P2RealNativeProductJob& other) const noexcept {
        return owner_==other.owner_&&prepared_==other.prepared_&&
            baseline_manual_revision_==other.baseline_manual_revision_&&
            baseline_manual_nodes_==other.baseline_manual_nodes_&&
            baseline_manual_bindings_==other.baseline_manual_bindings_&&
            max_block_frames_==other.max_block_frames_&&
            preserves_user_terminal()&&other.preserves_user_terminal();
    }
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
        return core::Error{core::ErrorCode::InvalidState,code};
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
