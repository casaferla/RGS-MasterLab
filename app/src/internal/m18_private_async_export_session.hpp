#pragma once
// M18 PRIVATE PREPRODUCTION — no QML exposure, no Product registration.
// A future Qt controller can own this nonblocking, one-job state machine
// once every authoritative upstream mutation shares ONE Product epoch.
// Do not equate encoded frames with successful WAV publication.
#include "m18_private_async_export_handoff.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cstdint>
#include <memory>
#include <optional>
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
        if(job_)job_->cancel();
    }

    [[nodiscard]] Snapshot snapshot() const {
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
        } else {
            current.phase=job_->cancellation_requested()
                ?Phase::CANCELLED :Phase::FAILED;
            if(completed &&completed->error())
                current.failure_code=completed->error()->code();
            else current.failure_code=core::ErrorCode::IoFailure;
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
