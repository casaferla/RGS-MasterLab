#pragma once
// L3-M13: Product-owned immutable verification input. A fresh PCM probe
// is distinct from the M12 accepted Preview and from M13 VERIFIED.
// Only ProjectSessionViewModel may issue the request after authenticating
// the complete current M12 Product owner and its real Project readback.
// The independent M16 ceiling is NOT the M17 WAV Export ceiling.
// Call the numerical function on a worker, never synchronously on Qt UI.
#include "l3_m12_product_preview_worker_ticket.hpp"
#include "l3_product_native_one_pass.hpp"
#include <atomic>
#include <optional>
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

namespace rgsml::app {
class ProjectSessionViewModel;
namespace l3_automatch::m13_frozen {

class M13ImmutableProductFreshProbeRequest final {
public:
    M13ImmutableProductFreshProbeRequest() = delete;
    [[nodiscard]] const m12_frozen::M12ImmutableProductPreviewTicket&
    exact_materialized_preview_owner() const noexcept
    {return *ticket_;}
    [[nodiscard]] double frozen_independent_m16_ceiling_db_tp()
        const noexcept{return ceilingDbTp_;}
    [[nodiscard]] std::uint64_t exact_product_ownership_epoch()
        const noexcept{return exactProductEpoch_;}
    [[nodiscard]] bool authorizes_m13_verified() const noexcept
    {return false;}
    [[nodiscard]] bool authorizes_wav_export() const noexcept
    {return false;}
private:
    friend class ::rgsml::app::ProjectSessionViewModel;
    explicit M13ImmutableProductFreshProbeRequest(
        std::shared_ptr<const m12_frozen::M12ImmutableProductPreviewTicket>
            owner,double ceiling,std::uint64_t product_epoch)
        :ticket_(std::move(owner)),ceilingDbTp_(ceiling),
         exactProductEpoch_(product_epoch){}
    std::shared_ptr<const m12_frozen::M12ImmutableProductPreviewTicket>
        ticket_;
    double ceilingDbTp_{};
    // Guards ABA: an Undo/Redo or same-SOURCE Project Open can restore
    // exactly equal DSP/PCM but never revive an earlier M13 worker result.
    std::uint64_t exactProductEpoch_{};
};

// Additional original observations captured in the same M20 analysis
// pass, only when an exact native source/grid specification is supplied.
struct M13FreshAdditionalNativeObservations final {
    m05m06_real::SpectralD10D12Native d10_d12;
    m05m06_real::StereoCoherentNativeResult s03_s06;
    m05m06_real::P04ProductObservation d07_d09;
    std::vector<std::uint64_t> selected_original_m20_ordinals;
    std::uint64_t exact_phase6_active_frames{};
};

struct M13FreshProductMasterProbe final {
    // New real DSP rendering; neither the prior Preview PCM nor the
    // M10 optimization candidate is passed into this result.
    render::RenderResult fresh_full_master_pcm;
    analysis::TruePeakMeasurement fresh_m16;
    analysis::BasicLevelAnalysisResult fresh_m19;
    analysis::TonalStereoAnalysisResult fresh_m20;
    // Ordinary DSP execution signatures from the REAL Product engine
    // for MANUAL, REF_LINKED and optional terminal P2 in strict order.
    m12_frozen::M12NativeStageExecutionWitness stage_witness;
    double independent_m16_ceiling_db_tp{};
    std::uint32_t logical_verification_evaluations{1U};
    std::optional<M13FreshAdditionalNativeObservations> additional_native;
    // Bind the worker output to the request it actually executed.
    // Keeping the immutable ticket alive also prevents pointer ABA.
    M13ImmutableProductFreshProbeRequest executed_request;
    [[nodiscard]] bool matches_executed_request(
        const M13ImmutableProductFreshProbeRequest& request)const noexcept
    {
        return &executed_request.exact_materialized_preview_owner()==
                   &request.exact_materialized_preview_owner()&&
               executed_request.exact_product_ownership_epoch()==
                   request.exact_product_ownership_epoch()&&
               executed_request.frozen_independent_m16_ceiling_db_tp()==
                   request.frozen_independent_m16_ceiling_db_tp();
    }
    [[nodiscard]] constexpr bool is_m13_verified() const noexcept
    {return false;}
    [[nodiscard]] constexpr bool is_qualification_receipt() const noexcept
    {return false;}
    [[nodiscard]] constexpr bool authorizes_wav_export() const noexcept
    {return false;}
};

[[nodiscard]] inline core::Result<M13FreshProductMasterProbe>
run_fresh_m13_product_postmaterialization_probe(
    const M13ImmutableProductFreshProbeRequest& admitted,
    core::FrameCount maximum_block_frames,
    std::optional<ProductNativeOnePassSpec> exact_native_spec=std::nullopt,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<M13FreshProductMasterProbe>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    const auto& owner=admitted.exact_materialized_preview_owner();
    if(!owner.replay||!owner.exact_prepared_owner||
       owner.replay->project_id.is_nil()||
       owner.replay->controller_id.is_nil()||
       owner.replay->plan_id.is_nil()||
       owner.replay->linked_chain_id.is_nil()||
       maximum_block_frames.value()<=0)
        return fail("L3_M13_PRODUCT_FRESH_M12_OWNER_INVALID");
    const double ceiling=admitted.frozen_independent_m16_ceiling_db_tp();
    if(!std::isfinite(ceiling))
        return fail("L3_M13_PRODUCT_FRESH_INDEPENDENT_M16_LIMIT_INVALID");
    if(exact_native_spec&&
       (!exact_native_spec->current_full_phase6_source_and_grid||
        exact_native_spec->exact_m20_fft_size<2U||
        exact_native_spec->spectral_bands.empty()||
        exact_native_spec->source_support.selected_native_frame_sink||
        exact_native_spec->source_support.selected_stereo_frame_sink))
        return fail("L3_M13_PRODUCT_FRESH_NATIVE_GRID_UNAVAILABLE");
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_M13_PRODUCT_FRESH_CANCELLED_BEFORE_RENDER");
    // One NEW full native traversal in the Product-approved order:
    // PREPARED -> protected MANUAL -> REF_LINKED -> optional user P2.
    // No WAV Export, no reuse of the already accepted PROCESSED render.
    m12_frozen::M12NativeStageExecutionWitness witness;
    auto render=m12_frozen::render_restored_m12_full_native_pcm_once(
        *owner.replay,*owner.exact_prepared_owner,maximum_block_frames,
        &witness);
    if(!render)return R::failure(*render.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_M13_PRODUCT_FRESH_CANCELLED_AFTER_RENDER");
    const double allowed=std::pow(10.0,ceiling/20.0);
    const auto safe_peak=[&](const analysis::TruePeakMeasurement& m16) {
        const double observed=m16.global_linear_peak();
        return std::isfinite(observed)&&observed>=0.0&&
            std::isfinite(allowed)&&allowed>0.0&&observed<=allowed;
    };
    // Use the ORIGINAL full-resource native analyzer when exact Phase6
    // inputs exist. It owns M19 and M20, so do not run either a second
    // time or use the earlier M10 candidate's PCM/observations.
    if(exact_native_spec) {
        auto observed_native=measure_real_full_owned_pcm_native_once(
            render.value()->buffer(),std::move(*exact_native_spec),
            cancellation);
        if(!observed_native)return R::failure(*observed_native.error());
        auto& n=*observed_native.value();
        if(!n.exact_full_resource_m16)
            return fail("L3_M13_PRODUCT_FRESH_M16_UNAVAILABLE");
        if(!safe_peak(*n.exact_full_resource_m16))
            return fail("L3_M13_PRODUCT_FRESH_M16_SAFETY_EXCEEDED");
        return R::success(M13FreshProductMasterProbe{
            std::move(*render.value()),std::move(*n.exact_full_resource_m16),
            std::move(n.levels),std::move(n.tonal),
            std::move(witness),ceiling,1U,
            M13FreshAdditionalNativeObservations{
                std::move(n.d10_d12),std::move(n.s03_s06),
                std::move(n.d07_d09),
                std::move(n.selected_original_m20_ordinals),
                n.exact_observed_active_frames},admitted});
    }
    auto m16=analysis::measure_true_peak_full_resource(render.value()->buffer());
    if(!m16)return R::failure(*m16.error());
    if(!safe_peak(*m16.value()))return fail("L3_M13_PRODUCT_FRESH_M16_SAFETY_EXCEEDED");
    auto m19=analysis::measure_basic_level_full_resource(
        render.value()->buffer());
    if(!m19)return R::failure(*m19.error());
    const auto view=render.value()->view();
    auto m20=analysis::measure_tonal_stereo_scope(
        view,view.absolute_start_frame(),view.frame_count(),
        view.timebase().frame_domain_id());
    if(!m20)return R::failure(*m20.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_M13_PRODUCT_FRESH_CANCELLED_AFTER_ANALYSIS");
    return R::success(M13FreshProductMasterProbe{
        std::move(*render.value()),std::move(*m16.value()),
        std::move(*m19.value()),std::move(*m20.value()),
        std::move(witness),ceiling,1U,std::nullopt,admitted});
}
} // namespace l3_automatch::m13_frozen
} // namespace rgsml::app
