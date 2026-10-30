#include "prepare_memory_advice_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "prepare_memory_advice_source_policy.hpp"
#include <rgsml/render/m17_offline_calibration.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <rgsml/core/frame_time.hpp>
#include <QMetaObject>
#include <cmath>
#include <thread>
#include <utility>

#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace rgsml::app {
namespace {
constexpr std::array<std::int64_t,6> kMasterRates{
    44100,48000,88200,96000,176400,192000};

[[nodiscard]] QString rate_label(std::int64_t hz)
{
    const auto decimals=(hz%1000)/100;
    return QString::number(hz/1000)
        +(decimals ? QStringLiteral(",")+QString::number(decimals) : QString())
        +QStringLiteral(" kHz");
}

[[nodiscard]] QVariantMap row(std::int64_t hz,const QString& label,
    const QString& kind,const QString& details)
{
    return QVariantMap{
        {QStringLiteral("rateLabel"),rate_label(hz)},
        {QStringLiteral("label"),label},
        {QStringLiteral("kind"),kind},
        {QStringLiteral("details"),details},
        {QStringLiteral("minimumLabel"),QString{}},
        {QStringLiteral("summary"),QString{}}};
}

[[nodiscard]] QVariantMap classified_row(
    const audio::MonitoringRateMemoryAdvice& advice,
    std::int64_t source_hz)
{
    using Kind=audio::MonitoringMemoryAdviceKind;
    const auto hz=advice.proposed_master_rate.value();
    QString label;
    QString kind;
    switch(advice.kind) {
    case Kind::RECOMMENDED_MEMORY_ONLY:
        label=QStringLiteral("Recommended");
        kind=QStringLiteral("recommended");
        break;
    case Kind::CAUTION_LIMITED_HEADROOM:
        label=QStringLiteral("Limited");
        kind=QStringLiteral("caution");
        break;
    case Kind::DISCOURAGED_INSUFFICIENT_HEADROOM:
        label=QStringLiteral("Insufficient");
        kind=QStringLiteral("discouraged");
        break;
    case Kind::UNKNOWN:
        label=QStringLiteral("Not assessed");
        kind=QStringLiteral("unknown");
        break;
    }
    constexpr std::uint64_t mib=UINT64_C(1024)*1024;
    constexpr std::uint64_t gib=mib*1024;
    const auto& resource=advice.resource;
    const auto& observed=advice.memory_observation;
    const auto firBytes=resource.experimental_fir_scratch_bytes;
    // Overflow cannot become a tiny, green memory recommendation.
    const bool overflow=firBytes>
        std::numeric_limits<std::uint64_t>::max()-resource.combined_pcm_bytes;
    const auto minimumBytes=overflow
        ? std::numeric_limits<std::uint64_t>::max()
        : resource.combined_pcm_bytes+firBytes;
    const auto roundedMib=minimumBytes/mib+(minimumBytes%mib!=0 ? 1U : 0U);
    const auto fmtGiB=[gib](std::uint64_t bytes) {
        return QString::number(static_cast<double>(bytes)/gib,'f',1)
            +QStringLiteral(" GiB");
    };
    QString details=QStringLiteral(
         "Known MASTER + DEVICE PCM lower bound: %1 MiB; FIR scratch %2 MiB. ")
        .arg(QString::number(resource.combined_pcm_bytes/mib
            +(resource.combined_pcm_bytes%mib!=0 ? 1U : 0U)),
             QString::number(firBytes/mib+(firBytes%mib!=0 ? 1U : 0U)));
    QString summary;
    if(observed.available_physical_bytes==0
       ||observed.available_commit_bytes==0) {
        summary=QStringLiteral(
            "Available RAM N/A · PCM+FIR >=%1 MiB · other DSP/Qt N/A")
            .arg(QString::number(roundedMib));
        details+=QStringLiteral(
             "Physical or commit memory could not be observed. ");
    } else {
        const auto available=std::min(observed.available_physical_bytes,
            observed.available_commit_bytes);
        const auto safety=std::max(UINT64_C(512)*mib,available/4);
        const auto remaining=advice.memory_headroom_after_reserve_bytes;
        summary=QStringLiteral(
            "Avail. %1 · safe budget %2 · PCM+FIR >=%3 MiB · DSP/Qt N/A")
            .arg(fmtGiB(available),fmtGiB(remaining),
                 QString::number(roundedMib));
        details+=QStringLiteral(
             "Available physical RAM %1; available commit %2; "
             "safety reserve %3; accounted other Prepare/DSP reserve %4; "
             "usable budget before PCM allocations %5. ")
            .arg(fmtGiB(observed.available_physical_bytes),
                 fmtGiB(observed.available_commit_bytes),fmtGiB(safety),
                 fmtGiB(observed.reserved_other_prepare_bytes),
                 fmtGiB(remaining));
    }
    details+=QStringLiteral(
         "The >= figure is ONLY a lower bound: additional SRC/M17/M16 peaks, "
         "Qt and render/export peaks are not yet measured or summed. "
        "No RAM is reserved; CPU/real-time performance is not guaranteed.");
    // Memory fitness is never permission to run an unqualified Final SRC.
    // Keep the hypothetical cost visible without presenting it as a usable
    // M17 Prepare recommendation. This is a display-only product guard.
    const auto admission=prepare_memory_display_admission(source_hz,hz);
    const bool candidateAdmissible=
        admission==PrepareMemoryDisplayAdmission::CANDIDATE_ADMISSIBLE;
    QString pathStatus;
    if(admission==PrepareMemoryDisplayAdmission::FINAL_SRC_UNQUALIFIED) {
        pathStatus=QStringLiteral("FINAL_SRC_UNQUALIFIED");
        label=QStringLiteral("SRC blocked");
        kind=QStringLiteral("blocked");
        const auto warning=QStringLiteral(
            "Final SRC UNQUALIFIED: M17 Prepare is unavailable for this "
            "SOURCE-to-MASTER pair. RAM is hypothetical, not a recommendation. ");
        details.prepend(warning);
        summary.prepend(QStringLiteral("SRC unavailable · "));
    } else if(admission==PrepareMemoryDisplayAdmission::SOURCE_OR_RATE_UNAVAILABLE) {
        pathStatus=QStringLiteral("RATE_UNAVAILABLE");
        label=QStringLiteral("Not available");
        kind=QStringLiteral("unknown");
        details.prepend(QStringLiteral(
            "SOURCE or MASTER rate is unsupported. RAM does not grant Prepare. "));
        summary.prepend(QStringLiteral("Rate unavailable · "));
    } else if(final_src_candidate_path(source_hz,hz)
              ==FinalSrcCandidatePath::EXISTING_44K_48K_FIR) {
        pathStatus=QStringLiteral("EXISTING_FIR_CANDIDATE");
        details.prepend(QStringLiteral(
            "Existing 44.1/48 FIR candidate is admitted by M17, "
            "but Final SRC release qualification is still pending. "));
    } else {
        pathStatus=QStringLiteral("IDENTITY");
        details.prepend(QStringLiteral(
            "Identity rate: no Final SRC required. "));
    }
    auto result=row(hz,label,kind,details);
    result[QStringLiteral("srcCandidateAdmissible")]=candidateAdmissible;
    result[QStringLiteral("srcCandidateStatus")]=pathStatus;
    result[QStringLiteral("minimumLabel")]=
        QStringLiteral(">=%1 MiB").arg(QString::number(roundedMib));
    result[QStringLiteral("summary")]=summary;
    return result;
}
} // namespace

