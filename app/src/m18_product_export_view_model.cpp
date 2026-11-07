#include "m18_product_export_view_model.hpp"
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <utility>

namespace rgsml::app {

M18ProductExportViewModel::M18ProductExportViewModel(
    Action& action,Controllers controllers,QObject* parent)
    :QObject(parent),action_(&action),controllers_(controllers)
{
    pollTimer_.setInterval(120);
    connect(&pollTimer_,&QTimer::timeout,
            this,&M18ProductExportViewModel::poll);
    connect(&controllers_.session,
            &TerminalPrivateProductSession::changed,
            this,&M18ProductExportViewModel::refresh);
    connect(&controllers_.upstream.source,
            &SourceSelectionViewModel::sourceChanged,
            this,&M18ProductExportViewModel::refresh);
    connect(&controllers_.upstream.rate,
            &MasterOutputRateState::changed,
            this,&M18ProductExportViewModel::refresh);
}

bool M18ProductExportViewModel::owner_thread() const noexcept
{
    return QThread::currentThread()==thread();
}

bool M18ProductExportViewModel::can_export() const
{
    if(!owner_thread()||action_->has_unacknowledged_export()
       ||!internal::M18ProductCompleteBinding::installed(controllers_)
       ||!controllers_.upstream.source.has_source()
       ||!controllers_.upstream.rate.has_supported_master_rate())
        return false;
    const auto rate=core::SampleRate::create(
        controllers_.upstream.rate.master_rate_hz());
    return rate &&static_cast<bool>(
        controllers_.session.capture_m18_verified_stage_for_worker(
            *rate.value()));
}

bool M18ProductExportViewModel::can_cancel() const
{
    return owner_thread()&&action_->can_cancel();
}

bool M18ProductExportViewModel::can_acknowledge() const
{
    return owner_thread()&&action_->can_acknowledge();
}

bool M18ProductExportViewModel::export_in_progress() const
{
    if(!owner_thread())return false;
    const auto phase=action_->snapshot().phase;
    return phase==internal::M18PrivateAsyncExportSession::Phase::
               ENCODING_OR_VERIFYING
        ||phase==internal::M18PrivateAsyncExportSession::Phase::CANCELLING;
}

double M18ProductExportViewModel::encoded_progress() const
{
    if(!owner_thread())return 0.0;
    const auto s=action_->snapshot();
    return s.total_frames==0?0.0
        :std::clamp(static_cast<double>(s.encoded_frames)
                    /static_cast<double>(s.total_frames),0.0,1.0);
}

QString M18ProductExportViewModel::status_text() const
{
    if(!owner_thread())return QStringLiteral("UNAVAILABLE");
    switch(action_->snapshot().phase) {
    case internal::M18PrivateAsyncExportSession::Phase::IDLE:
        return QStringLiteral("IDLE");
    case internal::M18PrivateAsyncExportSession::Phase::ENCODING_OR_VERIFYING:
        return QStringLiteral("EXPORTING / VERIFYING");
    case internal::M18PrivateAsyncExportSession::Phase::CANCELLING:
        return QStringLiteral("CANCELLING");
    case internal::M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED:
        return QStringLiteral("VERIFIED DELIVERED");
    case internal::M18PrivateAsyncExportSession::Phase::FAILED:
        return QStringLiteral("FAILED");
    case internal::M18PrivateAsyncExportSession::Phase::CANCELLED:
        return QStringLiteral("CANCELLED");
    }
    return QStringLiteral("UNAVAILABLE");
}

QString M18ProductExportViewModel::error_text() const
{
    if(!owner_thread())return {};
    if(!lastError_.isEmpty())return lastError_;
    if(action_->snapshot().phase==
       internal::M18PrivateAsyncExportSession::Phase::FAILED)
        return QStringLiteral(
            "WAV export failed. Check the destination and available space.");
    return {};
}

QString M18ProductExportViewModel::verified_sha256() const
{
    if(!owner_thread())return {};
    const auto s=action_->snapshot();
    if(s.phase!=internal::M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED)
        return {};
    return QString::fromStdString(s.verified_encoded_sha256);
}

qulonglong M18ProductExportViewModel::verified_bytes() const
{
    if(!owner_thread())return 0;
    const auto s=action_->snapshot();
    if(s.phase!=internal::M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED
       ||!s.verified_file_size_bytes)return 0;
    return static_cast<qulonglong>(*s.verified_file_size_bytes);
}

bool M18ProductExportViewModel::beginExport(
    const QUrl& destination,int formatIndex)
{
    if(!owner_thread())return false;
    lastError_.clear();
    const auto path=destination.toLocalFile();
    const auto filename=QFileInfo{path}.fileName();
    if(!destination.isLocalFile()||path.isEmpty()
       ||filename.isEmpty()||formatIndex<0||formatIndex>1
       ||!(filename.endsWith(QStringLiteral(".wav"),Qt::CaseInsensitive)
          ||filename.endsWith(QStringLiteral(".wave"),Qt::CaseInsensitive))) {
        lastError_=QStringLiteral("Choose a local .wav output file.");
        emit changed();
        return false;
    }
    auto ref=platform::windows::WindowsResourceWriter::
        make_write_reference(path.toUtf8().toStdString(),
                             filename.toUtf8().toStdString());
    if(!ref) {
        lastError_=QStringLiteral("The selected output location is invalid.");
        emit changed();
        return false;
    }
    const auto format=formatIndex==0
        ?audio::WavSampleFormat::PCM_S24
        :audio::WavSampleFormat::IEEE_F32;
    const auto seed=formatIndex==0
        ?std::optional<std::uint64_t>{UINT64_C(0x5247534d31380001)}
        :std::nullopt;
    const auto ceiling=
        controllers_.upstream.terminal.parameters().ceiling_db_tp();
    const auto begun=action_->begin(
        std::move(*ref.value()),format,seed,ceiling,true);
    if(!begun) {
        lastError_=QStringLiteral(
            "Export unavailable. Verify the Master and output location.");
        emit changed();
        return false;
    }
    pollTimer_.start();
    poll();
    return true;
}

void M18ProductExportViewModel::cancelExport()
{
    if(!owner_thread()||!action_->can_cancel())return;
    action_->cancel();
    poll();
}

bool M18ProductExportViewModel::acknowledgeExport()
{
    if(!owner_thread()||!action_->can_acknowledge())return false;
    const auto accepted=action_->acknowledge();
    if(!accepted) {
        lastError_=QStringLiteral("Export result could not be acknowledged.");
        emit changed();
        return false;
    }
    lastError_.clear();
    pollTimer_.stop();
    emit changed();
    return true;
}

void M18ProductExportViewModel::refresh()
{
    if(!owner_thread())return;
    emit changed();
}

void M18ProductExportViewModel::poll()
{
    if(!owner_thread())return;
    const auto p=action_->snapshot().phase;
    if(p==internal::M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED
       ||p==internal::M18PrivateAsyncExportSession::Phase::FAILED
       ||p==internal::M18PrivateAsyncExportSession::Phase::CANCELLED
       ||p==internal::M18PrivateAsyncExportSession::Phase::IDLE)
        pollTimer_.stop();
    emit changed();
}

} // namespace rgsml::app
