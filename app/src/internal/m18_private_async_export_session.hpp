#pragma once
// M18 private Product-owned nonblocking export session. main.cpp owns
// the outer action/ViewModel; this class never crosses the Qt thread.
// One detached Qt-free worker owns immutable PCM and a revocable epoch.
// Encoded frames are NOT successful WAV publication; only verified
// readback, terminal status and user ACK release the export state.
#include "m18_private_async_export_handoff.hpp"
#include "m18_export_format_policy.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace rgsml::app::internal {

class M18PrivateAsyncExportSession final {
public:
    enum class Phase : std::uint8_t {
        IDLE, ENCODING_OR_VERIFYING, CANCELLING,
        VERIFIED_DELIVERED, FAILED, CANCELLED
    };
    struct Snapshot final {
        Phase phase{Phase::IDLE};
        std::uint64_t encoded_frames{0};
        std::uint64_t total_frames{0};
        std::optional<core::ErrorCode> failure_code;
        // Present only after the transactional Windows exporter finished
        // WAV readback and returned a verified, published destination.
        std::optional<std::uint64_t> verified_file_size_bytes;
        std::string verified_encoded_sha256;
    };

    M18PrivateAsyncExportSession()
        :owner_thread_(std::this_thread::get_id()) {}
    M18PrivateAsyncExportSession(const M18PrivateAsyncExportSession&)=delete;
    M18PrivateAsyncExportSession&
    operator=(const M18PrivateAsyncExportSession&)=delete;
    ~M18PrivateAsyncExportSession() {cancel();}

    [[nodiscard]] core::Status begin(
        const TerminalPrivateProductSession& session,
        const SourceSelectionViewModel& source,
        const MasterOutputRateState& rate,
        const TerminalCandidateGate& gate,
        const platform::windows::internal::
            M18ProductEpochTicketAuthority::Ticket& ticket,
        core::ResourceReference selectedSource,
        core::ResourceReference destination,
        audio::WavSampleFormat format,
        std::optional<std::uint64_t> seed,
        double ceilingDbTp)
    {
        if(std::this_thread::get_id()!=owner_thread_)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_ASYNC_SESSION_BEGIN_REQUIRES_PRODUCT_OWNER_THREAD"});
        if(job_)return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_ASYNC_SESSION_ONE_EXPORT_AT_A_TIME"});
        // Refuse an invalid PCM24/F32 delivery choice before obtaining a
        // Product source snapshot or spawning an unbounded file worker.
        const auto choice=validate_m18_export_format(format,seed,ceilingDbTp);
        if(!choice)return choice;
        auto launched=launch_m18_async_export_from_live_product(
            session,source,rate,gate,ticket,
            std::move(selectedSource),std::move(destination),
            format,seed,ceilingDbTp);
        if(!launched)return core::Status::failure(*launched.error());
        job_=std::move(*launched.value());
        return core::Status::success();
    }

    // Nonblocking. Product teardown or a Cancel command can request
    // revocation without waiting for the WAV codec or filesystem.
    void cancel() noexcept {
        // job_ is a Product-owner-only shared_ptr, not an atomic handle.
        // A detached worker must use its immutable cancel token instead
        // of racing the Qt acknowledgement/reset of the live session.
        if(std::this_thread::get_id()!=owner_thread_)return;
        if(job_)job_->cancel();
    }

    [[nodiscard]] Snapshot snapshot() const {
        // The progress/cancellation atomics belong to M18AsyncExportWorker,
        // but access to job_ is Product-owner-thread serialized. Reject a
        // foreign reader before touching that non-atomic shared_ptr.
        if(std::this_thread::get_id()!=owner_thread_) {
            Snapshot denied;
            denied.phase=Phase::FAILED;
            denied.failure_code=core::ErrorCode::InvalidState;
            return denied;
        }
        if(!job_)return {};
        Snapshot current;
        current.encoded_frames=job_->encoded_frames();
        current.total_frames=job_->total_frames();
        if(!job_->finished()) {
            current.phase=job_->cancellation_requested()
                ?Phase::CANCELLING :Phase::ENCODING_OR_VERIFYING;
            return current;
        }
        const auto completed=job_->result_if_finished();
        if(completed && *completed) {
            current.phase=Phase::VERIFIED_DELIVERED;
            current.verified_file_size_bytes=
                completed->value()->exact_file_size_bytes();
            current.verified_encoded_sha256=
                completed->value()->encoded_file_sha256();
        } else {
            // Cancellation is NOT permission to hide an unrelated export
            // failure. Only the specific exporter cancellation evidence
            // is presented as CANCELLED; a failed SHA, destination race,
            // source revocation or I/O error remains FAILED even if the
            // user clicks Cancel after it has already happened.
            const auto* error=completed?completed->error():nullptr;
            if(error)current.failure_code=error->code();
            else current.failure_code=core::ErrorCode::IoFailure;
            constexpr std::string_view cancelledPrefix{
                "m18_export_cancelled_"};
            const auto message=error
                ?std::string_view{error->message()}:std::string_view{};
            const bool confirmedCancelled=
                job_->cancellation_requested()
                &&message.starts_with(cancelledPrefix);
            current.phase=confirmedCancelled
                ?Phase::CANCELLED :Phase::FAILED;
        }
        return current;
    }

    // A finished result must be ACKNOWLEDGED before any new request
    // can start. This prevents losing the verified completion evidence
    // merely because the Export button was clicked a second time.
    [[nodiscard]] core::Status acknowledge_finished() {
        if(std::this_thread::get_id()!=owner_thread_ ||!job_
           ||!job_->finished())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_ASYNC_SESSION_ACK_ONLY_FINISHED_ON_OWNER_THREAD"});
        job_.reset();
        return core::Status::success();
    }

private:
    const std::thread::id owner_thread_;
    std::shared_ptr<platform::windows::internal::M18AsyncExportWorker> job_;
};

} // namespace rgsml::app::internal
