#pragma once

// Research-only I0 adapter for the pinned R1d-P2 controller + M16 safety.
// Does NOT register a product DSP, replace M17, or qualify Windows callbacks.
#include "pinned/music_p2.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>

namespace rgsml::research::r1d_p2 {
using Frame = ::research::Frame;

enum class Status : std::uint8_t {
    ok, invalid_configuration, incompatible_fp, not_prepared,
    invalid_pcm, output_size_mismatch, safety_failure, fifo_overflow,
    fifo_underflow, wrong_lifecycle, frame_limit
};

struct Configuration final {
    int sample_rate = 48000;
    int channels = 2;
    int margin_percent = 2;
    double ceiling_linear = 0.8912509381337456; // -1 dBTP
    double drive_db = 0.0;
};

// One instance belongs to exactly one streaming epoch. No concurrent calls.
// The callback API always consumes and writes the same number of frames;
// sound is delayed by latency_frames() with zero-prefill. Once input ends,
// finish() emits exactly latency_frames() further output frames (including
// silence for a programme shorter than the latency). The caller must account
// for latency in transport and must NEVER publish analyzer-only bridge zeros.
class CallbackEngine final {
public:
    CallbackEngine() = default;
    CallbackEngine(const CallbackEngine&) = delete;
    CallbackEngine& operator=(const CallbackEngine&) = delete;
    CallbackEngine(CallbackEngine&&) = delete;
    CallbackEngine& operator=(CallbackEngine&&) = delete;

    // Prepare is OFF the audio thread; allocation is permitted here only.
    [[nodiscard]] Status prepare(Configuration config) {
        if (state_ != Lifecycle::fresh) return Status::wrong_lifecycle;
        if (!std::isfinite(config.ceiling_linear) ||
            !std::isfinite(config.drive_db) ||
            config.ceiling_linear < 0.25 || config.ceiling_linear > 1.0 ||
            config.drive_db < -24.0 || config.drive_db > 24.0 ||
            (config.margin_percent != 2 && config.margin_percent != 5) ||
            (config.channels != 1 && config.channels != 2))
            return Status::invalid_configuration;
        const int rates[] = {44100,48000,88200,96000,176400,192000};
        if (std::find(std::begin(rates), std::end(rates), config.sample_rate)
            == std::end(rates)) return Status::invalid_configuration;
        auto payload = std::unique_ptr<Payload>(new (std::nothrow) Payload(config));
        if (!payload) return Status::invalid_configuration;
        const ::research::Config safety_config{
            config.sample_rate, config.channels, config.margin_percent,
            config.ceiling_linear, false, true};
        const auto rc = payload->safety.prepare(safety_config);
        if (rc != ::research::Status::ok) {
            return rc == ::research::Status::incompatible_backend
                ? Status::incompatible_fp : Status::invalid_configuration;
        }
        latency_ = payload->music.delay() + payload->safety.horizon()
                 + ::research::quantum - 1;
        if (latency_ >= fifo_capacity) return Status::fifo_overflow;
        payload_ = std::move(payload);
        config_ = config;
        state_ = Lifecycle::running;
        return Status::ok;
    }

    [[nodiscard]] std::size_t latency_frames() const noexcept { return latency_; }
    [[nodiscard]] std::uint64_t input_frames() const noexcept { return input_count_; }
    [[nodiscard]] std::uint64_t output_frames() const noexcept { return output_count_; }
    [[nodiscard]] bool is_finished() const noexcept { return state_ == Lifecycle::finished; }

    // Real-time callback: no allocation, no locks, no syscalls, no exceptions.
    [[nodiscard]] Status process(std::span<const Frame> input,
                                 std::span<Frame> output) noexcept {
        if (input.size() != output.size()) return fail(Status::output_size_mismatch, output);
        if (state_ != Lifecycle::running) return fail(Status::not_prepared, output);
        if (input.size() > 4096 || input_count_ > UINT64_MAX-input.size())
            return fail(Status::frame_limit, output);
        // Preflight complete block: no partial callback acceptance on bad input.
        for (const auto& sample : input) {
            for (int c=0; c<config_.channels; ++c)
                if (!std::isfinite(sample[c]) || std::abs(sample[c]) > 1048576.)
                    return fail(Status::invalid_pcm, output);
        }
        for (std::size_t i=0;i<input.size();++i) {
            const auto rc = feed_music(input[i]);
            if (rc != Status::ok) return fail(rc, output);
            if (output_count_ < latency_) output[i] = Frame{};
            else if (!dequeue(output[i])) return fail(Status::fifo_underflow, output);
            ++input_count_;
            ++output_count_;
        }
        return Status::ok;
    }

