#pragma once
// L3-M09 P04 ACCEPTED_FROZEN: exact candidate/seed/dependency de-dup.
// All three byte tuples MUST come from the existing authenticated M07/M08
// canonical serializer. This layer does NOT serialize, hash, validate
// Phase-9 artifact identities, assess DSP availability or mint UUIDs.
// A conflict poisons the WHOLE generation: no first-wins publication.
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
inline constexpr std::string_view kCandidateSchema=
    "rgsml.reference-match.candidate-description.schema/1.0.0";
inline constexpr std::string_view kSeedPolicy=
    "rgsml.reference-match.analytical-seed-policy/1.0.0";
inline constexpr std::string_view kEnumerationPolicy=
    "rgsml.reference-match.candidate-enumeration/1.0.0";
inline constexpr std::string_view kCanonicalAdapter=
    "rgsml.reference-match.candidate.canonical-json/1.0.0";

using ExactBytes=std::vector<std::uint8_t>;
struct CandidateCarrier final {
    // Complete ordered semantic topology tuple, including variant, role,
    // source-grid, cluster, selector and discrete evidence distinctions.
    ExactBytes semantic_tuple;
    // Exact seed canonical payload, NOT a rounded approximate DSP match.
    ExactBytes seed_exact_content;
    // Complete exact dependency tuple: includes concrete feature, Amount,
    // target, registry, scope, descriptor and endpoint references.
    ExactBytes dependency_tuple;
    // Full exact snapshot identity, independent of the per-seed subset.
    ExactBytes generation_snapshot;
    std::vector<std::string> ordered_provenance;
    // A candidate carrier grants no executable/persistent/verified plan.
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};
enum class InsertStatus:std::uint8_t {
    inserted,exact_duplicate,invalid_input,candidate_key_seed_collision,
    generation_already_failed
};
class ExactCandidateGeneration final {
public:
    explicit ExactCandidateGeneration(ExactBytes authenticated_snapshot)
        :snapshot_(std::move(authenticated_snapshot)),
         failed_(snapshot_.empty()) {}

    [[nodiscard]] InsertStatus add(CandidateCarrier input) {
        if(failed_)return InsertStatus::generation_already_failed;
        if(input.semantic_tuple.empty()||input.seed_exact_content.empty()
           ||input.dependency_tuple.empty()
           ||input.generation_snapshot!=snapshot_
           ||input.ordered_provenance.empty()) {
            failed_=true;
            return InsertStatus::invalid_input;
        }
        for(const auto& item:input.ordered_provenance)
            if(item.empty()) {
                failed_=true;
                return InsertStatus::invalid_input;
            }
        for(auto& stored:carriers_) {
            if(stored.semantic_tuple!=input.semantic_tuple)continue;
            // Within ONE exact generation and deterministic policy, the
            // same complete semantic key MUST resolve to exactly one seed.
            // Different seed content is an ERROR even if the dependency
            // subset also differs: never silently co-emit or invent a tie.
            if(stored.seed_exact_content!=input.seed_exact_content) {
                failed_=true;
                collision_=true;
                return InsertStatus::candidate_key_seed_collision;
            }
            if(stored.dependency_tuple==input.dependency_tuple) {
                // True exact duplicate; retain arrival-order provenance.
                for(auto& source:input.ordered_provenance)
                    stored.ordered_provenance.push_back(std::move(source));
                return InsertStatus::exact_duplicate;
            }
        }
        // Equal semantic key/seed with different dependency evidence is
        // NOT an exact duplicate. Distinct snapshot revisions belong in
        // distinct ExactCandidateGeneration objects.
        carriers_.push_back(std::move(input));
        return InsertStatus::inserted;
    }

    [[nodiscard]] bool failed() const noexcept {return failed_;}
    [[nodiscard]] bool has_seed_collision() const noexcept {
        return collision_;
    }
    // Only a valid complete generation exposes descriptions to M10.
    // This is NOT a Product publish permission or availability proof.
    [[nodiscard]] const std::vector<CandidateCarrier>* descriptions()
        const noexcept {
        return failed_ ? nullptr : &carriers_;
    }
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
private:
    ExactBytes snapshot_;
    std::vector<CandidateCarrier> carriers_;
    bool failed_{};
    bool collision_{};
};
} // namespace rgsml::app::l3_automatch::m09_frozen
