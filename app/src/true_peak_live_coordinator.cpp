#include "true_peak_live_coordinator.hpp"
#include "internal/true_peak_chronological_buckets.hpp"

#include <QMetaObject>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace rgsml::app {

TruePeakLiveCoordinator::TruePeakLiveCoordinator(
    PrefixProvider prefix, PlaybackProvider playback,
    SourceProvider source, QObject* parent)
    : QObject(parent)
    , prefix_(std::move(prefix))
    , playback_(std::move(playback))
    , source_(std::move(source))
    , worker_([this] { run_worker(); })
{
    timer_.setInterval(50);
    timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_, &QTimer::timeout, this, &TruePeakLiveCoordinator::poll);
    timer_.start();
}

TruePeakLiveCoordinator::~TruePeakLiveCoordinator()
{
    timer_.stop();
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        pending_.reset();
    }
    cv_.notify_one();
    if (worker_.joinable()) worker_.join();
}

void TruePeakLiveCoordinator::invalidate()
{
    // A new source/selected target cannot borrow an old REAL peak even for
    // the 50-ms polling interval. serial_ also drops any queued completion.
    clear_display(false);
}

void TruePeakLiveCoordinator::clear_display(bool attempted)
{
    ++serial_; // reject any in-flight worker completion
    ++observationEpoch_; // reject reuse of FIR history after a missing interval
    monitoringAttempted_ = attempted;
    recentAvailable_ = false;
    holdAvailable_ = false;
    recent_ = {};
    hold_ = {};
    channelCount_ = 0;
    statusText_ = attempted ? QStringLiteral("GAP / UNAVAILABLE")
                            : QStringLiteral("NOT MONITORED");
    generation_.reset();
    seekSerial_.reset();
    traversalSerial_.reset();
    loopWrapCount_.reset();
    activeId_.reset();
    currentOwner_.reset();
    {
        std::lock_guard lock(mutex_);
        pending_.reset();
    }
    emit changed();
}

void TruePeakLiveCoordinator::poll()
{
    if (!playback_ || !source_ || !prefix_) {
        if (monitoringAttempted_) clear_display(false);
        return;
    }
    const auto source = source_();
    const auto before = playback_();
    if (!source || !source->owner || !before
        || (before.value()->state != core::PlaybackState::PLAYING
            && before.value()->state != core::PlaybackState::PAUSED)) {
        if (monitoringAttempted_) clear_display(false);
        return;
    }
    if (before.value()->state == core::PlaybackState::PAUSED) {
        // Freeze only an already-matching owner; do not advance accepted-time.
        if (activeId_ != std::optional{source->realizationId}
            || currentOwner_ != source->owner
            || !seekSerial_ || *seekSerial_ != before.value()->seekSerial
            || !traversalSerial_
            || *traversalSerial_ != before.value()->traversalSerial
            || !loopWrapCount_
            || *loopWrapCount_ != before.value()->loopWrapCount
            || before.value()->audibleRealization.phase
                != core::AudibleHandoffPhase::NEW
            || before.value()->audibleRealization.realizationId
                != std::optional{source->realizationId}) {
            clear_display(true);
        }
        return;
    }
    const auto prefix = prefix_();
    const auto after = playback_();
    if (!prefix || !after
        || after.value()->state != core::PlaybackState::PLAYING
        || after.value()->audibleRealization != before.value()->audibleRealization
        || after.value()->seekSerial != before.value()->seekSerial
        || after.value()->traversalSerial != before.value()->traversalSerial
        || after.value()->loopWrapCount != before.value()->loopWrapCount) {
        clear_display(true);
        return;
    }
    auto bound = bind_processed_live_input(source->owner,
        source->realizationId, source->expected,
        *after.value(), *prefix.value());
    if (!bound) {
        clear_display(true);
        return;
    }
    const bool fresh = !monitoringAttempted_
        || generation_ != std::optional{bound.value()->playbackGeneration}
        || activeId_ != std::optional{bound.value()->realizationId}
        || currentOwner_ != bound.value()->owner
        || seekSerial_ != std::optional{after.value()->seekSerial}
        || traversalSerial_ != std::optional{after.value()->traversalSerial}
        || loopWrapCount_ != std::optional{after.value()->loopWrapCount};
    if (fresh) {
        clear_display(true);
        generation_ = bound.value()->playbackGeneration;
        activeId_ = bound.value()->realizationId;
        currentOwner_ = bound.value()->owner;
        channelCount_ = bound.value()->layout == audio::ChannelLayout::MONO_C ? 1 : 2;
    }
    seekSerial_ = after.value()->seekSerial;
    traversalSerial_ = after.value()->traversalSerial;
    loopWrapCount_ = after.value()->loopWrapCount;
    // The latest immutable owner snapshot replaces queued work. The worker
    // retains its consumed cursor; it never computes peaks on the GUI thread.
    const auto id = ++serial_;
    {
        std::lock_guard lock(mutex_);
        pending_.emplace(Job{id, observationEpoch_, std::move(*bound.value())});
    }
    cv_.notify_one();
}

