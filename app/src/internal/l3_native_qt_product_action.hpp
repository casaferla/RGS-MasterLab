#pragma once
// Owner-thread action; only immutable input and cancellation cross threads.
// Search completion never applies a graph. Adoption is a separate command.
#include "l3_native_qt_product_session.hpp"
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
    [[nodiscard]] const Completion* verification_result()const noexcept {
        return verification_result_?&*verification_result_:nullptr;
    }
    [[nodiscard]] const m13_frozen::M13ProvisionalDiagnosticEnvelope* diagnostic()const noexcept {
        return diagnostic_?&*diagnostic_:nullptr;
    }
    [[nodiscard]] bool begin(NativeQtProductSession operation,
        m09_frozen::NativeOwnerAllocations::AllocateUuid allocate)
    {
        if(!owner()||busy()||!allocate||!current(operation))return false;
        error_.clear();
        try {
            auto input=std::make_shared<const NativeQtProductSession>(std::move(operation));
            auto cancel=std::make_shared<std::atomic<bool>>(false);
            auto task=std::async(std::launch::async,[input,cancel,allocate=std::move(allocate)]() mutable {
                return run_native_qt_product_session_worker(*input,std::move(allocate),cancel.get());
            });
            operation_=std::move(input);cancel_=std::move(cancel);
            completed_.reset();reviewed_.reset();diagnostic_.reset();verification_result_.reset();
            verification_owner_.reset();
            search_=std::move(task);phase_=Phase::searching;
            return true;
        }catch(...) {phase_=Phase::failed;return false;}
    }
    void cancel() {
        if(!owner())return;
        if(phase_==Phase::searching||phase_==Phase::verifying) {
            cancel_->store(true,std::memory_order_release);
            phase_=Phase::cancelling;
        }else if(can_adopt()) {
            const bool keep=replacing();
            completed_.reset();reviewed_.reset();operation_.reset();
            phase_=keep?Phase::kept_current:Phase::cancelled;
        }
    }
    [[nodiscard]] bool adopt() {
        if(!owner()||!can_adopt()||!operation_||!completed_)
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
        if(!reviewed_)return false;
        auto receipt=apply_native_qt_product_session_on_owner(project(),product_,fanout_,
            selector_,gold_,*operation_,*completed_,*reviewed_);
        if(!receipt){error_=receipt.error()->message();phase_=Phase::failed;return false;}
        phase_=Phase::materialized;return true;
    }
    // Undo/Redo changes the exact audible state. Retire the previous
    // action's candidate and diagnostic, not the newly restored ticket.
    [[nodiscard]] bool retire_after_history_transition() {
        if(!owner()||busy()||can_adopt())return false;
        completed_.reset();reviewed_.reset();operation_.reset();
        diagnostic_.reset();verification_result_.reset();verification_owner_.reset();
        cancel_.reset();error_.clear();phase_=Phase::idle;
        return true;
    }
    // This argument comes from the Product safety authority, not QML.
    [[nodiscard]] bool begin_verification(m13_frozen::MandatoryHardPredicate other_safety) {
        if(!owner()||phase_!=Phase::materialized||!operation_||!completed_||!reviewed_)
            return false;
        auto input=capture_native_qt_product_session_verification_on_owner(project(),gold_,
            *operation_,*completed_,*reviewed_,true,other_safety);
        if(!input){phase_=Phase::verification_incomplete;return false;}
        try {
            auto cancel=std::make_shared<std::atomic<bool>>(false);
            const auto block=operation_->capture.current.frozen_max_block_frames;
            auto task=std::async(std::launch::async,[input=std::move(*input),cancel,block]() mutable {
                return Completion::execute(std::move(input),block,cancel.get());
            });
            cancel_=std::move(cancel);verification_=std::move(task);
            phase_=Phase::verifying;return true;
        }catch(...) {phase_=Phase::verification_incomplete;return false;}
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
                            phase_=zero?Phase::zero_amount_prior_retained:Phase::no_correction_prior_retained;
                        } else {
                            error_="L3_PRODUCT_NO_CORRECTION_EVIDENCE_UNAVAILABLE";
                            completed_.emplace(std::move(done));
                            phase_=Phase::failed;
                        }
                    }else phase_=Phase::no_correction;
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
                completed_.emplace(std::move(done));reviewed_.emplace(std::move(*plan));
                phase_=Phase::candidate_ready;
            }catch(...) {phase_=Phase::failed;}
        }
        if(verification_.valid()&&verification_.wait_for(0ms)==std::future_status::ready) {
            try {
                auto done=verification_.get();
                if(cancel_->load(std::memory_order_acquire)||!done) {
                    phase_=Phase::verification_incomplete;return;
                }
                auto sealed=project().seal_private_m13_actual_native_completion(*done.value());
                if(!sealed){phase_=Phase::verification_incomplete;return;}
                // Sealing already acquired the physical M12/M18 lease.
                // Record its *post-seal* owner. GUI refresh must never
                // acquire another mutation lease merely to display it.
                verification_owner_=fanout_.current_owner_snapshot();
                if(!verification_owner_){phase_=Phase::verification_incomplete;return;}
                diagnostic_.emplace(std::move(*sealed.value()));
                verification_result_.emplace(std::move(*done.value()));
                phase_=verification_result_->assessment().native_numerical_evidence_passed()
                    ?Phase::numerical_pass:Phase::verification_incomplete;
            }catch(...) {phase_=Phase::verification_incomplete;}
        }
        if((phase_==Phase::candidate_ready||phase_==Phase::no_correction||
            phase_==Phase::no_correction_prior_retained||phase_==Phase::zero_amount_prior_retained)&&
           operation_&&!current(*operation_)) {
            reviewed_.reset();completed_.reset();phase_=Phase::stale;
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
    std::optional<m13_frozen::M13ProvisionalDiagnosticEnvelope> diagnostic_;
    std::optional<Completion> verification_result_;
    std::optional<Fanout::OwnerSnapshot> verification_owner_;
    std::future<NativeQtSearchWorkerResult> search_;
    std::future<core::Result<Completion>> verification_;
};
} // namespace rgsml::app::l3_automatch
