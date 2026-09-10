#include "waveform_presentation.hpp"

#include <algorithm>
#include <utility>

namespace rgsml::ui {

WaveformPresentation::WaveformPresentation(QObject* parent)
    : QObject(parent)
{
}

QString WaveformPresentation::state_token() const
{
    switch (state_) {
    case State::Empty:
        return QStringLiteral("EMPTY");
    case State::Building:
        return QStringLiteral("BUILDING");
    case State::Ready:
        return QStringLiteral("READY");
    case State::Failed:
        return QStringLiteral("FAILED");
    }
    return QStringLiteral("FAILED");
}

QString WaveformPresentation::status_text() const
{
    switch (state_) {
    case State::Empty:
        return QStringLiteral("Select a Source WAV to build its overview.");
    case State::Building:
        return QStringLiteral("Building bounded Source overview…");
    case State::Ready:
        return summary_ && summary_->source_frame_count().value() == 0
            ? QStringLiteral("The Source contains no audio frames.")
            : QStringLiteral("Full Source overview");
    case State::Failed:
        return failureMessage_.isEmpty()
            ? QStringLiteral("Waveform analysis failed. The Source is unchanged.")
            : failureMessage_;
    }
    return {};
}

bool WaveformPresentation::ready() const noexcept
{
    return state_ == State::Ready && static_cast<bool>(summary_);
}

bool WaveformPresentation::has_overrange() const noexcept { return hasOverrange_; }
int WaveformPresentation::channel_count() const noexcept
{
    return summary_ ? static_cast<int>(summary_->channel_count()) : 0;
}
qint64 WaveformPresentation::source_frame_count() const noexcept
{
    return summary_ ? summary_->source_frame_count().value() : 0;
}
qint64 WaveformPresentation::base_frames_per_bucket() const noexcept
{
    if (!summary_ || summary_->level_count() == 0U) {
        return 0;
    }
    const auto level = summary_->level(0U);
    return level ? level.value()->frames_per_bucket().value() : 0;
}
qint64 WaveformPresentation::base_bucket_count() const noexcept
{
    if (!summary_ || summary_->level_count() == 0U) {
        return 0;
    }
    const auto level = summary_->level(0U);
    return level ? level.value()->bucket_count().value() : 0;
}
qint64 WaveformPresentation::level_count() const noexcept
{
    return summary_ ? static_cast<qint64>(summary_->level_count()) : 0;
}
qint64 WaveformPresentation::payload_bytes() const noexcept
{
    return summary_ ? static_cast<qint64>(summary_->payload_bytes()) : 0;
}

std::shared_ptr<const audio::WaveformSummary> WaveformPresentation::summary() const noexcept
{
    return summary_;
}

void WaveformPresentation::publish_empty()
{
    state_ = State::Empty;
    failureMessage_.clear();
    summary_.reset();
    hasOverrange_ = false;
    emit changed();
}

void WaveformPresentation::publish_building()
{
    state_ = State::Building;
    failureMessage_.clear();
    summary_.reset();
    hasOverrange_ = false;
    emit changed();
}

void WaveformPresentation::publish_ready(
    std::shared_ptr<const audio::WaveformSummary> summary)
{
    if (!summary) {
        publish_failed(QStringLiteral("Waveform analysis returned no summary."));
        return;
    }
    bool overrange = false;
    if (summary->level_count() > 0U) {
        const auto coarsest = summary->level(summary->level_count() - 1U);
        if (coarsest) {
            for (std::size_t channel = 0; channel < summary->channel_count(); ++channel) {
                const auto peak = coarsest.value()->peak(channel, core::FrameIndex{0});
                if (peak && (peak.value()->minimum < -1.0 || peak.value()->maximum > 1.0)) {
                    overrange = true;
                    break;
                }
            }
        }
    }
    summary_ = std::move(summary);
    hasOverrange_ = overrange;
    failureMessage_.clear();
    state_ = State::Ready;
    emit changed();
}

void WaveformPresentation::publish_failed(QString message)
{
    state_ = State::Failed;
    summary_.reset();
    hasOverrange_ = false;
    failureMessage_ = std::move(message);
    emit changed();
}

void WaveformPresentation::requestRetry()
{
    if (state_ == State::Failed) {
        emit retryRequested();
    }
}

}  // namespace rgsml::ui