void TruePeakLiveCoordinator::run_worker()
{
    std::optional<analysis::internal::ChronologicalTruePeakBuckets> meter;
    std::shared_ptr<const render::RenderResult> owner;
    std::optional<core::RealizationId> realization;
    std::uint64_t generation = 0;
    std::uint64_t chainRevision = 0;
    std::uint64_t epoch = 0;
    std::int64_t sourceOrigin = 0;
    std::int64_t processed = 0;
    while (true) {
        std::optional<Job> job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
            if (stopping_) break;
            job.swap(pending_);
        }
        if (!job) continue;
        const auto& input = job->input;
        Completion result;
        const auto channels = input.layout == audio::ChannelLayout::MONO_C
            ? std::size_t{1} : std::size_t{2};
        if (epoch != job->observationEpoch
            || owner != input.owner
            || realization != std::optional{input.realizationId}
            || generation != input.playbackGeneration
            || chainRevision != input.chainRevision
            || sourceOrigin != input.consumedSourceBegin) {
            meter.reset();
            owner = input.owner;
            realization = input.realizationId;
            generation = input.playbackGeneration;
            chainRevision = input.chainRevision;
            epoch = job->observationEpoch;
            sourceOrigin = input.consumedSourceBegin;
            processed = sourceOrigin;
        }
        // A long GUI event-loop stall (e.g. continuous window resize) may
        // cause the latest exact sink-consumed suffix to outrun the worker
        // by >500 ms. Replay a bounded, independently source-mapped suffix
        // from the immutable PROCESSED owner; NEVER preserve pre-stall FIR
        // or 1-second HOLD history. Keeping sourceOrigin as the verified
        // slice origin (separate from the rebased processed cursor) avoids
        // the old permanent fail/retry-from-start feedback loop.
        const auto cap = input.sampleRate.value() / 2;
        if (input.consumedSourceEnd >= processed
            && input.consumedSourceEnd - processed > cap) {
            meter.reset();
            processed = std::max(sourceOrigin, input.consumedSourceEnd - cap);
        }
        if (!meter) {
            auto built = analysis::internal::ChronologicalTruePeakBuckets::create(
                input.sampleRate.value(), channels, processed);
            if (built) meter.emplace(std::move(*built.value()));
        }
        const auto remaining = input.consumedSourceEnd - processed;
        if (meter && remaining >= 0 && remaining <= cap) {
            const auto view = input.owner->view();
            const auto left = view.channel(0);
            const auto right = view.channel(channels == 2 ? 1U : 0U);
            if (left && right) {
                bool valid = true;
                while (processed < input.consumedSourceEnd) {
                    const auto offset = static_cast<std::size_t>(processed);
                    const std::array<double, 2> frame{
                        (*left.value())[offset],
                        channels == 2 ? (*right.value())[offset] : 0.0};
                    if (!meter->consume_frame(
                            std::span<const double>{frame.data(), channels}, true)) {
                        valid = false;
                        break;
                    }
                    ++processed;
                }
                if (valid) {
                    const auto window = meter->snapshot();
                    result.valid = true;
                    result.recent = window.recent100msComplete;
                    result.hold = window.shortHold1sComplete;
                    result.recentPeak = window.recentLinearPeak;
                    result.holdPeak = window.shortHoldLinearPeak;
                }
            }
        }
        if (!result.valid) {
            meter.reset();
            processed = sourceOrigin;
        }
        static_cast<void>(QMetaObject::invokeMethod(this,
            [this, serial = job->serial, result] {
                accept_result(serial, result);
            }, Qt::QueuedConnection));
    }
}

