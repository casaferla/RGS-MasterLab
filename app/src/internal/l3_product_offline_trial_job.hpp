#pragma once
// L3 offline AutoMatch: an immutable, in-memory PRODUCT-candidate render.
// Construct on the Qt Product owner from true PREPARED and chain snapshots;
// execute on a worker without QObject, QML, file I/O or Product mutation.
// This value layer does not authenticate the Qt owner by itself. The owner
// adapter must supply a fresh M18/SOURCE/REFERENCE epoch stamp, recheck it
// before accepting any result, and keep the actual M21 authority separate.
#include "l3_automatch_typed_gain_trial.hpp"
#include "l3_automatch_typed_dynamic_eq_trial.hpp"
#include "l3_automatch_typed_transient_trial.hpp"
#include "l3_m05m06_real_analysis_port.hpp"
#include "l3_m05_real_tonal_statistics.hpp"
#include "l3_m06_real_windowed_rms_crest.hpp"
#include "l3_m06_real_lra_native.hpp"
#include "l3_m05m06_real_source_bundle.hpp"
#include "l3_m06_native_spectral_d10_d12.hpp"
#include "l3_m06_m20_native_frame_energy_sink.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_result.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
struct OfflineProductOwnerStamp final {
    std::uint64_t m18_owner_epoch{};
    std::uint64_t source_generation{};
    std::uint64_t reference_selection_revision{};
    std::string source_provider_id;
    std::string source_locator;
    std::string reference_provider_id;
    std::string reference_locator;
    friend bool operator==(const OfflineProductOwnerStamp&,
                           const OfflineProductOwnerStamp&)=default;
    [[nodiscard]] bool populated() const noexcept {
        return m18_owner_epoch>0 &&source_generation>0
            &&reference_selection_revision>0
            &&!source_provider_id.empty()&&!source_locator.empty()
            &&!reference_provider_id.empty()&&!reference_locator.empty()
            &&(source_provider_id!=reference_provider_id
               ||source_locator!=reference_locator);
    }
};

struct OfflineProductCandidateMeasurement final {
    render::RenderResult rendered;
    analysis::BasicLevelAnalysisResult levels;
    analysis::TonalStereoAnalysisResult tonal;
    // Authentic canonical unrounded analysis; not a manufactured M21
    // "current" snapshot. Product Qt owner authenticates it after return.

    // Zero extra render/FFT: map the EXISTING Product candidate's actual
    // M19/M16/M20 output into the partial frozen feature carrier. This
    // is read-only and cannot elevate a caller-supplied SourceIdentity
    // to authoritative Phase6/Qt status or a complete 27-feature M10 cost.
    [[nodiscard]] m05m06_real::Import observed_m05m06(
        m05m06_real::SourceIdentity verified_by_caller) const
    {
        return m05m06_real::import_actual_full_source(
            levels,tonal,std::move(verified_by_caller));
    }
    // Read directly from this candidate's true RenderPreview PCM:
    // shared 100-ms blocks recover M06 D03/D04 native 400-ms
    // observations, without another render or synthetic M21 metrics.
    // Source/REFERENCE and current Qt owner still must be attested by
    // the M05/M06/M07 caller; this does not authorize M10 scoring.
    [[nodiscard]] m05m06_real::D03D04NativeSeries
    observed_m06_native_rms_crest() const
    {
        return m05m06_real::observe_d03_d04_real_pcm(rendered.view());
    }
    // D02 reads the SAME real M19 ungated native Short-term analysis.
    // Active-duration evidence and EBU file-based 1.5s trailing policy
    // are externally owner-authenticated. A short/stale result cannot
    // become M07/M10 authority just because it has a numerical LRA.
    [[nodiscard]] m05m06_real::LraNativeObservation
    observed_m06_native_lra(
        m05m06_real::SourceIdentity verified_by_caller,
        std::int64_t exact_active_frames,
        bool phase6_file_based_trailing_attested) const
    {
        const auto original=observed_m05m06(
            std::move(verified_by_caller));
        if(!original.source)return {};
        return m05m06_real::observe_lra_from_real_m19(
            *original.source,exact_active_frames,
            phase6_file_based_trailing_attested);
    }
    // One shared Product-native feature bundle from this very PCM
    // render + M19/M20 result, not a new analyzer/worker or M21 probe.
    // Phase6 source authority and M13 fresh verification remain external.
    [[nodiscard]] std::optional<m05m06_real::ProductNativeFeatureBundle>
    observed_m05m06_native_bundle(
        m05m06_real::SourceIdentity verified_by_caller,
        m05m06_real::CommonTonalBandwidth shared_common,
        double accepted_density_floor,
        std::int64_t exact_phase6_active_frames,
        bool exact_file_based_lra_trailing_attested) const
    {
        return m05m06_real::bundle_actual_product_m05m06(
            rendered.view(),levels,tonal,std::move(verified_by_caller),
            shared_common,accepted_density_floor,
            exact_phase6_active_frames,
            exact_file_based_lra_trailing_attested);
    }
    // The M07 owner must provide an EXACT shared common bandwidth and
    // accepted spectral floor. No additional audio pass or FFT occurs.
    [[nodiscard]] m05m06_real::TonalObservationBundle
    observed_m05_tonal(m05m06_real::CommonTonalBandwidth shared_common,
                      double accepted_density_floor) const
    {
        return m05m06_real::observe_m05_tonal_bundle(
            tonal,shared_common,accepted_density_floor);
    }
};

