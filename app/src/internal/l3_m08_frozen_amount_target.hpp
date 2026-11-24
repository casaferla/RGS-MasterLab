#pragma once
// L3-M08 P01 ACCEPTED_FROZEN pure Amount and DesiredTarget law.
// This source-independent typed projection DOES NOT write Project,
// infer Phase6 evidence, calculate M10 measured cost, approve hard
// constraint projection, materialize PCM or claim M13 verification.
// RNE binary64 steps must not contract into FMA: explicit volatile
// storage is intentionally used between every frozen arithmetic step.
#include "l3_m07_frozen_binding_projection.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace rgsml::app::l3_automatch::m08_frozen {
inline constexpr std::string_view kAmountSchema=
    "rgsml.reference-match.amount-config.schema/1.0.0";
inline constexpr std::string_view kNumericPolicy=
    "rgsml.reference-match.amount.numeric/1.0.0";
inline constexpr std::string_view kTargetSchema=
    "rgsml.reference-match.domain-target-specification.schema/1.0.0";
inline constexpr std::string_view kTargetPolicy=
    "rgsml.reference-match.target-policy/1.0.0";
inline constexpr std::string_view kSerializationAdapter=
    "rgsml.reference-match.target.canonical-json/1.0.0";
enum class AmountFailure:std::uint8_t{
    none,invalid_number,out_of_range,domain_set_invalid
};
struct DomainAmount final {
    std::string_view domain_id;
    double stored{};
};
struct AmountConfig final {
    double global{};
    std::array<double,4> domains{};
    std::uint64_t independent_amount_revision{1};
    [[nodiscard]] constexpr bool authorizes_persistence()
        const noexcept{return false;} // M08 canonical JSON not built
};
struct AmountCreation final {
    std::optional<AmountConfig> config;
    AmountFailure failure{AmountFailure::invalid_number};
};
[[nodiscard]] inline AmountCreation create_amount_config(
    double global,const std::array<DomainAmount,4>& ordered,
    std::uint64_t independent_amount_revision)
{
    if(independent_amount_revision==0)
        return {{},AmountFailure::domain_set_invalid};
    if(!std::isfinite(global))
        return {{},AmountFailure::invalid_number};
    if(global<0.0||global>1.0)
        return {{},AmountFailure::out_of_range};
    AmountConfig value;
    // M08 Amount carrier normalizes -0 to +0 but source zero signs
    // in the separate feature artifacts are explicitly NOT changed.
    value.global=global==0.0?0.0:global;
    for(std::size_t i=0;i<ordered.size();++i) {
        if(ordered[i].domain_id!=m07_frozen::kDomainIds[i])
            return {{},AmountFailure::domain_set_invalid};
        if(!std::isfinite(ordered[i].stored))
            return {{},AmountFailure::invalid_number};
        if(ordered[i].stored<0.0||ordered[i].stored>1.0)
            return {{},AmountFailure::out_of_range};
        value.domains[i]=ordered[i].stored==0.0?
            0.0:ordered[i].stored;
    }
    value.independent_amount_revision=independent_amount_revision;
    return {value,AmountFailure::none};
}
struct EffectiveAmount final {
    double rounded{};
    // True only for two STORED positive Amounts whose rounded
    // multiplication produces 0, NOT for any stored Amount=0.
    bool positive_underflow_to_zero{};
};
[[nodiscard]] inline EffectiveAmount effective_amount(
    const AmountConfig& config,std::size_t canonical_domain) noexcept
{
    if(canonical_domain>=config.domains.size())
        return {0.0,false}; // caller must validate canonical domain
    volatile double product=
        config.global*config.domains[canonical_domain];
    const double a=product;
    return {a,config.global>0.0
                  &&config.domains[canonical_domain]>0.0
                  &&a==0.0};
}
enum class Endpoint:std::uint8_t{
    ordinary,mono_side_muted,positive_width_floor,
    intermediary_not_endpoint_attested
};
struct CanonicalCoordinate final {
    std::optional<double> z;
    Endpoint endpoint{Endpoint::ordinary};
    friend bool operator==(const CanonicalCoordinate&,
                           const CanonicalCoordinate&)=default;
};
enum class TargetFailure:std::uint8_t {
    none,invalid_amount_domain,missing_or_nonfinite_input,
    effective_amount_underflow_to_zero,nonfinite_interpolation
};
struct DesiredCoordinate final {
    double desired_z{};
    Endpoint endpoint{Endpoint::ordinary};
    double effective_amount{};
    // Strict boundary: candidate achieved, M08 constraintAdjusted and
    // M13 fresh verification are NOT filled by interpolation math.
    std::optional<double> constraint_adjusted_z;
    std::optional<double> achieved_z;
    [[nodiscard]] constexpr bool authorizes_solver_or_publication()
        const noexcept{return false;}
};
struct TargetEvaluation final {
    std::optional<DesiredCoordinate> value;
    TargetFailure failure{TargetFailure::invalid_amount_domain};
    bool positive_effective_underflow{false};
};
[[nodiscard]] inline TargetEvaluation desired_for_component(
    const AmountConfig& amounts,std::size_t canonical_domain,
    CanonicalCoordinate prepared,CanonicalCoordinate gold)
{
    if(canonical_domain>=4)
        return {{},TargetFailure::invalid_amount_domain,false};
    if(!prepared.z||!gold.z
       ||!std::isfinite(*prepared.z)||!std::isfinite(*gold.z))
        return {{},TargetFailure::missing_or_nonfinite_input,false};
    const auto effective=effective_amount(amounts,canonical_domain);
    if(effective.positive_underflow_to_zero) {
        // Underflow is an explicit numeric reason, not a successful
        // zero-Amount controller suspension or source invalidity.
        return {{},TargetFailure::effective_amount_underflow_to_zero,true};
    }
    if(effective.rounded==0.0)
        return {DesiredCoordinate{
            *prepared.z,prepared.endpoint,0.0,{},{}},
            TargetFailure::none,false};
    if(effective.rounded==1.0)
        return {DesiredCoordinate{
            *gold.z,gold.endpoint,1.0,{},{}},
            TargetFailure::none,false};
    volatile double delta=*gold.z-*prepared.z;
    volatile double movement=effective.rounded*delta;
    volatile double sum=*prepared.z+movement;
    const double result=sum;
    if(!std::isfinite(result))
        return {{},TargetFailure::nonfinite_interpolation,false};
    // An intermediate result that happens to numerically equal the
    // MONO endpoint does NOT acquire a new exact endpoint identity.
    return {DesiredCoordinate{
        result,Endpoint::intermediary_not_endpoint_attested,
        effective.rounded,{},{}},TargetFailure::none,false};
}
} // namespace rgsml::app::l3_automatch::m08_frozen