void TruePeakLiveCoordinator::accept_result(
    std::uint64_t serial, Completion completed)
{
    if (serial != serial_ || !monitoringAttempted_) return;
    // Reconfirm after asynchronous computation: a source change, paused seek
    // or audible transition may happen after the last 50-ms polling instant.
    const auto active = source_ ? source_() : std::nullopt;
    const auto playback = playback_ ? playback_()
        : core::Result<core::PlaybackSnapshot>::failure(core::Error{
            core::ErrorCode::InvalidState, "M16 playback observer missing"});
    if (!active || !playback || active->owner != currentOwner_
        || activeId_ != std::optional{active->realizationId}
        || playback.value()->audibleRealization.phase
            != core::AudibleHandoffPhase::NEW
        || playback.value()->audibleRealization.realizationId
            != std::optional{active->realizationId}
        || (playback.value()->state != core::PlaybackState::PLAYING
            && playback.value()->state != core::PlaybackState::PAUSED)
        || seekSerial_ != std::optional{playback.value()->seekSerial}
        || traversalSerial_ != std::optional{playback.value()->traversalSerial}
        || loopWrapCount_ != std::optional{playback.value()->loopWrapCount}) {
        clear_display(true);
        return;
    }
    if (!completed.valid) {
        clear_display(true);
        return;
    }
    recentAvailable_ = completed.recent;
    holdAvailable_ = completed.hold;
    recent_ = completed.recentPeak;
    hold_ = completed.holdPeak;
    statusText_ = recentAvailable_
        ? QStringLiteral("LIVE · PROCESSED · UNQUALIFIED")
        : QStringLiteral("PARTIAL / WAITING FOR 100 MS");
    emit changed();
}

QString TruePeakLiveCoordinator::reading(double linear)
{
    if (linear <= 0.0) return QStringLiteral("−∞ dBTP");
    return QString::number(20.0 * std::log10(linear), 'f', 2)
        + QStringLiteral(" dBTP");
}

double TruePeakLiveCoordinator::bar_level(double linear) noexcept
{
    if (linear <= 0.0) return 0.0;
    return std::clamp((20.0 * std::log10(linear) + 60.0) / 66.0, 0.0, 1.0);
}

QString TruePeakLiveCoordinator::peak_text() const
{
    return recentAvailable_ ? reading(std::max(recent_[0], recent_[1]))
        : QStringLiteral("UNAVAILABLE");
}
QString TruePeakLiveCoordinator::hold_text() const
{
    return holdAvailable_ ? reading(std::max(hold_[0], hold_[1]))
        : QStringLiteral("UNAVAILABLE");
}
QString TruePeakLiveCoordinator::left_text() const
{
    return recentAvailable_ ? reading(recent_[0]) : QStringLiteral("—");
}
QString TruePeakLiveCoordinator::right_text() const
{
    return recentAvailable_ && channelCount_ == 2
        ? reading(recent_[1]) : QStringLiteral("—");
}
double TruePeakLiveCoordinator::left_level() const
{
    return recentAvailable_ ? bar_level(recent_[0]) : 0.0;
}
double TruePeakLiveCoordinator::right_level() const
{
    return recentAvailable_ && channelCount_ == 2
        ? bar_level(recent_[1]) : 0.0;
}
double TruePeakLiveCoordinator::left_hold_level() const
{
    return holdAvailable_ ? bar_level(hold_[0]) : 0.0;
}
double TruePeakLiveCoordinator::right_hold_level() const
{
    return holdAvailable_ && channelCount_ == 2
        ? bar_level(hold_[1]) : 0.0;
}

} // namespace rgsml::app
