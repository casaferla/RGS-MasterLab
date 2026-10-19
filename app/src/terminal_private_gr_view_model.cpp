#include "terminal_private_gr_view_model.hpp"

#include <QVariantMap>
#include <cmath>
#include <utility>

namespace rgsml::app {

TerminalPrivateGrViewModel::TerminalPrivateGrViewModel(
    Observation observe,QObject* parent)
    :QObject(parent),observe_(std::move(observe))
{}

void TerminalPrivateGrViewModel::invalidate()
{
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

void TerminalPrivateGrViewModel::refresh()
{
    if(!observe_) {invalidate();return;}
    const auto next=observe_();
    if(!next) {invalidate();return;}
    const auto& witnessed=*next.value();
    if(witnessed.realizationId.value==0
        ||witnessed.playbackGeneration==0
        ||witnessed.consumedFrameBegin<0
        ||witnessed.consumedFrameEndExclusive
            <=witnessed.consumedFrameBegin
        ||witnessed.buckets.empty()
        ||witnessed.buckets.size()>256) {
        invalidate();
        return;
    }
    // A newly observed generation or realization can ONLY replace history,
    // never silently append unconsumed old buckets.
    QVariantList points;
    points.reserve(static_cast<qsizetype>(witnessed.buckets.size()));
    auto cursor=witnessed.consumedFrameBegin;
    for(const auto& bucket:witnessed.buckets) {
        if(bucket.frameBegin!=cursor
            ||bucket.frameEndExclusive<=cursor
            ||bucket.frameEndExclusive>witnessed.consumedFrameEndExclusive
            ||!std::isfinite(bucket.peakReductionDb)
            ||bucket.peakReductionDb<0.0) {
            invalidate();
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
        invalidate();
        return;
    }
    if(verified_ && realization_==witnessed.realizationId
        && generation_==witnessed.playbackGeneration
        &&series_==points)
        return;
    series_=std::move(points);
    verified_=true;
    realization_=witnessed.realizationId;
    generation_=witnessed.playbackGeneration;
    status_=QStringLiteral("PRIVATE_SINK_CONSUMED_GR_UNQUALIFIED");
    emit changed();
}

} // namespace rgsml::app
