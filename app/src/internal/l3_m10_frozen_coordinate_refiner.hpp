#pragma once
// L3-M10 ACCEPTED_FROZEN deterministic numerical coordinate-descent
// skeleton. This is a pure, policy-conserving scheduler, NOT the M05/M06
// 27-feature objective or M09 topology/seed generator, and cannot publish
// plans or audio. Semantic node + parameter order and canonical score
// bytes must come from the authenticated frozen M07-M09 adapters.
// Compare CURRENT, MINUS, PLUS only after both directions finish.
// No adaptive steps, random restarts, rebaselining or hidden sweeps.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
constexpr std::size_t kMaxActiveCoordinates=48;
constexpr std::uint32_t kMaxRenderAnalyzeAttempts=800;
constexpr std::array<double,4> kSteps{
    0.25,0.125,0.0625,0.03125};

struct Coordinate final {
    // These are already-canonical unsigned bytes, not QML labels, order
    // indices, floating band frequencies or newly minted persistence IDs.
    std::vector<std::uint8_t> semantic_node_id;
    std::vector<std::uint8_t> stable_parameter_id;
    double normalized_u{};
};
struct MeasuredOrderKey final {
    double total_j{};
    double target_j{};
    double processing_r{};
    std::uint32_t active_owned_modules{};
    std::vector<std::uint8_t> canonical_current_tuple;
    std::vector<std::uint8_t> topology_evidence_identity;
};
[[nodiscard]] inline std::optional<std::int64_t> quantize_q1e9(
    double value) noexcept
{
    // Positive cost quantities, Q=1e-9 with ties-to-even. Fail closed on
    // nonfinite, negative and values whose integer quanta cannot be
    // represented exactly. The raw unrounded value stays in the key.
    if(!std::isfinite(value)||value<0.0)return std::nullopt;
    const double units=value*1e9;
    constexpr double max_exact=4503599627370495.0; // 2^52-1
    if(!std::isfinite(units)||units>max_exact)
        return std::nullopt;
    const double floor_value=std::floor(units);
    const double fraction=units-floor_value;
    double rounded=floor_value;
    if(fraction>0.5 ||
       (fraction==0.5 &&std::fmod(floor_value,2.0)!=0.0))
        rounded+=1.0;
    return static_cast<std::int64_t>(rounded);
}
[[nodiscard]] inline bool valid_key(const MeasuredOrderKey& key) noexcept {
    return quantize_q1e9(key.total_j).has_value()
        &&quantize_q1e9(key.target_j).has_value()
        &&quantize_q1e9(key.processing_r).has_value()
        &&key.active_owned_modules<=8
        &&!key.topology_evidence_identity.empty();
}
[[nodiscard]] inline int compare_measured(
    const MeasuredOrderKey& x,const MeasuredOrderKey& y)
{
    // Only call on valid keys. Retain the full P05 measured tie chain;
    // neither proxy cost nor active-band count is included.
    const auto compare=[](auto a,auto b) {
        return a<b?-1:a>b?1:0;
    };
    for(const std::pair<double,double> pair:{
            std::pair{x.total_j,y.total_j},
            std::pair{x.target_j,y.target_j},
            std::pair{x.processing_r,y.processing_r}}) {
        const auto a=*quantize_q1e9(pair.first);
        const auto b=*quantize_q1e9(pair.second);
        if(const int c=compare(a,b);c!=0)return c;
    }
    if(const int c=compare(x.active_owned_modules,
                           y.active_owned_modules);c!=0)return c;
    const auto bytes_compare=[](const auto& a,const auto& b) {
        if(std::lexicographical_compare(
            a.begin(),a.end(),b.begin(),b.end()))return -1;
        if(std::lexicographical_compare(
            b.begin(),b.end(),a.begin(),a.end()))return 1;
        return 0;
    };
    if(const int c=bytes_compare(x.canonical_current_tuple,
                                  y.canonical_current_tuple);c!=0)
        return c;
    return bytes_compare(x.topology_evidence_identity,
                         y.topology_evidence_identity);
}
enum class EvaluationKind :std::uint8_t {
    preflight_rejected, // zero attempted DSP+analyzer work
    completed_cache_hit, // exact authentic complete evidence only
    attempted_failure, // render, analyzer, hard-constraint failure
    attempted_complete // full authoritative render+analysis+cost
};
struct Evaluation final {
    EvaluationKind kind{EvaluationKind::preflight_rejected};
    std::optional<MeasuredOrderKey> measured;
};
enum class End :std::uint8_t {
    complete, budget_exhausted, cancelled
};
struct Outcome final {
    End end{End::complete};
    std::vector<Coordinate> coordinates;
    MeasuredOrderKey winner;
    std::uint32_t attempted_total{};
    std::uint32_t evaluated_complete{};
    std::uint32_t attempted_failures{};
    std::uint32_t completed_cache_hits{};
    std::uint32_t preflight_rejections{};
    std::uint32_t discarded_out_of_bounds{};
    // This numerical stage does not materialize/verify a Product master.
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
};
class CoordinateRefiner final {
public:
    // Called for each in-bounds direction (minus first) with a private,
    // complete normalized state. The adapter owns M09 inverse transforms,
    // coupled predicates, exact execution cache and M05/M06 measurement.
    // When allow_new_attempt=false, it may only return preflight_rejected
    // or completed_cache_hit. No 801st render may be launched.
    template<typename Evaluate>
    [[nodiscard]] static std::optional<Outcome> run(
        std::vector<Coordinate> coords,MeasuredOrderKey seed,
        std::uint32_t attempted_before,Evaluate&& evaluate,
        const std::atomic<bool>* cancellation=nullptr)
    {
        if(coords.size()>kMaxActiveCoordinates||!valid_key(seed)
           ||attempted_before>kMaxRenderAnalyzeAttempts)
            return std::nullopt;
        const auto key_less=[](const Coordinate& a,const Coordinate& b) {
            if(a.semantic_node_id!=b.semantic_node_id)
                return std::lexicographical_compare(
                    a.semantic_node_id.begin(),a.semantic_node_id.end(),
                    b.semantic_node_id.begin(),b.semantic_node_id.end());
            return std::lexicographical_compare(
                a.stable_parameter_id.begin(),a.stable_parameter_id.end(),
                b.stable_parameter_id.begin(),b.stable_parameter_id.end());
        };
        for(const auto& c:coords)
            if(c.semantic_node_id.empty()||c.stable_parameter_id.empty()
               ||!std::isfinite(c.normalized_u)
               ||c.normalized_u<0.0||c.normalized_u>1.0)
                return std::nullopt;
        std::sort(coords.begin(),coords.end(),key_less);
        for(std::size_t i=1;i<coords.size();++i)
            if(!key_less(coords[i-1],coords[i]))
                return std::nullopt;
        Outcome result;
        result.coordinates=std::move(coords);
        result.winner=std::move(seed);
        result.attempted_total=attempted_before;
        for(const double step:kSteps) {
            for(std::size_t i=0;i<result.coordinates.size();++i) {
                if(cancellation&&cancellation->load(
                      std::memory_order_acquire)) {
                    result.end=End::cancelled;
                    return result;
                }
                const double center=result.coordinates[i].normalized_u;
                std::optional<MeasuredOrderKey> best_change;
                double best_u=center;
                // Same baseline CURRENT for both direction proposals.
                for(const double sign:{-1.0,1.0}) {
                    if(cancellation&&cancellation->load(
                          std::memory_order_acquire)) {
                        result.end=End::cancelled;
                        return result;
                    }
                    const double moved=center+sign*step;
                    if(moved<0.0||moved>1.0) {
                        ++result.discarded_out_of_bounds;
                        continue; // not clamped, not an attempted render
                    }
                    std::vector<double> all;
                    all.reserve(result.coordinates.size());
                    for(std::size_t j=0;j<result.coordinates.size();++j)
                        all.push_back(j==i?moved:
                            result.coordinates[j].normalized_u);
                    const bool allow_new=
                        result.attempted_total<kMaxRenderAnalyzeAttempts;
                    const auto event=evaluate(all,allow_new);
                    switch(event.kind) {
                    case EvaluationKind::preflight_rejected:
                        if(event.measured)return std::nullopt;
                        ++result.preflight_rejections;
                        continue;
                    case EvaluationKind::completed_cache_hit:
                        if(!event.measured||!valid_key(*event.measured))
                            return std::nullopt;
                        ++result.completed_cache_hits;
                        break;
                    case EvaluationKind::attempted_failure:
                        if(event.measured||!allow_new)return std::nullopt;
                        ++result.attempted_total;
                        ++result.attempted_failures;
                        continue;
                    case EvaluationKind::attempted_complete:
                        if(!event.measured||!allow_new
                           ||!valid_key(*event.measured))
                            return std::nullopt;
                        ++result.attempted_total;
                        break;
                    default:
                        return std::nullopt;
                    }
                    ++result.evaluated_complete;
                    // Deterministic tie policy: CURRENT is first,
                    // then MINUS, then PLUS, but P05 canonical byte
                    // tuple may choose a truly better equal-J candidate.
                    const auto& prior=best_change?
                        *best_change:result.winner;
                    if(compare_measured(*event.measured,prior)<0) {
                        best_change=*event.measured;
                        best_u=moved;
                    }
                }
                if(best_change) {
                    result.winner=std::move(*best_change);
                    result.coordinates[i].normalized_u=best_u;
                }
            }
        }
        return result;
    }
};
} // namespace rgsml::app::l3_automatch::m10_frozen
