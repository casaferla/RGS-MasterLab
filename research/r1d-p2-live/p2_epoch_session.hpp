#pragma once
// R1d-P2 I1 RESEARCH ONLY. Transport epochs and latency-preserving bypass.
// This is NOT a product IModule. begin_epoch/cancel_epoch must be invoked by
// the host OFF the audio thread, with callbacks quiesced (single-writer barrier).
// The host owns device clock, restart crossfade, output seek mapping and M17.
#include "p2_callback_engine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <utility>

namespace rgsml::research::r1d_p2 {
enum class EpochStatus : std::uint8_t {
    ok, stale_epoch, invalid_epoch, not_prepared, invalid_configuration,
    engine_fault, invalid_pcm, frame_limit, output_size_mismatch,
    already_finished, wrong_lifecycle
};

// One and only one active epoch per session. No call may overlap with another.
// The host's epoch number must be monotonically increasing. If an epoch is
// abandoned, pending delayed PCM is permanently discarded, never replayed
// into the next source. This deliberate reset can produce an audible jump:
// the host MUST mute or crossfade during seek/loop/reconfigure. No click-free
// guarantee or runtime checkpoint claim is made by this research adapter.
class EpochSession final {
public:
    EpochSession() = default;
    EpochSession(const EpochSession&) = delete;
    EpochSession& operator=(const EpochSession&) = delete;
    EpochSession(EpochSession&&) = delete;
    EpochSession& operator=(EpochSession&&) = delete;

    // Off-thread only, with no callback in flight. Prepares the next instance
    // before publishing it; failed configuration does NOT destroy current audio.
    [[nodiscard]] EpochStatus begin_epoch(std::uint64_t epoch,
                                          Configuration config,
                                          bool bypassed) {
        if (epoch == 0 || epoch <= last_epoch_) return EpochStatus::invalid_epoch;
        auto candidate = std::unique_ptr<CallbackEngine>(new (std::nothrow) CallbackEngine());
        if (!candidate) return EpochStatus::invalid_configuration;
        const auto prepared = candidate->prepare(config);
        if (prepared != Status::ok) {
            last_engine_status_ = prepared;
            return EpochStatus::invalid_configuration;
        }
        const std::size_t latency = candidate->latency_frames();
        if (latency >= bypass_fifo_.size()) return EpochStatus::invalid_configuration;
        const bool abandoned = active_ && !ended_;
        engine_ = std::move(candidate); // destroy previous instance off-thread
        config_ = config;
        bypassed_ = bypassed;
        latency_ = latency;
        last_epoch_ = epoch;
        active_ = true;
        ended_ = false;
        faulted_ = false;
        abandoned_previous_ = abandoned;
        input_count_ = 0;
        output_count_ = 0;
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        last_engine_status_ = Status::ok;
        return EpochStatus::ok;
    }

    // Called off the audio thread after callbacks have been stopped; dropping
    // unconsumed tail is intentional and caller must mark a transport cut.
    [[nodiscard]] EpochStatus cancel_epoch(std::uint64_t epoch) noexcept {
        if (!active_) return EpochStatus::not_prepared;
        if (epoch != last_epoch_) return EpochStatus::stale_epoch;
        abandoned_previous_ = !ended_;
        engine_.reset();
        active_ = false;
        ended_ = false;
        head_ = tail_ = count_ = 0;
        return EpochStatus::ok;
    }

    [[nodiscard]] std::uint64_t epoch() const noexcept { return last_epoch_; }
    [[nodiscard]] std::size_t latency_frames() const noexcept { return latency_; }
    [[nodiscard]] bool bypassed() const noexcept { return bypassed_; }
    [[nodiscard]] bool is_active() const noexcept { return active_; }
    [[nodiscard]] bool previous_abandoned() const noexcept { return abandoned_previous_; }
    [[nodiscard]] std::uint64_t input_frames() const noexcept { return input_count_; }
    [[nodiscard]] std::uint64_t output_frames() const noexcept { return output_count_; }
    [[nodiscard]] Status last_engine_status() const noexcept { return last_engine_status_; }

