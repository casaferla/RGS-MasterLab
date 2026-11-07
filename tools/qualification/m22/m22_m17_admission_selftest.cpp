#include "m22_m17_admission_candidate.hpp"
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <string>
#include <iostream>
using namespace rgsml::m22_private;
using rgsml::analysis::TruePeakQualificationStatus;
using rgsml::audio::FrameDomainId;
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(std::string("M22_M17_ADMISSION_FAIL_")+std::to_string(__LINE__));}while(0)
static_assert(std::is_same_v<
    decltype(m22_observe_real_m17_outcome(
        std::declval<const rgsml::render::TruePeakLimiterMeasuredOutcome&>())),
    M17Evidence>);
int main() {
try{
    M17Evidence e{};
    e.frame_domain=FrameDomainId::OUTPUT_RATE;
    e.sample_rate_hz=48000;
    e.absolute_first_frame=0;
    e.full_frames=5151;
    e.has_final_m16_check=true;
    e.numerical_status=TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED;
    e.before_linear_peak=0.85;e.after_linear_peak=0.83;
    e.global_trim_gain=0.985;
    const M17FullProgrammeExpectation target{5151,48000,false};
    Prerequisites gates{};
    // Private all-green FQ never fakes a formal physical M17 gate.
    REQUIRE(m22_classify_m17_for_future_export(e,target,gates)==
            M17Admission::PREDECESSOR_GATES_OPEN);
    gates.m17_formal_and_physical_qualified=true;
    gates.post_m17_ci_audit_reset=true;
    gates.m18_integrated_qualified=true;
    gates.m19_integrated_qualified=true;
    gates.m20_integrated_qualified=true;
    gates.m21_integrated_qualified=true;
    REQUIRE(!gates.complete()); // real immutable ownership still unproven
    REQUIRE(m22_classify_m17_for_future_export(e,target,gates)==
            M17Admission::PREDECESSOR_GATES_OPEN);
    gates.exact_request_and_m17_realization_bound=true;
    REQUIRE(m22_classify_m17_for_future_export(e,target,gates)==
            M17Admission::PRIVATE_CHECKED_ELIGIBLE);
    auto alt=e;alt.nonconforming=true;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_NONCONFORMING);
    alt=e;alt.full_frames=5150;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_WRONG_OUTPUT_RATE_OR_DURATION);
    alt=e;alt.absolute_first_frame=1;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_WRONG_OUTPUT_RATE_OR_DURATION);
    alt=e;alt.frame_domain=FrameDomainId::SOURCE_PROCESSING_RATE;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_WRONG_OUTPUT_RATE_OR_DURATION);
    alt=e;alt.sample_rate_hz=44100;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_WRONG_OUTPUT_RATE_OR_DURATION);
    alt=e;alt.has_final_m16_check=false;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_INVALID_M16_EVIDENCE);
    alt=e;alt.numerical_status=TruePeakQualificationStatus::UNSUPPORTED;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_INVALID_M16_EVIDENCE);
    alt=e;alt.global_trim_gain=1.1;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_INVALID_M16_EVIDENCE);
    alt=e;alt.bypassed=true;
    REQUIRE(m22_classify_m17_for_future_export(alt,target,gates)==
            M17Admission::REJECT_STALE_REALIZATION);
    const M17FullProgrammeExpectation bypassTarget{5151,48000,true};
    alt=e;alt.bypassed=true;
    REQUIRE(m22_classify_m17_for_future_export(alt,bypassTarget,gates)==
            M17Admission::REJECT_M17_BYPASS_CONTRADICTION);
    alt.has_final_m16_check=false;
    alt.has_applied_gain_witness=false;
    REQUIRE(m22_classify_m17_for_future_export(alt,bypassTarget,gates)==
            M17Admission::PRIVATE_CHECKED_ELIGIBLE);
    alt.has_applied_gain_witness=true;
    REQUIRE(m22_classify_m17_for_future_export(alt,bypassTarget,gates)==
            M17Admission::REJECT_M17_BYPASS_CONTRADICTION);
    // False external predecessor gate always rejects even a clean bypass.
    gates.m21_integrated_qualified=false;
    REQUIRE(m22_classify_m17_for_future_export(e,target,gates)==
            M17Admission::PREDECESSOR_GATES_OPEN);
    std::cout<<"M22_PRIVATE_REAL_M17_TYPE_ADMISSION_FAIL_CLOSED_PASS_NOT_FQ\n";
    return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
}
