#include "terminal_private_product_session.hpp"
#include <rgsml/core/error.hpp>
#include <QPointer>
#include <QThread>
#include <memory>
#include <new>
#include <utility>

namespace rgsml::app {
namespace {
[[nodiscard]] core::Status deny(const char* reason)
{
    return core::Status::failure(core::Error{
        core::ErrorCode::InvalidState,reason});
}
class M18ProductThreadCommitLease final
    :public platform::windows::M18ExportCommitLease {
public:
    using Checker=std::function<core::Status()>;
    explicit M18ProductThreadCommitLease(Checker checker)
        :checker_(std::move(checker)) {}
    [[nodiscard]] core::Status revalidate_current() const override
    {
        // Non-reentrant Product callback. A thrown approval check fails
        // closed rather than unwinding across Windows candidate commit.
        try{return checker_();}
        catch(...){return deny("M18_EXPORT_REVALIDATION_CALLBACK_THROWN");}
    }
private:
    Checker checker_;
};
} // namespace

core::Result<platform::windows::M18TransactionalAudioExportRequest>
TerminalPrivateProductSession::make_private_m18_synchronous_export_request(
    core::SampleRate selectedMasterRate,
    core::ResourceReference source,
    core::ResourceReference destination,
    audio::WavSampleFormat format,
    std::optional<std::uint64_t> seed,
    double ceilingDbTp,
    const M18ExplicitExportConsent& explicitConsent) const
{
    using Request=platform::windows::M18TransactionalAudioExportRequest;
    using R=core::Result<Request>;
    if(QThread::currentThread()!=thread())
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "M18_EXPORT_WRONG_QOBJECT_THREAD"});
    if(!explicitConsent)
        return R::failure(core::Error{core::ErrorCode::AccessDenied,
            "M18_EXPORT_EXPLICIT_CONSENT_MISSING"});
    core::Status first=deny("M18_EXPORT_CONSENT_CALLBACK_THROWN");
    try{first=explicitConsent();}
    catch(...){}
    if(!first)return R::failure(*first.error());
    const auto stage=current_verified_private_master();
    if(!stage ||stage->rendered.output_rate!=selectedMasterRate
       ||!stage->rendered.terminal.checked.final_check)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "M18_EXPORT_VERIFIED_MASTER_NOT_CURRENT"});

    // Alias-retain the EXACT original M17 compensated PCM, not a copy,
    // SOURCE/DEVICE PCM or a newly prepared/quantized candidate.
    const auto* pcm=&stage->rendered.terminal.checked.terminal.compensated_pcm;
    const std::shared_ptr<const audio::AudioBuffer> master(stage,pcm);
    const QPointer<TerminalPrivateProductSession> weak(
        const_cast<TerminalPrivateProductSession*>(this));
    QThread* ownerThread=thread();
    // The factory and its lease enforce current live UI Project identity,
    // selected rate and fresh consent ON THE SAME owning thread. No support
    // for invoking the synchronous exporter on a worker thread: an async
    // lease that locks Single-Writer Epoch must be built separately.
    auto checker=[weak,ownerThread,stage,selectedMasterRate,
                  consent=explicitConsent]() -> core::Status {
        if(QThread::currentThread()!=ownerThread)
            return deny("M18_EXPORT_LEASE_WRONG_QOBJECT_THREAD");
        if(!weak)
            return deny("M18_EXPORT_OWNER_DESTROYED");
        try {
            auto permission=consent();
            if(!permission)return permission;
        } catch(...){
            return deny("M18_EXPORT_CONSENT_RECHECK_THROWN");
        }
        const auto current=weak->current_verified_private_master();
        if(!current ||current!=stage
           ||current->rendered.output_rate!=selectedMasterRate
           ||!current->rendered.terminal.checked.final_check)
            return deny("M18_EXPORT_SOURCE_OR_OUTPUT_EPOCH_CHANGED");
        return core::Status::success();
    };
    platform::windows::M18AcquireCommitLease leaseFactory=
        [checker]() -> core::Result<std::unique_ptr<
            platform::windows::M18ExportCommitLease>> {
        using Lease=platform::windows::M18ExportCommitLease;
        using LR=core::Result<std::unique_ptr<Lease>>;
        const auto current=checker();
        if(!current)return LR::failure(*current.error());
        try {
            std::unique_ptr<Lease> lease=
                std::make_unique<M18ProductThreadCommitLease>(checker);
            return LR::success(std::move(lease));
        } catch(const std::bad_alloc&) {
            return LR::failure(core::Error{core::ErrorCode::IoFailure,
                "M18_EXPORT_LEASE_ALLOCATION_FAILED"});
        }
    };
    return Request::create(
        std::move(source),std::move(master),std::move(destination),
        format,seed,ceilingDbTp,
        platform::windows::ExportCollisionPolicy::FAIL_IF_EXISTS,
        std::move(leaseFactory));
}
} // namespace rgsml::app
