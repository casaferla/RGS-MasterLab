#pragma once
// M18 PREPRODUCTION. One coordinator installs all existing Product
// mutation interlocks atomically on a single Qt owner BEFORE consent.
// It DOES NOT expose an Export button, mint a worker Ticket, or register
// itself in main.cpp. Installation must precede all future authorization.
#include "m18_product_upstream_binding.hpp"
#include "m18_product_private_session_binding.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QThread>

namespace rgsml::app::internal {

struct M18ProductCompleteBinding final {
    using Upstream=M18ProductUpstreamBinding;
    using Fanout=platform::windows::internal::M18OwnerMutationFanout;
    struct Controllers final {
        Upstream::Controllers upstream;
        TerminalPrivateProductSession& session;
        TerminalCandidateGate& candidates;
    };

    [[nodiscard]] static bool preflight(
        const Controllers& c,const Fanout& fanout) noexcept
    {
        // Refuse to install interlocks once Product has issued any
        // explicit export permission or a worker commit. Enabling guards
        // after a Ticket exists could preserve stale M17 PCM as current.
        return fanout.can_install_full_product_guards()
            &&Upstream::owner_thread_current(c.upstream)
            &&QThread::currentThread()==c.session.thread()
            &&c.session.thread()==c.upstream.source.thread()
            &&Upstream::unbound(c.upstream)
            &&c.upstream.source.m18_checked_source_committed_installed()
            &&M18ProductPrivateSessionBinding::can_bind(
                c.session,c.candidates);
    }

    [[nodiscard]] static bool installed(const Controllers& c) noexcept
    {
        return Upstream::installed(c.upstream)
            &&c.session.m18_private_mutation_admission_installed()
            &&c.candidates.m18_mutation_admission_installed();
    }

    [[nodiscard]] static core::Status bind(
        Controllers c,const Fanout& fanout)
    {
        if(!preflight(c,fanout))
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_COMPLETE_PRODUCT_BIND_REQUIRES_FRESH_UNCONSENTED_OWNER"});
        // All checks and the subsequent setters execute synchronously on
        // ONE Product Qt thread, with no event-loop pumps or Qt signals.
        // Install the upstream batch before enabling the terminal bridge:
        // a candidate cannot be exportable while any SOURCE/Project path
        // remains without the same shared mutation authority.
        const auto first=Upstream::bind(c.upstream,fanout);
        if(!first)return first;
        const auto second=M18ProductPrivateSessionBinding::bind(
            c.session,c.candidates,fanout);
        if(!second) {
            // No ticket or worker is possible while fresh unconsented
            // installation is in progress. Rollback is private and legal
            // ONLY here; no public teardown that bypasses a worker lease.
            Upstream::rollback_partial(c.upstream);
            return second;
        }
        if(!installed(c)) {
            c.candidates.set_private_m18_mutation_admission({});
            c.session.set_private_m18_mutation_admission({});
            Upstream::rollback_partial(c.upstream);
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_COMPLETE_PRODUCT_BIND_ROLLED_BACK_INCOMPLETE_AUTHORITY"});
        }
        return core::Status::success();
    }
};

} // namespace rgsml::app::internal
