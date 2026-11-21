#pragma once
// M21 private Product CPU executor. ONE detached worker maximum.
// No QObject, Qt event loop, controller references or QML ownership
// crosses the thread boundary. The worker captures an immutable Product
// request value. Results are copied to the Qt owner only by take_finished;
// the caller MUST perform live M18 epoch/source/resource acceptance there.
// Detaching is lifetime-safe because the worker owns its shared State;
// destroying the executor never blocks the GUI or accesses a dead QObject.
// This class does NOT qualify a FeatureStatus or authorize QML numbers.
#include "m21_metrics_comparison_candidate.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::app::internal {
// A private non-owning identity for a successfully launched render analysis.
// M18 epoch/resource revision alone cannot distinguish two actual immutable
// PREPARED/PROCESSED PCM owners published during the SAME physical epoch.
// Do not keep old PCM alive solely to suppress duplicate analysis.
class M21PrivateRenderAttempt final {
public:
    template<class T>
    [[nodiscard]] bool matches(
        std::string_view key,
        const std::shared_ptr<const T>& owner) const noexcept
    {
        const auto prior=previous_.lock();
        return prior &&owner &&key_==key &&prior.get()==owner.get();
    }

    template<class T>
    void remember(std::string key,
                  const std::shared_ptr<const T>& owner) noexcept
    {
        key_=std::move(key);
        previous_=owner;
    }

    void reset() noexcept {
        previous_.reset();
        key_.clear();
    }
private:
    std::string key_;
    std::weak_ptr<const void> previous_;
};
class M21PrivateSingleFlight final {
public:
    using Snapshot=m21_private::Snapshot;
    using Task=std::function<std::optional<Snapshot>()>;
    struct Completion {
        std::uint64_t request_id{};
        std::optional<Snapshot> result{};
    };

    // This check is independent of all Qt/Product pointers. It prevents
    // a completion accidentally identified as another role or generation
    // from ever reaching a more permissive inbox acceptance path.
    // It does NOT replace the subsequent live M18 owner/realization gate.
    [[nodiscard]] static bool matches_expected(
        const Completion& completed,std::uint64_t request_id,
        m21_private::Column column,std::uint64_t generation) noexcept
    {
        return request_id!=0 &&generation!=0
            &&completed.request_id==request_id
            &&completed.result
            &&completed.result->column==column
            &&completed.result->generation_token==generation
            &&completed.result->lifecycle==m21_private::Lifecycle::current;
    }

    // SOURCE provider and locator may contain separator characters.
    // Length framing makes attempted-job identity collision-free for
    // arbitrary two-component strings; these keys are private and never
    // serialized as Product identifiers or checksums.
    static void append_identity_field(std::string& out,
                                      std::string_view field) {
        out+=std::to_string(field.size());
        out+=':';
        out.append(field);
        out+='|';
    }

    M21PrivateSingleFlight() : owner_(std::this_thread::get_id()) {}
    M21PrivateSingleFlight(const M21PrivateSingleFlight&)=delete;
    M21PrivateSingleFlight&
    operator=(const M21PrivateSingleFlight&)=delete;

    [[nodiscard]] bool start(std::uint64_t request_id,Task task) {
        if(std::this_thread::get_id()!=owner_ ||state_
           ||request_id==0 ||!task)
            return false;
        auto state=std::make_shared<State>();
        try {
            // All captures and the task move into the worker's closure.
            // The detached worker only writes to its independently
            // reference-counted State, never to *this or a QObject.
            std::thread([state,task=std::move(task)]() mutable {
                try {
                    auto value=task();
                    {
                        std::lock_guard lock(state->mutex);
                        state->result=std::move(value);
                    }
                }catch(...) {
                    // Fail closed: a failed analysis is never a measurement.
                }
                state->finished.store(true,std::memory_order_release);
            }).detach();
        }catch(...) {
            return false; // no worker admitted
        }
        active_request_id_=request_id;
        state_=std::move(state);
        return true;
    }

    [[nodiscard]] bool busy() const noexcept {
        return std::this_thread::get_id()==owner_ &&state_!=nullptr;
    }
    [[nodiscard]] bool finished() const noexcept {
        return std::this_thread::get_id()==owner_ &&state_
            &&state_->finished.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::optional<Completion> take_finished() {
        if(!finished())return std::nullopt;
        Completion out;
        out.request_id=active_request_id_;
        {
            std::lock_guard lock(state_->mutex);
            out.result=std::move(state_->result);
        }
        active_request_id_=0;
        state_.reset();
        return out;
    }
private:
    struct State {
        std::mutex mutex;
        std::optional<Snapshot> result{};
        std::atomic<bool> finished{false};
    };
    const std::thread::id owner_;
    std::uint64_t active_request_id_{};
    std::shared_ptr<State> state_{};
};
} // namespace rgsml::app::internal
