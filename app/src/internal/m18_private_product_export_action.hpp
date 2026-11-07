#pragma once
// M18 private Product action, constructed in app/src/main.cpp AFTER the
// complete SOURCE/Project/M17 epoch binding. Only the typed QObject
// ViewModel is exposed to QML: this class is not Q_INVOKABLE and never
// grants user consent without an exact current verified M17 Master.
// A running/finished worker keeps a single explicit ticket until ACK.
#include "m18_product_complete_binding.hpp"
#include "m18_private_async_export_session.hpp"
#include "m18_export_format_policy.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <QThread>
#include <optional>
#include <cstdint>
#include <utility>

namespace rgsml::app::internal {

class M18PrivateProductExportAction final {
    using Binder=M18ProductCompleteBinding;
    using Fanout=Binder::Fanout;
    using Ticket=platform::windows::internal::
        M18ProductEpochTicketAuthority::Ticket;
public:
    M18PrivateProductExportAction(
        Binder::Controllers controllers,const Fanout& fanout)
        :controllers_(controllers),fanout_(&fanout) {}
    M18PrivateProductExportAction(const M18PrivateProductExportAction&)=delete;
    M18PrivateProductExportAction&
    operator=(const M18PrivateProductExportAction&)=delete;

    // Read-only Product-to-UI eligibility, without minting consent.
    // Keep the visible Export affordance as strict as the actual
    // SOURCE/chain/Project/M17 authorizer, including an unconsented epoch.
    [[nodiscard]] bool can_begin_verified_export() const {
        return QThread::currentThread()==controllers_.session.thread()
            &&!ticket_
            &&Binder::can_offer_verified_master(controllers_,*fanout_);
    }

    // A real user choice is a single event, NOT a sticky global boolean.
    // Reject invalid formats before revoking Product export consent.
    // SOURCE is obtained exclusively from the guarded Product selector,
    // never from a caller-supplied path that merely shares a filename.
    [[nodiscard]] core::Status begin(
        core::ResourceReference destination,
        audio::WavSampleFormat format,
        std::optional<std::uint64_t> pcm24DitherSeed,
        double ceilingDbTp,bool confirmedByUser)
    {
        if(QThread::currentThread()!=controllers_.session.thread())
            return deny("M18_PRODUCT_EXPORT_START_REQUIRES_QT_OWNER");
        if(ticket_)return deny("M18_PRODUCT_EXPORT_ACK_BEFORE_RESTART");
        const auto formatCheck=validate_m18_export_format(
            format,pcm24DitherSeed,ceilingDbTp);
        if(!formatCheck)return formatCheck;
        // The verifier's dBTP ceiling is the EXACT accepted M17 limiter
        // control. An independent export-dialog value would silently
        // certify a different master, even if the encoded WAV is valid.
        if(controllers_.upstream.terminal.bypassed()
           ||controllers_.upstream.terminal.parameters().ceiling_db_tp()
                 !=ceilingDbTp)
            return deny("M18_PRODUCT_EXPORT_CEILING_NOT_CURRENT_M17");
        // Check destination before minting a consent Ticket: a bad output
        // path or same-source logical alias is not an export transaction.
        const auto* source=controllers_.upstream.source.source_resource();
        if(!source||!source->reference().permissions().can_read()
           ||!destination.permissions().can_write()
           ||source->reference().same_resource_identity(destination))
            return deny("M18_PRODUCT_EXPORT_SOURCE_OR_DESTINATION_INVALID");
        auto authorized=Binder::authorize_verified_master_for_user(
            controllers_,*fanout_,confirmedByUser);
        if(!authorized)return core::Status::failure(*authorized.error());
        ticket_.emplace(std::move(*authorized.value()));
        const auto started=session_.begin(
            controllers_.session,controllers_.upstream.source,
            controllers_.upstream.rate,controllers_.candidates,*ticket_,
            source->reference(),std::move(destination),format,
            pcm24DitherSeed,ceilingDbTp);
        if(!started) {
            // A failed launch has no worker commit lease: withdraw only
            // the consent minted for THIS Product and allow retry.
            const auto withdrawn=fanout_->withdraw_completed_export_consent(
                *ticket_);
            if(!withdrawn)return withdrawn;
            ticket_.reset();
            return started;
        }
        return core::Status::success();
    }

    // An async Cancel never makes a terminal file claim and never revokes
    // the worker's commit authority mid-publication. Product ACK is what
    // releases the owner ticket after the worker reports terminal state.
    void cancel() noexcept {
        if(QThread::currentThread()!=controllers_.session.thread())return;
        if(!can_cancel())return;
        session_.cancel();
    }

    [[nodiscard]] bool can_cancel() const {
        // Ticket optional is owner-thread serialized, not an atomic
        // cross-thread permission bit. Check affinity BEFORE reading it.
        if(QThread::currentThread()!=controllers_.session.thread())
            return false;
        return ticket_&&session_.snapshot().phase
            ==M18PrivateAsyncExportSession::Phase::ENCODING_OR_VERIFYING;
    }

    [[nodiscard]] bool can_acknowledge() const {
        if(QThread::currentThread()!=controllers_.session.thread()
           ||!ticket_)return false;
        const auto phase=session_.snapshot().phase;
        return phase==M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED
            ||phase==M18PrivateAsyncExportSession::Phase::FAILED
            ||phase==M18PrivateAsyncExportSession::Phase::CANCELLED;
    }

    [[nodiscard]] M18PrivateAsyncExportSession::Snapshot snapshot() const {
        return session_.snapshot();
    }

    [[nodiscard]] bool has_unacknowledged_export() const noexcept {
        if(QThread::currentThread()!=controllers_.session.thread())
            return false;
        return ticket_.has_value();
    }

    [[nodiscard]] core::Status acknowledge() {
        if(QThread::currentThread()!=controllers_.session.thread()
           ||!ticket_)return deny("M18_PRODUCT_EXPORT_NO_FINISHED_TICKET");
        const auto finished=session_.snapshot();
        using Phase=M18PrivateAsyncExportSession::Phase;
        if(finished.phase!=Phase::VERIFIED_DELIVERED
           &&finished.phase!=Phase::FAILED
           &&finished.phase!=Phase::CANCELLED)
            return deny("M18_PRODUCT_EXPORT_WORKER_NOT_TERMINAL");
        // Revoke consent BEFORE discarding the verified result. If a
        // deferred commit lease is still active, fail the ACK without
        // losing the terminal evidence and allow a later safe retry.
        const auto released=fanout_->withdraw_completed_export_consent(
            *ticket_);
        if(!released) {
            // An already accepted newer Product mutation irrevocably
            // withdrew the previous ticket. A stale Ticket cannot be
            // used for a second worker, even after acknowledgement.
            const auto current=fanout_->current_owner_snapshot();
            if(!current ||(current->epoch==ticket_->epoch()
                           &&current->identity==ticket_->identity()))
                return released;
        }
        const auto ack=session_.acknowledge_finished();
        if(!ack)return ack;
        ticket_.reset();
        return core::Status::success();
    }
private:
    [[nodiscard]] static core::Status deny(const char* code) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,code});
    }

    Binder::Controllers controllers_;
    const Fanout* fanout_;
    M18PrivateAsyncExportSession session_;
    std::optional<Ticket> ticket_;
};

} // namespace rgsml::app::internal
