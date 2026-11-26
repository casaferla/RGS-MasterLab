#pragma once
// L3-M13 P03 ACCEPTED/FROZEN: the sole Reference Match projection of
// rgsml.qualification.tolerance/1.0.0 (PROVISIONAL). This is NOT a
// second QTP and never grants release-authoritative VERIFIED status.
// Frozen M07 owns 27 binding IDs, source features and transforms.
#include "l3_m07_frozen_binding_projection.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace rgsml::app::l3_automatch::m13_frozen {
inline constexpr std::string_view kQtpId=
    "rgsml.qualification.tolerance/1.0.0";
inline constexpr std::string_view kQtpState="PROVISIONAL";
inline constexpr std::string_view kVerifierId=
    "rgsml.reference-match.verifier/1.0.0";
inline constexpr std::string_view kReportSchemaId=
    "rgsml.reference-match.verification-report.schema/1.0.0";
inline constexpr std::string_view kVerificationPolicyId=
    "rgsml.reference-match.verification-policy/1.0.0";

enum class QtpCategory:std::uint8_t {
    band_profile_db,slope_db_per_octave,positive_hz,
    loudness_lu,lra_lu,generic_db,positive_ms,
    correlation_raw_rho,positive_width_ratio,true_peak_db_tp
};
struct QtpBinding final {
    std::string_view binding_id;
    QtpCategory category;
    std::string_view native_unit;
    m07_frozen::Transform source_transform;
};
// P03's exact 27 positions, in existing M07 canonical slot order.
// T1 selects exactly one primary/fallback grid. D10/D11 are generic
// spread dB, NOT T1 profile RMS. T2/T6 use slope coefficient category.
inline constexpr std::array<QtpBinding,27> kQtpBindings{{
    {m07_frozen::kBindings[0].binding_id,QtpCategory::band_profile_db,
     "POWER_FRACTION_DB",m07_frozen::Transform::power_fraction_db},
    {m07_frozen::kBindings[1].binding_id,QtpCategory::slope_db_per_octave,
     "dB/oct",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[2].binding_id,QtpCategory::positive_hz,
     "Hz",m07_frozen::Transform::log2_positive},
    {m07_frozen::kBindings[3].binding_id,QtpCategory::positive_hz,
     "Hz",m07_frozen::Transform::log2_positive},
    {m07_frozen::kBindings[4].binding_id,QtpCategory::positive_hz,
     "Hz",m07_frozen::Transform::log2_positive},
    {m07_frozen::kBindings[5].binding_id,QtpCategory::slope_db_per_octave,
     "dB/oct",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[6].binding_id,QtpCategory::loudness_lu,
     "LU",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[7].binding_id,QtpCategory::lra_lu,
     "LU",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[8].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[9].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[10].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[11].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[12].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[13].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[14].binding_id,QtpCategory::positive_ms,
     "ms",m07_frozen::Transform::log2_positive},
    {m07_frozen::kBindings[15].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[16].binding_id,QtpCategory::generic_db,
     "dB",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[17].binding_id,QtpCategory::correlation_raw_rho,
     "raw rho",m07_frozen::Transform::fisher_z},
    {m07_frozen::kBindings[18].binding_id,QtpCategory::positive_width_ratio,
     "linear width ratio",m07_frozen::Transform::mono_endpoint_log2},
    {m07_frozen::kBindings[19].binding_id,QtpCategory::correlation_raw_rho,
     "raw rho",m07_frozen::Transform::fisher_z},
    {m07_frozen::kBindings[20].binding_id,QtpCategory::positive_width_ratio,
     "linear width ratio",m07_frozen::Transform::mono_endpoint_log2},
    {m07_frozen::kBindings[21].binding_id,QtpCategory::correlation_raw_rho,
     "raw rho",m07_frozen::Transform::fisher_z},
    {m07_frozen::kBindings[22].binding_id,QtpCategory::positive_width_ratio,
     "linear width ratio",m07_frozen::Transform::mono_endpoint_log2},
    {m07_frozen::kBindings[23].binding_id,QtpCategory::correlation_raw_rho,
     "raw rho",m07_frozen::Transform::fisher_z},
    {m07_frozen::kBindings[24].binding_id,QtpCategory::loudness_lu,
     "LUFS",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[25].binding_id,QtpCategory::loudness_lu,
     "LUFS",m07_frozen::Transform::identity},
    {m07_frozen::kBindings[26].binding_id,QtpCategory::true_peak_db_tp,
     "dBTP",m07_frozen::Transform::identity}
}};
[[nodiscard]] consteval bool frozen_qtp_projection_valid() {
    for(std::size_t i=0;i<kQtpBindings.size();++i) {
        if(kQtpBindings[i].binding_id!=m07_frozen::kBindings[i].binding_id||
           kQtpBindings[i].source_transform!=
               m07_frozen::kBindings[i].transform||
           kQtpBindings[i].native_unit.empty())return false;
        for(std::size_t j=0;j<i;++j)
            if(kQtpBindings[j].binding_id==kQtpBindings[i].binding_id)
                return false;
    }
    return true;
}
static_assert(frozen_qtp_projection_valid());
inline constexpr double kSameRunProximityFractionBound=0.0005;
inline constexpr double kIndependentPlatformProximityFractionBound=0.0002;

