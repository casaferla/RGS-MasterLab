#ifndef RGSML_M21_PRIVATE_METRICS_COMPARISON_CANDIDATE_HPP
#define RGSML_M21_PRIVATE_METRICS_COMPARISON_CANDIDATE_HPP
// L1-M21 PRIVATE PREPRODUCTION; NOT a production FeatureStatus, ViewModel or registry.
// Caller MUST present authenticated canonical M19/M20 feature values/status, real
// PREPARED/GOLD/PROCESSED provenance and generation tokens. No analyzer DSP here.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::m21_private {
enum class Column : std::uint8_t { prepared=0, gold=1, processed=2 };
enum class Quality : std::uint8_t {
    present, low_confidence, missing, stale, invalid, not_applicable, incompatible
};
enum class Lifecycle : std::uint8_t { current, stale, invalid, missing };
enum class Metric : std::uint8_t {
    integrated_lufs, full_scope_rms, sample_peak, true_peak, crest,
    width_ratio, correlation_p10, correlation_p50, correlation_p90
};
struct MetricSpec {
    const char* name;
    const char* absolute_unit;
    const char* delta_unit;
};
inline constexpr std::array<MetricSpec,9> kMetricSpecs{{
    {"Integrated LUFS","LUFS","LU"},
    {"RMS","dBFS","dB"},
    {"Sample Peak","dBFS","dB"},
    {"True Peak","dBTP","dB"},
    {"Crest","dB","dB"},
    {"Width Ratio","ratio","ratio"},
    {"Correlation P10","ratio","ratio"},
    {"Correlation P50","ratio","ratio"},
    {"Correlation P90","ratio","ratio"}
}};
inline constexpr std::array<const char*,3> kColumnNames{{
    "PREPARED","GOLD","PROCESSED"
}};
struct Cell {
    std::optional<double> unrounded{};
    Quality quality{Quality::missing};
    std::string reason{};
};
inline bool usable(const Cell& x) {
    return (x.quality==Quality::present ||
            x.quality==Quality::low_confidence) &&
           x.unrounded && std::isfinite(*x.unrounded);
}
struct TonalBand {
    // Producer-owned exact normalized-band identity; not inferred from frequency.
    std::uint32_t band_id{};
    // Caller-certified commonComparisonBandwidth and canonical band eligibility.
    bool in_common_support{false};
    Cell normalized_shape_db{};
};
struct SpectrumPoint {
    // Canonical producer-owned static spectrum, passed through WITHOUT analysis.
    double hz{};
    Cell power_density{};
};
struct Snapshot {
    Column column{Column::prepared};
    std::uint64_t generation_token{};
    std::string resource_identity{};
    std::string analysis_scope_label{}; // e.g. "Full Track" or "Region"
    std::uint64_t absolute_scope_begin{};
    std::uint64_t scope_frames{};
    std::uint32_t sample_rate_hz{};
    Lifecycle lifecycle{Lifecycle::missing};
    std::array<Cell,9> metrics{};
    std::uint8_t profile_denominator{}; // exactly 12 or substitute 3; 0 if absent
    // Pair-scoped proof is supplied by a future authoritative producer, not UI.
    std::string common_comparison_support_id{};
    std::vector<TonalBand> tonal_profile{};
    std::vector<SpectrumPoint> average_spectrum{};
};
inline Cell state_cell(Lifecycle lifecycle) {
    if(lifecycle==Lifecycle::stale) return {{},Quality::stale,"STALE artifact"};
    if(lifecycle==Lifecycle::invalid) return {{},Quality::invalid,"INVALID artifact"};
    return {{},Quality::missing,"No current authenticated artifact"};
}
inline Cell delta(const Snapshot* current, const Snapshot* gold,
                  std::size_t metric) {
    if(!gold) return state_cell(Lifecycle::missing);
    if(!current) return state_cell(Lifecycle::missing);
    if(gold->lifecycle!=Lifecycle::current) return state_cell(gold->lifecycle);
    if(current->lifecycle!=Lifecycle::current) return state_cell(current->lifecycle);
    if(metric>=kMetricSpecs.size()) return {{},Quality::invalid,"Unknown metric"};
    const Cell& a=current->metrics[metric];
    const Cell& g=gold->metrics[metric];
    for(const auto* c:{&g,&a}) {
        if(c->quality!=Quality::present && c->quality!=Quality::low_confidence)
            return {{},c->quality,c->reason};
        if(!c->unrounded || !std::isfinite(*c->unrounded))
            return {{},Quality::invalid,"Invalid upstream metric"};
    }
    const double result=*g.unrounded-*a.unrounded; // never use rounded display
    if(!std::isfinite(result))
        return {{},Quality::invalid,"Non-finite difference"};
    const bool qualified=a.quality==Quality::low_confidence ||
                         g.quality==Quality::low_confidence;
    return {result,qualified?Quality::low_confidence:Quality::present,
            qualified?"LOW_CONFIDENCE upstream support":""};
}
inline Cell tonal_delta(const Snapshot* current, const Snapshot* gold,
                        std::uint32_t band_id) {
    if(!current || !gold) return state_cell(Lifecycle::missing);
    if(current->lifecycle!=Lifecycle::current) return state_cell(current->lifecycle);
    if(gold->lifecycle!=Lifecycle::current) return state_cell(gold->lifecycle);
    if(current->common_comparison_support_id.empty() ||
       current->common_comparison_support_id!=gold->common_comparison_support_id)
        return {{},Quality::incompatible,"No shared canonical common-band proof"};
    if(current->profile_denominator!=gold->profile_denominator ||
       (current->profile_denominator!=12 && current->profile_denominator!=3))
        return {{},Quality::incompatible,"Incompatible tonal resolution"};
    const TonalBand* a=nullptr;
    const TonalBand* g=nullptr;
    for(const auto& b:current->tonal_profile)
        if(b.band_id==band_id) {if(a) return {{},Quality::invalid,"Duplicate band"};a=&b;}
    for(const auto& b:gold->tonal_profile)
        if(b.band_id==band_id) {if(g) return {{},Quality::invalid,"Duplicate band"};g=&b;}
    if(!a || !g) return {{},Quality::missing,"Band missing"};
    if(!a->in_common_support || !g->in_common_support)
        return {{},Quality::incompatible,"Outside validated common bandwidth"};
    for(const auto* b:{g,a}) {
        if(b->normalized_shape_db.quality!=Quality::present &&
           b->normalized_shape_db.quality!=Quality::low_confidence)
            return {{},b->normalized_shape_db.quality,b->normalized_shape_db.reason};
        if(!b->normalized_shape_db.unrounded ||
           !std::isfinite(*b->normalized_shape_db.unrounded))
            return {{},Quality::invalid,"Invalid upstream normalized shape"};
    }
    const double d=*g->normalized_shape_db.unrounded -
                   *a->normalized_shape_db.unrounded;
    if(!std::isfinite(d))return {{},Quality::invalid,"Non-finite tonal difference"};
    const bool low=a->normalized_shape_db.quality==Quality::low_confidence ||
                   g->normalized_shape_db.quality==Quality::low_confidence;
    return {d,low?Quality::low_confidence:Quality::present,
            low?"LOW_CONFIDENCE band support":""};
}
struct Comparison {
    std::array<std::optional<Snapshot>,3> columns{};
    std::array<Cell,9> prepared_to_gold{};
    std::array<Cell,9> processed_to_gold{};
    // No inferred timeline/sample alignment across independent Gold scopes.
    bool independent_scopes{true};
};
inline Comparison compose(const std::array<std::optional<Snapshot>,3>& snapshots) {
    Comparison c{};
    c.columns=snapshots;
    // Three COLUMN IDENTITIES are not interchangeable. Caller-indexed slots
    // must not relabel a RAW/Gold/Processed producer. Fail closed even when
    // private tests supply valid-looking numbers in the wrong semantic slot.
    for(std::size_t i=0;i<c.columns.size();++i)
        if(c.columns[i] &&
           static_cast<std::size_t>(c.columns[i]->column)!=i)
            c.columns[i]->lifecycle=Lifecycle::invalid;
    const Snapshot* p=c.columns[0] ? &*c.columns[0]:nullptr;
    const Snapshot* g=c.columns[1] ? &*c.columns[1]:nullptr;
    const Snapshot* x=c.columns[2] ? &*c.columns[2]:nullptr;
    for(std::size_t i=0;i<9;++i) {
        c.prepared_to_gold[i]=delta(p,g,i);
        c.processed_to_gold[i]=delta(x,g,i);
    }
    return c;
}
// Independent producer generation tokens, NEVER fabricated from UI selection.
// Advancing only the changed column preserves other columns and marks older
// published evidence STALE; no stale async result can be installed as current.
// Invariant-only checks for private preproduction. Production status/epoch
// ownership remains a separate Phase-6 canonical contract.
inline bool supported_private_rate(std::uint32_t fs) {
    return fs==44100U || fs==48000U || fs==88200U ||
           fs==96000U || fs==176400U || fs==192000U;
}
inline bool valid_cell_storage(const Cell& c) {
    const bool numeric=c.quality==Quality::present ||
                       c.quality==Quality::low_confidence;
    return numeric ?
      c.unrounded && std::isfinite(*c.unrounded) :
      !c.unrounded;
}
// Structural evidence only, NEVER a Phase-6 producer or FeatureStatus.
// A static PSD must have increasing physical frequencies within Nyquist.
// Missing-valued bins may remain absent; they cannot carry phantom numbers.
inline bool valid_private_spectrum(const Snapshot& s) {
    if(s.average_spectrum.size()>1048576U)return false;
    const double nyquist=static_cast<double>(s.sample_rate_hz)*0.5;
    double previous=-1.;
    for(const auto& point:s.average_spectrum) {
        if(!std::isfinite(point.hz)||point.hz<0.||
           point.hz<=previous||point.hz>nyquist||
           !valid_cell_storage(point.power_density)||
           (point.power_density.unrounded &&
            *point.power_density.unrounded<0.))
            return false;
        previous=point.hz;
    }
    return true;
}
class PrivatePublicationGuard {
public:
    bool expect(Column column,std::uint64_t monotonic_token) {
        const auto i=static_cast<std::size_t>(column);
        if(i>=3 || monotonic_token==0 || monotonic_token<=expected_[i])
            return false;
        expected_[i]=monotonic_token;
        if(slots_[i]) slots_[i]->lifecycle=Lifecycle::stale;
        return true;
    }
    bool publish(const Snapshot& incoming) {
        // One immutable authoritative result per producer generation. A late
        // second completion in the SAME epoch cannot overwrite first-current.
        // Re-analyze must first mint a fresh producer generation via expect().
        const auto i=static_cast<std::size_t>(incoming.column);
        if(i>=3 || expected_[i]==0 || incoming.generation_token!=expected_[i] ||
           incoming.lifecycle!=Lifecycle::current ||
           incoming.resource_identity.empty() || incoming.analysis_scope_label.empty() ||
           incoming.scope_frames==0 || incoming.sample_rate_hz==0 ||
           (incoming.analysis_scope_label!="Full Track" &&
            incoming.analysis_scope_label!="Region") ||
           incoming.absolute_scope_begin>
               (std::numeric_limits<std::uint64_t>::max)()-incoming.scope_frames)
            return false;
        if(slots_[i] && slots_[i]->lifecycle==Lifecycle::current &&
           slots_[i]->generation_token==incoming.generation_token)
            return false;
        if(!supported_private_rate(incoming.sample_rate_hz))
            return false;
        for(const auto& c:incoming.metrics)
            if(!valid_cell_storage(c)) return false;
        for(const auto& b:incoming.tonal_profile)
            if(!valid_cell_storage(b.normalized_shape_db) ||
               (!b.in_common_support && b.normalized_shape_db.unrounded))
                return false;
        for(const auto& p:incoming.average_spectrum)
            if(!std::isfinite(p.hz) || p.hz<0 ||
               !valid_cell_storage(p.power_density) ||
               (p.power_density.unrounded && *p.power_density.unrounded<0))
                return false;
        if(incoming.profile_denominator!=0 &&
           incoming.profile_denominator!=3 && incoming.profile_denominator!=12) return false;
        if(!incoming.tonal_profile.empty() &&
           (incoming.profile_denominator==0 ||
            incoming.common_comparison_support_id.empty())) return false;
        slots_[i]=incoming;
        return true;
    }
    Comparison view()const{return compose(slots_);}
private:
    std::array<std::uint64_t,3> expected_{};
    std::array<std::optional<Snapshot>,3> slots_{};
};
} // namespace rgsml::app::m21_private
#endif
