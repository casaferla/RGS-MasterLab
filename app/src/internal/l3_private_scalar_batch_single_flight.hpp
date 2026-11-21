#pragma once
// Single asynchronous L3 scalar candidate batch worker, nonpublishing.
// No QObject, QML, audio callback or Product pointer enters the thread.
// The Qt owner must first capture a real ProductQtOfflineAdmission, then
// revalidate M18/SOURCE/REFERENCE/bindings after take_finished().
// Detached lifetime is safe: the thread owns an independent shared State.
// Cancellation is cooperative between full-prefix DSP evaluations, never
// a claim of safe interruption within an IModule.process_block callback.
#include "l3_private_scalar_candidate_batch.hpp"
#include <rgsml/core/error.hpp>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
class PrivateScalarBatchSingleFlight final {
public:
    struct Completion final {
        std::uint64_t request_id{};
        std::optional<PrivateBatchReport> report;
        std::optional<core::Error> error;
    };
    PrivateScalarBatchSingleFlight() noexcept
        :owner_(std::this_thread::get_id()) {}
    PrivateScalarBatchSingleFlight(const PrivateScalarBatchSingleFlight&)=delete;
    PrivateScalarBatchSingleFlight& operator=(
        const PrivateScalarBatchSingleFlight&)=delete;
    ~PrivateScalarBatchSingleFlight() {
        // May run on any thread; detached worker only owns its shared State,
        // immutable audio jobs and shared cancellation/progress atomics.
        if(progress_)progress_->request_cancel();
    }

    [[nodiscard]] bool start(
        std::uint64_t request_id,
        std::vector<OfflineProductTrialJob> jobs,
        m21_private::Snapshot reference,
        std::vector<Objective> objectives)
    {
        if(std::this_thread::get_id()!=owner_ ||state_
           ||request_id==0||jobs.empty()||jobs.size()>800
           ||objectives.empty())
            return false;
        std::shared_ptr<State> state;
        std::shared_ptr<PrivateBatchProgress> progress;
        try {
            state=std::make_shared<State>();
            progress=std::make_shared<PrivateBatchProgress>();
            std::thread([state,progress,jobs=std::move(jobs),
                         reference=std::move(reference),
                         objectives=std::move(objectives)]() mutable {
                try {
                    auto outcome=PrivateScalarCandidateBatch::run(
                        jobs,reference,objectives,progress);
                    {
                        std::lock_guard lock(state->mutex);
                        if(outcome)state->report=std::move(*outcome.value());
                        else state->error=std::move(*outcome.error());
                    }
                }catch(...) {
                    // No exception/partial result from the worker may reach
                    // Qt as a measured AudioMatch result or Product success.
                    std::lock_guard lock(state->mutex);
                    state->error=core::Error{
                        core::ErrorCode::InvalidState,
                        "L3_PRIVATE_BATCH_WORKER_EXCEPTION"};
                }
                state->finished.store(true,std::memory_order_release);
            }).detach();
        }catch(...) {
            return false;
        }
        request_id_=request_id;
        state_=std::move(state);
        progress_=std::move(progress);
        return true;
    }
    [[nodiscard]] bool busy() const noexcept {
        return std::this_thread::get_id()==owner_ &&state_!=nullptr;
    }
    [[nodiscard]] bool finished() const noexcept {
        return std::this_thread::get_id()==owner_ &&state_
            &&state_->finished.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::shared_ptr<const PrivateBatchProgress>
    progress() const noexcept {
        if(std::this_thread::get_id()!=owner_)return {};
        return progress_;
    }
    [[nodiscard]] bool cancel() noexcept {
        if(std::this_thread::get_id()!=owner_||!state_||!progress_
           ||state_->finished.load(std::memory_order_acquire))
            return false;
        progress_->request_cancel();
        return true;
    }
    [[nodiscard]] std::optional<Completion> take_finished() {
        if(!finished())return std::nullopt;
        Completion out;
        out.request_id=request_id_;
        {
            std::lock_guard lock(state_->mutex);
            out.report=std::move(state_->report);
            out.error=std::move(state_->error);
        }
        request_id_=0;
        state_.reset();
        progress_.reset();
        return out;
    }
private:
    struct State {
        std::mutex mutex;
        std::optional<PrivateBatchReport> report;
        std::optional<core::Error> error;
        std::atomic<bool> finished{false};
    };
    const std::thread::id owner_;
    std::uint64_t request_id_{};
    std::shared_ptr<State> state_;
    std::shared_ptr<PrivateBatchProgress> progress_;
};
} // namespace rgsml::app::l3_automatch
