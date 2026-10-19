#include "playback_transport_view_model.hpp"
#include "terminal_audio_session_gate.hpp"

#include <algorithm>
#include <utility>

namespace rgsml::app {
namespace {

[[nodiscard]] QString user_message(core::ErrorCode code)
{
    switch (code) {
    case core::ErrorCode::ResourceNotFound:
        return QStringLiteral("Playback Source is no longer available. Select it again.");
    case core::ErrorCode::AccessDenied:
        return QStringLiteral("Audio output or Source access was denied. Check availability and retry.");
    case core::ErrorCode::UnsupportedAudioEncoding:
    case core::ErrorCode::UnsupportedAudioLayout:
    case core::ErrorCode::UnsupportedOperation:
        return QStringLiteral("The default output cannot play this Source format exactly.");
    case core::ErrorCode::InvalidAudioSample:
    case core::ErrorCode::MalformedAudioContainer:
    case core::ErrorCode::TruncatedAudioData:
        return QStringLiteral("Playback stopped because the Source audio is invalid or incomplete.");
    case core::ErrorCode::InvalidState:
        return QStringLiteral("Playback command is unavailable in the current state.");
    default:
        return QStringLiteral("Playback stopped. Check the audio output and retry.");
    }
}

}  // namespace

PlaybackTransportViewModel::PlaybackTransportViewModel(
    std::unique_ptr<core::IAudioPlaybackService> service,
    QObject* parent)
    : QObject(parent)
    , service_(std::move(service))
{
    refreshTimer_.setInterval(50);
    refreshTimer_.setTimerType(Qt::CoarseTimer);
    connect(&refreshTimer_, &QTimer::timeout, this, &PlaybackTransportViewModel::refresh);
    refreshTimer_.start();
    refresh();
}

bool PlaybackTransportViewModel::playback_available() const noexcept
{
    return playbackAvailable_;
}

QString PlaybackTransportViewModel::state_label() const
{
    if (!errorMessage_.isEmpty()) {
        return QStringLiteral("Unavailable");
    }
    switch (state_) {
    case core::PlaybackState::NO_SOURCE:
        return QStringLiteral("No Source");
    case core::PlaybackState::STOPPED:
        return QStringLiteral("Stopped");
    case core::PlaybackState::PLAYING:
        return QStringLiteral("Playing");
    case core::PlaybackState::PAUSED:
        return QStringLiteral("Paused");
    }
    return QStringLiteral("Unavailable");
}

bool PlaybackTransportViewModel::is_playing() const noexcept
{
    return state_ == core::PlaybackState::PLAYING;
}

bool PlaybackTransportViewModel::is_paused() const noexcept
{
    return state_ == core::PlaybackState::PAUSED;
}

bool PlaybackTransportViewModel::can_play() const noexcept
{
    return playbackAvailable_
        && (state_ == core::PlaybackState::STOPPED
            || state_ == core::PlaybackState::PAUSED);
}

bool PlaybackTransportViewModel::can_pause() const noexcept
{
    return playbackAvailable_ && state_ == core::PlaybackState::PLAYING;
}

bool PlaybackTransportViewModel::can_stop() const noexcept
{
    return playbackAvailable_ && state_ != core::PlaybackState::NO_SOURCE;
}

qint64 PlaybackTransportViewModel::position_frames() const noexcept
{
    return positionFrames_;
}

qint64 PlaybackTransportViewModel::duration_frames() const noexcept
{
    return durationFrames_;
}

QString PlaybackTransportViewModel::position_label() const
{
    return format_frames(positionFrames_);
}

QString PlaybackTransportViewModel::duration_label() const
{
    return format_frames(durationFrames_);
}

QString PlaybackTransportViewModel::error_message() const
{
    return errorMessage_;
}

void PlaybackTransportViewModel::prepare_source(
    const core::ResourceReference& source,
    qint64 sampleRateHz)
{
    static_cast<void>(prepare_file(source, sampleRateHz));
}

void PlaybackTransportViewModel::set_pcm_prepare_handler(
    PcmPrepareHandler handler)
{
    pcmPrepareHandler_ = std::move(handler);
}

void PlaybackTransportViewModel::set_pcm_handoff_handler(
    PcmHandoffHandler handler)
{
    pcmHandoffHandler_ = std::move(handler);
}

core::Status PlaybackTransportViewModel::stop_and_clear()
{
    if (!service_) {
        playbackAvailable_ = false;
        errorMessage_ = QStringLiteral("Playback service is unavailable.");
        emit playbackChanged();
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState, "Playback service is unavailable."});
    }

    auto current = service_->snapshot();
    if (current && current.value()->state != core::PlaybackState::NO_SOURCE) {
        const auto stopped = service_->stop();
        if (!stopped) {
            publish_failure(*stopped.error());
            return stopped;
        }
    }
    const auto cleared = service_->clear();
    if (!cleared) {
        publish_failure(*cleared.error());
        return cleared;
    }
    terminalLease_.reset();
    terminalGate_ = nullptr;
    sampleRateHz_ = 0;
    errorMessage_.clear();
    refresh();
    return core::Status::success();
}