PrepareMemoryAdviceViewModel::PrepareMemoryAdviceViewModel(
    const SourceSelectionViewModel* source,
    const platform::windows::WindowsAudioPlaybackService* playback,
    QObject* parent)
    :QObject(parent),source_(source),playback_(playback)
{
    reset_unknown(QStringLiteral("Load a source"));
    connect(source_,&SourceSelectionViewModel::sourceChanged,
        this,[this] {
            cancel_calibration();
            clear_calibration(QStringLiteral("SOURCE changed · calibrate again"));
        });
    calibrationThread_=std::jthread([this] {calibration_loop();});
}

PrepareMemoryAdviceViewModel::~PrepareMemoryAdviceViewModel()
{
    activeCalibrationSerial_.fetch_add(1,std::memory_order_release);
    {
        std::lock_guard lock(calibrationMutex_);
        stoppingCalibration_=true;
        pendingCalibration_.reset();
    }
    calibrationWake_.notify_one();
    if(calibrationThread_.joinable())calibrationThread_.join();
}

QVariantList PrepareMemoryAdviceViewModel::recommendations() const
{
    return recommendations_;
}

QString PrepareMemoryAdviceViewModel::status_text() const
{
    return statusText_;
}

void PrepareMemoryAdviceViewModel::reset_unknown(const QString& status)
{
    statusText_=status;
    recommendations_.clear();
    for(const auto hz:kMasterRates) {
        recommendations_.append(row(hz,
            QStringLiteral("Not assessed"),QStringLiteral("unknown"),
            QStringLiteral("No verified memory budget; "
                "no master rate is automatically recommended.")));
    }
    emit changed();
}

