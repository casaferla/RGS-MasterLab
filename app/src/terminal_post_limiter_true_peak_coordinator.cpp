#include "terminal_post_limiter_true_peak_coordinator.hpp"
#include "internal/true_peak_chronological_buckets.hpp"
#include <QMetaObject>
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>

namespace rgsml::app {

TerminalPostLimiterTruePeakCoordinator::TerminalPostLimiterTruePeakCoordinator(
    Observer observe,QObject* parent)
    : QObject(parent),observe_(std::move(observe)),
      worker_([this]{worker_loop();})
{
    timer_.setInterval(50);
    timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_,&QTimer::timeout,this,
            &TerminalPostLimiterTruePeakCoordinator::poll);
    timer_.start();
}
TerminalPostLimiterTruePeakCoordinator::~TerminalPostLimiterTruePeakCoordinator()
{
    timer_.stop();
    {
        std::lock_guard lock(mutex_);
        stopping_=true;
        pending_.reset();
    }
    cv_.notify_one();
    if(worker_.joinable())worker_.join();
}

void TerminalPostLimiterTruePeakCoordinator::invalidate(){clear(false);}

void TerminalPostLimiterTruePeakCoordinator::clear(bool attempted)
{
    ++serial_;
    ++epoch_;
    attempted_=attempted;
    recent_=false;
    hold_=false;
    recentPeaks_={};
    holdPeaks_={};
    channels_=0;
    id_.reset();
    generation_.reset();
    seek_.reset();
    traversal_.reset();
    wrap_.reset();
    owner_.reset();
    status_=attempted?QStringLiteral("GAP / UNAVAILABLE")
                     :QStringLiteral("NOT MONITORED");
    {
        std::lock_guard lock(mutex_);
        pending_.reset();
    }
    emit changed();
}

void TerminalPostLimiterTruePeakCoordinator::poll()
{
    if(!observe_) {
        if(attempted_)clear(false);
        return;
    }
    const auto result=observe_();
    if(!result || !result.value()->owner) {
        if(attempted_)clear(true);
        return;
    }
    const auto& proof=*result.value();
    const auto view=proof.owner->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    const auto channels=view.format().channel_layout()
        ==audio::ChannelLayout::MONO_C?1:2;
    if(proof.realizationId.value==0
       ||proof.playbackGeneration==0
       ||proof.firstTerminalFrame<0
       ||proof.lastTerminalFrameExclusive<=proof.firstTerminalFrame
       ||proof.lastTerminalFrameExclusive>view.frame_count().value()
       ||proof.consumedOutputWatermarkExclusive
           <proof.lastTerminalFrameExclusive-proof.firstTerminalFrame
       ||(channels!=1&&channels!=2)) {
        if(attempted_)clear(true);
        return;
    }
    const bool newEpoch=!attempted_ || owner_!=proof.owner
        ||id_!=std::optional{proof.realizationId}
        ||generation_!=std::optional{proof.playbackGeneration}
        ||seek_!=std::optional{proof.seekSerial}
        ||traversal_!=std::optional{proof.traversalSerial}
        ||wrap_!=std::optional{proof.loopWrapCount};
    if(newEpoch) {
        clear(true);
        owner_=proof.owner;
        id_=proof.realizationId;
        generation_=proof.playbackGeneration;
        seek_=proof.seekSerial;
        traversal_=proof.traversalSerial;
        wrap_=proof.loopWrapCount;
        channels_=channels;
    }
    const auto serial=++serial_;
    {
        std::lock_guard lock(mutex_);
        pending_.emplace(Job{serial,epoch_,proof});
    }
    cv_.notify_one();
}

void TerminalPostLimiterTruePeakCoordinator::worker_loop()
{
    using Meter=analysis::internal::ChronologicalTruePeakBuckets;
    std::optional<Meter> meter;
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> owner;
    std::optional<core::RealizationId> identity;
    std::uint64_t generation=0,epoch=0,wrap=0,seek=0,traversal=0;
    std::int64_t processed=0,origin=0;
    while(true) {
        std::optional<Job> job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock,[this]{return stopping_||pending_.has_value();});
            if(stopping_)break;
            job.swap(pending_);
        }
        if(!job)continue;
        const auto& proof=job->proof;
        const auto pcm=proof.owner->rendered.terminal.checked.terminal
            .compensated_pcm.view();
        const auto channels=pcm.format().channel_layout()
            ==audio::ChannelLayout::MONO_C?std::size_t{1}:std::size_t{2};
        const auto rate=pcm.format().sample_rate().value();
        const bool fresh=job->epoch!=epoch||owner!=proof.owner
            ||identity!=std::optional{proof.realizationId}
            ||generation!=proof.playbackGeneration
            ||seek!=proof.seekSerial||traversal!=proof.traversalSerial
            ||wrap!=proof.loopWrapCount
            ||processed<proof.firstTerminalFrame
            ||processed>proof.lastTerminalFrameExclusive;
        if(fresh) {
            meter.reset();
            owner=proof.owner;
            identity=proof.realizationId;
            generation=proof.playbackGeneration;
            epoch=job->epoch;
            seek=proof.seekSerial;
            traversal=proof.traversalSerial;
            wrap=proof.loopWrapCount;
            origin=proof.firstTerminalFrame;
            processed=origin;
        }
        // Never synthesize more than 500 ms of samples after GUI stalls;
        // discard FIR/HOLD support when rebasing the already consumed suffix.
        const auto cap=rate/2;
        if(proof.lastTerminalFrameExclusive-processed>cap) {
            meter.reset();
            processed=std::max(
                proof.firstTerminalFrame,
                proof.lastTerminalFrameExclusive-cap);
            origin=processed;
        }
        if(!meter) {
            auto created=Meter::create(rate,channels,processed);
            if(created)meter.emplace(std::move(*created.value()));
        }
        Completion completion;
        if(meter && processed>=proof.firstTerminalFrame
           &&proof.lastTerminalFrameExclusive>=processed
           &&proof.lastTerminalFrameExclusive-processed<=cap) {
            const auto left=pcm.channel(0);
            const auto right=pcm.channel(channels==2?1U:0U);
            if(left&&right) {
                bool valid=true;
                while(processed<proof.lastTerminalFrameExclusive) {
                    const auto pos=static_cast<std::size_t>(processed);
                    const std::array<double,2> samples{
                        (*left.value())[pos],
                        channels==2?(*right.value())[pos]:0.0};
                    // A fresh loop traversal has unknown prior FIR context.
                    const bool proven=proof.loopWrapCount==0
                        ||processed-origin>=11;
                    if(!meter->consume_frame(
                        std::span<const double>{samples.data(),channels},
                        proven)) {
                        valid=false;
                        break;
                    }
                    ++processed;
                }
                if(valid) {
                    const auto window=meter->snapshot();
                    completion.valid=true;
                    completion.recent=window.recent100msComplete;
                    completion.hold=window.shortHold1sComplete;
                    completion.recentPeaks=window.recentLinearPeak;
                    completion.holdPeaks=window.shortHoldLinearPeak;
                }
            }
        }
        if(!completion.valid) {
            meter.reset();
            processed=origin;
        }
        static_cast<void>(QMetaObject::invokeMethod(this,
            [this,serial=job->serial,completion] {
                accept(serial,completion);
            },Qt::QueuedConnection));
    }
}

