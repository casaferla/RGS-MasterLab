#pragma once
// Owner-thread action; only immutable input and cancellation cross threads.
// Search completion never applies a graph. Adoption is a separate command.
#include "l3_native_qt_product_session.hpp"
#include "l3_m12_review_diff.hpp"
#include <chrono>
#include <future>

namespace rgsml::app::l3_automatch {
class NativeQtProductAction final {
public:
    using Controllers=ProductQtP2NativeAdmission::Controllers;
    using Fanout=ProductQtP2NativeAdmission::Fanout;
    using Completion=m13_frozen::M13ActualNativeWorkerCompletion;
    enum class Phase {idle,searching,cancelling,candidate_ready,no_correction,no_correction_prior_retained,no_correction_cleared,zero_amount_prior_retained,match_suspended,kept_current,
        cancelled,stale,failed,materialized,verifying,numerical_pass,verification_incomplete};
    NativeQtProductAction(Controllers product,const Fanout& fanout,
        const AuditionSourceSelector& selector,const GoldSelectionViewModel& gold)
        :product_(product),fanout_(fanout),selector_(selector),gold_(gold){}
    NativeQtProductAction(const NativeQtProductAction&)=delete;
    NativeQtProductAction& operator=(const NativeQtProductAction&)=delete;
    ~NativeQtProductAction() {
        // std::async futures join on destruction. Only shutdown may wait.
        // No worker owns this action or any Qt object.
        if(cancel_)cancel_->store(true,std::memory_order_release);
    }
    [[nodiscard]] Phase phase()const noexcept{return phase_;}
    [[nodiscard]] const std::string& error()const noexcept{return error_;}
    [[nodiscard]] bool replacing()const noexcept {
        return operation_&&operation_->expected.exact_existing_linked_snapshot.has_value();
    }
    [[nodiscard]] bool can_adopt()const noexcept {
        return phase_==Phase::candidate_ready||phase_==Phase::no_correction_prior_retained||
            phase_==Phase::zero_amount_prior_retained;
    }
    [[nodiscard]] bool busy()const noexcept {
        return phase_==Phase::searching||phase_==Phase::cancelling||phase_==Phase::verifying;
    }
    [[nodiscard]] const NativeQtSearchWorkerResult* result()const noexcept {
        return completed_?&*completed_:nullptr;
    }
    [[nodiscard]] const m12_frozen::AcceptedPlanAndOwner* reviewed()const noexcept {
        return reviewed_?&*reviewed_:nullptr;
    }
    [[nodiscard]] const m12_frozen::M12ReviewDiff* review_diff()const noexcept {
        return review_diff_?&*review_diff_:nullptr;
    }
    struct ReviewMetrics final {
        double exact_m16_linear_peak{};
        double independent_ceiling_db_tp{};
        std::uint32_t attempted{};
        std::int64_t original_ms{},fixed_ms{},seed_ms{},search_ms{};
        std::optional<std::int64_t> first_admissible_ms,last_attempt_ms;
        std::uint32_t completed{},failed{},cache_hits{};
    };
    [[nodiscard]] std::optional<ReviewMetrics> review_metrics()const noexcept {
        if(!operation_||!completed_||!can_adopt())return {};
        const auto& worker=completed_->result;
        std::optional<double> peak;
        std::uint32_t attempts=0;
        if(phase_==Phase::candidate_ready) {
            const auto& r=worker.search.refinement;
            if(!r||!r->winning_refinement_index||
               *r->winning_refinement_index>=r->candidate_refinements.size())return {};
            const auto& winner=r->candidate_refinements[*r->winning_refinement_index].numerical.winner;
            if(!winner.native_prediction)return {};
            peak=winner.native_prediction->independent_full_m16_linear_peak;
            attempts=r->total_attempted;
        }else if(worker.fixed)peak=worker.fixed->measured_m16_linear_peak;
        if(!peak||!std::isfinite(*peak)||*peak<0.0)return {};
        ReviewMetrics metrics{*peak,operation_->policy.independent_ceiling_db_tp,attempts,
            completed_->original_pair_ms,worker.timing.fixed_ms,worker.timing.seed_ms,worker.timing.search_ms,
            {},{},0U,0U,0U};
        const auto& perf=worker.performance;
        if(phase_==Phase::candidate_ready&&perf.counts_reconciled()&&
           perf.attempted==attempts) {
            metrics.first_admissible_ms=perf.first_admissible_ms;
            metrics.last_attempt_ms=perf.last_attempt_ms;
            metrics.completed=perf.completed;
            metrics.failed=perf.failed;
            metrics.cache_hits=perf.cache_hits;
        }
        return metrics;
    }
    [[nodiscard]] const Completion* verification_result()const noexcept {
        return verification_result_?&*verification_result_:nullptr;
    }
    [[nodiscard]] const m13_frozen::M13ProvisionalDiagnosticEnvelope* diagnostic()const noexcept {
        return diagnostic_?&*diagnostic_:nullptr;
    }
    [[nodiscard]] bool begin(NativeQtProductSession operation,
        m09_frozen::NativeOwnerAllocations::AllocateUuid allocate)
    {
        if(!owner()||busy()||can_adopt()||!allocate||!current(operation))return false;
        error_.clear();
        try {
            auto input=std::make_shared<const NativeQtProductSession>(std::move(operation));
            auto cancel=std::make_shared<std::atomic<bool>>(false);
            auto task=std::async(std::launch::async,[input,cancel,allocate=std::move(allocate)]() mutable {
                return run_native_qt_product_session_worker(*input,std::move(allocate),cancel.get());
            });
            operation_=std::move(input);cancel_=std::move(cancel);
            completed_.reset();reviewed_.reset();review_diff_.reset();reviewed_project_.reset();diagnostic_.reset();verification_result_.reset();
            verification_owner_.reset();
            search_=std::move(task);phase_=Phase::searching;
            return true;
        }catch(...) {error_="L3_PRODUCT_SEARCH_DISPATCH_EXCEPTION";phase_=Phase::failed;return false;}
    }
    void cancel() {
        if(!owner())return;
        if(phase_==Phase::searching||phase_==Phase::verifying) {
            cancel_->store(true,std::memory_order_release);
            phase_=Phase::cancelling;
        }else if(can_adopt()) {
            const bool keep=replacing();
            completed_.reset();reviewed_.reset();review_diff_.reset();reviewed_project_.reset();operation_.reset();
            phase_=keep?Phase::kept_current:Phase::cancelled;
        }
    }
    [[nodiscard]] bool adopt() {
        if(!owner()||!can_adopt()||!operation_||!completed_||!review_diff_)
            return false;
        error_.clear();
        if(!current(*operation_)){
            error_="L3_PRODUCT_ACTION_STALE_CANDIDATE";
            phase_=Phase::stale;return false;
        }
        if(phase_==Phase::no_correction_prior_retained||phase_==Phase::zero_amount_prior_retained) {
            const bool suspending=phase_==Phase::zero_amount_prior_retained;
            const auto applied=apply_native_qt_fixed_only_on_owner(project(),product_,fanout_,
                selector_,gold_,*operation_,*completed_);
            if(!applied){error_=applied.error()->message();phase_=Phase::failed;return false;}
            phase_=suspending?Phase::match_suspended:Phase::no_correction_cleared;return true;
        }
        if(!reviewed_||!reviewed_project_)return false;
        const auto staged=m12_frozen::prepare_current_native_search_transaction(
            completed_->result,*reviewed_,operation_->capture.native_worker_scope(),
            operation_->policy.exact_generation,operation_->baseline,operation_->linked_chain_id,
            operation_->expected);
        if(!staged||!m12_frozen::m12_exact_project_snapshot_equal(
                *reviewed_project_,staged.value()->candidate_not_yet_committed)) {
            error_="L3_PRODUCT_REVIEWED_PROJECT_CHANGED";phase_=Phase::failed;return false;
        }
        auto receipt=apply_native_qt_product_session_on_owner(project(),product_,fanout_,
            selector_,gold_,*operation_,*completed_,*reviewed_);
        if(!receipt){error_=receipt.error()->message();phase_=Phase::failed;return false;}
        phase_=Phase::materialized;return true;
    }
    // Undo/Redo changes the exact audible state. Retire the previous
    // action's candidate and diagnostic, not the newly restored ticket.
    [[nodiscard]] bool retire_after_history_transition() {
        if(!owner()||busy()||can_adopt())return false;
        completed_.reset();reviewed_.reset();review_diff_.reset();reviewed_project_.reset();operation_.reset();
        diagnostic_.reset();verification_result_.reset();verification_owner_.reset();
        cancel_.reset();error_.clear();phase_=Phase::idle;
        return true;
    }
    // This argument comes from the Product safety authority, not QML.
    [[nodiscard]] bool begin_verification(m13_frozen::MandatoryHardPredicate other_safety,
        bool derive_product_native_safety=false) {
        if(!owner()||phase_!=Phase::materialized||!operation_||!completed_||!reviewed_)
            return false;
        auto input=capture_native_qt_product_session_verification_on_owner(project(),gold_,
            *operation_,*completed_,*reviewed_,true,other_safety);
        if(!input){error_="L3_PRODUCT_VERIFICATION_CAPTURE_UNAVAILABLE";phase_=Phase::verification_incomplete;return false;}
        input->derive_product_native_safety=derive_product_native_safety;
        try {
            auto cancel=std::make_shared<std::atomic<bool>>(false);
            const auto block=operation_->capture.current.frozen_max_block_frames;
            auto task=std::async(std::launch::async,[input=std::move(*input),cancel,block]() mutable {
                return Completion::execute(std::move(input),block,cancel.get());
            });
            cancel_=std::move(cancel);verification_=std::move(task);
            phase_=Phase::verifying;return true;
        }catch(...) {error_="L3_PRODUCT_VERIFICATION_DISPATCH_EXCEPTION";phase_=Phase::verification_incomplete;return false;}
    }
    // Nonblocking. Called on the Qt owner by a timer or queued callback.
    void poll() {
        if(!owner())return;
        using namespace std::chrono_literals;
        if(search_.valid()&&search_.wait_for(0ms)==std::future_status::ready) {
            try {
                auto done=search_.get();
                if(cancel_->load(std::memory_order_acquire)||
                   done.result.phase==NativeAutoMatchWorkerPhase::cancelled) {
                    phase_=Phase::cancelled;return;
                }
                if(!operation_||!current(*operation_)){phase_=Phase::stale;return;}
                if(done.result.phase==NativeAutoMatchWorkerPhase::search_completed&&
                   done.result.search.status==m09_frozen::NativeProductSearchStatus::no_correction_required) {
                    if(replacing()) {
                        if(native_qt_fixed_only_no_correction(*operation_,done)) {
                            const bool zero=native_qt_zero_effective_amount(*operation_);
                            completed_.emplace(std::move(done));
                            auto after=operation_->expected.exact_existing_linked_snapshot;
                            if(zero&&std::all_of(after->modules.begin(),after->modules.end(),
                                    [](const auto& node){return node.controllerSuspended;})) {
                                // Already suspended: the actual measured Fixed gate
                                // has passed, but there is no new state change to adopt.
                                phase_=Phase::match_suspended;return;
                            }
                            if(zero)for(auto& node:after->modules)node.controllerSuspended=true;
                            else after.reset();
                            review_diff_=m12_frozen::describe_exact_m12_review_diff(
                                operation_->expected.exact_existing_linked_snapshot,after);
                            phase_=zero?Phase::zero_amount_prior_retained:Phase::no_correction_prior_retained;
                        } else {
                            error_="L3_PRODUCT_NO_CORRECTION_EVIDENCE_UNAVAILABLE";
                            completed_.emplace(std::move(done));
                            phase_=Phase::failed;
                        }
                    }else if(native_qt_fixed_only_no_correction(*operation_,done)) {
                        completed_.emplace(std::move(done));
                        phase_=Phase::no_correction;
                    }else {
                        completed_.emplace(std::move(done));
                        error_="L3_PRODUCT_NO_CORRECTION_EVIDENCE_UNAVAILABLE";
                        phase_=Phase::failed;
                    }
                    return;
                }
                auto plan=review_native_qt_product_session_on_owner(project(),product_,fanout_,
                    selector_,gold_,*operation_,done);
                if(!plan){
                    error_="L3_PRODUCT_SEARCH_NO_REVIEWABLE_WINNER: worker="+
                        std::to_string(static_cast<int>(done.result.phase))+
                        " search="+std::to_string(static_cast<int>(done.result.search.status))+
                        " fixed="+std::to_string(static_cast<int>(done.result.search.fixed_gate.status));
                    completed_.emplace(std::move(done));
                    phase_=Phase::failed;return;
                }
                const auto draft=m12_frozen::prepare_current_native_search_transaction(
                    done.result,*plan,operation_->capture.native_worker_scope(),operation_->policy.exact_generation,
                    operation_->baseline,operation_->linked_chain_id,operation_->expected);
                if(!draft){error_=draft.error()->message();phase_=Phase::failed;return;}
                const auto& chains=draft.value()->candidate_not_yet_committed.document().chains;
                const auto linked=std::find_if(chains.begin(),chains.end(),[&](const auto& c){
                    return c.chainId==operation_->linked_chain_id&&c.stage=="MASTER"&&c.segment=="REF_LINKED";
                });
                if(linked==chains.end()){phase_=Phase::failed;return;}
                review_diff_=m12_frozen::describe_exact_m12_review_diff(
                    operation_->expected.exact_existing_linked_snapshot,std::optional<project::Chain>{*linked});
                reviewed_project_.emplace(draft.value()->candidate_not_yet_committed);
                completed_.emplace(std::move(done));reviewed_.emplace(std::move(*plan));
                phase_=Phase::candidate_ready;
            }catch(...) {error_="L3_PRODUCT_SEARCH_COMPLETION_EXCEPTION";phase_=Phase::failed;}
        }
        if(verification_.valid()&&verification_.wait_for(0ms)==std::future_status::ready) {
            try {
                auto done=verification_.get();
                if(cancel_->load(std::memory_order_acquire)) {
                    error_="L3_PRODUCT_VERIFICATION_CANCELLED";phase_=Phase::verification_incomplete;return;
                }
                if(!done){error_=done.error()->message();phase_=Phase::verification_incomplete;return;}
                auto sealed=project().seal_private_m13_actual_native_completion(*done.value());
                if(!sealed){error_=sealed.error()->message();phase_=Phase::verification_incomplete;return;}
                // Sealing already acquired the physical M12/M18 lease.
                // Record its *post-seal* owner. GUI refresh must never
                // acquire another mutation lease merely to display it.
                verification_owner_=fanout_.current_owner_snapshot();
                if(!verification_owner_){error_="L3_PRODUCT_VERIFICATION_OWNER_UNAVAILABLE";phase_=Phase::verification_incomplete;return;}
                diagnostic_.emplace(std::move(*sealed.value()));
                verification_result_.emplace(std::move(*done.value()));
                phase_=verification_result_->assessment().native_numerical_evidence_passed()
                    ?Phase::numerical_pass:Phase::verification_incomplete;
            }catch(...) {error_="L3_PRODUCT_VERIFICATION_COMPLETION_EXCEPTION";phase_=Phase::verification_incomplete;}
        }
        if((phase_==Phase::candidate_ready||phase_==Phase::no_correction||
            phase_==Phase::no_correction_prior_retained||phase_==Phase::zero_amount_prior_retained)&&
           operation_&&!current(*operation_)) {
            reviewed_.reset();review_diff_.reset();reviewed_project_.reset();completed_.reset();phase_=Phase::stale;
        }
        if(verification_result_) {
            const auto currentOwner=fanout_.current_owner_snapshot();
            // Conservative read-only display validity. An intervening
            // epoch change retires the displayed evidence; it never
            // revokes a valid M17 render or touches an in-flight Export.
            if(!currentOwner||!verification_owner_||
               currentOwner->epoch!=verification_owner_->epoch||
               currentOwner->identity!=verification_owner_->identity||
               gold_.selection_revision()!=verification_result_->source_reference_owner().reference_selection_revision) {
                diagnostic_.reset();verification_result_.reset();verification_owner_.reset();
                phase_=Phase::stale;
            }
        }
    }
private:
    [[nodiscard]] ProjectSessionViewModel& project()const noexcept{return product_.upstream.project;}
    [[nodiscard]] bool owner()const noexcept{return QThread::currentThread()==project().thread();}
    [[nodiscard]] bool current(const NativeQtProductSession& operation)const {
        return native_qt_search_still_current_on_owner(product_,fanout_,selector_,gold_,operation.capture);
    }
    Controllers product_;
    const Fanout& fanout_;
    const AuditionSourceSelector& selector_;
    const GoldSelectionViewModel& gold_;
    Phase phase_{Phase::idle};
    std::string error_;
    std::shared_ptr<const NativeQtProductSession> operation_;
    std::shared_ptr<std::atomic<bool>> cancel_;
    std::optional<NativeQtSearchWorkerResult> completed_;
    std::optional<m12_frozen::AcceptedPlanAndOwner> reviewed_;
    std::optional<m12_frozen::M12ReviewDiff> review_diff_;
    std::optional<project::ProjectSnapshot> reviewed_project_;
    std::optional<m13_frozen::M13ProvisionalDiagnosticEnvelope> diagnostic_;
    std::optional<Completion> verification_result_;
    std::optional<Fanout::OwnerSnapshot> verification_owner_;
    std::future<NativeQtSearchWorkerResult> search_;
    std::future<core::Result<Completion>> verification_;
};
} // namespace rgsml::app::l3_automatch
