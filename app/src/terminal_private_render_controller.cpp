#include "terminal_private_render_controller.hpp"
#include <rgsml/core/error.hpp>
#include "private_io/phase11_pinned_private_fir_loader.hpp"
#include <numeric>
#include <QMetaObject>
#include <QDebug>
#include <QSettings>
#include <QThread>
#include <QUrl>
#include <chrono>
#include <algorithm>
#include <limits>
#include <new>
#include <utility>
namespace rgsml::app {
TerminalPrivateRenderController::TerminalPrivateRenderController(
    TerminalCandidateGate& gate,TerminalLimiterState& terminal,
    Snapshot prepared,Snapshot manual,ManualId id,ManualChain chain,QObject* parent)
    : QObject(parent),gate_(&gate),terminal_(&terminal),
      prepared_(std::move(prepared)),manual_(std::move(manual)),
      id_(std::move(id)),chain_(std::move(chain)),
      worker_([this]{worker_loop();}) {}
TerminalPrivateRenderController::~TerminalPrivateRenderController() {
    // Cancel an in-flight resource before joining; never wait for a retired
    // entire 12s or 3min M17 render if a block boundary can stop it.
    activeSerial_.fetch_add(1,std::memory_order_release);
    { std::lock_guard lock(mutex_);stopping_=true;pending_.reset(); }
    cv_.notify_all();
    if(worker_.joinable())worker_.join();
}
void TerminalPrivateRenderController::invalidate() {
    // This is a QObject-owned mutation. Check affinity BEFORE reading the
    // candidate's non-atomic M18 std::function installation state; a
    // detached worker may not race guard changes or retire M17 PCM.
    if(QThread::currentThread()!=thread())return;
    // A legacy void caller cannot report BUSY. Once the M18 Product
    // candidate authority is installed, the entire renderer withdrawal
    // must be admitted FIRST. Silently changing the render serial while
    // the candidate gate refuses to retire a worker-owned PCM would
    // break the verified-M17/worker single-writer invariant.
    if(gate_->m18_mutation_admission_installed()) {
        static_cast<void>(try_invalidate_for_m18());
        return;
    }
    // Without M18 enrollment, preserve all M17 cancel/return behavior.
    invalidate_local_after_candidate_gate(false);
}
core::Status TerminalPrivateRenderController::try_invalidate_for_m18() {
    if(QThread::currentThread()!=thread())
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_RENDER_INVALIDATION_REQUIRES_QT_OWNER_THREAD"});
    // Admission MUST precede serial, progress, M17 candidate, and pending
    // worker changes. A BUSY rejection leaves all these exactly untouched.
    const auto allowed=gate_->try_invalidate_for_m18();
    if(!allowed)return allowed;
    invalidate_local_after_candidate_gate(true);
    return core::Status::success();
}
void TerminalPrivateRenderController::invalidate_local_after_candidate_gate(
    bool gateAlreadyInvalidated) {
    ++serial_;
    activeSerial_.store(serial_,std::memory_order_release);
    progressPhase_.store(0,std::memory_order_release);
    progressFrames_.store(0,std::memory_order_relaxed);
    progressTotal_.store(0,std::memory_order_relaxed);
    if(!gateAlreadyInvalidated)gate_->invalidate();
    lastApprovedPrepareTiming_.reset(); // Revoked PCM can never calibrate ETA.
    {std::lock_guard lock(mutex_);pending_.reset();}
    status_=QStringLiteral("IDLE");error_.clear();lastError_.reset();emit changed();
}
void TerminalPrivateRenderController::set_original_q0_fir_root(
    std::filesystem::path root)
{
    // Startup and explicit UI selection only: a worker may own a snapshot
    // of the previous trusted root but must never observe a path mutation.
    if(QThread::currentThread()!=thread())return;
    privateOriginalFirFamilyReady_=false;
    privateOriginalFirRoot_.clear();
    originalQ0Status_=QStringLiteral("14 original Q0 FIRs not available");
    if(!root.empty()
       &&audio::internal::all_14_phase11_original_pins_present(root)) {
        privateOriginalFirRoot_=std::move(root);
        privateOriginalFirFamilyReady_=true;
        originalQ0Status_=QStringLiteral(
            "14 original Q0 FIRs verified · reusable for future FQ");
    }
    emit originalQ0FamilyChanged();
}

bool TerminalPrivateRenderController::remember_existing_original_q0_folder(
    const QUrl& folder)
{
    // The chooser is NOT a ZIP installer, verifier script, network fetch,
    // FIR generator or final Q1-Q6 release authorization.
    if(QThread::currentThread()!=thread()
       ||status_!=QStringLiteral("IDLE")
       ||!folder.isValid()||!folder.isLocalFile()
       ||!qgetenv("RGSML_FINAL_SRC_Q0_PRIVATE_DIR").isEmpty()) {
        originalQ0Status_=QStringLiteral(
            "Stop M17 and clear any explicit FIR override before selection");
        emit originalQ0FamilyChanged();
        return false;
    }
    const auto local=folder.toLocalFile();
    if(local.isEmpty())return false;
#ifdef _WIN32
    const auto candidate=std::filesystem::path{local.toStdWString()};
#else
    const auto candidate=std::filesystem::path{local.toStdString()};
#endif
    // Check all 14 frozen, SHA-pinned original RGSMSR1 files BEFORE writing
    // any persistent user preference. Symlinks/missing or corrupt files fail.
    if(!audio::internal::all_14_phase11_original_pins_present(candidate)) {
        originalQ0Status_=QStringLiteral(
            "Selected folder is not the verified original 14-Q0 FIR bank");
        emit originalQ0FamilyChanged();
        return false;
    }
    QSettings preferences{QSettings::IniFormat,QSettings::UserScope,
        QStringLiteral("RGS MasterLab"),QStringLiteral("RGS MasterLab")};
    // A path is only a locator, never a trusted hash or certificate.
    preferences.setValue(QStringLiteral("privateQ0/originalDirectory"),local);
    preferences.sync();
    if(preferences.status()!=QSettings::NoError) {
        originalQ0Status_=QStringLiteral("Unable to remember private FIR folder");
        emit originalQ0FamilyChanged();
        return false;
    }
    set_original_q0_fir_root(candidate);
    return privateOriginalFirFamilyReady_;
}
core::Status TerminalPrivateRenderController::request_private(core::SampleRate rate) {
    // The worker never starts a new M17 prepare nor reads gate internals.
    // Check QObject affinity before querying non-atomic Product guard.
    if(QThread::currentThread()!=thread())
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_RENDER_PREPARE_REQUIRES_QT_OWNER_THREAD"});
    // First authorization BEFORE clearing the M17 stage, advancing the
    // renderer serial or touching any pending job. In an M18-aware Product,
    // BUSY means no mutation whatsoever. Unconfigured M17 keeps the exact
    // legacy invalidation sequence.
    if(gate_->m18_mutation_admission_installed()) {
        const auto authorized=try_invalidate_for_m18();
        if(!authorized)return authorized;
    }else {
        invalidate();
    }
    if(!prepared_||!manual_||!id_||!chain_)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,"Private M17 render owner providers missing."});
    auto prepared=prepared_();auto manual=manual_();
    auto id=id_();auto chain=chain_();
    auto captured=gate_->capture(prepared,manual,id,chain,*terminal_,rate);
    if(!captured) {
        status_=QStringLiteral("ERROR");
        lastError_=*captured.error();
        error_=QString::fromStdString(lastError_->message());
        emit changed();
        return core::Status::failure(*captured.error());
    }
    // A verified-M17 candidate must not be silently retained when
    // authentication of the selected original Q0 bank fails. If an
    // M18 file commit acquired the exact epoch, report BUSY rather than
    // cancelling the renderer locally and leaving stage provenance split.
    const auto retire_q0_failed_candidate=[this]()->core::Status {
        if(gate_->m18_mutation_admission_installed())
            return try_invalidate_for_m18();
        invalidate();
        return core::Status::success();
    };
    std::vector<std::uint8_t> originalFir;
    const auto fin=captured.value()->manual_owner->view()
        .format().sample_rate().value();
    const auto fout=rate.value();
    if(fin!=fout &&privateOriginalFirFamilyReady_) {
        const auto g=std::gcd(fin,fout);
        const auto* pin=audio::internal::phase11_frozen_original_fir_pin(
            static_cast<std::uint32_t>(fout/g),
            static_cast<std::uint32_t>(fin/g));
        if(!pin) {
            if(const auto retired=retire_q0_failed_candidate();!retired)
                return retired;
            return core::Status::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "M17_FINAL_SRC_Q0_RATIO_NOT_FROZEN"});
        }
        auto loaded=audio::internal::load_phase11_original_pinned_private_fir(
            privateOriginalFirRoot_,pin->L,pin->M);
        if(!loaded) {
            // Re-authenticate the actual file per render. A changed/unsafe
            // asset never falls back to experimental or the legacy path.
            if(const auto retired=retire_q0_failed_candidate();!retired)
                return retired;
            return core::Status::failure(*loaded.error());
        }
        originalFir=std::move(*loaded.value());
    }
    auto block=core::FrameCount::create(4096);
    if(!block)return core::Status::failure(*block.error());
    Job job{serial_,std::move(*captured.value()),std::move(chain),
        terminal_->chain(),terminal_->execution_bindings(),*block.value(),
        std::chrono::steady_clock::now(),std::move(originalFir)};
    {std::lock_guard lock(mutex_);pending_.emplace(std::move(job));}
    status_=QStringLiteral("RENDERING");error_.clear();
    cv_.notify_one();emit changed();return core::Status::success();
}
void TerminalPrivateRenderController::worker_loop() {
    for(;;) {
        std::optional<Job> job;
        {std::unique_lock lock(mutex_);
         cv_.wait(lock,[this]{return stopping_||pending_.has_value();});
         if(stopping_)return;
         job.swap(pending_);}
        if(!job)continue;
        const auto started=std::chrono::steady_clock::now();
        auto captured=job->capture;
        // The predicate never touches Qt, DSP state or floating-point data;
        // it only observes the serial of this immutable private job.
        const render::TerminalRenderCancel cancelled=[this,serial=job->serial] {
            return activeSerial_.load(std::memory_order_acquire)!=serial;
        };
        render::TerminalRenderStageTimings stages{};
        stages.progress=[this,serial=job->serial](
            render::TerminalRenderProgress observation) {
            if(activeSerial_.load(std::memory_order_acquire)!=serial)
                return;
            // A passive atomic sidecar; no QObject/QML work on worker thread.
            // Phase is published last, so a new stage has current counters.
            progressFrames_.store(observation.processed_frames,
                                  std::memory_order_relaxed);
            progressTotal_.store(observation.total_frames,
                                 std::memory_order_relaxed);
            progressPhase_.store(static_cast<std::uint8_t>(observation.phase),
                                 std::memory_order_release);
        };
        std::optional<render::TruePeakLimiterOwnedMeasuredNonconformingCandidate> recovered;
        // Count only real entered M17 whole-resource executions; never a
        // device SRC, playback restart, loop/seek or inspector refresh.
        renderInvocationCount_.fetch_add(1,std::memory_order_release);
        auto result=render::render_true_peak_limiter_owned_output_rate_candidate(
            captured.manual_owner,job->manualChain,
            captured.full_source_frames,captured.manual_chain_revision,
            captured.output_rate,job->terminalChain,job->bindings,
            registry_,job->block,cancelled,&stages,true,&recovered,
            std::span<const std::uint8_t>{job->authenticatedOriginalFir});
        const auto finished=std::chrono::steady_clock::now();
        const auto queueMs=std::chrono::duration_cast<
            std::chrono::milliseconds>(started-job->enqueuedAt).count();
        const auto renderMs=std::chrono::duration_cast<
            std::chrono::milliseconds>(finished-started).count();
        // One diagnostic per completed job, never per DSP frame/tick.
        // No filenames, samples, rendering shortcuts or invented progress.
        qInfo().nospace() << "RGS M17 render serial=" << job->serial
            << " queue_ms=" << queueMs
            << " render_ms=" << renderMs
            << " src_us=" << stages.src_us
            << " m17_core_us=" << stages.m17_core_us
            << " m16_primary_us=" << stages.m16_primary_us
            << " trim_us=" << stages.verification_trim_us
            << " m16_secondary_us=" << stages.m16_secondary_us
            << " result=" << (result ? "accepted_for_gate" : "failure");
        if(!result && stages.verification_ceiling_linear>0.0) {
            // No PCM, personal audio, file path or guessed diagnosis here.
            // This scalar-only bounded diagnostic distinguishes excess by
            // a few binary64 ULPs from a genuine limiter/verification gap.
            qInfo().nospace() << "RGS M17 verification serial=" << job->serial
                << " ceiling_linear=" << QString::number(
                    stages.verification_ceiling_linear,'g',17)
                << " before_linear=" << QString::number(
                    stages.verification_before_linear_peak,'g',17)
                << " global_trim_gain=" << QString::number(
                    stages.verification_trim_gain,'g',17)
                << " after_linear=" << QString::number(
                    stages.verification_after_linear_peak,'g',17)
                << " excess_ulps=" << stages.verification_post_trim_excess_ulps
                << " post_m16_unsupported="
                << stages.verification_post_trim_m16_unsupported;
        }
        // Snapshot scalars only. The completion gate, on the owning GUI
        // thread, decides whether this genuine Prepare is still current and
        // accepted. No stale/failure/nonconforming run enters the profile.
        const ApprovedPrepareTiming observed{
            captured.manual_owner->view().format().sample_rate(),
            captured.output_rate,captured.full_source_frames,
            captured.manual_owner->view().format().channel_layout(),
            captured.ceiling_db_tp,
            captured.manual_chain_revision,
            captured.terminal_chain_revision,
            captured.terminal_parameter_revision,
            std::chrono::duration_cast<std::chrono::microseconds>(
                started-job->enqueuedAt).count(),
            std::chrono::duration_cast<std::chrono::microseconds>(
                finished-started).count(),
            stages.src_us,stages.m17_core_us,stages.m16_primary_us,
            stages.verification_trim_us,stages.m16_secondary_us};
        auto delivery=std::make_shared<
            core::Result<render::TruePeakLimiterOwnedSourceCandidate>>(
            std::move(result));
        auto retained=std::make_shared<std::optional<
            render::TruePeakLimiterOwnedMeasuredNonconformingCandidate>>(
                std::move(recovered));
        static_cast<void>(QMetaObject::invokeMethod(
            this,[this,serial=job->serial,captured,delivery,retained,observed] {
                complete(serial,captured,delivery,retained,observed);
            },Qt::QueuedConnection));
    }
}
void TerminalPrivateRenderController::complete(
    std::uint64_t serial,TerminalCandidateGate::Capture captured,
    std::shared_ptr<core::Result<render::TruePeakLimiterOwnedSourceCandidate>> outcome,
    std::shared_ptr<std::optional<render::TruePeakLimiterOwnedMeasuredNonconformingCandidate>> recovered,
    ApprovedPrepareTiming observed)
{
    if(serial!=serial_||!prepared_||!manual_||!id_||!chain_
       ||prepared_()!=captured.prepared_owner
       ||manual_()!=captured.manual_owner
       ||id_()!=std::optional{captured.manual_realization_id}) {
        ++staleDiscarded_;return;
    }
    if(!outcome||!*outcome) {
        if(recovered&&recovered->has_value()&&chain_) {
            auto accepted=gate_->accept_measured(captured,
                std::move(**recovered),prepared_(),manual_(),id_(),chain_(),
                *terminal_);
            if(accepted) {
                // Store the real measured PCM, but do not install into the
                // verified-only publisher, issue RealizationId, or export.
                status_=QStringLiteral("MEASURED_NONCONFORMING_PRIVATE");
                error_.clear();
                lastError_=outcome&&outcome->error()
                    ?std::optional<core::Error>{*outcome->error()}
                    :std::nullopt;
                emit changed();
                return;
            }
        }
        gate_->invalidate();
        status_=QStringLiteral("ERROR");
        lastError_=outcome&&outcome->error()
            ?std::optional<core::Error>{*outcome->error()}
            :std::optional<core::Error>{core::Error{
                core::ErrorCode::InvalidState,
                "M17 private rendering returned no output."}};
        error_=QString::fromStdString(lastError_->message());
        emit changed();return;
    }
    auto chain=chain_();
    auto accepted=gate_->accept(captured,std::move(*outcome->value()),
        prepared_(),manual_(),id_(),chain,*terminal_);
    if(!accepted) {
        ++staleDiscarded_;
        status_=QStringLiteral("STALE");error_.clear();emit changed();return;
    }
    // The only point at which a real whole-resource timing observation
    // becomes trustworthy: exact renderer owner and authoritative M17/M16
    // gate have both accepted the still-current immutable MASTER.
    if(approved_timing_eligible(observed)) {
        lastApprovedPrepareTiming_=observed;
        // History survives an ordinary cancel, never a stale PCM owner.
        try {
            constexpr std::size_t kCapacity=24;
            if(approvedPrepareHistory_.size()==kCapacity)
                approvedPrepareHistory_.erase(approvedPrepareHistory_.begin());
            approvedPrepareHistory_.push_back(observed);
        } catch(const std::bad_alloc&) {
            // Optional telemetry must not fail already accepted audio.
        }
    }
    status_=QStringLiteral("READY_PRIVATE");error_.clear();lastError_.reset();emit changed();
}
std::optional<TerminalPrivateRenderController::ApprovedPrepareHistoricalRange>
TerminalPrivateRenderController::exact_historical_prepare_range(
    core::SampleRate source,core::SampleRate master,
    core::FrameCount frames,audio::ChannelLayout layout,double ceiling,
    std::uint64_t manualRevision,std::uint64_t terminalChainRevision,
    std::uint64_t terminalParameterRevision) const noexcept
{
    // A single observed duration is not an interval; avoid invented
    // uncertainty. This is an exact-key empirical observation, NOT a
    // projection to a different song, build, chain or target format.
    std::int64_t low=std::numeric_limits<std::int64_t>::max();
    std::int64_t high=0;
    std::size_t samples=0;
    for(const auto& item:approvedPrepareHistory_) {
        if(item.source_rate!=source||item.master_rate!=master
           ||item.source_frames!=frames||item.layout!=layout
           ||item.ceiling_db_tp!=ceiling
           ||item.manual_chain_revision!=manualRevision
           ||item.terminal_chain_revision!=terminalChainRevision
           ||item.terminal_parameter_revision!=terminalParameterRevision
           ||!approved_timing_eligible(item))
            continue;
        const auto wall=item.queue_us+item.render_wall_us;
        low=std::min(low,wall);
        high=std::max(high,wall);
        ++samples;
    }
    if(samples<2)return std::nullopt;
    return ApprovedPrepareHistoricalRange{low,high,samples};
}
} // namespace rgsml::app
