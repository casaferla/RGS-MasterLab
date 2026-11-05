#include "terminal_private_gr_view_model.hpp"

#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace rgsml::app {

namespace {
// Only actual immutable M17 applied_gain_frames may feed offline GR.
// Each reported point is the PEAK OF A BOUNDED SET OF REAL SAMPLES from
// its indicated master interval; unexamined frames are not invented.
// This is a sampled visualization, NOT a sink-consumed LIVE certificate.
[[nodiscard]] std::optional<QVariantList> sampled_offline_m17_gr(
    const render::TruePeakLimiterTerminalCandidate& terminal)
{
    const auto pcm=terminal.compensated_pcm.view();
    const auto n=pcm.frame_count().value();
    const auto& gains=terminal.applied_gain_frames;
    const auto delay=terminal.algorithmic_latency_frames;
    if(terminal.bypassed||delay<=0||n<=0
       ||pcm.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
       ||pcm.absolute_start_frame()!=core::FrameIndex{0}
       ||static_cast<std::uint64_t>(n)!=gains.size())
        return std::nullopt;
    constexpr std::int64_t kBuckets=256;
    constexpr std::int64_t kSamplesPerBucket=256;
    const auto buckets=std::min(kBuckets,n);
    const auto base=n/buckets;
    const auto extra=n%buckets;
    QVariantList result;
    result.reserve(static_cast<qsizetype>(buckets));
    const auto checked=[&](std::int64_t frame)
        ->std::optional<double> {
        const auto& sample=gains[static_cast<std::size_t>(frame)];
        if(frame>std::numeric_limits<std::int64_t>::max()-delay
           ||sample.programme_frame!=frame
           ||sample.raw_output_frame!=frame+delay
           ||!std::isfinite(sample.reduction_db)
           ||sample.reduction_db<0.0
           ||!std::isfinite(sample.gain)
           ||sample.gain<=0.0 ||sample.gain>1.0
           ||std::abs(sample.reduction_db
                +20.0*std::log10(sample.gain))>1e-5)
            return std::nullopt;
        return sample.reduction_db;
    };
    for(std::int64_t i=0;i<buckets;++i) {
        // Quotient/remainder partition avoids i*n signed overflow.
        const auto begin=i*base+std::min(i,extra);
        const auto end=(i+1)*base+std::min(i+1,extra);
        const auto span=end-begin;
        if(span<=0)return std::nullopt;
        const auto step=1+(span-1)/kSamplesPerBucket;
        double peak=0.0;
        for(auto cursor=begin;cursor<end;) {
            const auto gain=checked(cursor);
            if(!gain)return std::nullopt;
            peak=std::max(peak,*gain);
            if(end-cursor<=step)break;
            cursor+=step;
        }
        const auto tail=checked(end-1);
        if(!tail)return std::nullopt;
        peak=std::max(peak,*tail);
        QVariantMap point;
        point.insert(QStringLiteral("peakReductionDb"),peak);
        point.insert(QStringLiteral("frameBegin"),
                     QVariant::fromValue(static_cast<qlonglong>(begin)));
        point.insert(QStringLiteral("frameEndExclusive"),
                     QVariant::fromValue(static_cast<qlonglong>(end)));
        result.append(point);
    }
    return result;
}
} // namespace

TerminalPrivateGrViewModel::TerminalPrivateGrViewModel(
    Observation observe,QObject* parent)
    :QObject(parent),observe_(std::move(observe))
{}
TerminalPrivateGrViewModel::TerminalPrivateGrViewModel(
    EvidenceObservation observe,QObject* parent)
    :QObject(parent),observeEvidence_(std::move(observe))
{}

void TerminalPrivateGrViewModel::suspend_unverified()
{
    lastEvidence_.reset(); // Never reuse projected gains after ANY failed read.
    // No stale GR is ever rendered while Windows cannot prove consumption.
    if(!verified_ && series_.isEmpty()
        &&status_==QStringLiteral("UNAVAILABLE_NO_VERIFIED_GR"))
        return;
    verified_=false;
    series_.clear();
    realization_.reset();
    generation_=0;
    status_=QStringLiteral("UNAVAILABLE_NO_VERIFIED_GR");
    emit changed();
}