void PrepareMemoryAdviceViewModel::refresh()
{
    if(!source_||!source_->has_source()) {
        reset_unknown(QStringLiteral("Load a source"));
        return;
    }
    if(!playback_) {
        reset_unknown(QStringLiteral("Device unavailable"));
        return;
    }
    const auto sourceRate=core::SampleRate::create(source_->sample_rate_hz());
    const auto sourceFrames=core::FrameCount::create(source_->frame_count());
    const auto channelCount=source_->channel_count();
    if(!sourceRate||!sourceFrames||(channelCount!=1&&channelCount!=2)) {
        reset_unknown(QStringLiteral("Source format unavailable"));
        return;
    }
    const auto layout=channelCount==1
        ? audio::ChannelLayout::MONO_C
        : audio::ChannelLayout::STEREO_LR;
    // Snapshot is fresh for every Prepare opening, new Source and explicit
    // refresh. NO automatic output-rate change; no allocations of PCM.
    const auto advice=playback_->advise_prepare_master_rate_memory(
        *sourceRate.value(),*sourceFrames.value(),layout);
    if(!advice||advice.value()->size()!=kMasterRates.size()) {
        reset_unknown(QStringLiteral("Device or memory unavailable"));
        return;
    }
    // All six rate rows must describe one coherent DEVICE snapshot and
    // the exact standard order; never accidentally recommend stale/extra rows.
    const auto deviceRate=advice.value()->front().observed_device_rate;
    for(std::size_t i=0;i<kMasterRates.size();++i) {
        const auto& entry=(*advice.value())[i];
        if(entry.proposed_master_rate.value()!=kMasterRates[i]
           ||entry.observed_device_rate!=deviceRate) {
            reset_unknown(QStringLiteral("Incoherent device/rate advice"));
            return;
        }
    }
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(kMasterRates.size()));
    for(const auto& entry:*advice.value()) {
        rows.append(classified_row(entry,source_->sample_rate_hz()));
    }
    recommendations_=std::move(rows);
    const auto deviceHz=advice.value()->front().observed_device_rate.value();
    statusText_=QStringLiteral("DEVICE %1").arg(rate_label(deviceHz));
    emit changed();
}

void PrepareMemoryAdviceViewModel::clear_calibration(const QString& message)
{
    calibrationRunning_=false;
    completedCalibrationRates_=0;
    calibrationValid_=false;
    dspEstimates_.clear();
    calibratedCeiling_=0.0;
    calibrationStatus_=message;
    emit calibrationChanged();
}

