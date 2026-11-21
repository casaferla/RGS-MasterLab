#pragma once
// L3-M10 frozen full-evidence exact completed result reuse.
// NO cache of attempted failures, proxy costs, nine-cell M21 probes,
// partial FFT metrics, verifier-tolerated numeric similarity or old
// source/target/Fixed/owner generations. Cache is nonpersistent.
#include "l3_m10_frozen_coordinate_refiner.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m10_frozen {
using ExactCacheBytes=std::vector<std::uint8_t>;
struct ExactExecutionCacheKey final {
    // Full M07-M11 derivation, revision, checksum, source/reference,
    // Fixed+protected, target/Amount, phase-6 feature/policy identity.
    ExactCacheBytes immutable_generation_and_policy;
    // Full exact typed chain/parameter content and analysis/format/scope.
    ExactCacheBytes candidate_execution_content;
    // For correlation with authentic M10 measured tie identities.
    ExactCacheBytes canonical_current_tuple;
    ExactCacheBytes topology_evidence_identity;
};
enum class CacheInsert:std::uint8_t {
    inserted,already_exact,invalid_or_unattested,
    deterministic_result_collision,full_capacity,generation_poisoned
};
class ExactCompletedEvaluationCache final {
public:
    explicit ExactCompletedEvaluationCache(
        ExactCacheBytes current_immutable_snapshot)
        :snapshot_(std::move(current_immutable_snapshot)),
         poisoned_(snapshot_.empty()) {}
    [[nodiscard]] std::optional<MeasuredOrderKey> find(
        const ExactExecutionCacheKey& key) const {
        if(poisoned_||!valid_key_fields(key))return std::nullopt;
        const auto it=completed_.find(key.candidate_execution_content);
        if(it==completed_.end())return std::nullopt;
        // Exact candidate bytes identify ONE solver-stage measurement
        // under this one immutable snapshot; never borrow another.
        if(it->second.first.canonical_current_tuple!=
                key.canonical_current_tuple||
           it->second.first.topology_evidence_identity!=
                key.topology_evidence_identity)
            return std::nullopt;
        return it->second.second;
    }
    [[nodiscard]] CacheInsert insert_completed(
        const ExactExecutionCacheKey& key,
        const MeasuredOrderKey& measured,
        bool fully_attempted_render_analysis_hard_constraints_current)
    {
        if(poisoned_)return CacheInsert::generation_poisoned;
        if(!fully_attempted_render_analysis_hard_constraints_current||
           !valid_key_fields(key)||!valid_key(measured)||
           measured.canonical_current_tuple!=key.canonical_current_tuple||
           measured.topology_evidence_identity!=
                key.topology_evidence_identity)
            return CacheInsert::invalid_or_unattested;
        const auto it=completed_.find(key.candidate_execution_content);
        if(it!=completed_.end()) {
            const auto& old=it->second.second;
            if(it->second.first.canonical_current_tuple!=
                   key.canonical_current_tuple||
               it->second.first.topology_evidence_identity!=
                   key.topology_evidence_identity||
               old.total_j!=measured.total_j||
               old.target_j!=measured.target_j||
               old.processing_r!=measured.processing_r||
               old.active_owned_modules!=measured.active_owned_modules) {
                poisoned_=true;
                completed_.clear();
                return CacheInsert::deterministic_result_collision;
            }
            return CacheInsert::already_exact;
        }
        if(completed_.size()>=kMaxRenderAnalyzeAttempts)
            return CacheInsert::full_capacity;
        completed_.emplace(key.candidate_execution_content,
                           Entry{key,measured});
        return CacheInsert::inserted;
    }
    [[nodiscard]] bool poisoned() const noexcept {return poisoned_;}
    [[nodiscard]] std::size_t size() const noexcept {
        return completed_.size();
    }
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept{return false;}
private:
    struct Entry final {
        ExactExecutionCacheKey first;
        MeasuredOrderKey second;
    };
    [[nodiscard]] bool valid_key_fields(
        const ExactExecutionCacheKey& key) const noexcept
    {
        return key.immutable_generation_and_policy==snapshot_&&
            !key.candidate_execution_content.empty()&&
            !key.canonical_current_tuple.empty()&&
            !key.topology_evidence_identity.empty();
    }
    ExactCacheBytes snapshot_;
    std::map<ExactCacheBytes,Entry> completed_;
    bool poisoned_{};
};
} // namespace rgsml::app::l3_automatch::m10_frozen
