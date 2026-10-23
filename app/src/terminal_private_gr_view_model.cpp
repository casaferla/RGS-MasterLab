#include "terminal_private_gr_view_model.hpp"

#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <utility>

namespace rgsml::app {

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

void TerminalPrivateGrViewModel::invalidate()
{
    // A product/source/limiter ownership change destroys retained history.
    suspend_unverified();
    lastProven_.reset();
    lastProvenPoints_.clear();
    historicalPrefix_.clear();
    historicalLoopMarker_=false;
}

void TerminalPrivateGrViewModel::refresh()
{
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
