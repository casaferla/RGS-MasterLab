#pragma once
// One exact M10/P2 full native candidate: optionally reuse verified
// pre-P2 PCM, then run ONE actual P2 suffix render, ONE M19, ONE M20
// with all native frame taps, one P04 and independent whole-track M16.
// This remains a nonpublishing fragment; M07 authenticated 27-feature
// binding, M11 hard gate and M10 canonical total cost remain external.
#include "l3_m10_p2_real_native_product_job.hpp"
#include "l3_product_native_one_pass.hpp"
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <atomic>
#include <cmath>
#include <optional>
#include <utility>

namespace rgsml::app::l3_automatch {
struct M10P2FullNativeEvidence final {
    ProductNativeOnePassResult native;
    analysis::TruePeakMeasurement m16_actual_p2_output;
    [[nodiscard]] constexpr bool authentic_27_slot_m10_score()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool authorizes_master_publication()
        const noexcept{return false;}
};
// Allows the existing ProductNativeOnePass M19/M20 observation path to
// accept a proof-bound prefix without re-rendering its fixed DSP.
class M10P2ExactPrefixJobView final {
public:
    M10P2ExactPrefixJobView(
        const M10P2RealNativeProductJob& job,
        const M10P2ExactPrefix& prefix) noexcept
        :job_(job),prefix_(prefix) {}
    [[nodiscard]] core::Result<render::RenderResult> execute() const {
        return job_.execute_with_exact_pre_p2_prefix(prefix_);
    }
    [[nodiscard]] bool result_has_expected_scope(
        const render::RenderResult& result) const noexcept {
        return job_.result_has_expected_scope(result);
    }
private:
    const M10P2RealNativeProductJob& job_;
    const M10P2ExactPrefix& prefix_;
};
class M10ExactFixedPrefixJobView final {
public:
    M10ExactFixedPrefixJobView(const M10P2RealNativeProductJob& job,
        const M10ExactFixedPrefix& prefix) noexcept:job_(job),prefix_(prefix){}
    [[nodiscard]] core::Result<render::RenderResult> execute() const {
        return job_.execute_with_exact_fixed_prefix(prefix_);
    }
    [[nodiscard]] bool result_has_expected_scope(const render::RenderResult& result) const noexcept {
        return job_.result_has_expected_scope(result);
    }
private:
    const M10P2RealNativeProductJob& job_;
    const M10ExactFixedPrefix& prefix_;
};
[[nodiscard]] inline core::Result<M10P2FullNativeEvidence>
measure_m10_p2_full_native_once(
    const M10P2RealNativeProductJob& job,
    ProductNativeOnePassSpec spec,
    const M10P2ExactPrefix* exact_prefix=nullptr,
    const std::atomic<bool>* cancellation=nullptr,
    const M10ExactFixedPrefix* exact_fixed_prefix=nullptr)
{
    using R=core::Result<M10P2FullNativeEvidence>;
    if(exact_prefix&&exact_fixed_prefix)return R::failure(core::Error{
        core::ErrorCode::InvalidState,"L3_M10_AMBIGUOUS_PREFIX_CACHE"});
    core::Result<ProductNativeOnePassResult> measured=
        exact_prefix
        ?measure_real_product_candidate_once(
             M10P2ExactPrefixJobView{job,*exact_prefix},
             std::move(spec),cancellation)
        :exact_fixed_prefix
            ?measure_real_product_candidate_once(
                M10ExactFixedPrefixJobView{job,*exact_fixed_prefix},std::move(spec),cancellation)
            :measure_real_product_candidate_once(job,std::move(spec),cancellation);
    if(!measured)return R::failure(*measured.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M10_P2_CANCELLED_AFTER_REAL_ANALYSIS"});
    const auto m16=analysis::measure_true_peak_full_resource(
        measured.value()->actual.rendered.buffer());
    if(!m16)return R::failure(*m16.error());
    if(!std::isfinite(m16.value()->global_linear_peak())||
       m16.value()->global_linear_peak()<0.0)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M10_P2_ACTUAL_M16_INVALID"});
    return R::success(M10P2FullNativeEvidence{
        std::move(*measured.value()),std::move(*m16.value())});
}
} // namespace rgsml::app::l3_automatch