    [[nodiscard]] EpochStatus process(std::uint64_t epoch,
                                      std::span<const Frame> input,
                                      std::span<Frame> output) noexcept {
        if (!active_) return silence(output, EpochStatus::not_prepared);
        if (epoch != last_epoch_) return silence(output, EpochStatus::stale_epoch);
        if (ended_ || faulted_) return silence(output, EpochStatus::wrong_lifecycle);
        if (input.size() != output.size()) return fault(output, EpochStatus::output_size_mismatch);
        if (input.size() > 4096 || input_count_ > UINT64_MAX-input.size())
            return fault(output, EpochStatus::frame_limit);
        if (!bypassed_) {
            const Status rc = engine_->process(input,output);
            if (rc != Status::ok) {
                last_engine_status_ = rc;
                faulted_ = true;
                return EpochStatus::engine_fault;
            }
        } else {
            // Preflight entire block before mutating the bypass delay line.
            for(const auto& sample : input) {
                for(int c=0;c<config_.channels;++c)
                    if (!std::isfinite(sample[c]) || std::abs(sample[c]) > 1048576.0)
                        return fault(output, EpochStatus::invalid_pcm);
            }
            for(std::size_t i=0;i<input.size();++i) {
                Frame frame = input[i];
                if (config_.channels == 1) frame[1] = 0.;
                if (count_ == bypass_fifo_.size()) return fault(output, EpochStatus::frame_limit);
                bypass_fifo_[tail_] = frame;
                tail_ = (tail_ + 1) % bypass_fifo_.size();
                ++count_;
                if (output_count_ < latency_) output[i] = Frame{};
                else {
                    output[i] = bypass_fifo_[head_];
                    head_ = (head_ + 1) % bypass_fifo_.size();
                    --count_;
                }
                ++output_count_;
            }
        }
        input_count_ += input.size();
        if (!bypassed_) output_count_ += input.size();
        return EpochStatus::ok;
    }

    // EOS is a separate explicit step; no hidden output frames or M16 analyzer
    // bridge zeros. Requires exactly latency_frames() output slots.
    [[nodiscard]] EpochStatus finish(std::uint64_t epoch,
                                     std::span<Frame> tail) noexcept {
        if (!active_) return silence(tail, EpochStatus::not_prepared);
        if (epoch != last_epoch_) return silence(tail, EpochStatus::stale_epoch);
        if (ended_) return silence(tail, EpochStatus::already_finished);
        if (faulted_) return silence(tail, EpochStatus::wrong_lifecycle);
        if (tail.size() != latency_) return fault(tail, EpochStatus::output_size_mismatch);
        if (!bypassed_) {
            const Status rc = engine_->finish(tail);
            if (rc != Status::ok) {
                last_engine_status_ = rc;
                faulted_ = true;
                return EpochStatus::engine_fault;
            }
        } else {
            for(std::size_t i=0;i<latency_;++i) {
                if (output_count_ < latency_) tail[i] = Frame{};
                else {
                    if (!count_) return fault(tail, EpochStatus::wrong_lifecycle);
                    tail[i] = bypass_fifo_[head_];
                    head_ = (head_ + 1) % bypass_fifo_.size();
                    --count_;
                }
                ++output_count_;
            }
            if (count_) return fault(tail, EpochStatus::wrong_lifecycle);
        }
        if (!bypassed_) output_count_ += latency_;
        if (output_count_ != input_count_ + latency_)
            return fault(tail, EpochStatus::wrong_lifecycle);
        ended_ = true;
        return EpochStatus::ok;
    }

private:
    [[nodiscard]] static EpochStatus silence(std::span<Frame> output,
                                             EpochStatus rc) noexcept {
        std::fill(output.begin(),output.end(),Frame{});
        return rc;
    }
    [[nodiscard]] EpochStatus fault(std::span<Frame> output,
                                    EpochStatus rc) noexcept {
        faulted_ = true;
        return silence(output,rc);
    }
    std::unique_ptr<CallbackEngine> engine_{};
    Configuration config_{};
    static constexpr std::size_t kCapacity = 4096;
    std::array<Frame,kCapacity> bypass_fifo_{};
    std::uint64_t last_epoch_ = 0;
    std::uint64_t input_count_ = 0;
    std::uint64_t output_count_ = 0;
    std::size_t latency_ = 0;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;
    bool bypassed_ = false;
    bool active_ = false;
    bool ended_ = false;
    bool faulted_ = false;
    bool abandoned_previous_ = false;
    Status last_engine_status_ = Status::ok;
};
} // namespace rgsml::research::r1d_p2