void TerminalPrivateGrViewModel::clear_offline()
{
    offlineVerifiedOwner_.reset();
    offlineMeasuredOwner_.reset();
    if(offlineSeries_.isEmpty()
       &&offlineStatus_==QStringLiteral("NOT PREPARED · NO GR DATA"))
        return;
    offlineSeries_.clear();
    offlineStatus_=QStringLiteral("NOT PREPARED · NO GR DATA");
    emit changed();
}

void TerminalPrivateGrViewModel::refresh_offline()
{
    if(!observeOffline_)return;
    const auto capture=observeOffline_();
    // One current source, never a verified and a nonconforming identity
    // simultaneously. This read is independently owner-guarded in session.
    if(static_cast<bool>(capture.verified)
       ==static_cast<bool>(capture.nonconforming)) {
        clear_offline();
        return;
    }
    if(!offlineSeries_.isEmpty()
       &&capture.verified==offlineVerifiedOwner_
       &&capture.nonconforming==offlineMeasuredOwner_)
        return;
    const render::TruePeakLimiterTerminalCandidate* terminal=nullptr;
    QString label;
    if(capture.verified) {
        const auto& bound=capture.verified->rendered.terminal.checked;
        if(!bound.final_check) {clear_offline();return;}
        terminal=&bound.terminal;
        label=QStringLiteral("OFFLINE · VERIFIED MASTER · SAMPLED");
    } else {
        terminal=&capture.nonconforming->rendered.terminal.terminal;
        label=QStringLiteral("OFFLINE · NON-CONFORMING · SAMPLED");
    }
    const auto points=sampled_offline_m17_gr(*terminal);
    if(!points ||points->isEmpty()) {clear_offline();return;}
    offlineVerifiedOwner_=capture.verified;
    offlineMeasuredOwner_=capture.nonconforming;
    offlineSeries_=*points;
    offlineStatus_=std::move(label);
    emit changed();
}

void TerminalPrivateGrViewModel::invalidate()
{
    // Both witnessed LIVE and source-owned OFFLINE data must be invalidated
    // immediately at an epoch transition. The next separate refresh re-proves
    // the current source; never show a retained old Master by default.
    clear_offline();
    suspend_unverified();
    lastProven_.reset();
    lastProvenPoints_.clear();
    historicalPrefix_.clear();
    historicalLoopMarker_=false;
}

