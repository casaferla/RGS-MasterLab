// L1-M22 strict PRIVATE stale-publication adversarial rehearsal.
// NOT the canonical application scheduler or an authoritative export gate.
#pragma once
#include "m22_request_plan_candidate.hpp"
#include "m22_m18_offline_export_port_candidate.hpp"

#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::m22_private {
struct M22PrivateCompletionTicket final {
    std::uint64_t writer_epoch{0};
    Intent exact_frozen_intent;
    std::string exact_verified_full_m17_owner;
    friend bool operator==(const M22PrivateCompletionTicket&,
                           const M22PrivateCompletionTicket&) = default;
};

// Bounded one-writer epoch prototype. The REAL application must obtain
// mutable snapshot provenance and exact M17 ownership from authoritative
// backend APIs. A GUI pointer, a user-displayed label, or a green CI is NOT
// acceptable evidence. This model never publishes any file itself.
class M22PrivatePublicationEpoch final {
public:
    [[nodiscard]] std::optional<M22PrivateCompletionTicket> begin(
        const RenderExecutionPlan& plan,
        std::string_view externally_verified_m17_owner)
    {
        std::scoped_lock lock(mutex_);
        if (externally_verified_m17_owner.empty() ||
            exhausted_ || epoch_==std::numeric_limits<std::uint64_t>::max()) {
            active_.reset();
            exhausted_=true;
            return std::nullopt;
        }
        ++epoch_;
        M22PrivateCompletionTicket next{
            epoch_,plan.request().intent(),std::string(externally_verified_m17_owner)};
        active_=next;
        return next;
    }
    void invalidate() noexcept {
        std::scoped_lock lock(mutex_);
        active_.reset();
        if (epoch_==std::numeric_limits<std::uint64_t>::max())
            exhausted_=true;
        else ++epoch_;
    }
    [[nodiscard]] bool still_current(
        const M22PrivateCompletionTicket& ticket,
        const Intent& latest_authoritative_intent,
        std::string_view latest_verified_m17_owner) const
    {
        std::scoped_lock lock(mutex_);
        return !exhausted_ && active_.has_value()
            && ticket.writer_epoch==epoch_
            && *active_==ticket
            && active_->exact_frozen_intent==latest_authoritative_intent
            && active_->exact_verified_full_m17_owner==latest_verified_m17_owner;
    }
private:
    mutable std::mutex mutex_;
    std::uint64_t epoch_{0};
    bool exhausted_{false};
    std::optional<M22PrivateCompletionTicket> active_;
};

// M18 already supports cancellation at block boundaries and prior to finish.
// This adapter rejects stale M22 epochs. The final publish race must be
// resolved by the future authoritative owner/transaction integration.
class M22PrivateM18StaleStop final :
    public rgsml::audio::internal::M18ExportStopSignal {
public:
    M22PrivateM18StaleStop(const M22PrivatePublicationEpoch& gate,
        M22PrivateCompletionTicket frozen) :
        gate_(gate), frozen_(std::move(frozen)) {}
    [[nodiscard]] bool cancelled() const noexcept override {
        return !gate_.still_current(
            frozen_,frozen_.exact_frozen_intent,
            frozen_.exact_verified_full_m17_owner);
    }
private:
    const M22PrivatePublicationEpoch& gate_;
    const M22PrivateCompletionTicket frozen_;
};
} // namespace rgsml::m22_private
