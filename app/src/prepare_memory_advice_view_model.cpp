#include "prepare_memory_advice_view_model.hpp"
#include "source_selection_view_model.hpp"

#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QVariantMap>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

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
    const audio::MonitoringRateMemoryAdvice& advice)
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
    auto result=row(hz,label,kind,details);
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
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(kMasterRates.size()));
    for(const auto& entry:*advice.value()) {
        rows.append(classified_row(entry));
    }
    recommendations_=std::move(rows);
    const auto deviceHz=advice.value()->front().observed_device_rate.value();
    statusText_=QStringLiteral("DEVICE %1").arg(rate_label(deviceHz));
    emit changed();
}
} // namespace rgsml::app
