#include "true_peak_static_coordinator.hpp"

#include <QMetaObject>
#include <QString>

#include <utility>

namespace rgsml::app {

QString TruePeakStaticCoordinator::state_text() const
{
    switch (state_) {
    case State::NO_SOURCE: return QStringLiteral("NO SOURCE");
    case State::NO_PROCESSED: return QStringLiteral("NO PROCESSED RENDER");
    case State::MEASURING: return QStringLiteral("MEASURING");
    case State::READY: return QStringLiteral("READY · NOT NECESSARILY AUDIBLE");
    case State::UNAVAILABLE: return QStringLiteral("UNAVAILABLE · INVALID OR PARTIAL RESOURCE");
    }
    return QStringLiteral("UNAVAILABLE");
}

QString TruePeakStaticCoordinator::peak_text() const
{
    if (!completed_) return QStringLiteral("—");
    const auto& measurement = completed_->measurement;
    if (measurement.is_exact_silence()) return QStringLiteral("−∞ dBTP");
    if (!measurement.global_db_tp()) return QStringLiteral("—");
    return QString::number(*measurement.global_db_tp(), 'f', 2)
        + QStringLiteral(" dBTP");
}

QString TruePeakStaticCoordinator::channel_text() const
{
    if (!completed_) return QStringLiteral("UNAVAILABLE");
    switch (completed_->measurement.global_peak_channel()) {
    case analysis::TruePeakPeakChannel::NONE: return QStringLiteral("NONE · EXACT SILENCE");
    case analysis::TruePeakPeakChannel::MONO_C: return QStringLiteral("MONO C");
    case analysis::TruePeakPeakChannel::LEFT: return QStringLiteral("LEFT");
    case analysis::TruePeakPeakChannel::RIGHT: return QStringLiteral("RIGHT");
    case analysis::TruePeakPeakChannel::TIE: return QStringLiteral("STEREO TIE");
    }
    return QStringLiteral("UNAVAILABLE");
}

QString TruePeakStaticCoordinator::sample_rate_text() const
{
    return completed_
        ? QString::number(completed_->measurement.sample_rate_hz()) + QStringLiteral(" Hz")
        : QStringLiteral("—");
}

QString TruePeakStaticCoordinator::qualification_text() const
{
    // Gate G is OPEN: never treat an automated CI or future enum value as
    // accepted exact-build release provenance.
    return completed_ ? QStringLiteral("SUPPORTED · UNQUALIFIED")
                      : QStringLiteral("NO QUALIFIED MEASUREMENT");
}

QString TruePeakStaticCoordinator::realization_text() const
{
    return completed_ ? QStringLiteral("PROCESSED #")
        + QString::number(static_cast<qulonglong>(completed_->realizationId.value))
        : QStringLiteral("NO PUBLISHED REALIZATION");
}

TruePeakStaticCoordinator::TruePeakStaticCoordinator(QObject* parent)
    : QObject(parent)
    , worker_([this] { worker_loop(); })
{
}

TruePeakStaticCoordinator::~TruePeakStaticCoordinator()
{
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        pending_.reset();
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void TruePeakStaticCoordinator::invalidate_source()
{
    ++generation_;
    source_.reset();
    expected_.reset();
    currentRender_.reset();
    currentId_.reset();
    completed_.reset();
    state_ = State::NO_SOURCE;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        pending_.reset();
    }
    emit changed();
}

void TruePeakStaticCoordinator::source_committed(
    core::ResourceReference reference, TruePeakExpectedSource expected)
{
    invalidate_source();
    source_ = std::move(reference);
    expected_ = expected;
    state_ = State::NO_PROCESSED;
    emit changed();
}

void TruePeakStaticCoordinator::processed_changed(
    std::shared_ptr<const render::RenderResult> render,
    std::optional<core::RealizationId> id)
{
    if (!source_ || !expected_) return;
    if (render == currentRender_ && id == currentId_) return;

    ++generation_;
    currentRender_ = render;
    currentId_ = id;
    completed_.reset();
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        pending_.reset();
        if (render && id && id->value != 0) {
            pending_.emplace(Job{generation_, render, *id, *expected_});
        }
    }
    if (render && id && id->value != 0) {
        state_ = State::MEASURING;
        cv_.notify_one();
    } else {
        state_ = State::NO_PROCESSED;
    }
    emit changed();
}

void TruePeakStaticCoordinator::worker_loop()
{
    for (;;) {
        std::optional<Job> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stopping_ || pending_.has_value();
            });
            if (stopping_) return;
            job = std::move(pending_);
            pending_.reset();
        }
        auto outcome = std::make_shared<core::Result<BoundStaticTruePeak>>(
            measure_accepted_processed_true_peak(
                std::move(job->render), job->realizationId, job->expected));
        const auto gen = job->generation;
        QMetaObject::invokeMethod(this, [this, gen, outcome] {
            complete_job(gen, outcome);
        }, Qt::QueuedConnection);
    }
}

void TruePeakStaticCoordinator::complete_job(
    std::uint64_t generation,
    std::shared_ptr<core::Result<BoundStaticTruePeak>> result)
{
    if (generation != generation_ || !source_ || !currentRender_
        || !currentId_) {
        ++staleDiscarded_;
        return;
    }
    completed_.reset();
    if (result && *result
        && result->value()->owner == currentRender_
        && result->value()->realizationId == *currentId_) {
        completed_.emplace(std::move(*result->value()));
        state_ = State::READY;
    } else {
        state_ = State::UNAVAILABLE;
    }
    emit changed();
}

} // namespace rgsml::app
