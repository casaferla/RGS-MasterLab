#pragma once
// M18 PRIVATE PREPRODUCTION — one Qt-free product-epoch admission callback
// shared by independent Product controllers. NOT INSTALLED IN main.cpp.
// The callback revokes export consent BEFORE a permitted mutation begins.
// It does NOT mint a ticket or prove that all Product paths are covered.
#include "m18_product_epoch_ticket.hpp"
#include <rgsml/core/error.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
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
        // Only an owner-held Project-open batch may use the approved
        // nested child calls without advancing a second Product epoch.
        std::atomic<bool> projectOpenActive{false};
        std::atomic<bool> sourceCommitActive{false};
        std::atomic<bool> retired{false};
    };
    // The typed token owns BOTH the exclusive physical Product lease and
    // its nested-child-mode lifetime. Destroy lease before dropping mode.
    struct HeldProjectOpen final {
        HeldProjectOpen(std::shared_ptr<Shared> owner,
            std::unique_ptr<M18EpochCommitBarrier::MutationLease> lease)
            :state(std::move(owner)),held(std::move(lease)) {}
        ~HeldProjectOpen(){
            held.reset();
            state->projectOpenActive.store(
                false,std::memory_order_release);
        }
        HeldProjectOpen(const HeldProjectOpen&)=delete;
        HeldProjectOpen& operator=(const HeldProjectOpen&)=delete;
        std::shared_ptr<Shared> state;
        std::unique_ptr<M18EpochCommitBarrier::MutationLease> held;
    };
    struct HeldSourceCommit final {
        HeldSourceCommit(std::shared_ptr<Shared> owner,
            std::unique_ptr<M18EpochCommitBarrier::MutationLease> lease)
            :state(std::move(owner)),held(std::move(lease)) {}
        ~HeldSourceCommit(){
            held.reset();
            state->sourceCommitActive.store(
                false,std::memory_order_release);
        }
        HeldSourceCommit(const HeldSourceCommit&)=delete;
        HeldSourceCommit& operator=(const HeldSourceCommit&)=delete;
        std::shared_ptr<Shared> state;
        std::unique_ptr<M18EpochCommitBarrier::MutationLease> held;
    };
