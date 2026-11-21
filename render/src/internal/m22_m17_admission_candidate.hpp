// L1-M22 PRIVATE gate rehearsal. Real M17 terminal outcome type is read-only.
// Does NOT qualify M17/M22, does NOT publish any PCM, no automatic consent.
#pragma once
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace rgsml::m22_private {
enum class M17Admission : std::uint8_t {
    PRIVATE_CHECKED_ELIGIBLE, PREDECESSOR_GATES_OPEN, REJECT_NONCONFORMING,
    REJECT_STALE_REALIZATION, REJECT_WRONG_OUTPUT_RATE_OR_DURATION,
    REJECT_INVALID_M16_EVIDENCE, REJECT_M17_BYPASS_CONTRADICTION
};
struct Prerequisites final {
    bool m17_formal_and_physical_qualified{false};
    bool post_m17_ci_audit_reset{false};
    bool m18_integrated_qualified{false};
    bool m19_integrated_qualified{false};
    bool m20_integrated_qualified{false};
    bool m21_integrated_qualified{false};
    bool exact_request_and_m17_realization_bound{false};
    friend bool operator==(const Prerequisites&, const Prerequisites&) = default;
    [[nodiscard]] bool complete() const noexcept {
        return m17_formal_and_physical_qualified && post_m17_ci_audit_reset
          && m18_integrated_qualified && m19_integrated_qualified
          && m20_integrated_qualified && m21_integrated_qualified
          && exact_request_and_m17_realization_bound;
    }
};
struct M17Evidence final {
    bool nonconforming{false};
    bool bypassed{false};
    bool has_final_m16_check{false};
    bool has_applied_gain_witness{false};
    rgsml::analysis::TruePeakQualificationStatus numerical_status{
        rgsml::analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED
    };
    rgsml::audio::FrameDomainId frame_domain{
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE
    };
    std::int64_t sample_rate_hz{0};
    std::int64_t absolute_first_frame{-1};
    std::int64_t full_frames{0};
    double before_linear_peak{0.0};
    double after_linear_peak{0.0};
    double global_trim_gain{1.0};
    friend bool operator==(const M17Evidence&, const M17Evidence&) = default;
};
struct M17FullProgrammeExpectation final {
    std::int64_t whole_source_frames{0};
    std::int64_t same_rate_hz{0};
    bool terminal_bypassed{false};
};

// Bridge reads actual M17 typed evidence, including the explicit
// nonconforming variant. The sample-rate and provenance/authentication
// checks must be provided by future real full-programme ownership.
[[nodiscard]] inline M17Evidence m22_observe_real_m17_outcome(
    const rgsml::render::TruePeakLimiterMeasuredOutcome& outcome) noexcept
{
    using namespace rgsml;
    M17Evidence e{};
    if (const auto* rejected=std::get_if<
          render::TruePeakLimiterMeasuredNonconformingCandidate>(&outcome)) {
        e.nonconforming=true;
        e.bypassed=rejected->terminal.bypassed;
        e.numerical_status=rejected->measurement_status;
        return e; // Never evaluate or forward potentially nonconforming PCM.
    }
    const auto& checked=std::get<render::TruePeakLimiterCheckedCandidate>(outcome);
    const auto view=checked.terminal.compensated_pcm.view();
    e.nonconforming=false;
    e.bypassed=checked.terminal.bypassed;
    e.has_final_m16_check=checked.final_check.has_value();
    e.has_applied_gain_witness=!checked.terminal.applied_gain_frames.empty();
    e.frame_domain=view.timebase().frame_domain_id();
    e.sample_rate_hz=view.format().sample_rate().value();
    e.absolute_first_frame=view.absolute_start_frame().value();
    e.full_frames=view.frame_count().value();
    if (checked.final_check) {
        e.numerical_status=checked.final_check->qualification_status;
        e.before_linear_peak=checked.final_check->before_linear_peak;
        e.after_linear_peak=checked.final_check->after_linear_peak;
        e.global_trim_gain=checked.final_check->global_trim_gain;
    }
    return e;
}

// A conditional *PRIVATE* admission model, not a real export permission.
// The prerequisites cannot currently be established from a qualified M22
// owner; all default FALSE. Green private CI can NEVER set them.
[[nodiscard]] inline M17Admission m22_classify_m17_for_future_export(
    const M17Evidence& actual,
    const M17FullProgrammeExpectation& expected,
    const Prerequisites& gates) noexcept
{
    using rgsml::analysis::TruePeakQualificationStatus;
    using rgsml::audio::FrameDomainId;
    if (actual.nonconforming) return M17Admission::REJECT_NONCONFORMING;
    if (!gates.complete()) return M17Admission::PREDECESSOR_GATES_OPEN;
    if (expected.whole_source_frames<=0 || expected.same_rate_hz<=0
        || actual.frame_domain!=FrameDomainId::OUTPUT_RATE
        || actual.absolute_first_frame!=0
        || actual.full_frames!=expected.whole_source_frames
        || actual.sample_rate_hz!=expected.same_rate_hz)
        return M17Admission::REJECT_WRONG_OUTPUT_RATE_OR_DURATION;
    if (actual.bypassed!=expected.terminal_bypassed)
        return M17Admission::REJECT_STALE_REALIZATION;
    if (actual.bypassed) {
        // Real M17 BYPASS_IDENTITY must have no limiter GR or false M16 check.
        return (!actual.has_final_m16_check && !actual.has_applied_gain_witness)
             ? M17Admission::PRIVATE_CHECKED_ELIGIBLE
             : M17Admission::REJECT_M17_BYPASS_CONTRADICTION;
    }
    // Only actual full-resource supported M16 evidence. No fabricated
    // numerical zero, no relabeling a nonconforming M17 outcome.
    if (!actual.has_final_m16_check
        || actual.numerical_status==TruePeakQualificationStatus::UNSUPPORTED
        || !std::isfinite(actual.before_linear_peak)
        || !std::isfinite(actual.after_linear_peak)
        || !std::isfinite(actual.global_trim_gain)
        || actual.before_linear_peak<0.0
        || actual.after_linear_peak<0.0
        || actual.global_trim_gain<=0.0
        || actual.global_trim_gain>1.0)
        return M17Admission::REJECT_INVALID_M16_EVIDENCE;
    // A SUPPORTED_UNQUALIFIED measurement remains correctly labeled so.
    // This only models candidate readiness *after* external formal gates,
    // never promotes numerical status to QUALIFIED or a product render.
    return M17Admission::PRIVATE_CHECKED_ELIGIBLE;
}
} // namespace rgsml::m22_private