void TerminalPrivateGrViewModel::refresh()
{
    // Independent from the Windows consumed-output observer: even when
    // LIVE proof is impossible, a current immutable prepared MASTER may
    // truthfully provide a separately labeled offline sampled GR history.
    refresh_offline();
    std::optional<TerminalPrivateGrSeries> sample;
    std::optional<EvidenceKey> acceptedKey;
    if(observeEvidence_) {
        // Every tick repeats BOTH serialized Windows reads and the owner /
        // epoch / gap guards. A stable PAUSED watermark avoids ONLY the
        // full-rate applied-gain scan and the QML repaint.
        const auto evidence=observeEvidence_();
        if(!evidence) {suspend_unverified();return;}
        const auto& p=*evidence.value();
        EvidenceKey key{p.owner,p.realizationId,p.playbackGeneration,
            p.loopWrapCount,p.seekSerial,p.traversalSerial,
            p.consumedOutputWatermarkExclusive,p.firstTerminalFrame,
            p.lastTerminalFrameExclusive};
        if(verified_ && lastEvidence_ && *lastEvidence_==key)return;
        const auto projected=project_private_terminal_gr(p);
        if(!projected) {suspend_unverified();return;}
        sample.emplace(*projected.value());
        acceptedKey.emplace(std::move(key));
    } else {
        if(!observe_) {suspend_unverified();return;}
        const auto next=observe_();
        if(!next) {suspend_unverified();return;}
        sample.emplace(*next.value());
    }
    const auto& witnessed=*sample;
    if(witnessed.realizationId.value==0
        ||witnessed.playbackGeneration==0
        ||witnessed.consumedFrameBegin<0
        ||witnessed.consumedFrameEndExclusive
            <=witnessed.consumedFrameBegin
        ||witnessed.buckets.empty()
        ||witnessed.buckets.size()>256) {
        suspend_unverified();
        return;
    }

    QVariantList points;
    points.reserve(static_cast<qsizetype>(witnessed.buckets.size()));
    auto cursor=witnessed.consumedFrameBegin;
    for(const auto& bucket:witnessed.buckets) {
        if(bucket.frameBegin!=cursor
            ||bucket.frameEndExclusive<=cursor
            ||bucket.frameEndExclusive>witnessed.consumedFrameEndExclusive
            ||!std::isfinite(bucket.peakReductionDb)
            ||bucket.peakReductionDb<0.0) {
            suspend_unverified();
            return;
        }
        QVariantMap point;
        point.insert(QStringLiteral("peakReductionDb"),
                     bucket.peakReductionDb);
        point.insert(QStringLiteral("frameBegin"),
                     QVariant::fromValue(static_cast<qlonglong>(bucket.frameBegin)));
        point.insert(QStringLiteral("frameEndExclusive"),
                     QVariant::fromValue(static_cast<qlonglong>(
                         bucket.frameEndExclusive)));
        points.append(point);
        cursor=bucket.frameEndExclusive;
    }
    if(cursor!=witnessed.consumedFrameEndExclusive) {
        suspend_unverified();
        return;
    }

    const ProvenEpoch current{
        witnessed.realizationId,witnessed.playbackGeneration,
        witnessed.consumedFrameBegin,witnessed.consumedFrameEndExclusive,
        witnessed.consumedOutputWatermarkExclusive,witnessed.loopWrapCount,
        witnessed.seekSerial,witnessed.traversalSerial};
    const bool sameOwner=lastProven_
        &&lastProven_->realizationId==current.realizationId;
    if(!sameOwner) {
        // Never bridge two different RealizationIds. A different source can
        // only produce a completely fresh verified plot.
        historicalPrefix_.clear();
        historicalLoopMarker_=false;
    } else {
        const bool epochChanged=
            current.playbackGeneration!=lastProven_->playbackGeneration
            ||current.loopWrapCount!=lastProven_->loopWrapCount
            ||current.seekSerial!=lastProven_->seekSerial
            ||current.traversalSerial!=lastProven_->traversalSerial
            ||current.frameBegin>lastProven_->frameEndExclusive
            ||current.frameEndExclusive<lastProven_->frameEndExclusive;
        if(epochChanged) {
            // The Compressor GR grammar retains only already VERIFIED
            // history, separated by a two-slot break and 1px marker.
            // Only the next independently accepted worker proof can show
            // these points again. Never bridge unconsumed samples.
            constexpr qsizetype kOldTail=48;
            historicalPrefix_=lastProvenPoints_.mid(
                std::max<qsizetype>(0,lastProvenPoints_.size()-kOldTail));
            historicalLoopMarker_=
                current.loopWrapCount!=lastProven_->loopWrapCount;
        }
    }
    lastProven_=current;
    if(!historicalPrefix_.isEmpty() && sameOwner) {
        constexpr qsizetype kNewPoints=208;
        if(points.size()>kNewPoints)
            points=points.mid(points.size()-kNewPoints);
        // Save ONLY the newly proved traversal. Keeping the combined
        // presentation here would recursively copy older epoch segments
        // into subsequent seek/loop breaks and duplicate GR history.
        lastProvenPoints_=points;
        QVariantMap first=points.front().toMap();
        first.insert(QStringLiteral("breakBefore"),true);
        first.insert(QStringLiteral("loopBefore"),historicalLoopMarker_);
        points.front()=first;
        QVariantList combined=historicalPrefix_;
        combined.append(points);
        points=std::move(combined);
    } else {
        lastProvenPoints_=points;
    }

    if(verified_ && realization_==current.realizationId
        && generation_==current.playbackGeneration
        &&series_==points) {
        // An advancing but unchanged peak can still have a newer,
        // independently validated worker watermark. Cache that exact key.
        lastEvidence_=std::move(acceptedKey);
        return;
    }
    series_=std::move(points);
    verified_=true;
    realization_=current.realizationId;
    generation_=current.playbackGeneration;
    status_=QStringLiteral("PRIVATE_SINK_CONSUMED_GR_UNQUALIFIED");
    lastEvidence_=std::move(acceptedKey);
    emit changed();
}

} // namespace rgsml::app
