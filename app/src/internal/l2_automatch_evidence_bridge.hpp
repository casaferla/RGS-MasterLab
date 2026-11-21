#pragma once
// L2 minimal development bridge toward L3 AutoMatch.
// Consumes ONLY a live, owner-rechecked M21ProductPublicationInbox::view_current()
// on the Qt Product owner. This value function is NOT a second producer,
// authority, optimizer, audio renderer, qualification gate, or GUI surface.
// Never infer a new M20 tonal shape, force a common sample rate, or decide
// a metric's weighting. L3 will own candidate scoring and evaluation.
#include "m21_metrics_comparison_candidate.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l2_minimal {

using m21_private::Column;
using m21_private::Comparison;
using m21_private::Lifecycle;
using m21_private::Quality;
using m21_private::Snapshot;

enum class PairProblem : std::uint8_t {
    none, invalid_column, missing_observation, stale_owner,
    invalid_identity, unsupported_scope, no_usable_metrics
};

struct MetricPair final {
    double candidate{};
    double reference{};
    double reference_minus_candidate{}; // full unrounded Gold - Current
};

struct AutoMatchEvidencePair final {
    Column candidate_column{Column::processed};
    std::uint64_t candidate_generation{};
    std::uint64_t reference_generation{};
    std::string candidate_identity;
    std::string reference_identity;
    std::uint32_t candidate_sample_rate_hz{};
    std::uint32_t reference_sample_rate_hz{};
    // An absent cell is NOT a zero target or a directive to minimize it.
    // Only M21 Quality::present is admitted; low-confidence TP and missing
    // mono stereo metrics remain non-objectives until L3 chooses a policy.
    std::array<std::optional<MetricPair>,9> usable_metrics{};
    std::size_t usable_count{};
};

struct PairResult final {
    std::optional<AutoMatchEvidencePair> evidence;
    PairProblem problem{PairProblem::missing_observation};
};

[[nodiscard]] inline bool valid_full_track_owner(const Snapshot& s) {
    return s.lifecycle==Lifecycle::current
        &&s.generation_token!=0
        &&!s.resource_identity.empty()
        &&s.analysis_scope_label=="Full Track"
        &&s.absolute_scope_begin==0
        &&s.scope_frames!=0
        &&m21_private::supported_private_rate(s.sample_rate_hz);
}

// The caller must pass the live M21 inbox 'view_current()' result, never a
// source-constructed Snapshot. L2 retains only scalar value copies and
// provenance: no PCM, PSD/FFT buffers, Qt pointers, or audio device leases.
[[nodiscard]] inline PairResult make_evidence_pair(
    const Comparison& live,Column candidate)
{
    auto reject=[](PairProblem p){return PairResult{{},p};};
    if(candidate!=Column::prepared &&candidate!=Column::processed)
        return reject(PairProblem::invalid_column);
    const auto ci=static_cast<std::size_t>(candidate);
    const auto& c=live.columns[ci];
    const auto& g=live.columns[static_cast<std::size_t>(Column::gold)];
    if(!c ||!g)return reject(PairProblem::missing_observation);
    if(c->column!=candidate ||g->column!=Column::gold
       ||!live.independent_scopes)
        return reject(PairProblem::invalid_identity);
    if(c->lifecycle!=Lifecycle::current ||g->lifecycle!=Lifecycle::current)
        return reject(PairProblem::stale_owner);
    if(c->generation_token==0 ||g->generation_token==0
       ||c->resource_identity.empty() ||g->resource_identity.empty())
        return reject(PairProblem::invalid_identity);
    if(!valid_full_track_owner(*c) ||!valid_full_track_owner(*g))
        return reject(PairProblem::unsupported_scope);

    AutoMatchEvidencePair out;
    out.candidate_column=candidate;
    out.candidate_generation=c->generation_token;
    out.reference_generation=g->generation_token;
    out.candidate_identity=c->resource_identity;
    out.reference_identity=g->resource_identity;
    out.candidate_sample_rate_hz=c->sample_rate_hz;
    out.reference_sample_rate_hz=g->sample_rate_hz;
    for(std::size_t i=0;i<out.usable_metrics.size();++i) {
        // Preserve the previously developed M21 oracle, not a second
        // delta or an AutoMatch-specific loudness/peak calculation.
        const auto d=m21_private::delta(&*c,&*g,i);
        if(d.quality!=Quality::present ||!d.unrounded)continue;
        const auto& cv=c->metrics[i].unrounded;
        const auto& rv=g->metrics[i].unrounded;
        if(!cv ||!rv ||!std::isfinite(*cv)||!std::isfinite(*rv)
           ||!std::isfinite(*d.unrounded))
            continue;
        out.usable_metrics[i]=MetricPair{*cv,*rv,*d.unrounded};
        ++out.usable_count;
    }
    if(out.usable_count==0)return reject(PairProblem::no_usable_metrics);
    return PairResult{std::move(out),PairProblem::none};
}

// A subsequent candidate render or replacement REFERENCE must invalidate
// the previous typed evidence before L3 uses it for any parameter decision.
// This only checks provenance: Product remains the owner of live authority.
[[nodiscard]] inline bool matches_current(
    const AutoMatchEvidencePair& saved,const Comparison& live)
{
    const auto ci=static_cast<std::size_t>(saved.candidate_column);
    if(ci>=live.columns.size() ||saved.candidate_column==Column::gold)
        return false;
    const auto& current=live.columns[ci];
    const auto& reference=live.columns[static_cast<std::size_t>(Column::gold)];
    return current &&reference &&live.independent_scopes
        &&valid_full_track_owner(*current)
        &&valid_full_track_owner(*reference)
        &&current->column==saved.candidate_column
        &&reference->column==Column::gold
        &&current->generation_token==saved.candidate_generation
        &&reference->generation_token==saved.reference_generation
        &&current->resource_identity==saved.candidate_identity
        &&reference->resource_identity==saved.reference_identity
        &&current->sample_rate_hz==saved.candidate_sample_rate_hz
        &&reference->sample_rate_hz==saved.reference_sample_rate_hz;
}

} // namespace rgsml::app::l2_minimal
