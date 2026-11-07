#pragma once
// M18 Qt-free Windows export worker, launched by the Product-owned
// private action after explicit user consent. It retains a COPY of the
// immutable M17 request (shared PCM owner, resource references and
// epoch lease factory), never a QObject*, QPointer or UI callback.
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <rgsml/core/error.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

namespace rgsml::platform::windows::internal {

class M18AsyncExportWorker final {
public:
    using Result=core::Result<TransactionalAudioExportResult>;
    M18AsyncExportWorker(const M18AsyncExportWorker&)=delete;
    M18AsyncExportWorker& operator=(const M18AsyncExportWorker&)=delete;

    // Launches exactly ONE detached, self-contained worker. The owning UI
    // handle can be destroyed without waiting for the long PCM pass; this
    // sets a cancellation flag and the worker owns the State lifetime.
    [[nodiscard]] static core::Result<std::shared_ptr<M18AsyncExportWorker>>
    launch(M18TransactionalAudioExportRequest request)
    {
        using R=core::Result<std::shared_ptr<M18AsyncExportWorker>>;
        try {
            auto state=std::make_shared<State>();
            state->total_frames.store(
                static_cast<std::uint64_t>(
                    request.payload().frame_count().value()),
                std::memory_order_release);
            auto handle=std::shared_ptr<M18AsyncExportWorker>(
                new M18AsyncExportWorker(state));
            std::thread workerThread([state,request=std::move(request)]() mutable {
                // No Qt calls and no application source/MASTER object read.
                // In particular, never spin/wait on the owning GUI thread.
                const auto cancelled=[state]() noexcept {
                    return state->cancelled.load(std::memory_order_acquire);
                };
                const auto progress=[state](
                    std::uint64_t encoded,std::uint64_t total) noexcept {
                    state->total_frames.store(
                        total,std::memory_order_release);
                    state->encoded_frames.store(
                        encoded,std::memory_order_release);
                };
                try {
                    auto completed=TransactionalAudioExporter::export_new_file(
                        request,cancelled,progress);
                    {
                        std::lock_guard lock(state->mutex);
                        state->outcome.emplace(std::move(completed));
                    }
                }catch(...) {
                    // A worker exception cannot crash Qt or silently
                    // report success. The transactional writer must also
                    // remove any partial candidate on its own error paths.
                    try {
                        std::lock_guard lock(state->mutex);
                        state->outcome.emplace(Result::failure(core::Error{
                            core::ErrorCode::IoFailure,
                            "M18_ASYNC_EXPORT_WORKER_UNEXPECTED_EXCEPTION"}));
                    }catch(...) {state->unexpected_failure.store(
                        true,std::memory_order_release);}
                }
                state->finished.store(true,std::memory_order_release);
            });
            try {workerThread.detach();}
            catch(...) {
                // Never let a joinable std::thread destructor call
                // std::terminate if the OS rejects detach().
                state->cancelled.store(true,std::memory_order_release);
                workerThread.join();
                throw;
            }
            return R::success(std::move(handle));
        }catch(const std::bad_alloc&) {
            return R::failure(core::Error{
                core::ErrorCode::IoFailure,
                "M18_ASYNC_EXPORT_WORKER_ALLOCATION_FAILED"});
        }catch(const std::system_error&) {
            return R::failure(core::Error{
                core::ErrorCode::IoFailure,
                "M18_ASYNC_EXPORT_THREAD_CREATE_FAILED"});
        }
    }

    ~M18AsyncExportWorker() {cancel();}
    void cancel() noexcept {
        state_->cancelled.store(true,std::memory_order_release);
    }
    [[nodiscard]] bool cancellation_requested() const noexcept {
        return state_->cancelled.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool finished() const noexcept {
        return state_->finished.load(std::memory_order_acquire);
    }
    // Encoded PCM frames only. A fully encoded WAV may still fail SHA,
    // True-Peak, epoch permission or destination readback. Do NOT label
    // 100% encoded as delivered until finished() + success outcome.
    [[nodiscard]] std::uint64_t encoded_frames() const noexcept {
        return state_->encoded_frames.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::uint64_t total_frames() const noexcept {
        return state_->total_frames.load(std::memory_order_acquire);
    }
    // Only a complete outcome is observable. No partially written file
    // may be treated as successful delivery or a valid MASTER reference.
    [[nodiscard]] std::optional<Result> result_if_finished() const {
        if(!finished())return std::nullopt;
        std::lock_guard lock(state_->mutex);
        if(state_->outcome)return state_->outcome;
        if(state_->unexpected_failure.load(std::memory_order_acquire))
            return Result::failure(core::Error{
                core::ErrorCode::IoFailure,
                "M18_ASYNC_EXPORT_RESULT_UNAVAILABLE"});
        return std::nullopt;
    }

private:
    struct State final {
        std::mutex mutex;
        std::atomic_bool cancelled{false};
        std::atomic_bool finished{false};
        std::atomic_bool unexpected_failure{false};
        std::atomic<std::uint64_t> encoded_frames{0};
        std::atomic<std::uint64_t> total_frames{0};
        std::optional<Result> outcome;
    };
    explicit M18AsyncExportWorker(std::shared_ptr<State> state)
        :state_(std::move(state)) {}
    std::shared_ptr<State> state_;
};

} // namespace rgsml::platform::windows::internal