struct OfflineProductNativeSpectralCandidate final {
    OfflineProductCandidateMeasurement measurement;
    m05m06_real::SpectralD10D12Native measured_native_d10_d12;
    std::vector<std::uint64_t> original_selected_stft_ordinals;
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
class OfflineProductTrialJob final {
public:
    // An accepted, fully validated Gain or Dynamic EQ trial is a complete
    // immutable graph copy; neither mutates Product registry/chain/bindings.
    template<typename Trial>
    [[nodiscard]] static core::Result<OfflineProductTrialJob> create(
        OfflineProductOwnerStamp stamp,
        std::shared_ptr<const render::RenderResult> prepared,
        std::shared_ptr<const dsp::ModuleRegistry> product_registry,
        const dsp::ProcessingChain& live_chain,
        std::span<const dsp::ModuleExecutionBinding> live_bindings,
        const Trial& trial,core::FrameCount maximum_block_frames)
    {
        static_assert(std::is_same_v<Trial,BoundGainTrial>
                   ||std::is_same_v<Trial,BoundDynamicEqTrial>
                   ||std::is_same_v<Trial,BoundTransientTrial>);
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
            std::shared_ptr<const dsp::ModuleRegistry> registry=
                std::move(product_registry);
            // Attach ONLY the frozen placeholders actually required by
            // the immutable Product candidate graph. A registry already
            // activated by the owner can be shared across many trials.
            const auto needed=[&](std::string_view name) {
                return std::any_of(instances.begin(),instances.end(),
                    [name](const dsp::ModuleInstance& instance) {
                        return instance.module_type_id()==name;
                    });
            };
            if(needed("rgsml.dsp.dynamic-eq") &&
               !registry->has_factory("rgsml.dsp.dynamic-eq")) {
                auto opt=dsp::ModuleRegistry::with_dynamic_eq_offline(*registry);
                if(!opt)return R::failure(*opt.error());
                registry=std::make_shared<const dsp::ModuleRegistry>(
                    std::move(*opt.value()));
            }
            if(needed("rgsml.dsp.transient-shaper") &&
               !registry->has_factory("rgsml.dsp.transient-shaper")) {
                auto opt=dsp::ModuleRegistry::with_transient_shaper_offline(*registry);
                if(!opt)return R::failure(*opt.error());
                registry=std::make_shared<const dsp::ModuleRegistry>(
                    std::move(*opt.value()));
            }
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
    // Worker-only Product analysis. No M21 publication, no invented
    // reference or reweighting; real M19 and M20 measurements of actual
    // full-track RenderPreview output, once per attempted candidate.
    [[nodiscard]] core::Result<OfflineProductCandidateMeasurement>
    evaluate() const {
        using R=core::Result<OfflineProductCandidateMeasurement>;
        auto rendered=execute();
        if(!rendered)return R::failure(*rendered.error());
        if(!result_has_expected_scope(*rendered.value()))
            return R::failure(failed("L3_OFFLINE_PRODUCT_RENDER_SCOPE_STALE"));
        const auto levels=analysis::measure_basic_level_full_resource(
            rendered.value()->buffer());
        if(!levels)return R::failure(*levels.error());
        const auto view=rendered.value()->view();
        const auto tonal=analysis::measure_tonal_stereo_scope(
            view,view.absolute_start_frame(),view.frame_count(),
            view.timebase().frame_domain_id());
        if(!tonal)return R::failure(*tonal.error());
        return R::success(OfflineProductCandidateMeasurement{
            std::move(*rendered.value()),
            std::move(*levels.value()),std::move(*tonal.value())});
    }
    // Explicit opt-in that extracts REAL per-frame native E[t,b]
    // INSIDE this candidate's existing M20 FFT pass. Not an extra
    // analysis FFT or a surrogate M21 nine-metric probe. Invalid owner/
    // native Phase6 policy is rejected before executing any render.
    [[nodiscard]] core::Result<OfflineProductNativeSpectralCandidate>
    evaluate_with_native_spectral_d10_d12(
        std::vector<m05m06_real::NativeFrameBand> exact_bands,
        std::uint64_t current_m20_native_fft_size,
        bool current_full_phase6_frame_grid_attested,
        std::uint64_t phase6_exact_active_duration_frames,
        analysis::TonalStereoTrustedSupport supplied_support={}) const
    {
        using R=core::Result<OfflineProductNativeSpectralCandidate>;
        if(!current_full_phase6_frame_grid_attested||
           exact_bands.empty()||
           supplied_support.selected_native_frame_sink)
            return R::failure(
                failed("L3_M06_NATIVE_FRAME_AUTHORITY_UNAVAILABLE"));
        auto rendered=execute();
        if(!rendered)return R::failure(*rendered.error());
        if(!result_has_expected_scope(*rendered.value()))
            return R::failure(failed("L3_NATIVE_RENDER_SCOPE_STALE"));
        const auto levels=analysis::measure_basic_level_full_resource(
            rendered.value()->buffer());
        if(!levels)return R::failure(*levels.error());
        const auto view=rendered.value()->view();
        m05m06_real::AuthenticM20FrameEnergySink sink{
            static_cast<std::uint32_t>(view.format().sample_rate().value()),
            current_m20_native_fft_size,std::move(exact_bands),true};
        if(!sink.valid())
            return R::failure(
                failed("L3_M06_NATIVE_BAND_GRID_NOT_AUTHENTIC"));
        supplied_support.selected_native_frame_sink=&sink;
        const auto tonal=analysis::measure_tonal_stereo_scope(
            view,view.absolute_start_frame(),view.frame_count(),
            view.timebase().frame_domain_id(),supplied_support);
        if(!tonal)return R::failure(*tonal.error());
        if(!sink.valid()||
           sink.exact_e_tb().size()!=
             tonal.value()->selected_spectral_windows)
            return R::failure(
                failed("L3_M06_NATIVE_FRAME_SOURCE_MISMATCH"));
        auto d10_d12=m05m06_real::reduce_native_spectral_d10_d12(
            sink.exact_e_tb(),sink.configured_bands(),true,
            phase6_exact_active_duration_frames,
            static_cast<std::uint64_t>(
                view.format().sample_rate().value()));
        if(d10_d12.source_reason==
              m05m06_real::SpectralNativeReason::invalid_arithmetic||
           d10_d12.source_reason==
              m05m06_real::SpectralNativeReason::
                  invalid_energy_or_geometry)
            return R::failure(
                failed("L3_M06_NATIVE_STFT_ENERGY_INVALID"));
        return R::success(OfflineProductNativeSpectralCandidate{
            {std::move(*rendered.value()),std::move(*levels.value()),
             std::move(*tonal.value())},
            std::move(d10_d12),
            sink.canonical_original_ordinals()});
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
    [[nodiscard]] const std::shared_ptr<const dsp::ModuleRegistry>&
    registry_snapshot() const noexcept { return registry_; }
    [[nodiscard]] const std::shared_ptr<const render::RenderResult>&
    prepared_owner() const noexcept { return prepared_; }
    [[nodiscard]] const std::vector<dsp::ModuleExecutionBinding>&
    baseline_bindings() const noexcept { return baseline_bindings_; }
    [[nodiscard]] const std::string& ticket_identity() const noexcept {
        return ticket_identity_;
    }
    [[nodiscard]] bool identical_execution_to(
        const OfflineProductTrialJob& other) const noexcept {
        if(stamp_!=other.stamp_||prepared_!=other.prepared_
           ||registry_!=other.registry_
           ||maximum_block_frames_!=other.maximum_block_frames_
           ||chain_.context()!=other.chain_.context()
           ||chain_.revision()!=other.chain_.revision()
           ||candidate_bindings_!=other.candidate_bindings_)
            return false;
        const auto a=chain_.instances();
        const auto b=other.chain_.instances();
        return a.size()==b.size()
            &&std::equal(a.begin(),a.end(),b.begin());
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