void PrepareMemoryAdviceViewModel::cancel_calibration()
{
    ++calibrationSerial_;
    activeCalibrationSerial_.store(calibrationSerial_,std::memory_order_release);
    {
        std::lock_guard lock(calibrationMutex_);
        pendingCalibration_.reset();
    }
    if(calibrationRunning_)
        clear_calibration(QStringLiteral("Calibration cancelled · no partial ETA"));
}

bool PrepareMemoryAdviceViewModel::calibrate(double ceiling)
{
    if(calibrationRunning_)return false;
    if(!source_||!source_->has_source()
       ||!std::isfinite(ceiling))
        return false;
    const auto rate=source_->sample_rate_hz();
    const auto frames=source_->frame_count();
    const auto channels=source_->channel_count();
    if(std::find(kMasterRates.begin(),kMasterRates.end(),rate)
           ==kMasterRates.end()
       ||frames<=0 || (channels!=1&&channels!=2)
       ||!dsp::TruePeakLimiterParameters::create(ceiling))
        return false;
    ++calibrationSerial_;
    activeCalibrationSerial_.store(calibrationSerial_,std::memory_order_release);
    {
        std::lock_guard lock(calibrationMutex_);
        pendingCalibration_=CalibrationJob{
            calibrationSerial_,rate,frames,channels,ceiling};
    }
    clear_calibration(QStringLiteral(
        "Measuring real M17/M16 offline · no device, track or WAV"));
    calibrationRunning_=true;
    emit calibrationChanged();
    calibrationWake_.notify_one();
    return true;
}

void PrepareMemoryAdviceViewModel::calibration_loop()
{
    for(;;) {
        std::optional<CalibrationJob> job;
        {
            std::unique_lock lock(calibrationMutex_);
            calibrationWake_.wait(lock,[this] {
                return stoppingCalibration_ || pendingCalibration_.has_value();
            });
            if(stoppingCalibration_)return;
            job.swap(pendingCalibration_);
        }
        if(!job)continue;
        const auto cancelled=[this,serial=job->serial] {
            return activeCalibrationSerial_.load(std::memory_order_acquire)
                !=serial;
        };
        auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
        auto parameters=dsp::TruePeakLimiterParameters::create(job->ceiling);
        auto block=core::FrameCount::create(256);
        std::array<DspProbeEstimate,6> completed{};
        if(registry && parameters && block) {
            for(std::size_t index=0;index<kMasterRates.size();++index) {
                if(cancelled())break;
                const auto hz=kMasterRates[index];
                auto rate=core::SampleRate::create(hz);
                auto probe=core::FrameCount::create(hz/20); // 50ms genuine M17
                const auto q=job->sourceFrames/job->sourceRate;
                const auto r=job->sourceFrames%job->sourceRate;
                const auto tail=(r*hz+job->sourceRate-1)/job->sourceRate;
                if(q>(std::numeric_limits<std::int64_t>::max()-tail)/hz
                   ||!rate||!probe)continue;
                auto projected=core::FrameCount::create(q*hz+tail);
                if(!projected)continue;
                const auto layout=job->channels==1
                    ?audio::ChannelLayout::MONO_C
                    :audio::ChannelLayout::STEREO_LR;
                std::int64_t low=std::numeric_limits<std::int64_t>::max();
                std::int64_t high=0;
                bool valid=true;
                // Two deterministic programme classes, two repeated
                // authentic M17+M16 measurements per rate. Neither transient
                // success nor timing is silently fabricated on failure.
                for(const auto stimulus:{
                    render::M17CalibrationStimulus::QUIET_PROGRAMME,
                    render::M17CalibrationStimulus::TRANSIENT_STRESS}) {
                    for(int repeat=0;repeat<2;++repeat) {
                        if(cancelled()){valid=false;break;}
                        auto measured=render::calibrate_m17_offline_once(
                            *rate.value(),layout,*parameters.value(),
                            stimulus,*probe.value(),*registry.value(),
                            *block.value(),cancelled);
                        if(!measured){valid=false;break;}
                        auto estimate=render::estimate_m17_m16_microseconds(
                            *measured.value(),*rate.value(),layout,job->ceiling,
                            stimulus,*projected.value());
                        if(!estimate){valid=false;break;}
                        low=std::min(low,*estimate.value());
                        high=std::max(high,*estimate.value());
                    }
                    if(!valid)break;
                }
                if(valid && low>0 && high>=low)
                    completed[index]={low,high,true};
                if(cancelled())break;
                const auto finished=static_cast<int>(index)+1;
                static_cast<void>(QMetaObject::invokeMethod(this,
                    [this,serial=job->serial,finished] {
                        if(serial!=calibrationSerial_ || !calibrationRunning_)
                            return;
                        completedCalibrationRates_=finished;
                        calibrationStatus_=QStringLiteral(
                            "M17/M16 local calibration %1/6 rates · TOTAL ETA N/A")
                            .arg(finished);
                        emit calibrationChanged();
                    },Qt::QueuedConnection));
            }
        }
        if(cancelled())continue;
        // All QObjects and QVariant presentations are GUI-thread only.
        static_cast<void>(QMetaObject::invokeMethod(this,
            [this,serial=job->serial,ceiling=job->ceiling,completed] {
                finish_calibration(serial,ceiling,completed);
            },Qt::QueuedConnection));
    }
}

