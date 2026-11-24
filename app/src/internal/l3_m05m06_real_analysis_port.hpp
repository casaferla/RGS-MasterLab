#pragma once
// L3-M05/M06 read-only REAL analysis carrier. Both tracks must be full
// owning programme measurements from existing public M19/M20 producers.
// It extracts only facts those producers ACTUALLY contain; it never
// generates the missing M05/M06 features, Phase-6 status, authoritative
// M08 optimizationTarget, or M10 J/achieved/verified labels.
#include "l3_native_membership_identity.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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
    std::int64_t sample_rate_hz{};
    std::int64_t scope_frame_count{};
    std::size_t channels{};
    std::string k_weighting_bank_member_id;
    std::string k_weighting_bank_sha256;
    Value integrated_lufs;    // L1
    Value short_term_p50_lufs; // L2, same original M19 K bank
    struct ShortTermWindow final {
        std::int64_t absolute_start_frame{};
        Value lufs;
    };
    std::vector<ShortTermWindow> short_term_3s_grid100;
    std::uint64_t short_term_complete_windows{};
    std::uint64_t short_term_valid_windows{};
    std::optional<double> short_term_native_support;
    Value true_peak_db_tp;    // L3; M16 must be QUALIFIED
    Value plr;                // D05 = true peak - Integrated (same scope)
    Value global_crest_db;    // D06, exact M19 public producer
    Value broadband_width;   // S01 raw ratio
    Value broadband_width_z; // S01 MONO_ENDPOINT_LOG2
    Value correlation_p10;   // S02 raw Grid100 Type7
    Value correlation_p50;
    Value correlation_p90;
    std::array<Value,3> correlation_fisher_z; // S02 transformed vector
    std::uint64_t valid_correlation_windows{};
    std::uint64_t complete_correlation_windows{};
    std::optional<double> correlation_native_support; // valid/complete
    struct Band final {
        int canonical_index{};
        double center_hz{},low_hz{},high_hz{};
        Value power_fraction_db; // 10log10(f), not raw linear fraction
    };
    std::vector<Band> profile_1_12; // T1 partial, no 1/3 fallback
    std::string short_term_population_identity{};
    std::string correlation_population_identity{};
    std::string full_pcm_population_identity{};
    std::string selected_spectral_population_identity{};
    std::uint64_t complete_spectral_frames{};
    std::uint64_t selected_spectral_frames{};
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
// M06 frozen MONO_ENDPOINT_LOG2: actual zero-Side endpoint maps to z=-24
// with its own identity; positive subfloor values are INVALID, not clamped.
[[nodiscard]] inline Value width_mono_endpoint_log2(const Value& raw) {
    if(!raw.ready())return raw;
    const double x=*raw.unrounded;
    if(x<0.0 ||(x>0.0&&x<std::exp2(-24.0)))
        return {{},Readiness::invalid};
    if(x==0.0)return {-24.0,Readiness::measured};
    const auto z=std::log2(x);
    return std::isfinite(z)?
        Value{z,Readiness::measured}:
        Value{{},Readiness::invalid};
}
// M06 frozen FISHER_Z with exact rho endpoints ±1 separately flagged by
// the producer. Atanh(±1) must not blow up or be replaced by zero.
[[nodiscard]] inline Value correlation_fisher_z(const Value& raw) {
    if(!raw.ready())return raw;
    const double rho=*raw.unrounded;
    if(rho< -1.0||rho>1.0)
        return {{},Readiness::invalid};
    constexpr double epsilon=1e-6;
    const double mapped=rho==1.0?1.0-epsilon:
                        rho== -1.0?-1.0+epsilon:rho;
    const double z=std::atanh(mapped);
    return std::isfinite(z)?
        Value{z,Readiness::measured}:
        Value{{},Readiness::invalid};
}
[[nodiscard]] inline Value plr_from_existing_m19(
    const Value& peak,const Value& loudness) {
    if(!peak.ready()||!loudness.ready())
        return {{},peak.state==Readiness::unqualified
            ?Readiness::unqualified:Readiness::missing};
    const double v=*peak.unrounded-*loudness.unrounded;
    return std::isfinite(v)?Value{v,Readiness::measured}:
        Value{{},Readiness::invalid};
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
    SourceIdentity owner,
    std::span<const std::uint64_t> exact_selected_m20_ordinals={})
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
    value.sample_rate_hz=levels.sample_rate_hz;
    value.scope_frame_count=levels.scope_frame_count;
    value.channels=levels.channel_count;
    NativeMembershipIdentity full{"native-full-PCM-scope/1"};
    full.append(static_cast<std::uint64_t>(levels.sample_rate_hz));
    full.append(static_cast<std::uint64_t>(levels.scope_begin_frame));
    full.append(static_cast<std::uint64_t>(levels.scope_frame_count));
    full.append(levels.channel_count);
    value.full_pcm_population_identity=full.finish();
    value.complete_spectral_frames=tonal.complete_spectral_windows;
    value.selected_spectral_frames=tonal.selected_spectral_windows;
    if(!exact_selected_m20_ordinals.empty()) {
        if(exact_selected_m20_ordinals.size()!=tonal.selected_spectral_windows||
           tonal.selected_spectral_windows>tonal.complete_spectral_windows||
           std::adjacent_find(exact_selected_m20_ordinals.begin(),
             exact_selected_m20_ordinals.end(),[](auto a,auto b){return a>=b;})!=
             exact_selected_m20_ordinals.end())
            return fail(ImportProblem::invalid_geometry);
        NativeMembershipIdentity spectral{"M20-selected-PSD-scope/1"};
        spectral.append(static_cast<std::uint64_t>(levels.sample_rate_hz));
        spectral.append(static_cast<std::uint64_t>(levels.scope_frame_count));
        spectral.append(tonal.complete_spectral_windows);
        spectral.append(tonal.spectral_all_frames_fallback?1U:0U);
        spectral.append(tonal.mean_linear_psd.size());
        for(auto ordinal:exact_selected_m20_ordinals)spectral.append(ordinal);
        value.selected_spectral_population_identity=spectral.finish();
    }
    value.k_weighting_bank_member_id=
        std::string{levels.k_weighting_bank_member_id};
    value.k_weighting_bank_sha256=
        std::string{levels.k_weighting_bank_sha256};
    value.short_term_p50_lufs=
        from_m19(levels.short_term_3s_p50_lufs);
    value.short_term_complete_windows=
        levels.short_term_3s_complete_windows;
    if(levels.short_term_3s_observation_complete) {
        if(levels.short_term_3s_grid100.size()!=
           levels.short_term_3s_complete_windows)
            return fail(ImportProblem::scope_mismatch);
        NativeMembershipIdentity membership{"M19-ST-grid100/1"};
        membership.append(static_cast<std::uint64_t>(levels.sample_rate_hz));
        membership.append(static_cast<std::uint64_t>(levels.scope_frame_count));
        value.short_term_3s_grid100.reserve(
            levels.short_term_3s_grid100.size());
        const std::int64_t quarter=levels.sample_rate_hz/10;
        for(std::size_t i=0;i<levels.short_term_3s_grid100.size();++i) {
            const auto& reading=levels.short_term_3s_grid100[i];
            if(reading.absolute_start_frame!=
               static_cast<std::int64_t>(i)*quarter
               ||reading.absolute_start_frame<0
               ||reading.absolute_start_frame+3*levels.sample_rate_hz>
                   levels.scope_frame_count
               ||(reading.lufs&&!std::isfinite(*reading.lufs)))
                return fail(ImportProblem::scope_mismatch);
            const Value observed=reading.lufs?
                Value{reading.lufs,Readiness::measured}:
                Value{{},Readiness::missing};
            membership.append(static_cast<std::uint64_t>(reading.absolute_start_frame));
            membership.append(observed.ready()?1U:0U);
            if(observed.ready())++value.short_term_valid_windows;
            value.short_term_3s_grid100.push_back({
                reading.absolute_start_frame,observed});
        }
        value.short_term_population_identity=membership.finish();
        if(value.short_term_complete_windows>0)
            value.short_term_native_support=
                static_cast<double>(value.short_term_valid_windows)/
                static_cast<double>(value.short_term_complete_windows);
        if(value.short_term_p50_lufs.ready() !=
           (value.short_term_valid_windows>0))
            return fail(ImportProblem::scope_mismatch);
    }else if(value.short_term_complete_windows>0
             ||!levels.short_term_3s_grid100.empty()
             ||value.short_term_p50_lufs.ready())
        return fail(ImportProblem::scope_mismatch);
    value.integrated_lufs=from_m19(levels.integrated_lufs);
    value.true_peak_db_tp=from_m19(levels.true_peak_db_tp);
    value.plr=plr_from_existing_m19(
        value.true_peak_db_tp,value.integrated_lufs);
    value.global_crest_db=from_m19(levels.sample_peak_crest_db);
    value.broadband_width=from_m20(tonal.width_ratio);
    value.broadband_width_z=width_mono_endpoint_log2(
        value.broadband_width);
    value.correlation_p10=from_m20(tonal.correlation_p10);
    value.correlation_p50=from_m20(tonal.correlation_p50);
    value.correlation_p90=from_m20(tonal.correlation_p90);
    value.correlation_fisher_z={
        correlation_fisher_z(value.correlation_p10),
        correlation_fisher_z(value.correlation_p50),
        correlation_fisher_z(value.correlation_p90)};
    value.complete_correlation_windows=
        static_cast<std::uint64_t>(tonal.correlation_grid100.size());
    NativeMembershipIdentity correlation_membership{"M20-correlation-grid100/1"};
    correlation_membership.append(static_cast<std::uint64_t>(levels.sample_rate_hz));
    correlation_membership.append(static_cast<std::uint64_t>(levels.scope_frame_count));
    for(const auto& window:tonal.correlation_grid100) {
        const bool ready=from_m20(window.correlation).ready();
        correlation_membership.append(window.absolute_window_start);
        correlation_membership.append(ready?1U:0U);
        if(ready)++value.valid_correlation_windows;
    }
    value.correlation_population_identity=correlation_membership.finish();
    if(value.complete_correlation_windows>0)
        value.correlation_native_support=
            static_cast<double>(value.valid_correlation_windows)/
            static_cast<double>(value.complete_correlation_windows);
    value.profile_1_12.reserve(tonal.primary_bands_1_12.size());
    std::optional<int> last_index;
    for(const auto& band:tonal.primary_bands_1_12) {
        // M20's 1/12-octave canonical indices range around 1 kHz
        // and legitimately include NEGATIVE k values (e.g. 500 Hz).
        if((last_index&&band.band_index<=*last_index)
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
    PairedValue integrated_lufs;    // M05 L1 measured; M08 target separate
    PairedValue short_term_p50_lufs; // M05 L2 measured, not reference-grade
    std::optional<double> short_term_pair_support; // min native valid/complete
    PairedValue true_peak_db_tp;    // M05 L3 qualified only
    PairedValue plr;                // M06 D05
    PairedValue global_crest_db;    // M06 D06
    PairedValue broadband_width;   // M06 S01 raw
    PairedValue broadband_width_z; // M06 S01 transformed
    PairedValue correlation_p10;   // M06 S02 raw safety
    PairedValue correlation_p50;
    PairedValue correlation_p90;
    std::array<PairedValue,3> correlation_fisher_z;
    std::optional<double> correlation_pair_support; // M06 B03
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
           reference.owner.resource_identity
       ||candidate.sample_rate_hz<=0||reference.sample_rate_hz<=0
       ||candidate.scope_frame_count<=0||reference.scope_frame_count<=0
       ||candidate.channels<1||candidate.channels>2
       ||reference.channels<1||reference.channels>2
       ||candidate.k_weighting_bank_member_id.empty()
       ||reference.k_weighting_bank_member_id.empty()
       ||candidate.k_weighting_bank_sha256.empty()
       ||reference.k_weighting_bank_sha256.empty())
        return reject(PairProblem::invalid_owner);
    if(candidate.profile_1_12.size()!=reference.profile_1_12.size())
        return reject(PairProblem::incompatible_measurement_geometry);
    Pair result;
    result.integrated_lufs={
        candidate.integrated_lufs,reference.integrated_lufs};
    result.short_term_p50_lufs={
        candidate.short_term_p50_lufs,reference.short_term_p50_lufs};
    if(candidate.short_term_native_support
       &&reference.short_term_native_support)
        result.short_term_pair_support=std::min(
            *candidate.short_term_native_support,
            *reference.short_term_native_support);
    result.true_peak_db_tp={
        candidate.true_peak_db_tp,reference.true_peak_db_tp};
    result.plr={candidate.plr,reference.plr};
    result.global_crest_db={
        candidate.global_crest_db,reference.global_crest_db};
    result.broadband_width={
        candidate.broadband_width,reference.broadband_width};
    result.broadband_width_z={
        candidate.broadband_width_z,reference.broadband_width_z};
    for(std::size_t k=0;k<3;++k)
        result.correlation_fisher_z[k]={
            candidate.correlation_fisher_z[k],
            reference.correlation_fisher_z[k]};
    if(candidate.correlation_native_support
       &&reference.correlation_native_support)
        result.correlation_pair_support=std::min(
            *candidate.correlation_native_support,
            *reference.correlation_native_support);
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
