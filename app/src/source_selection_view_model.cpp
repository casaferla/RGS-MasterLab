#include "source_selection_view_model.hpp"

#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <QFileInfo>

#include <cstdint>
#include <string>
#include <utility>

namespace rgsml::app {
namespace {

[[nodiscard]] QString container_label_for(audio::WavContainerKind kind)
{
    switch (kind) {
    case audio::WavContainerKind::RIFF:
        return QStringLiteral("RIFF/WAVE");
    case audio::WavContainerKind::RF64:
        return QStringLiteral("RF64/WAVE");
    }
    return QStringLiteral("N/A");
}

[[nodiscard]] QString sample_format_label_for(audio::WavSampleFormat format)
{
    switch (format) {
    case audio::WavSampleFormat::PCM_S16:
        return QStringLiteral("PCM 16-bit");
    case audio::WavSampleFormat::PCM_S24:
        return QStringLiteral("PCM 24-bit");
    case audio::WavSampleFormat::PCM_S32:
        return QStringLiteral("PCM 32-bit");
    case audio::WavSampleFormat::IEEE_F32:
        return QStringLiteral("IEEE float 32-bit");
    case audio::WavSampleFormat::IEEE_F64:
        return QStringLiteral("IEEE float 64-bit");
    }
    return QStringLiteral("N/A");
}

[[nodiscard]] QString channel_layout_label_for(audio::ChannelLayout layout)
{
    switch (layout) {
    case audio::ChannelLayout::MONO_C:
        return QStringLiteral("Mono (C)");
    case audio::ChannelLayout::STEREO_LR:
        return QStringLiteral("Stereo (L/R)");
    }
    return QStringLiteral("N/A");
}

[[nodiscard]] QString format_duration(std::int64_t frames, std::int64_t rate)
{
    if (frames < 0 || rate <= 0) {
        return QStringLiteral("N/A");
    }

    qint64 seconds = frames / rate;
    const qint64 remainder = frames % rate;
    qint64 milliseconds = (remainder * 1000 + rate / 2) / rate;
    if (milliseconds == 1000) {
        ++seconds;
        milliseconds = 0;
    }

    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds / 60) % 60;
    const qint64 trailingSeconds = seconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(trailingSeconds, 2, 10, QLatin1Char('0'))
            .arg(milliseconds, 3, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2.%3")
        .arg(minutes)
        .arg(trailingSeconds, 2, 10, QLatin1Char('0'))
        .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

[[nodiscard]] QString user_message(core::ErrorCode code)
{
    switch (code) {
    case core::ErrorCode::ResourceNotFound:
        return QStringLiteral("Source file not found. Choose an existing WAV file.");
    case core::ErrorCode::AccessDenied:
        return QStringLiteral("Source cannot be read. Check file access and try again.");
    case core::ErrorCode::UnsupportedAudioEncoding:
    case core::ErrorCode::UnsupportedAudioLayout:
        return QStringLiteral("Unsupported WAV format. Choose PCM or IEEE mono/stereo WAV.");
    case core::ErrorCode::MalformedAudioContainer:
    case core::ErrorCode::TruncatedAudioData:
        return QStringLiteral("Invalid or incomplete WAV file. Choose another Source.");
    case core::ErrorCode::InvalidArgument:
        return QStringLiteral("Choose one local WAV file.");
    default:
        return QStringLiteral("Source could not be opened. Choose another WAV file.");
    }
}

}  // namespace

SourceSelectionViewModel::SourceSelectionViewModel(QObject* parent)
    : QObject(parent)
{
}

bool SourceSelectionViewModel::has_source() const noexcept
{
    return source_.has_value();
}

QString SourceSelectionViewModel::source_state() const
{
    if (!errorMessage_.isEmpty()) {
        return QStringLiteral("SOURCE_ERROR");
    }
    return source_ ? QStringLiteral("SOURCE_READY") : QStringLiteral("NO_SOURCE");
}

QString SourceSelectionViewModel::display_name() const
{
    return displayName_;
}

QString SourceSelectionViewModel::container_label() const
{
    return containerLabel_;
}

QString SourceSelectionViewModel::sample_format_label() const
{
    return sampleFormatLabel_;
}

qint64 SourceSelectionViewModel::sample_rate_hz() const noexcept
{
    return sampleRateHz_;
}

QString SourceSelectionViewModel::channel_layout_label() const
{
    return channelLayoutLabel_;
}

int SourceSelectionViewModel::channel_count() const noexcept
{
    return channelCount_;
}

qint64 SourceSelectionViewModel::frame_count() const noexcept
{
    return frameCount_;
}

QString SourceSelectionViewModel::duration_label() const
{
    return durationLabel_;
}

bool SourceSelectionViewModel::read_only() const noexcept
{
    return source_.has_value();
}

QString SourceSelectionViewModel::error_message() const
{
    return errorMessage_;
}

void SourceSelectionViewModel::selectSource(const QUrl& selectedFile)
{
    if (!selectedFile.isValid() || !selectedFile.isLocalFile()) {
        publish_error(QStringLiteral("Choose one local WAV file."));
        return;
    }

    const auto localPath = selectedFile.toLocalFile();
    const auto displayName = QFileInfo{localPath}.fileName();
    const auto pathUtf8 = localPath.toUtf8();
    const auto displayNameUtf8 = displayName.toUtf8();

    auto reference = platform::windows::WindowsResourceReader::make_read_reference(
        std::string_view{pathUtf8.constData(), static_cast<std::size_t>(pathUtf8.size())},
        std::string_view{
            displayNameUtf8.constData(),
            static_cast<std::size_t>(displayNameUtf8.size()),
        });
    if (!reference) {
        publish_error(*reference.error());
        return;
    }

    auto reader = platform::windows::WindowsResourceReader::open_read_only(
        std::move(*reference.value()));
    if (!reader) {
        publish_error(*reader.error());
        return;
    }

    auto candidate = audio::SourceResource::probe(std::move(*reader.value()));
    if (!candidate) {
        publish_error(*candidate.error());
        return;
    }

    const auto& info = candidate.value()->wav_info();
    const auto sampleRate = info.audio_format().sample_rate().value();
    const auto frames = info.frame_count().value();

    displayName_ = QString::fromUtf8(
        candidate.value()->reference().display_name().data(),
        static_cast<qsizetype>(candidate.value()->reference().display_name().size()));
    containerLabel_ = container_label_for(info.container_kind());
    sampleFormatLabel_ = sample_format_label_for(info.encoded_sample_format());
    sampleRateHz_ = sampleRate;
    channelLayoutLabel_ = channel_layout_label_for(info.audio_format().channel_layout());
    channelCount_ = static_cast<int>(info.audio_format().channel_count());
    frameCount_ = frames;
    durationLabel_ = format_duration(frames, sampleRate);
    source_ = std::move(*candidate.value());

    const bool hadError = !errorMessage_.isEmpty();
    errorMessage_.clear();
    emit sourceChanged();
    emit sourceStateChanged();
    if (hadError) {
        emit errorChanged();
    }
}

void SourceSelectionViewModel::cancelSourceSelection() noexcept
{
    // A native picker cancellation is intentionally a complete no-op.
}

void SourceSelectionViewModel::publish_error(const QString& message)
{
    if (errorMessage_ == message) {
        return;
    }
    errorMessage_ = message;
    emit errorChanged();
    emit sourceStateChanged();
}

void SourceSelectionViewModel::publish_error(const core::Error& error)
{
    publish_error(user_message(error.code()));
}

}  // namespace rgsml::app