public:
    using StatusAdmission=std::function<core::Status(const char*)>;
    using RateAdmission=std::function<bool()>;
    using ProjectSaveBatch=std::function<
        core::Result<std::shared_ptr<const void>>()>;

    M18OwnerMutationFanout(
        std::shared_ptr<M18ProductEpochTicketAuthority> authority,
        std::uint64_t currentEpoch,M18ProductExportIdentity currentIdentity)
        :shared_(std::make_shared<Shared>(
            std::move(authority),currentEpoch,currentIdentity)){}
    M18OwnerMutationFanout(const M18OwnerMutationFanout&)=delete;
    M18OwnerMutationFanout& operator=(const M18OwnerMutationFanout&)=delete;
    ~M18OwnerMutationFanout() {
        shared_->retired.store(true,std::memory_order_release);
    }

    struct OwnerSnapshot final {
        std::uint64_t epoch;
        M18ProductExportIdentity identity;
    };
    // Diagnostic-only Product-side provenance; never a consent token.
    // Reading the fanout's mutable epoch on a worker would be a data race.
    [[nodiscard]] std::optional<OwnerSnapshot> current_owner_snapshot() const noexcept {
        const auto state=shared_;
        if(!state
           ||std::this_thread::get_id()!=state->ownerThread
           ||state->retired.load(std::memory_order_acquire))
            return std::nullopt;
        return OwnerSnapshot{state->epoch,state->identity};
    }

    // Product-wide activation is legal only BEFORE consent/ticket minting.
    // The immutable authority/owner thread are checked without changing
    // epoch or touching any controller. No QObjects reach the worker.
    [[nodiscard]] bool can_install_full_product_guards() const noexcept {
        const auto state=shared_;
        return state
            &&std::this_thread::get_id()==state->ownerThread
            &&!state->retired.load(std::memory_order_acquire)
            &&!state->admitting
            &&!state->projectOpenActive.load(std::memory_order_acquire)
            &&!state->sourceCommitActive.load(std::memory_order_acquire)
            &&state->authority &&state->identity.valid()
            &&state->epoch>0
            &&state->authority->can_install_full_product_guards(
                state->epoch,state->identity);
    }

    // PRIVATE Product-only consent step. This is NOT a UI command and
    // MUST be called only after the COMPLETE binder proves all installed
    // guards and a fresh exact M16-verified M17 stage. The verified identity
    // is not inferred from a filename or the worker. Consent takes one new
    // epoch without changing the already accepted SOURCE/Project/MASTER.
    [[nodiscard]] core::Result<M18ProductEpochTicketAuthority::Ticket>
    capture_after_explicit_verified_consent(
        M18ProductExportIdentity verifiedCurrentIdentity,
        bool confirmedByUser) const
    {
        using R=core::Result<M18ProductEpochTicketAuthority::Ticket>;
        const auto state=shared_;
        if(!state||std::this_thread::get_id()!=state->ownerThread
           ||state->retired.load(std::memory_order_acquire)
           ||state->admitting||!confirmedByUser
           ||state->projectOpenActive.load(std::memory_order_acquire)
           ||state->sourceCommitActive.load(std::memory_order_acquire)
           ||!state->authority||state->epoch==0
           ||state->epoch==std::numeric_limits<std::uint64_t>::max()
           ||!verifiedCurrentIdentity.valid()
           ||verifiedCurrentIdentity!=state->identity
           ||!state->authority->can_install_full_product_guards(
               state->epoch,state->identity))
            return R::failure(core::Error{
                core::ErrorCode::AccessDenied,
                "M18_EXPORT_NEW_USER_CONSENT_REQUIRES_EXACT_CURRENT_PRODUCT"});
        state->admitting=true;
        struct Unwind final {
            bool* active;
            ~Unwind(){*active=false;}
        } unwind{&state->admitting};
        const auto newEpoch=state->epoch+1;
        const auto accepted=state->authority->try_transition(
            state->epoch,newEpoch,verifiedCurrentIdentity,true);
        if(!accepted)return R::failure(*accepted.error());
        state->epoch=newEpoch;
        return state->authority->capture(verifiedCurrentIdentity);
    }

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
            if(state->retired.load(std::memory_order_acquire))
                return reject("M18_FANOUT_PRODUCT_OWNER_RETIRED");
            if(state->admitting)
                return reject("M18_FANOUT_REENTRANT_MUTATION");
            if(!operation ||!*operation)
                return reject("M18_FANOUT_OPERATION_REQUIRED");
            if(state->projectOpenActive.load(std::memory_order_acquire)) {
                // The external Project operation already owns the single
                // exclusive barrier. Do not advance or withdraw a second
                // generation for its carefully enumerated child writes.
                return approved_project_open_child_operation(operation)
                    ?core::Status::success()
                    :reject("M18_PROJECT_OPEN_UNEXPECTED_CHILD_MUTATION");
            }
            if(state->sourceCommitActive.load(std::memory_order_acquire)) {
                // SOURCE replacement publishes rate, prepared/processed
                // and GOLD retirement through synchronous Qt callbacks.
                // Those expected children share ONE physical writer lease.
                return approved_source_child_operation(operation)
                    ?core::Status::success()
                    :reject("M18_SOURCE_UNEXPECTED_CHILD_MUTATION");
            }
            // Direct one-shot SOURCE/Project operations must also retire
            // their own owner identity, not merely the M17 Master stage.
            // Physical SOURCE/Project batches do this once at the outer
            // lease and their nested child operations bypass this path.
            const std::string_view op{operation};
            const bool changesSource=op=="source_selection"
                ||op=="source_committed"||op=="source_committed_same_fs"
                ||op=="source_adopt_probed"||op=="source_cleared";
            const bool changesProject=op=="project_open"
                ||op=="project_save"||op=="project_close";
            if(state->epoch==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.master_generation
                      ==std::numeric_limits<std::uint64_t>::max()
               ||(changesSource&&state->identity.source_generation
                      ==std::numeric_limits<std::uint64_t>::max())
               ||(changesProject&&state->identity.project_generation
                      ==std::numeric_limits<std::uint64_t>::max()))
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
            if(changesSource)++nextIdentity.source_generation;
            if(changesProject)++nextIdentity.project_generation;
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
    // External SOURCE picker: ONE physical mutation lease spans the
    // entire SOURCE callback cascade. During a nested Project Open, the
    // already-held Project lease is reused without a second epoch.
    [[nodiscard]] ProjectSaveBatch source_commit_batch() const {
        return [weak=std::weak_ptr<Shared>{shared_}]()
            ->core::Result<std::shared_ptr<const void>> {
            using R=core::Result<std::shared_ptr<const void>>;
            auto state=weak.lock();
            if(!state)return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_SOURCE_BATCH_PRODUCT_OWNER_RETIRED"});
            if(std::this_thread::get_id()!=state->ownerThread
               ||state->retired.load(std::memory_order_acquire)
               ||!state->authority||!state->identity.valid()
               ||state->epoch==0||state->admitting)
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_SOURCE_BATCH_INVALID_OWNER_OR_REENTRANT"});
            if(state->projectOpenActive.load(std::memory_order_acquire)) {
                // ProjectSession owns the exact exclusive lease until
                // the child SOURCE adoption and its Qt callback return.
                return R::success(
                    std::static_pointer_cast<const void>(state));
            }
            if(state->sourceCommitActive.load(std::memory_order_acquire))
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_SOURCE_BATCH_REENTRANT"});
            if(state->epoch==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.master_generation
                      ==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.source_generation
                      ==std::numeric_limits<std::uint64_t>::max())
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_SOURCE_BATCH_EPOCH_EXHAUSTED"});
            state->admitting=true;
            struct Unwind final {
                bool* active;
                ~Unwind(){*active=false;}
            } unwind{&state->admitting};
            auto nextIdentity=state->identity;
            // A same-file, same-rate SOURCE re-selection is still a new
            // Product identity. This exact batch revokes once for SOURCE
            // and once for MASTER provenance, with one physical writer.
            ++nextIdentity.source_generation;
            ++nextIdentity.master_generation;
            const auto nextEpoch=state->epoch+1;
            auto physical=state->authority->try_begin_product_mutation(
                state->epoch,nextEpoch,nextIdentity,false);
            if(!physical)return R::failure(*physical.error());
            state->identity=nextIdentity;
            state->epoch=nextEpoch;
            try {
                auto token=std::make_shared<HeldSourceCommit>(
                    state,std::move(*physical.value()));
                state->sourceCommitActive.store(
                    true,std::memory_order_release);
                return R::success(
                    std::static_pointer_cast<const void>(std::move(token)));
            }catch(const std::bad_alloc&) {
                return R::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "M18_SOURCE_BATCH_TOKEN_ALLOCATION_FAILED"});
            }
        };
    }

    // Project SaveAs requires a physical exclusive mutation lease from
    // before snapshot capture through file readback and Qt publication.
    // It cannot use a one-shot status callback. Project OPEN remains
    // separately unresolved because it invokes several nested mutators.
    [[nodiscard]] ProjectSaveBatch project_save_batch() const {
        return [weak=std::weak_ptr<Shared>{shared_}]()
            ->core::Result<std::shared_ptr<const void>> {
            using R=core::Result<std::shared_ptr<const void>>;
            auto state=weak.lock();
            if(!state)return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_SAVE_BATCH_PRODUCT_OWNER_RETIRED"});
            if(std::this_thread::get_id()!=state->ownerThread
               ||state->retired.load(std::memory_order_acquire)
               ||!state->authority ||!state->identity.valid()
               ||state->epoch==0 ||state->admitting
               ||state->projectOpenActive.load(std::memory_order_acquire)
               ||state->sourceCommitActive.load(std::memory_order_acquire))
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_SAVE_BATCH_WRONG_OWNER_OR_REENTRANT"});
            if(state->epoch==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.master_generation
                      ==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.project_generation
                      ==std::numeric_limits<std::uint64_t>::max())
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_SAVE_BATCH_EPOCH_EXHAUSTED"});
            state->admitting=true;
            struct Unwind final {
                bool* active;
                ~Unwind(){*active=false;}
            } unwind{&state->admitting};
            auto nextIdentity=state->identity;
            // A new Project SaveAs destination changes Project identity
            // even if SOURCE samples and MASTER rate remain identical.
            ++nextIdentity.project_generation;
            ++nextIdentity.master_generation;
            const auto newEpoch=state->epoch+1;
            auto lease=state->authority->try_begin_product_mutation(
                state->epoch,newEpoch,nextIdentity,false);
            if(!lease)return R::failure(*lease.error());
            // Generation/consent change is irreversible even if the
            // subsequent filesystem operation fails: no stale export
            // identity may be resurrected by a failed SaveAs.
            state->epoch=newEpoch;
            state->identity=nextIdentity;
            try {
                std::shared_ptr<M18EpochCommitBarrier::MutationLease> held{
                    std::move(*lease.value())};
                return R::success(
                    std::static_pointer_cast<const void>(std::move(held)));
            } catch(const std::bad_alloc&) {
                return R::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "M18_SAVE_BATCH_LEASE_ALLOCATION_FAILED"});
            }
        };
    }

    // Exclusive Project Open must advance its source/project/master epoch
    // ONCE and retain the physical writer lease across SOURCE, MASTER,
    // GOLD, region, chain restoration and final Project Qt publication.
    // Internal Product callbacks are admitted only by the allowlist above.
    [[nodiscard]] ProjectSaveBatch project_open_batch() const {
        return [weak=std::weak_ptr<Shared>{shared_}]()
            ->core::Result<std::shared_ptr<const void>> {
            using R=core::Result<std::shared_ptr<const void>>;
            auto state=weak.lock();
            if(!state)return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_OPEN_BATCH_PRODUCT_OWNER_RETIRED"});
            if(std::this_thread::get_id()!=state->ownerThread
               ||state->retired.load(std::memory_order_acquire)
               ||!state->authority||!state->identity.valid()
               ||state->epoch==0||state->admitting
               ||state->projectOpenActive.load(std::memory_order_acquire)
               ||state->sourceCommitActive.load(std::memory_order_acquire))
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_OPEN_BATCH_INVALID_OWNER_OR_NESTED"});
            if(state->epoch==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.master_generation
                      ==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.project_generation
                      ==std::numeric_limits<std::uint64_t>::max()
               ||state->identity.source_generation
                      ==std::numeric_limits<std::uint64_t>::max())
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_OPEN_BATCH_EPOCH_EXHAUSTED"});
            state->admitting=true;
            struct Unwind final {
                bool* active;
                ~Unwind(){*active=false;}
            } unwind{&state->admitting};
            auto nextIdentity=state->identity;
            // Even if the project reopens the same path and same SOURCE,
            // its accepted Project/SOURCE owner epoch is new. The nested
            // SOURCE child reuses THIS writer lease and does not advance.
            ++nextIdentity.project_generation;
            ++nextIdentity.source_generation;
            ++nextIdentity.master_generation;
            const auto nextEpoch=state->epoch+1;
            auto physical=state->authority->try_begin_product_mutation(
                state->epoch,nextEpoch,nextIdentity,false);
            if(!physical)return R::failure(*physical.error());
            // An aborted/partial Project may never resurrect the previous
            // M17 export ticket or consent.
            state->epoch=nextEpoch;
            state->identity=nextIdentity;
            try {
                auto token=std::make_shared<HeldProjectOpen>(
                    state,std::move(*physical.value()));
                state->projectOpenActive.store(
                    true,std::memory_order_release);
                return R::success(
                    std::static_pointer_cast<const void>(std::move(token)));
            }catch(const std::bad_alloc&) {
                return R::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "M18_OPEN_BATCH_TOKEN_ALLOCATION_FAILED"});
            }
        };
    }

private:
    [[nodiscard]] static bool approved_source_child_operation(
        std::string_view operation) noexcept
    {
        return operation=="source_committed"
            ||operation=="switch_target"
            ||operation=="set_prepared"
            ||operation=="set_processed"
            ||operation=="clear_gold"
            ||operation=="invalidate_candidate"
            ||operation=="master_rate_change";
    }

    [[nodiscard]] static bool approved_project_open_child_operation(
        std::string_view operation) noexcept
    {
        // This list is intentionally narrow. A user edit or an unrelated
        // callback must not be smuggled through the Project batch.
        return operation=="source_adopt_probed"
            ||operation=="source_committed"
            ||operation=="switch_target"
            ||operation=="set_prepared"
            ||operation=="set_processed"
            ||operation=="set_gold"
            ||operation=="clear_gold"
            ||operation=="region_set"
            ||operation=="region_clear"
            ||operation=="loop_toggle"
            ||operation=="invalidate_candidate"
            ||operation=="master_rate_change";
    }

    [[nodiscard]] static core::Status reject(const char* message) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,message});
    }
    std::shared_ptr<Shared> shared_;
};

} // namespace rgsml::platform::windows::internal