void TerminalPostLimiterTruePeakCoordinator::accept(
    std::uint64_t serial,Completion done)
{
    if(serial!=serial_||!attempted_)return;
    // Recheck independently AFTER off-thread FIR work. A seek/loop/handoff
    // between job submission and completion cannot publish an old reading.
    const auto checked=observe_?observe_()
        :core::Result<TerminalConsumedOutputEvidence>::failure(
            core::Error{core::ErrorCode::InvalidState,
                "Post-limiter sink observer is unavailable."});
    if(!done.valid||!checked||!checked.value()->owner
       ||owner_!=checked.value()->owner
       ||id_!=std::optional{checked.value()->realizationId}
       ||generation_!=std::optional{checked.value()->playbackGeneration}
       ||seek_!=std::optional{checked.value()->seekSerial}
       ||traversal_!=std::optional{checked.value()->traversalSerial}
       ||wrap_!=std::optional{checked.value()->loopWrapCount}) {
        clear(true);
        return;
    }
    recent_=done.recent;
    hold_=done.hold;
    recentPeaks_=done.recentPeaks;
    holdPeaks_=done.holdPeaks;
    status_=recent_
        ?QStringLiteral("LIVE · POST-LIMITER OUTPUT_RATE · UNQUALIFIED")
        :QStringLiteral("PARTIAL / WAITING FOR 100 MS");
    emit changed();
}

QString TerminalPostLimiterTruePeakCoordinator::reading(double v)
{
    if(v<=0.0)return QStringLiteral("−∞ dBTP");
    return QString::number(20.0*std::log10(v),'f',2)
        +QStringLiteral(" dBTP");
}
double TerminalPostLimiterTruePeakCoordinator::normalized(double v) noexcept
{
    return v<=0.0?0.0:std::clamp(
        (20.0*std::log10(v)+60.0)/66.0,0.0,1.0);
}
QString TerminalPostLimiterTruePeakCoordinator::peak_text() const
{
    return recent_?reading(std::max(recentPeaks_[0],recentPeaks_[1]))
        :QStringLiteral("UNAVAILABLE");
}
QString TerminalPostLimiterTruePeakCoordinator::hold_text() const
{
    return hold_?reading(std::max(holdPeaks_[0],holdPeaks_[1]))
        :QStringLiteral("UNAVAILABLE");
}
QString TerminalPostLimiterTruePeakCoordinator::left_text() const
{
    return recent_?reading(recentPeaks_[0]):QStringLiteral("—");
}
QString TerminalPostLimiterTruePeakCoordinator::right_text() const
{
    return recent_&&channels_==2?reading(recentPeaks_[1]):QStringLiteral("—");
}
double TerminalPostLimiterTruePeakCoordinator::left_level() const noexcept
{return recent_?normalized(recentPeaks_[0]):0.0;}
double TerminalPostLimiterTruePeakCoordinator::right_level() const noexcept
{return recent_&&channels_==2?normalized(recentPeaks_[1]):0.0;}
double TerminalPostLimiterTruePeakCoordinator::left_hold_level() const noexcept
{return hold_?normalized(holdPeaks_[0]):0.0;}
double TerminalPostLimiterTruePeakCoordinator::right_hold_level() const noexcept
{return hold_&&channels_==2?normalized(holdPeaks_[1]):0.0;}
bool TerminalPostLimiterTruePeakCoordinator::left_over_zero() const noexcept
{return recent_&&recentPeaks_[0]>1.0;}
bool TerminalPostLimiterTruePeakCoordinator::right_over_zero() const noexcept
{return recent_&&channels_==2&&recentPeaks_[1]>1.0;}
} // namespace rgsml::app