core::Status PlaybackTransportViewModel::prepare_file(
    const core::ResourceReference& source,
    qint64 sampleRateHz)
{
    auto cleared = stop_and_clear();
    if (!cleared) {
        return cleared;
    }

    const auto prepared = service_->prepare(source);
    if (!prepared) {
        playbackAvailable_ = false;
        state_ = core::PlaybackState::NO_SOURCE;
        positionFrames_ = 0;
        durationFrames_ = 0;
        sampleRateHz_ = 0;
        publish_failure(*prepared.error());
        return prepared;
    }
    sampleRateHz_ = sampleRateHz;
    playbackAvailable_ = true;
    errorMessage_.clear();
    refresh();
    return core::Status::success();
}

core::Status PlaybackTransportViewModel::prepare_pcm(
    audio::AudioBufferView source,
    std::shared_ptr<const void> lifetime,
    std::optional<core::RealizationId> realizationId)
{
    if (!pcmPrepareHandler_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "The Windows PCM audition seam is unavailable."});
    }
    auto cleared = stop_and_clear();
    if (!cleared) {
        return cleared;
    }
    auto prepared = pcmPrepareHandler_(
        source, std::move(lifetime), realizationId);
    if (!prepared) {
        publish_failure(*prepared.error());
        return prepared;
    }
    sampleRateHz_ = source.format().sample_rate().value();
    errorMessage_.clear();
    refresh();
    return core::Status::success();
}

core::Status PlaybackTransportViewModel::handoff_pcm(
    audio::AudioBufferView source,
    std::shared_ptr<const void> lifetime,
    std::optional<core::RealizationId> realizationId)
{
    if (terminalGate_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "SOURCE PCM handoff requires retiring the terminal session."});
    }
    if (!pcmHandoffHandler_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "The Windows PCM handoff seam is unavailable."});
    }
    auto handedOff = pcmHandoffHandler_(
        source, std::move(lifetime), realizationId);
    if (!handedOff) {
        publish_failure(*handedOff.error());
        return handedOff;
    }
    sampleRateHz_ = source.format().sample_rate().value();
    errorMessage_.clear();
    refresh();
    return core::Status::success();
}

void PlaybackTransportViewModel::set_source_derived_active(bool active) noexcept
{
    sourceDerivedActive_ = active;
}

core::Status PlaybackTransportViewModel::bind_private_terminal_source_controls(
    TerminalAudioSessionGate& gate)
{
    if (!service_ || terminalGate_ || !sourceDerivedActive_
        || !gate.has_session() || !gate.owns_playback(*service_)) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "No uniquely owned terminal OUTPUT_RATE session for SOURCE binding."});
    }
    auto display=gate.source_snapshot();
    auto rate=gate.source_rate();
    auto lease=gate.session_liveness();
    if (!display || !rate || lease.expired()) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Invalid or stale terminal SOURCE mapping."});
    }
    terminalGate_=&gate;
    terminalLease_=std::move(lease);
    sampleRateHz_=rate.value()->value();
    errorMessage_.clear();
    refresh();
    return core::Status::success();
}

core::Result<PlaybackTransportViewModel::SourceDisplaySnapshot>
PlaybackTransportViewModel::source_display_snapshot() const
{
    using S=SourceDisplaySnapshot;
    if (terminalGate_) {
        if (terminalLease_.expired())
            return core::Result<S>::failure(core::Error{
                core::ErrorCode::InvalidState,
                "Stale M17 SOURCE adapter: expired terminal lease."});
        auto mapped=terminalGate_->source_snapshot();
        if (!mapped) return core::Result<S>::failure(*mapped.error());
        return core::Result<S>::success(S{
            mapped.value()->state,mapped.value()->position,
            mapped.value()->duration,mapped.value()->loop});
    }
    auto raw=playback_snapshot();
    if (!raw) return core::Result<S>::failure(*raw.error());
    if (!raw.value()->duration)
        return core::Result<S>::failure(core::Error{
            core::ErrorCode::InvalidState,"No SOURCE duration available."});
    return core::Result<S>::success(S{
        raw.value()->state,raw.value()->position,*raw.value()->duration,
        raw.value()->loop});
}

core::Status PlaybackTransportViewModel::seek_target_frame(
    core::FrameIndex position)
{
    if (!service_ || terminalGate_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Raw target seek forbidden during terminal OUTPUT_RATE session."});
    }
    auto result = service_->seek(position);
    if (!result) {
        publish_failure(*result.error());
        return result;
    }
    errorMessage_.clear();
    refresh();
    return core::Status::success();
}

