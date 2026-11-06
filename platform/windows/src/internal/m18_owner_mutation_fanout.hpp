#pragma once
// M18 PRIVATE PREPRODUCTION — one Qt-free product-epoch admission callback
// shared by independent Product controllers. NOT INSTALLED IN main.cpp.
// The callback revokes export consent BEFORE a permitted mutation begins.
// It does NOT mint a ticket or prove that all Product paths are covered.
#include "m18_product_epoch_ticket.hpp"
#include <rgsml/core/error.hpp>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <utility>

namespace rgsml::platform::windows::internal {

class M18OwnerMutationFanout final {
    struct Shared final {
        Shared(std::shared_ptr<M18ProductEpochTicketAuthority> owner,
               std::uint64_t initialEpoch,M18ProductExportIdentity initialId)
            :authority(std::move(owner)),ownerThread(std::this_thread::get_id()),
             epoch(initialEpoch),identity(initialId){}
        std::shared_ptr<M18ProductEpochTicketAuthority> authority;
        const std::thread::id ownerThread;
        std::uint64_t epoch;
        M18ProductExportIdentity identity;
        bool admitting{false};
    };
public:
    using StatusAdmission=std::function<core::Status(const char*)>;
    using RateAdmission=std::function<bool()>;
    M18OwnerMutationFanout(
        std::shared_ptr<M18ProductEpochTicketAuthority> authority,
        std::uint64_t currentEpoch,M18ProductExportIdentity currentIdentity)
        :shared_(std::make_shared<Shared>(
            std::move(authority),currentEpoch,currentIdentity)){}
    M18OwnerMutationFanout(const M18OwnerMutationFanout&)=delete;
    M18OwnerMutationFanout& operator=(const M18OwnerMutationFanout&)=delete;
    ~M18OwnerMutationFanout()=default;

    [[nodiscard]] StatusAdmission status_admission() const {
        return [weak=std::weak_ptr<Shared>{shared_}](
            const char* operation)->core::Status {
            auto state=weak.lock();
            if(!state)return reject("M18_FANOUT_PRODUCT_OWNER_RETIRED");
            if(std::this_thread::get_id()!=state->ownerThread)
                return reject("M18_FANOUT_REQUIRES_PRODUCT_OWNER_THREAD");
            if(!state->authority ||!state->identity.valid()
               ||state->epoch==0)
                return reject("M18_FANOUT_INVALID_PRODUCT_AUTHORITY");
            if(state->admitting)
                return reject("M18_FANOUT_REENTRANT_MUTATION");
            if(!operation ||!*operation)
                return reject("M18_FANOUT_OPERATION_REQUIRED");
            if(state->epoch==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.master_generation
                      ==std::numeric_limits<std::uint64_t>::max())
                return reject("M18_FANOUT_EPOCH_EXHAUSTED");
            state->admitting=true;
            struct Unwind final {
                bool* inProgress;
                ~Unwind(){*inProgress=false;}
            } unwind{&state->admitting};
            auto nextIdentity=state->identity;
            // Conservative invalidation: every authorized Product write
            // retires its prior verified-M17 export generation. The real
            // source/project/chain identity MUST be reconciled by Product
            // before it is ever permitted to mint a NEW worker ticket.
            ++nextIdentity.master_generation;
            const auto nextEpoch=state->epoch+1;
            const auto allowed=state->authority->try_transition(
                state->epoch,nextEpoch,nextIdentity,false);
            if(!allowed)return allowed;
            state->identity=nextIdentity;
            state->epoch=nextEpoch;
            return core::Status::success();
        };
    }

    // Adapt to the existing MasterOutputRateState bool admission. This
    // grants no separate epoch and is only for true rate/source edits.
    [[nodiscard]] RateAdmission rate_admission() const {
        auto same=status_admission();
        return [same=std::move(same)] {
            return static_cast<bool>(same("master_rate_change"));
        };
    }
private:
    [[nodiscard]] static core::Status reject(const char* message) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,message});
    }
    std::shared_ptr<Shared> shared_;
};

} // namespace rgsml::platform::windows::internal