enum class QtpOutcome:std::uint8_t {
    pass,mismatch,unavailable
};
struct QtpNumericPredicate final {
    QtpOutcome outcome{QtpOutcome::unavailable};
    std::optional<double> absolute_delta;
    std::optional<double> allowed_bound;
    [[nodiscard]] bool passed() const noexcept
    {return outcome==QtpOutcome::pass;}
};
[[nodiscard]] inline QtpNumericPredicate compare_qtp_native_scalar(
    QtpCategory category,std::optional<double> reference,
    std::optional<double> actual)
{
    if(category==QtpCategory::band_profile_db||!reference||!actual||
       !std::isfinite(*reference)||!std::isfinite(*actual))
        return {};
    // Endpoint and positive/native branch identity is categorical:
    // do not admit a zero or sign change under numeric QTP slack.
    if(category==QtpCategory::positive_hz||
       category==QtpCategory::positive_ms) {
        if(*reference<=0.0||*actual<=0.0)
            return {QtpOutcome::mismatch,{},{}};
    }
    if(category==QtpCategory::positive_width_ratio) {
        if(*reference<0.0||*actual<0.0||
           ((*reference==0.0)!=(*actual==0.0)))
            return {QtpOutcome::mismatch,{},{}};
    }
    if(category==QtpCategory::correlation_raw_rho) {
        if(std::abs(*reference)>1.0||std::abs(*actual)>1.0||
           ((*reference==1.0)!=(*actual==1.0))||
           ((*reference==-1.0)!=(*actual==-1.0)))
            return {QtpOutcome::mismatch,{},{}};
    }
    double bound{};
    switch(category) {
    case QtpCategory::slope_db_per_octave:
        bound=std::max(1e-4,1e-4*std::abs(*reference));break;
    case QtpCategory::positive_hz:
    case QtpCategory::positive_ms:
    case QtpCategory::positive_width_ratio:
        bound=std::max(1e-6,1e-5*std::abs(*reference));break;
    case QtpCategory::correlation_raw_rho:
        bound=2e-5;break;
    case QtpCategory::lra_lu: bound=0.02;break;
    case QtpCategory::true_peak_db_tp:
    case QtpCategory::generic_db:
    case QtpCategory::loudness_lu: bound=0.01;break;
    case QtpCategory::band_profile_db: return {};
    default: return {};
    }
    const double delta=std::abs(*actual-*reference);
    if(!std::isfinite(delta)||!std::isfinite(bound))
        return {QtpOutcome::mismatch,{},{}};
    return {delta<=bound?QtpOutcome::pass:QtpOutcome::mismatch,
            delta,bound};
}
struct QtpProfilePredicate final {
    QtpOutcome outcome{QtpOutcome::unavailable};
    std::optional<double> max_component_error_db;
    std::optional<double> unweighted_rms_error_db;
};
[[nodiscard]] inline QtpProfilePredicate compare_qtp_t1_profile(
    std::span<const double> reference,std::span<const double> actual)
{
    if(reference.empty()||reference.size()!=actual.size())return {};
    double maximum=0.0,sum=0.0;
    for(std::size_t i=0;i<reference.size();++i) {
        if(!std::isfinite(reference[i])||!std::isfinite(actual[i]))
            return {};
        const double error=std::abs(actual[i]-reference[i]);
        if(!std::isfinite(error))return {QtpOutcome::mismatch,{},{}};
        maximum=std::max(maximum,error);
        sum+=error*error;
    }
    const double rms=std::sqrt(sum/static_cast<double>(reference.size()));
    if(!std::isfinite(rms))return {QtpOutcome::mismatch,{},{}};
    return {maximum<=0.01&&rms<=0.003
        ?QtpOutcome::pass:QtpOutcome::mismatch,maximum,rms};
}
} // namespace rgsml::app::l3_automatch::m13_frozen