void PrepareMemoryAdviceViewModel::finish_calibration(
    std::uint64_t serial,double ceiling,
    std::array<DspProbeEstimate,6> results)
{
    if(serial!=calibrationSerial_ || !calibrationRunning_)return;
    const auto formatDuration=[](std::int64_t us)->QString {
        constexpr std::int64_t million=1'000'000;
        const auto seconds=std::max<std::int64_t>(
            1,us/million+(us%million!=0?1:0));
        if(seconds>=3600)
            return QStringLiteral("%1h").arg(seconds/3600);
        if(seconds>=60)
            return QStringLiteral("%1m").arg(seconds/60);
        return QStringLiteral("%1s").arg(seconds);
    };
    QVariantList rows;
    rows.reserve(6);
    int measured=0;
    for(const auto& result:results) {
        QVariantMap row;
        if(result.valid) {
            ++measured;
            const auto low=formatDuration(result.lowUs);
            const auto high=formatDuration(result.highUs);
            row[QStringLiteral("label")]=low==high
                ?QStringLiteral("~%1*").arg(low)
                :QStringLiteral("%1-%2*").arg(low,high);
            row[QStringLiteral("details")]=QStringLiteral(
                "LOCAL M17+M16 compute-time projection ONLY (%1 to %2); "
                "two actual quiet + two transient 50ms probes on this CPU. "
                "NOT total Prepare time: Final SRC, queue, allocation, "
                "finalization and real-track effects NOT modeled. "
                "No guaranteed duration; do not use as an export ETA.")
                .arg(low,high);
        } else {
            row[QStringLiteral("label")]=QStringLiteral("N/A");
            row[QStringLiteral("details")]=QStringLiteral(
                "Genuine M17/M16 calibration did not produce four "
                "complete, verified measurements at this rate. "
                "Total Prepare estimate unavailable.");
        }
        rows.append(row);
    }
    dspEstimates_=std::move(rows);
    calibratedCeiling_=ceiling;
    calibrationRunning_=false;
    completedCalibrationRates_=6;
    calibrationValid_=measured>0;
    calibrationStatus_=QStringLiteral(
        "%1/6 M17+M16 rate probes completed · TOTAL Prepare ETA N/A")
        .arg(measured);
    emit calibrationChanged();
}

} // namespace rgsml::app
