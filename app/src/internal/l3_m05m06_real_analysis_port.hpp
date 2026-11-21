#pragma once
// L3-M05/M06 read-only REAL analysis carrier. Both tracks must be full
// owning programme measurements from existing public M19/M20 producers.
// It extracts only facts those producers ACTUALLY contain; it never
// generates the missing M05/M06 features, Phase-6 status, authoritative
// M08 optimizationTarget, or M10 J/achieved/verified labels.
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
enum class Readiness:std::uint8_t {
    measured, missing, unqualified, incompatible, invalid
};
struct Value final {
    std::optional<double> unrounded;
    Readiness state{Readiness::missing};
    [[nodiscard]] bool ready() const noexcept {
        return state==Readiness::measured&&unrounded.has_value();
    }
};
struct SourceIdentity final {
    // Assertions from the actual M18/SOURCE/REFERENCE owner; this value
    // type itself cannot certify Qt generation, Phase-6 status or the
    // freshness of the file. Owner rechecks remain mandatory.
    std::string resource_identity;
    std::uint64_t generation{};
    bool full_track_owner_confirmed{};
};
struct SourceReadings final {
    SourceIdentity owner;
    Value integrated_lufs;    // frozen L1
    Value true_peak_db_tp;    // frozen L3 only if M16 QUALIFIED
    Value broadband_width;   // frozen S01 raw observation, not M06 score
    // M20 raw M20 correlation P10/P50/P90. Frozen S02 support/transform
    // is NOT yet bound here: these do not make S02 ready automatically.
    Value correlation_p10;
    Value correlation_p50;
    Value correlation_p90;
    struct Band final {
        int canonical_index{};
        double center_hz{},low_hz{},high_hz{};
        Value power_fraction_db; // 10log10(f), not raw linear fraction
    };
    std::vector<Band> profile_1_12; // T1 partial, no 1/3 fallback
};
enum class ImportProblem:std::uint8_t {
    none,owner_unconfirmed,scope_mismatch,invalid_geometry
};
struct Import final {
    std::optional<SourceReadings> source;
    ImportProblem problem{ImportProblem::owner_unconfirmed};
};
[[nodiscard]] inline Value from_m19(
    const analysis::BasicLevelValue& input)
{
    using S=analysis::BasicLevelValueState;
    if(input.state==S::AVAILABLE) {
        if(input.db&&std::isfinite(*input.db))
            return {input.db,Readiness::measured};
        return {{},Readiness::invalid};
    }
    if(input.state==S::M16_SUPPORTED_UNQUALIFIED)
        return {{},Readiness::unqualified};
    if(input.state==S::NUMERIC_UNDEFINED)
        return {{},Readiness::invalid};
    return {{},Readiness::missing};
}
[[nodiscard]] inline Value from_m20(
    const analysis::TonalStereoValue& input)
{
    using S=analysis::TonalStereoMetricState;
    if(input.state==S::AVAILABLE) {
        if(input.value&&std::isfinite(*input.value))
            return {input.value,Readiness::measured};
        return {{},Readiness::invalid};
    }
    if(input.state==S::UNDEFINED_DENOMINATOR)
        return {{},Readiness::invalid};
    if(input.state==S::NOT_APPLICABLE)
        return {{},Readiness::incompatible};
    return {{},Readiness::missing};
}
// Do not substitute an M20 raw power fraction or a bar-panel delta for
// M05 T1's POWER_FRACTION_DB; this is the one exact component transform.
[[nodiscard]] inline Value power_fraction_to_db(
    const analysis::TonalStereoValue& fraction)
{
    const auto measured=from_m20(fraction);
    if(!measured.ready())return measured;
    if(*measured.unrounded<=0.0 ||*measured.unrounded>1.0)
        return {{},Readiness::invalid};
    const auto db=10.0*std::log10(*measured.unrounded);
    if(!std::isfinite(db))
        return {{},Readiness::invalid};
    return {db,Readiness::measured};
}
[[nodiscard]] inline Import import_actual_full_source(
    const analysis::BasicLevelAnalysisResult& levels,
    const analysis::TonalStereoAnalysisResult& tonal,
    SourceIdentity owner)
{
    const auto fail=[](ImportProblem problem) {
        return Import{{},problem};
    };
    if(owner.resource_identity.empty()||owner.generation==0
       ||!owner.full_track_owner_confirmed)
        return fail(ImportProblem::owner_unconfirmed);
    if(levels.scope_begin_frame!=0||levels.scope_frame_count<=0
       ||levels.scope_begin_frame!=tonal.scope_begin_frame
       ||levels.scope_frame_count!=tonal.scope_frame_count
       ||levels.sample_rate_hz!=tonal.sample_rate_hz
       ||levels.frame_domain!=tonal.frame_domain
       ||levels.channel_count!=tonal.channel_count
       ||levels.channel_count<1||levels.channel_count>2
       ||levels.frame_domain!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE
       ||levels.sample_rate_hz<=0
       ||levels.k_weighting_bank_member_id.empty()
       ||levels.k_weighting_bank_sha256.empty())
        return fail(ImportProblem::scope_mismatch);
    SourceReadings value;
    value.owner=std::move(owner);
    value.integrated_lufs=from_m19(levels.integrated_lufs);
    value.true_peak_db_tp=from_m19(levels.true_peak_db_tp);
    value.broadband_width=from_m20(tonal.width_ratio);
    value.correlation_p10=from_m20(tonal.correlation_p10);
    value.correlation_p50=from_m20(tonal.correlation_p50);
    value.correlation_p90=from_m20(tonal.correlation_p90);
    value.profile_1_12.reserve(tonal.primary_bands_1_12.size());
    std::optional<int> last_index;
    for(const auto& band:tonal.primary_bands_1_12) {
        if(band.band_index<0
           ||(last_index&&band.band_index<=*last_index)
           ||!std::isfinite(band.center_hz)
           ||!std::isfinite(band.low_hz)
           ||!std::isfinite(band.high_hz)
           ||band.low_hz<=0.0||band.center_hz<=band.low_hz
           ||band.high_hz<=band.center_hz)
            return fail(ImportProblem::invalid_geometry);
        last_index=band.band_index;
        auto reading=power_fraction_to_db(band.normalized_fraction);
        if(!band.fully_supported
           &&reading.state==Readiness::measured)
            return fail(ImportProblem::invalid_geometry);
        value.profile_1_12.push_back({
            band.band_index,band.center_hz,band.low_hz,band.high_hz,
            reading});
    }
    return {std::move(value),ImportProblem::none};
}
struct PairedValue final {
    Value candidate;
    Value reference;
    [[nodiscard]] bool ready() const noexcept {
        return candidate.ready()&&reference.ready();
    }
};
struct PairedBand final {
    int canonical_index{};
    double center_hz{},low_hz{},high_hz{};
    PairedValue power_fraction_db;
};
enum class PairProblem:std::uint8_t {
    none,invalid_owner,incompatible_measurement_geometry
};
struct Pair final {
    PairedValue integrated_lufs;    // M05 L1 measured; target M08 separate
    PairedValue true_peak_db_tp;    // M05 L3, no M16 unqualified shortcut
    PairedValue broadband_width;   // M06 S01 measured, no policy invented
    PairedValue correlation_p10;   // M20 observations, not M06 S02 proxy
    PairedValue correlation_p50;
    PairedValue correlation_p90;
    std::vector<PairedBand> profile_1_12; // M05 T1 vector observations
    [[nodiscard]] bool full_m05m06_objective_ready()
        const noexcept {return false;} // other frozen 27 bindings absent
};
struct PairedImport final {
    std::optional<Pair> pair;
    PairProblem problem{PairProblem::invalid_owner};
};
[[nodiscard]] inline PairedImport pair_actual_sources(
    const SourceReadings& candidate,const SourceReadings& reference)
{
    const auto reject=[](PairProblem why){return PairedImport{{},why};};
    if(candidate.owner.generation==0||reference.owner.generation==0
       ||candidate.owner.resource_identity.empty()
       ||reference.owner.resource_identity.empty()
       ||!candidate.owner.full_track_owner_confirmed
       ||!reference.owner.full_track_owner_confirmed
       ||candidate.owner.resource_identity==
           reference.owner.resource_identity)
        return reject(PairProblem::invalid_owner);
    if(candidate.profile_1_12.size()!=reference.profile_1_12.size())
        return reject(PairProblem::incompatible_measurement_geometry);
    Pair result;
    result.integrated_lufs={
        candidate.integrated_lufs,reference.integrated_lufs};
    result.true_peak_db_tp={
        candidate.true_peak_db_tp,reference.true_peak_db_tp};
    result.broadband_width={
        candidate.broadband_width,reference.broadband_width};
    result.correlation_p10={
        candidate.correlation_p10,reference.correlation_p10};
    result.correlation_p50={
        candidate.correlation_p50,reference.correlation_p50};
    result.correlation_p90={
        candidate.correlation_p90,reference.correlation_p90};
    result.profile_1_12.reserve(candidate.profile_1_12.size());
    for(std::size_t i=0;i<candidate.profile_1_12.size();++i) {
        const auto& a=candidate.profile_1_12[i];
        const auto& b=reference.profile_1_12[i];
        if(a.canonical_index!=b.canonical_index
           ||a.center_hz!=b.center_hz
           ||a.low_hz!=b.low_hz||a.high_hz!=b.high_hz)
            return reject(PairProblem::incompatible_measurement_geometry);
        // M05 requires one common fixed eligible profile population.
        // Never silently intersect or recompute the denominator.
        if(a.power_fraction_db.ready()!=b.power_fraction_db.ready())
            return reject(PairProblem::incompatible_measurement_geometry);
        result.profile_1_12.push_back({
            a.canonical_index,a.center_hz,a.low_hz,a.high_hz,
            {a.power_fraction_db,b.power_fraction_db}});
    }
    return {std::move(result),PairProblem::none};
}
} // namespace rgsml::app::l3_automatch::m05m06_real
