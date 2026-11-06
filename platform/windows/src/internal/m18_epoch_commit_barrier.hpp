#pragma once
// M18 preproduction: non-Qt cross-thread commit admission primitive.
// NOT a Product session authority until *every* Source/Project/MASTER
// mutation is routed through try_advance_epoch()/try_retire().
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <utility>

namespace rgsml::platform::windows::internal {

class M18EpochCommitBarrier final {
    struct State final {
        explicit State(std::uint64_t e) noexcept : epoch(e) {}
        mutable std::mutex mutex;
        std::uint64_t epoch;
        bool active{false};
        bool retired{false};
    };
    class HeldLease final : public M18ExportCommitLease {
    public:
        HeldLease(std::shared_ptr<State> state,std::uint64_t epoch) noexcept
            :state_(std::move(state)),epoch_(epoch) {}
        HeldLease(const HeldLease&)=delete;
        HeldLease& operator=(const HeldLease&)=delete;
        ~HeldLease() override {
            std::lock_guard guard{state_->mutex};
            // try_advance_epoch/retire cannot succeed while active.
            if(state_->active &&state_->epoch==epoch_)
                state_->active=false;
        }
        [[nodiscard]] core::Status revalidate_current() const override {
            std::lock_guard guard{state_->mutex};
            if(!state_->active ||state_->retired ||state_->epoch!=epoch_)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,"M18_EPOCH_LEASE_NOT_CURRENT"});
            return core::Status::success();
        }
    private:
        std::shared_ptr<State> state_;
        std::uint64_t epoch_;
    };
public:
    explicit M18EpochCommitBarrier(std::uint64_t epoch)
        :state_(std::make_shared<State>(epoch)) {}
    M18EpochCommitBarrier(const M18EpochCommitBarrier&)=delete;
    M18EpochCommitBarrier& operator=(const M18EpochCommitBarrier&)=delete;

    // A caller-issued ticket has no commit authority until this succeeds.
    // The acquired lease may be moved and revalidated on ANY thread. It
    // keeps active=true throughout rename and final checksum readback.
    [[nodiscard]] core::Result<std::unique_ptr<M18ExportCommitLease>>
    try_acquire(std::uint64_t expectedEpoch) const {
        using R=core::Result<std::unique_ptr<M18ExportCommitLease>>;
        std::lock_guard guard{state_->mutex};
        if(expectedEpoch==0 ||state_->retired ||state_->active
           ||expectedEpoch!=state_->epoch)
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_EPOCH_REVOKED_STALE_OR_COMMIT_BUSY"});
        // Allocate BEFORE publishing active=true: allocation failure
        // cannot strand a busy lease. No QObject callback or UI wait.
        try {
            std::unique_ptr<M18ExportCommitLease> lease=
                std::make_unique<HeldLease>(state_,expectedEpoch);
            state_->active=true;
            return R::success(std::move(lease));
        }catch(const std::bad_alloc&){
            return R::failure(core::Error{
                core::ErrorCode::IoFailure,
                "M18_EPOCH_LEASE_ALLOCATION_FAILED"});
        }
    }

    // OWNER-THREAD ONLY contract: call BEFORE publishing any change to
    // Source/Project/selected-MASTER. Never block the Qt event loop while
    // a worker is committing: if active, reject/defer the proposed mutation.
    // The Product caller MUST honor failure and leave its epoch unchanged.
    [[nodiscard]] core::Status try_advance_epoch(
        std::uint64_t expectedEpoch,std::uint64_t nextEpoch) const {
        std::lock_guard guard{state_->mutex};
        if(state_->active ||state_->retired ||expectedEpoch==0
           ||expectedEpoch!=state_->epoch
           ||nextEpoch<=expectedEpoch)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_EPOCH_ADVANCE_DENIED_ACTIVE_OR_STALE"});
        state_->epoch=nextEpoch;
        return core::Status::success();
    }

    // The owner cannot destruct/rebind Project while a commit is held.
    // Must be called before final Product retirement, never after.
    [[nodiscard]] core::Status try_retire(std::uint64_t expectedEpoch) const {
        std::lock_guard guard{state_->mutex};
        if(state_->active ||state_->retired ||expectedEpoch==0
           ||state_->epoch!=expectedEpoch)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_EPOCH_RETIRE_DENIED_ACTIVE_OR_STALE"});
        state_->retired=true;
        return core::Status::success();
    }

    // Diagnostic only, NEVER a source of authorization for a new export.
    [[nodiscard]] std::uint64_t epoch_snapshot() const noexcept {
        std::lock_guard guard{state_->mutex};
        return state_->epoch;
    }
private:
    std::shared_ptr<State> state_;
};
} // namespace rgsml::platform::windows::internal
