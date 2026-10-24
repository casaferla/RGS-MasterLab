#include "prepare_memory_advice_view_model.hpp"
#include "source_selection_view_model.hpp"

#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/platform/windows/windows_audio_playback_service.hpp>

#include <QVariantMap>

#include <array>
#include <cstdint>

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
        {QStringLiteral("details"),details}};
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
        label=QStringLiteral("Consigliata per memoria");
        kind=QStringLiteral("recommended");
        break;
    case Kind::CAUTION_LIMITED_HEADROOM:
        label=QStringLiteral("Memoria limitata");
        kind=QStringLiteral("caution");
        break;
    case Kind::DISCOURAGED_INSUFFICIENT_HEADROOM:
        label=QStringLiteral("Sconsigliata");
        kind=QStringLiteral("discouraged");
        break;
    case Kind::UNKNOWN:
        label=QStringLiteral("Non valutabile");
        kind=QStringLiteral("unknown");
        break;
    }
    constexpr std::uint64_t mib=UINT64_C(1024)*1024;
    const auto bytes=advice.resource.combined_pcm_bytes;
    const auto roundedMib=bytes/mib+(bytes%mib!=0 ? 1U : 0U);
    const auto details=QStringLiteral(
        "PCM MASTER + MONITOR minimo stimato: %1 MiB. "
        "Il consiglio non riserva RAM, non include ogni overhead DSP/Qt "
        "e non garantisce prestazioni CPU o audio real-time.")
        .arg(QString::number(roundedMib));
    return row(hz,label,kind,details);
}
} // namespace

PrepareMemoryAdviceViewModel::PrepareMemoryAdviceViewModel(
    const SourceSelectionViewModel* source,
    const platform::windows::WindowsAudioPlaybackService* playback,
    QObject* parent)
    :QObject(parent),source_(source),playback_(playback)
{
    reset_unknown(QStringLiteral("Carica una sorgente"));
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
            QStringLiteral("Non valutabile"),QStringLiteral("unknown"),
            QStringLiteral("Nessun budget di memoria verificato; "
                "nessuna frequenza è automaticamente consigliata.")));
    }
    emit changed();
}

void PrepareMemoryAdviceViewModel::refresh()
{
    if(!source_||!source_->has_source()) {
        reset_unknown(QStringLiteral("Carica una sorgente"));
        return;
    }
    if(!playback_) {
        reset_unknown(QStringLiteral("Dispositivo non valutabile"));
        return;
    }
    const auto sourceRate=core::SampleRate::create(source_->sample_rate_hz());
    const auto sourceFrames=core::FrameCount::create(source_->frame_count());
    const auto channelCount=source_->channel_count();
    if(!sourceRate||!sourceFrames||(channelCount!=1&&channelCount!=2)) {
        reset_unknown(QStringLiteral("Formato sorgente non valutabile"));
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
        reset_unknown(QStringLiteral("Dispositivo o memoria non valutabili"));
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
