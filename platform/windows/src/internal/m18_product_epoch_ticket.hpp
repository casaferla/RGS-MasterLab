#pragma once
// M18 preproduction, NO Product/QML/worker integration.
// This Qt-free contract is inert until ALL authoritative Product mutators
// are routed through try_transition BEFORE changing source/project/MASTER.
#include "m18_epoch_commit_barrier.hpp"
#include <rgsml/core/error.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace rgsml::platform::windows::internal {

struct M18ProductExportIdentity final {
    std::uint64_t source_generation{0};
    std::uint64_t project_generation{0};
    std::uint64_t master_generation{0};
    std::uint64_t chain_revision{0};
    std::uint32_t master_rate_hz{0};

    [[nodiscard]] bool valid() const noexcept {
        const bool rate=master_rate_hz==44100U ||master_rate_hz==48000U
            ||master_rate_hz==88200U ||master_rate_hz==96000U
            ||master_rate_hz==176400U ||master_rate_hz==192000U;
        return source_generation!=0 &&project_generation!=0
            &&master_generation!=0 &&chain_revision!=0 &&rate;
    }
    friend bool operator==(const M18ProductExportIdentity&,
                           const M18ProductExportIdentity&)=default;
};

// Standalone permission protocol; no captured QObject, QPointer, callback,
// raw AudioBufferView or change to the existing Product M17 orchestration.
// A genuine Product must additionally retain/recheck the exact immutable M17
// owner and source FILE identity and honour EVERY rejected mutation.
class M18ProductEpochTicketAuthority final {
    struct State final {
        State(std::uint64_t initial,M18ProductExportIdentity id)
            :epoch(initial),identity(id),barrier(initial){}
        std::mutex mutex;
        std::uint64_t epoch;
        M18ProductExportIdentity identity;
        bool explicit_consent{false};
        M18EpochCommitBarrier barrier;
    };
public:
    class Ticket final {
        friend class M18ProductEpochTicketAuthority;
        Ticket(std::shared_ptr<State> state,std::uint64_t epoch,
               M18ProductExportIdentity identity)
            :state_(std::move(state)),epoch_(epoch),identity_(identity){}
    public:
        [[nodiscard]] std::uint64_t epoch() const noexcept {return epoch_;}
        [[nodiscard]] M18ProductExportIdentity identity() const noexcept {
            return identity_;
        }
        // This is a worker-safe lease factory ONLY. The Product has no
        // caller for it yet. Consent cannot be changed during a held commit:
        // transition fails BUSY, leaving the original authority unchanged.
        [[nodiscard]] M18AcquireCommitLease commit_lease_factory() const {
            return [state=state_,epoch=epoch_,id=identity_]()
                ->core::Result<std::unique_ptr<M18ExportCommitLease>> {
                using R=core::Result<std::unique_ptr<M18ExportCommitLease>>;
                std::lock_guard guard{state->mutex};
                if(!state->explicit_consent ||state->epoch!=epoch
                   ||state->identity!=id ||!id.valid())
                    return R::failure(core::Error{
                        core::ErrorCode::InvalidState,
                        "M18_PRODUCT_TICKET_REVOKED_OR_IDENTITY_CHANGED"});
                // Same lock as every owner transition: no gap between
                // consent/identity validation and commit lease admission.
                return state->barrier.try_acquire(epoch);
            };
        }
    private:
        std::shared_ptr<State> state_;
        std::uint64_t epoch_;
        M18ProductExportIdentity identity_;
    };

    explicit M18ProductEpochTicketAuthority(
        std::uint64_t initialEpoch,M18ProductExportIdentity initialIdentity)
        :state_(std::make_shared<State>(initialEpoch,initialIdentity)){}
    M18ProductEpochTicketAuthority(const M18ProductEpochTicketAuthority&)=delete;
    M18ProductEpochTicketAuthority& operator=(const M18ProductEpochTicketAuthority&)=delete;

    // Product-owner-thread only. Call BEFORE ANY real source/project/chain
    // mutation, consent change or M17 candidate retirement. A failed
    // transition MUST leave the actual Product state unchanged.
    // nextEpoch is strictly monotonic; never wrap/reuse prior epoch IDs.
    [[nodiscard]] core::Status try_transition(
        std::uint64_t expectedEpoch,std::uint64_t nextEpoch,
        M18ProductExportIdentity nextIdentity,bool consent) const {
        std::lock_guard guard{state_->mutex};
        if(!nextIdentity.valid() ||expectedEpoch==0
           ||state_->epoch!=expectedEpoch ||nextEpoch<=expectedEpoch)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PRODUCT_EPOCH_INVALID_TRANSITION"});
        auto advanced=state_->barrier.try_advance_epoch(
            expectedEpoch,nextEpoch);
        if(!advanced)return advanced;
        state_->identity=nextIdentity;
        state_->explicit_consent=consent;
        state_->epoch=nextEpoch;
        return core::Status::success();
    }

    // Product-owner-thread admission. Only a separately checked current
    // verified-M17 owner and explicit user choice may supply this identity.
    [[nodiscard]] core::Result<Ticket> capture(
        M18ProductExportIdentity currentIdentity) const {
        using R=core::Result<Ticket>;
        std::lock_guard guard{state_->mutex};
        if(state_->epoch==0 ||!currentIdentity.valid()
           ||state_->identity!=currentIdentity
           ||!state_->explicit_consent)
            return R::failure(core::Error{
                core::ErrorCode::AccessDenied,
                "M18_PRODUCT_TICKET_EXPLICIT_CURRENT_CONSENT_REQUIRED"});
        return R::success(Ticket{state_,state_->epoch,state_->identity});
    }
private:
    std::shared_ptr<State> state_;
};
} // namespace rgsml::platform::windows::internal
