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

    // SOURCE/Project mutations may change Fs, accepted MANUAL chain
    // revision and Project rate revision. Bind these current model values
    // to the single Product authority AFTER the physical writer batch has
    // completed. This is NOT consent and never permits an old M17 ticket.
    [[nodiscard]] static core::Status reconcile_after_product_mutation(
        const Controllers& c,const Fanout& fanout)
    {
        if(!Upstream::owner_thread_current(c.upstream)
           ||QThread::currentThread()!=c.session.thread()
           ||!c.candidates.m18_owner_thread_current()
           ||!installed(c))
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_RECONCILE_REQUIRES_COMPLETE_QT_PRODUCT"});
        const auto* source=c.upstream.source.source_resource();
        const auto& rate=c.upstream.rate;
        const auto snapshot=fanout.current_owner_snapshot();
        if(!source||!source->reference().permissions().can_read()
           ||!snapshot||!rate.has_supported_master_rate()
           ||rate.rate_revision()==0)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_RECONCILE_NEEDS_CURRENT_SOURCE_AND_PROJECT_MASTER"});
        return fanout.reconcile_current_rate_and_chain(
            *snapshot,c.upstream.chain.chain().revision(),
            static_cast<std::uint32_t>(rate.master_rate_hz()),
            rate.rate_revision());
    }

    // A legitimate user Export click is only eligible when the EXACT
    // immutable verified M17 Master still matches the live SOURCE, the
    // accepted M11–15 chain, the Project output rate/revision, and all
    // previously installed Product gates. This cannot be simulated with
    // a synthetic ticket or initiated from a detached worker.
    [[nodiscard]] static core::Result<
        platform::windows::internal::M18ProductEpochTicketAuthority::Ticket>
    authorize_verified_master_for_user(
        const Controllers& c,const Fanout& fanout,bool explicitUserChoice)
    {
        using Ticket=platform::windows::internal::
            M18ProductEpochTicketAuthority::Ticket;
        using R=core::Result<Ticket>;
        if(!explicitUserChoice
           ||!Upstream::owner_thread_current(c.upstream)
           ||QThread::currentThread()!=c.session.thread()
           ||!c.candidates.m18_owner_thread_current()
           ||!installed(c))
            return R::failure(core::Error{
                core::ErrorCode::AccessDenied,
                "M18_EXPORT_CONSENT_REQUIRES_ONE_COMPLETE_PRODUCT_BINDING"});
        const auto snapshot=fanout.current_owner_snapshot();
        if(!snapshot||!snapshot->identity.valid())
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_EXPORT_MISSING_CURRENT_PRODUCT_EPOCH"});
        const auto& id=snapshot->identity;
        const auto& rate=c.upstream.rate;
        const auto selected=core::SampleRate::create(rate.master_rate_hz());
        const auto* source=c.upstream.source.source_resource();
        if(!rate.has_supported_master_rate()||!selected
           ||!source||!source->reference().permissions().can_read()
           ||id.master_rate_hz!=static_cast<std::uint32_t>(
               rate.master_rate_hz())
           ||id.master_rate_revision!=rate.rate_revision()
           ||id.chain_revision!=c.upstream.chain.chain().revision())
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_EXPORT_SOURCE_CHAIN_OR_RATE_NO_LONGER_CURRENT"});
        const auto verified=c.session.capture_m18_verified_stage_for_worker(
            *selected.value());
        if(!verified||verified!=c.candidates.staged()
           ||!verified->rendered.terminal.checked.final_check
           ||verified->rendered.terminal.checked.terminal.bypassed
           ||verified->rendered.manual_chain_revision!=id.chain_revision
           ||verified->rendered.terminal.terminal_chain_revision
                !=c.upstream.terminal.chain().revision()
           ||verified->rendered.terminal.limiter_instance_id
                !=c.upstream.terminal.instance_id()
           ||verified->accepted_terminal_parameter_revision
                !=c.upstream.terminal.parameter_revision()
           ||verified->accepted_ceiling_db_tp
                !=c.upstream.terminal.parameters().ceiling_db_tp()
           ||c.upstream.terminal.bypassed()
           ||verified->rendered.output_rate!=*selected.value())
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_EXPORT_NO_EXACT_CURRENT_VERIFIED_M17_MASTER"});
        // No Qt signals or further Product writes between this verified
        // stage proof and the owner-only atomic consent/epoch transition.
        // The returned Ticket is immutable. A later physical SOURCE or
        // Project mutation withdraws it through this SAME fanout.
        return fanout.capture_after_explicit_verified_consent(
            id,explicitUserChoice);
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
