#include "m18_private_export_progress_view_model.hpp"
#include <rgsml/core/error.hpp>
#include <QThread>
#include <algorithm>
#include <utility>

namespace rgsml::app {

namespace {
using Phase=internal::M18PrivateAsyncExportSession::Phase;
}
M18PrivateExportProgressViewModel::M18PrivateExportProgressViewModel(
    QObject* parent)
    :QObject(parent)
{
    pollTimer_.setInterval(80);
    pollTimer_.setSingleShot(false);
    connect(&pollTimer_,&QTimer::timeout,this,
            &M18PrivateExportProgressViewModel::poll);
}

M18PrivateExportProgressViewModel::~M18PrivateExportProgressViewModel()
{
    // The worker holds its own immutable MASTER, value references and
    // Qt-free epoch factory; cancel without blocking Qt shutdown.
    pollTimer_.stop();
    session_.cancel();
}

core::Status M18PrivateExportProgressViewModel::begin_private_export(
    const TerminalPrivateProductSession& sourceSession,
    const SourceSelectionViewModel& source,
    const MasterOutputRateState& rate,
    const TerminalCandidateGate& gate,
    const platform::windows::internal::
        M18ProductEpochTicketAuthority::Ticket& ticket,
    core::ResourceReference selectedSource,
    core::ResourceReference destination,
    audio::WavSampleFormat format,
    std::optional<std::uint64_t> seed,
    double ceilingDbTp)
{
    if(QThread::currentThread()!=thread())
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_EXPORT_VIEWMODEL_START_REQUIRES_QT_OWNER_THREAD"});
    const auto started=session_.begin(
        sourceSession,source,rate,gate,ticket,
        std::move(selectedSource),std::move(destination),format,
        seed,ceilingDbTp);
    if(!started)return started;
    snapshot_=session_.snapshot();
    pollTimer_.start();
    emit changed();
    return core::Status::success();
}

QString M18PrivateExportProgressViewModel::status_text() const
{
    switch(snapshot_.phase) {
    case Phase::IDLE:return QStringLiteral("IDLE");
    case Phase::ENCODING_OR_VERIFYING:
        return snapshot_.total_frames>0
            &&snapshot_.encoded_frames>=snapshot_.total_frames
                ?QStringLiteral("VERIFYING")
                :QStringLiteral("ENCODING");
    case Phase::CANCELLING:return QStringLiteral("CANCELLING");
    case Phase::VERIFIED_DELIVERED:
        return QStringLiteral("VERIFIED_DELIVERED");
    case Phase::FAILED:return QStringLiteral("FAILED");
    case Phase::CANCELLED:return QStringLiteral("CANCELLED");
    }
    return QStringLiteral("FAILED");
}

double M18PrivateExportProgressViewModel::encoded_progress() const noexcept
{
    if(snapshot_.total_frames==0)return 0.0;
    return std::min(1.0,static_cast<double>(snapshot_.encoded_frames)/
        static_cast<double>(snapshot_.total_frames));
}

bool M18PrivateExportProgressViewModel::export_in_progress() const noexcept
{
    return snapshot_.phase==Phase::ENCODING_OR_VERIFYING
        ||snapshot_.phase==Phase::CANCELLING;
}

bool M18PrivateExportProgressViewModel::can_cancel() const noexcept
{
    return snapshot_.phase==Phase::ENCODING_OR_VERIFYING;
}

bool M18PrivateExportProgressViewModel::can_acknowledge() const noexcept
{
    return snapshot_.phase==Phase::VERIFIED_DELIVERED
        ||snapshot_.phase==Phase::FAILED
        ||snapshot_.phase==Phase::CANCELLED;
}

bool M18PrivateExportProgressViewModel::verified_delivered() const noexcept
{
    return snapshot_.phase==Phase::VERIFIED_DELIVERED;
}

QString M18PrivateExportProgressViewModel::error_text() const
{
    if(!snapshot_.failure_code)return {};
    const auto token=core::error_code_token(*snapshot_.failure_code);
    return QString::fromLatin1(
        token.data(),static_cast<qsizetype>(token.size()));
}

void M18PrivateExportProgressViewModel::cancelExport()
{
    if(QThread::currentThread()!=thread())return;
    session_.cancel();
    poll();
}

bool M18PrivateExportProgressViewModel::acknowledgeExport()
{
    if(QThread::currentThread()!=thread())return false;
    const auto acknowledged=session_.acknowledge_finished();
    if(!acknowledged)return false;
    pollTimer_.stop();
    snapshot_=session_.snapshot();
    emit changed();
    return true;
}

void M18PrivateExportProgressViewModel::poll()
{
    if(QThread::currentThread()!=thread())return;
    const auto next=session_.snapshot();
    if(next.phase!=snapshot_.phase
       ||next.encoded_frames!=snapshot_.encoded_frames
       ||next.total_frames!=snapshot_.total_frames
       ||next.failure_code!=snapshot_.failure_code) {
        snapshot_=next;
        emit changed();
    }
    if(snapshot_.phase==Phase::VERIFIED_DELIVERED
       ||snapshot_.phase==Phase::FAILED
       ||snapshot_.phase==Phase::CANCELLED)
        pollTimer_.stop();
}

} // namespace rgsml::app