core::Status PlaybackTransportViewModel::seek_source_frame(
    core::FrameIndex position)
{
    if (terminalGate_) {
        if (!sourceDerivedActive_ || terminalLease_.expired())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"Terminal SOURCE seek owner stale."});
        auto result=terminalGate_->seek_source(position);
        if (!result) publish_failure(*result.error());
        else { errorMessage_.clear(); refresh(); }
        return result;
    }
    if (!sourceDerivedActive_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Source waveform seek is unavailable during Gold audition."});
    }
    return seek_target_frame(position);
}

core::Status PlaybackTransportViewModel::set_loop_source_range(
    std::optional<core::FrameRange> loop)
{
    if (terminalGate_) {
        if (!sourceDerivedActive_ || terminalLease_.expired())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"Terminal SOURCE loop owner stale."});
        auto result=terminalGate_->set_source_loop(std::move(loop));
        if (!result) publish_failure(*result.error());
        else { errorMessage_.clear(); refresh(); }
        return result;
    }
    if (!sourceDerivedActive_) {
        return core::Status::success();
    }
    if (!service_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Playback service is unavailable."});
    }
    const auto result = service_->set_loop(std::move(loop));
    if (!result) {
        publish_failure(*result.error());
        refresh();
        return result;
    }
    errorMessage_.clear();
    refresh();
    return core::Status::success();
}

core::Result<core::PlaybackSnapshot>
PlaybackTransportViewModel::playback_snapshot() const
{
    if (!service_) {
        return core::Result<core::PlaybackSnapshot>::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Playback service is unavailable."});
    }
    return service_->snapshot();
}

void PlaybackTransportViewModel::playOrResume()
{
    if (!service_) {
        return;
    }
    if (terminalGate_ && (terminalLease_.expired()
        || !terminalGate_->source_snapshot())) {
        publish_failure(core::Error{
            core::ErrorCode::InvalidState,"Expired terminal owner: Play refused."});
        refresh();
        return;
    }
    const auto result = service_->play();
    if (!result) {
        publish_failure(*result.error());
        return;
    }
    errorMessage_.clear();
    refresh();
}

void PlaybackTransportViewModel::pause()
{
    if (!service_) {
        return;
    }
    const auto result = service_->pause();
    if (!result) {
        publish_failure(*result.error());
        return;
    }
    errorMessage_.clear();
    refresh();
}

void PlaybackTransportViewModel::stop()
{
    if (!service_) {
        return;
    }
    const auto result = service_->stop();
    if (!result) {
        publish_failure(*result.error());
        return;
    }
    errorMessage_.clear();
    refresh();
}

void PlaybackTransportViewModel::refresh()
{
    if (!service_) {
        return;
    }
    auto snapshot = service_->snapshot();
    if (!snapshot) {
        publish_failure(*snapshot.error());
        return;
    }

    // Never present OUTPUT_RATE positions or duration as SOURCE frames.
    auto mapped=source_display_snapshot();
    const bool stale=terminalGate_ && !mapped;
    const auto newState=stale
        ? core::PlaybackState::NO_SOURCE : snapshot.value()->state;
    const auto newPosition=stale ? 0
        : (terminalGate_ ? mapped.value()->position.value()
                         : snapshot.value()->position.value());
    const auto newDuration=stale ? 0
        : (terminalGate_ ? mapped.value()->duration.value()
                         : (snapshot.value()->duration
                             ? snapshot.value()->duration->value() : 0));
    const bool newAvailable=!stale
        && newState != core::PlaybackState::NO_SOURCE;
    if (state_ == newState
        && positionFrames_ == newPosition
        && durationFrames_ == newDuration
        && playbackAvailable_ == newAvailable) {
        return;
    }
    state_ = newState;
    positionFrames_ = newPosition;
    durationFrames_ = newDuration;
    playbackAvailable_ = newAvailable;
    emit playbackChanged();
}

void PlaybackTransportViewModel::publish_failure(const core::Error& error)
{
    const auto message = user_message(error.code());
    if (errorMessage_ != message) {
        errorMessage_ = message;
        emit playbackChanged();
    }
}

QString PlaybackTransportViewModel::format_frames(qint64 frames) const
{
    if (sampleRateHz_ <= 0 || frames < 0) {
        return QStringLiteral("0:00.000");
    }
    qint64 seconds = frames / sampleRateHz_;
    const auto remainder = frames % sampleRateHz_;
    qint64 milliseconds = (remainder * 1000 + sampleRateHz_ / 2) / sampleRateHz_;
    if (milliseconds == 1000) {
        ++seconds;
        milliseconds = 0;
    }
    const auto minutes = seconds / 60;
    const auto trailingSeconds = seconds % 60;
    return QStringLiteral("%1:%2.%3")
        .arg(minutes)
        .arg(trailingSeconds, 2, 10, QLatin1Char('0'))
        .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

}  // namespace rgsml::app
