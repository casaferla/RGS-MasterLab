#include "m18_product_export_view_model.hpp"
#include "internal/m23_alpha_diagnostic_report.hpp"
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <array>
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
    // This MUST agree with Product consent authority. M17 READY alone
    // cannot prove that the chain, source or rate still owns its stage.
    return owner_thread()&&action_->can_begin_verified_export();
}

QString M18ProductExportViewModel::export_readiness_reason() const
{
    if(!owner_thread())return QStringLiteral("OWNER THREAD");
    return action_->export_readiness_reason();
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
    const auto s=action_->snapshot();
    if(s.phase!=internal::M18PrivateAsyncExportSession::Phase::FAILED)
        return {};
    // Classify only fixed, internally known exporter reasons. Raw error
    // strings, selected user paths and unverified candidate filenames
    // must never become a QML-visible diagnostic or an export success.
    using Reason=internal::M18PrivateAsyncExportSession::FailureReason;
    switch(s.failure_reason) {
    case Reason::DELIVERED_TP_ABOVE_CEILING:
        return QStringLiteral(
            "The encoded WAV exceeded the verified M17 True Peak Ceiling "
            "after bit-depth conversion/dither. No WAV was delivered. "
            "Changing the destination folder cannot fix this audio "
            "conformance failure.");
    case Reason::DESTINATION_EXISTS:
        return QStringLiteral(
            "A file with that name already exists. Choose a new WAV name; "
            "existing files are never overwritten.");
    case Reason::SOURCE_COLLISION:
        return QStringLiteral(
            "The destination is the selected SOURCE or its file alias. "
            "Choose a different, new WAV file.");
    case Reason::PRODUCT_EPOCH_REVOKED:
        return QStringLiteral(
            "The SOURCE, Project or verified Master changed while exporting. "
            "Re-Prepare the current Master before exporting again.");
    case Reason::UNKNOWN:
        break;
    }
    // Fallback is a stable error category, never an unchecked
    // filesystem message or unverified output pathname.
    if(s.failure_code) {
        switch(*s.failure_code) {
        case core::ErrorCode::AccessDenied:
            return QStringLiteral(
                "Export denied. Choose a writable folder and a new filename.");
        case core::ErrorCode::ResourceNotFound:
            return QStringLiteral(
                "The destination folder or source is no longer available.");
        case core::ErrorCode::InvalidState:
            return QStringLiteral(
                "Export was refused: the verified Master, output path or "
                "Product authorization is no longer valid. "
                "Check the filename and Prepare the Master again if needed.");
        case core::ErrorCode::UnsupportedAudioEncoding:
        case core::ErrorCode::UnsupportedAudioLayout:
            return QStringLiteral(
                "This WAV format or channel layout is not supported.");
        case core::ErrorCode::InvalidAudioSample:
        case core::ErrorCode::MalformedAudioContainer:
        case core::ErrorCode::TruncatedAudioData:
            return QStringLiteral(
                "The exported WAV did not pass audio verification. "
                "No verified delivery can be claimed.");
        default:
            break;
        }
    }
    return QStringLiteral(
        "WAV delivery failed. Check the destination, free disk space "
        "and filename; existing files are never overwritten.");
}

QString M18ProductExportViewModel::diagnostic_summary() const
{
    // M23 read-only local projection of the actual M18 terminal failure
    // code. The worker discards untrusted raw Error.message and never
    // publishes a stage-specific fault location: preserve UNKNOWN.
    if(!owner_thread())return {};
    const auto outcome=action_->snapshot();
    if(outcome.phase!=internal::M18PrivateAsyncExportSession::Phase::FAILED
       ||!outcome.failure_code)return {};
    const auto status=core::Status::failure(core::Error{*outcome.failure_code});
    const std::array observed{
        internal::M23FailedStatus{internal::M23Operation::EXPORT,
            internal::M23Stage::UNKNOWN,&status}};
    const auto report=internal::AlphaDiagnosticReport::from_failed_statuses(
        observed,internal::M23SafetyEvidence{});
    if(!report)return {};
    return QString::fromStdString(report.value()->sanitized_summary());
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
    // Store the explicit WAV choice in the Project's canonical output
    // preference. This may NOT mint an M18 epoch ticket or silently
    // coerce the complete uint64 PCM24 TPDF seed through QML Number.
    if(!controllers_.upstream.project.chooseM18WavEncoding(formatIndex)) {
        lastError_=QStringLiteral(
            "The Project is busy; WAV format selection was not accepted.");
        emit changed();
        return false;
    }
    const auto format=formatIndex==0
        ?audio::WavSampleFormat::PCM_S24
        :audio::WavSampleFormat::IEEE_F32;
    const auto seed=formatIndex==0
        ?std::optional<std::uint64_t>{
            controllers_.upstream.project.m18_pcm24_seed()}
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