    // Outside the callback's active programme; output span MUST have exactly
    // latency_frames() slots. No heap, locks or I/O. One successful call only.
    [[nodiscard]] Status finish(std::span<Frame> latency_tail) noexcept {
        if (state_ != Lifecycle::running) return fail(Status::wrong_lifecycle, latency_tail);
        if (latency_tail.size() != latency_) return fail(Status::output_size_mismatch, latency_tail);
        // Predictive lookahead output for the final original programme frames.
        for (std::size_t i=0;i<payload_->music.delay();++i) {
            const auto rc = feed_music(Frame{});
            if (rc != Status::ok) return fail(rc, latency_tail);
        }
        for (;;) {
            const auto step = payload_->safety.finish(payload_->scratch, payload_->gains);
            if (step.status != ::research::Status::ok)
                return fail(Status::safety_failure, latency_tail);
            if (!step.frames) break;
            if (!enqueue(step.frames)) return fail(Status::fifo_overflow, latency_tail);
        }
        if (payload_->safety.committed() != input_count_)
            return fail(Status::safety_failure, latency_tail);
        std::array<Frame, ::research::fir_tail> analyzer_only{};
        const auto bridge = payload_->safety.bridge(analyzer_only);
        if (bridge.status != ::research::Status::ok ||
            bridge.frames != ::research::fir_tail)
            return fail(Status::safety_failure, latency_tail);
        for (std::size_t i=0;i<latency_;++i) {
            if (output_count_ < latency_) latency_tail[i] = Frame{};
            else if (!dequeue(latency_tail[i]))
                return fail(Status::fifo_underflow, latency_tail);
            ++output_count_;
        }
        if (count_ || output_count_ != input_count_ + latency_)
            return fail(Status::safety_failure, latency_tail);
        state_ = Lifecycle::finished;
        return Status::ok;
    }

private:
    struct Payload final {
        explicit Payload(Configuration cfg)
            : music(cfg.sample_rate, cfg.channels, cfg.drive_db, cfg.ceiling_linear) {}
        ::research::PredictiveMusic music;
        ::research::Stream safety;
        std::array<Frame, ::research::quantum> scratch{};
        std::array<double, ::research::quantum> gains{};
    };
    enum class Lifecycle : std::uint8_t { fresh, running, faulted, finished };
    static constexpr std::size_t fifo_capacity = 4096;
    std::unique_ptr<Payload> payload_{};
    Configuration config_{};
    std::array<Frame, fifo_capacity> fifo_{};
    std::size_t head_ = 0, tail_ = 0, count_ = 0, latency_ = 0;
    std::uint64_t input_count_ = 0, output_count_ = 0;
    Lifecycle state_ = Lifecycle::fresh;

    [[nodiscard]] Status feed_music(Frame raw) noexcept {
        Frame sample{};
        if (!payload_->music.push(raw, sample)) return Status::ok;
        const auto step = payload_->safety.submit(
            std::span<const Frame>(&sample, 1), payload_->scratch, payload_->gains);
        if (step.status != ::research::Status::ok) return Status::safety_failure;
        if (!enqueue(step.frames)) return Status::fifo_overflow;
        return Status::ok;
    }
    [[nodiscard]] bool enqueue(std::size_t n) noexcept {
        if (n > fifo_capacity-count_) return false;
        for (std::size_t i=0;i<n;++i) {
            fifo_[tail_] = payload_->scratch[i];
            tail_ = (tail_ + 1) % fifo_capacity;
        }
        count_ += n;
        return true;
    }
    [[nodiscard]] bool dequeue(Frame& sample) noexcept {
        if (!count_) return false;
        sample = fifo_[head_];
        head_ = (head_ + 1) % fifo_capacity;
        --count_;
        return true;
    }
    [[nodiscard]] Status fail(Status rc, std::span<Frame> output) noexcept {
        std::fill(output.begin(),output.end(),Frame{});
        state_ = Lifecycle::faulted;
        return rc;
    }
};
}  // namespace rgsml::research::r1d_p2
