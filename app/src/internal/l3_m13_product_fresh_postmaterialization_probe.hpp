#pragma once
// L3-M13: Product-owned immutable verification input. A fresh PCM probe
// is distinct from the M12 accepted Preview and from M13 VERIFIED.
// Only ProjectSessionViewModel may issue the request after authenticating
// the complete current M12 Product owner and its real Project readback.
// The independent M16 ceiling is NOT the M17 WAV Export ceiling.
// Call the numerical function on a worker, never synchronously on Qt UI.
#include "l3_m12_product_preview_worker_ticket.hpp"
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
    core::FrameCount maximum_block_frames)
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
    // One NEW full native traversal in the Product-approved order:
    // PREPARED -> protected MANUAL -> REF_LINKED -> optional user P2.
    // No WAV Export, no reuse of the already accepted PROCESSED render.
    m12_frozen::M12NativeStageExecutionWitness witness;
    auto render=m12_frozen::render_restored_m12_full_native_pcm_once(
        *owner.replay,*owner.exact_prepared_owner,maximum_block_frames,
        &witness);
    if(!render)return R::failure(*render.error());
    auto m16=analysis::measure_true_peak_full_resource(
        render.value()->buffer());
    if(!m16)return R::failure(*m16.error());
    const double observed=m16.value()->global_linear_peak();
    const double allowed=std::pow(10.0,ceiling/20.0);
    if(!std::isfinite(observed)||!std::isfinite(allowed)||
       allowed<=0.0||observed>allowed)
        return fail("L3_M13_PRODUCT_FRESH_M16_SAFETY_EXCEEDED");
    auto m19=analysis::measure_basic_level_full_resource(
        render.value()->buffer());
    if(!m19)return R::failure(*m19.error());
    const auto view=render.value()->view();
    auto m20=analysis::measure_tonal_stereo_scope(
        view,view.absolute_start_frame(),view.frame_count(),
        view.timebase().frame_domain_id());
    if(!m20)return R::failure(*m20.error());
    return R::success(M13FreshProductMasterProbe{
        std::move(*render.value()),std::move(*m16.value()),
        std::move(*m19.value()),std::move(*m20.value()),
        std::move(witness),ceiling,1U});
}
} // namespace l3_automatch::m13_frozen
} // namespace rgsml::app
