#pragma once
// L3 AutoMatch: first deterministic offline evaluation controller.
// It DOES NOT render audio or mutate a Product DSP chain. Qt Product must
// apply a typed, validated parameter binding, render via its EXISTING chain,
// publish authentic M19-M21 measurements and pass a fresh owner-checked M21
// comparison to complete(). No user-facing score or unqualified TP is made.
#include "l2_automatch_evidence_bridge.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
namespace l2=l2_minimal;
namespace m21=m21_private;

struct Objective final {
    m21::Metric metric{};
    double tolerance{}; // unit-correct, EXPLICIT caller policy; never guessed
    double weight{}; // positive relative importance, EXPLICIT caller policy
};
struct Proposal final {
    // Caller must assign this to an exact, validated full DSP binding set.
    // It is an opaque identity, NOT a parameter value or transport command.
    std::string typed_dsp_fingerprint;
};
struct Ticket final {
    std::uint64_t run_id{};
    std::size_t ordinal{};
    std::string typed_dsp_fingerprint;
};
enum class Submission : std::uint8_t {
    accepted, improved, no_pending_ticket, mismatched_ticket,
    stale_or_invalid_product, missing_objective, nonfinite_score, cancelled
};
struct Best final {
    std::optional<std::string> typed_dsp_fingerprint; // absent == starting DSP
    double weighted_error{};
    std::uint64_t processed_generation{};
};

class OfflineSearch final {
public:
    // Proposed candidates are prevalidated Product DSP configurations;
    // engine never invents allowed EQ bands, ceiling or compressor settings.
    [[nodiscard]] static std::optional<OfflineSearch> begin(
        std::uint64_t run_id,const m21::Comparison& live,
        std::vector<Proposal> candidates,std::vector<Objective> objectives)
    {
        if(run_id==0 ||candidates.empty() ||candidates.size()>64
           ||objectives.empty() ||objectives.size()>9)
            return std::nullopt;
        const auto initial=l2::make_evidence_pair(live,m21::Column::processed);
        if(!initial.evidence)return std::nullopt;
        std::array<bool,9> selected{};
        for(const auto& objective:objectives) {
            const auto k=static_cast<std::size_t>(objective.metric);
            if(k>=selected.size() ||selected[k]
               ||!std::isfinite(objective.tolerance)
               ||!std::isfinite(objective.weight)
               ||objective.tolerance<=0.0 ||objective.weight<=0.0
               ||!initial.evidence->usable_metrics[k])
                return std::nullopt;
            selected[k]=true;
        }
        for(std::size_t i=0;i<candidates.size();++i) {
            if(candidates[i].typed_dsp_fingerprint.empty())return std::nullopt;
            for(std::size_t j=0;j<i;++j)
                if(candidates[j].typed_dsp_fingerprint==
                   candidates[i].typed_dsp_fingerprint)
                    return std::nullopt;
        }
        const auto first=calculate(*initial.evidence,objectives);
        if(!first)return std::nullopt;
        OfflineSearch out;
        out.run_id_=run_id;
        out.reference_id_=initial.evidence->reference_identity;
        out.reference_generation_=initial.evidence->reference_generation;
        out.reference_rate_=initial.evidence->reference_sample_rate_hz;
        out.processed_rate_=initial.evidence->candidate_sample_rate_hz;
        out.last_processed_generation_=initial.evidence->candidate_generation;
        out.best_=Best{std::nullopt,*first,
            initial.evidence->candidate_generation};
        out.candidates_=std::move(candidates);
        out.objectives_=std::move(objectives);
        return out;
    }

    [[nodiscard]] std::optional<Ticket> next() {
        if(cancelled_ ||pending_ ||index_>=candidates_.size())
            return std::nullopt;
        pending_=Ticket{run_id_,index_,
            candidates_[index_].typed_dsp_fingerprint};
        return pending_;
    }
    // Always call with a new M21ProductPublicationInbox::view_current()
    // after Product completes its full real offline DSP render/analysis.
    [[nodiscard]] Submission complete(
        const Ticket& ticket,const m21::Comparison& live)
    {
        if(cancelled_)return Submission::cancelled;
        if(!pending_)return Submission::no_pending_ticket;
        if(ticket.run_id!=pending_->run_id
           ||ticket.ordinal!=pending_->ordinal
           ||ticket.typed_dsp_fingerprint!=pending_->typed_dsp_fingerprint)
            return Submission::mismatched_ticket;
        const auto measured=l2::make_evidence_pair(
            live,m21::Column::processed);
        if(!measured.evidence
           ||measured.evidence->reference_identity!=reference_id_
           ||measured.evidence->reference_generation!=reference_generation_
           ||measured.evidence->reference_sample_rate_hz!=reference_rate_
           ||measured.evidence->candidate_sample_rate_hz!=processed_rate_
           ||measured.evidence->candidate_generation
                   <=last_processed_generation_) {
            cancel();
            return Submission::stale_or_invalid_product;
        }
        const auto score=calculate(*measured.evidence,objectives_);
        if(!score) {
            cancel();
            return Submission::missing_objective;
        }
        if(!std::isfinite(*score)) {
            cancel();
            return Submission::nonfinite_score;
        }
        last_processed_generation_=measured.evidence->candidate_generation;
        ++index_;
        pending_.reset();
        if(*score<best_.weighted_error) {
            best_=Best{ticket.typed_dsp_fingerprint,*score,
                last_processed_generation_};
            return Submission::improved;
        }
        return Submission::accepted;
    }
    void cancel() noexcept {cancelled_=true;pending_.reset();}
    [[nodiscard]] bool cancelled() const noexcept {return cancelled_;}
    [[nodiscard]] bool complete() const noexcept {
        return !cancelled_ &&index_==candidates_.size() &&!pending_;
    }
    [[nodiscard]] const Best& best() const noexcept{return best_;}
    [[nodiscard]] std::size_t attempts() const noexcept{return index_;}
private:
    [[nodiscard]] static std::optional<double> calculate(
        const l2::AutoMatchEvidencePair& value,
        const std::vector<Objective>& objectives)
    {
        double weighted=0.0,weights=0.0;
        for(const auto& objective:objectives) {
            const auto index=static_cast<std::size_t>(objective.metric);
            if(index>=value.usable_metrics.size()
               ||!value.usable_metrics[index])
                return std::nullopt;
            const double d=value.usable_metrics[index]
                ->reference_minus_candidate/objective.tolerance;
            const double part=objective.weight*d*d;
            if(!std::isfinite(part))return std::nullopt;
            weighted+=part;
            weights+=objective.weight;
        }
        if(!std::isfinite(weighted)||!std::isfinite(weights)||weights<=0)
            return std::nullopt;
        const double score=weighted/weights;
        if(!std::isfinite(score))return std::nullopt;
        return score;
    }
    std::uint64_t run_id_{};
    std::uint64_t reference_generation_{};
    std::uint64_t last_processed_generation_{};
    std::uint32_t reference_rate_{};
    std::uint32_t processed_rate_{};
    std::string reference_id_;
    std::vector<Proposal> candidates_;
    std::vector<Objective> objectives_;
    std::size_t index_{};
    std::optional<Ticket> pending_;
    bool cancelled_{};
    Best best_{};
};
} // namespace rgsml::app::l3_automatch
