#include "automatch_view_model.hpp"
#include <QUuid>
#include <QStringList>
#include <QThread>
#include <cmath>
#include <algorithm>
#include <limits>

namespace rgsml::app {
namespace a=l3_automatch;
namespace {
core::Uuid new_uuid() {
    const auto parsed=core::Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
    return parsed?*parsed.value():core::Uuid{};
}
}
AutoMatchViewModel::AutoMatchViewModel(Action::Controllers product,const Action::Fanout& fanout,
    const AuditionSourceSelector& selector,const GoldSelectionViewModel& gold,QObject* parent)
    :QObject(parent),product_(product),fanout_(fanout),selector_(selector),gold_(gold),
     action_(product,fanout,selector,gold)
{
    timer_.setInterval(100);
    connect(&timer_,&QTimer::timeout,this,&AutoMatchViewModel::refresh);
    connect(&product_.upstream.source,&SourceSelectionViewModel::sourceChanged,
        this,&AutoMatchViewModel::refresh,Qt::QueuedConnection);
    connect(&gold_,&GoldSelectionViewModel::changed,this,&AutoMatchViewModel::refresh,Qt::QueuedConnection);
    connect(&selector_,&AuditionSourceSelector::changed,this,&AutoMatchViewModel::refresh,Qt::QueuedConnection);
    connect(&product_.upstream.project,&ProjectSessionViewModel::changed,
        this,&AutoMatchViewModel::refresh,Qt::QueuedConnection);
}
bool AutoMatchViewModel::owner()const{return QThread::currentThread()==thread();}
bool AutoMatchViewModel::busy()const{return owner()&&(publishing_||action_.busy());}
bool AutoMatchViewModel::can_start()const {
    return owner()&&!busy()&&product_.upstream.source.has_source()&&gold_.has_gold()&&
        selector_.prepared_available();
}
bool AutoMatchViewModel::can_adopt()const {
    return owner()&&!busy()&&action_.can_adopt();
}
bool AutoMatchViewModel::can_cancel()const {
    return owner()&&!publishing_&&(action_.phase()==Action::Phase::searching||
        action_.can_adopt()||action_.phase()==Action::Phase::verifying);
}
bool AutoMatchViewModel::can_undo()const {
    return owner()&&!busy()&&!action_.can_adopt()&&
        product_.upstream.project.can_undo_private_m12_materialization();
}
bool AutoMatchViewModel::can_redo()const {
    return owner()&&!busy()&&!action_.can_adopt()&&
        product_.upstream.project.can_redo_private_m12_materialization();
}
bool AutoMatchViewModel::can_resume()const {
    return owner()&&!busy()&&!action_.can_adopt()&&
        product_.upstream.project.can_resume_private_m12_suspension();
}
QString AutoMatchViewModel::current_match_text()const {
    if(!owner())return {};
    using S=ProjectSessionViewModel::M12CurrentMatchState;
    switch(product_.upstream.project.private_m12_match_state()) {
    case S::none:return QStringLiteral("No linked match");
    case S::active:return QStringLiteral("Linked match");
    case S::suspended:return QStringLiteral("Linked match suspended");
    case S::stale:return QStringLiteral("Linked match unavailable / stale");
    }
    return {};
}
QString AutoMatchViewModel::error_text()const {
    if(!owner())return {};
    if(!error_.isEmpty())return error_;
    return QString::fromStdString(action_.error());
}
QString AutoMatchViewModel::review_text()const {
    if(!owner()||!action_.can_adopt()||!action_.review_diff())return {};
    using C=a::m12_frozen::ReviewNodeChange;
    QStringList lines;
    for(const auto& row:action_.review_diff()->nodes) {
        QString operation;
        switch(row.change) {
        case C::add:operation=QStringLiteral("Add");break;
        case C::remove:operation=QStringLiteral("Remove");break;
        case C::update:operation=QStringLiteral("Update");break;
        case C::retain:operation=QStringLiteral("Retain");break;
        case C::suspend:operation=QStringLiteral("Suspend");break;
        case C::resume:operation=QStringLiteral("Resume");break;
        }
        auto type=QString::fromStdString(row.type);
        if(type.startsWith(QStringLiteral("rgsml.dsp.")))type.remove(0,10);
        type.replace(QLatin1Char('-'),QLatin1Char(' '));
        lines.push_back(operation+QStringLiteral(": ")+type);
    }
    return lines.isEmpty()?QStringLiteral("No linked DSP changes"):lines.join(QLatin1Char('\n'));
}
QString AutoMatchViewModel::review_metrics_text()const {
    if(!owner())return {};
    const auto metrics=action_.review_metrics();
    if(!metrics)return {};
    const auto peak=metrics->exact_m16_linear_peak==0.0?QStringLiteral("-inf"):
        QString::number(20.0*std::log10(metrics->exact_m16_linear_peak),'f',2);
    const auto elapsed=static_cast<double>(metrics->original_ms+metrics->fixed_ms+
        metrics->seed_ms+metrics->search_ms)/1000.0;
    auto text=QStringLiteral("Full-output TP: %1 dBTP / limit %2\nTrials: %3 | %4 s")
        .arg(peak).arg(metrics->independent_ceiling_db_tp,0,'f',2)
        .arg(metrics->attempted).arg(elapsed,0,'f',1);
    if(metrics->first_admissible_ms)
        text+=QStringLiteral("\nFirst admissible: %1 s")
            .arg(static_cast<double>(*metrics->first_admissible_ms)/1000.0,0,'f',1);
    if(metrics->first_admissible_ms)
        text+=QStringLiteral(" | %1 complete, %2 failed, %3 cached")
            .arg(metrics->completed).arg(metrics->failed).arg(metrics->cache_hits);
    return text;
}
QString AutoMatchViewModel::failure_diagnostics_text()const {
    if(!owner()||action_.phase()!=Action::Phase::failed||!action_.result())
        return {};
    const auto& completed=*action_.result();
    const auto& worker=completed.result;
    using S=a::m09_frozen::NativeProductSearchStatus;
    using F=a::m10_frozen::NativeFixedSearchStatus;
    using R=a::m10_frozen::RefinementBatchReason;
    QString search;
    switch(worker.search.status) {
    case S::unavailable:search=QStringLiteral("unavailable");break;
    case S::no_correction_required:search=QStringLiteral("no correction required");break;
    case S::measured_candidate:search=QStringLiteral("measured candidate");break;
    case S::no_measured_candidate:search=QStringLiteral("no measured candidate");break;
    case S::cancelled:search=QStringLiteral("cancelled");break;
    case S::measured_none_candidate:search=QStringLiteral("empty candidate");break;
    }
    QString fixed;
    switch(worker.search.fixed_gate.status) {
    case F::unavailable:fixed=QStringLiteral("unavailable");break;
    case F::no_correction_required:fixed=QStringLiteral("no correction required");break;
    case F::correction_required:fixed=QStringLiteral("correction required");break;
    case F::targets_met_but_safety_unavailable_or_failed:
        fixed=QStringLiteral("safety unavailable/failed");break;
    }
    QString text=QStringLiteral("Fixed: %1 | Search: %2").arg(fixed,search);
    if(worker.search.refinement) {
        const auto& batch=*worker.search.refinement;
        QString reason;
        switch(batch.reason) {
        case R::complete:reason=QStringLiteral("complete");break;
        case R::budget_exhausted:reason=QStringLiteral("budget exhausted");break;
        case R::cancelled:reason=QStringLiteral("cancelled");break;
        case R::invalid_inventory:reason=QStringLiteral("invalid inventory");break;
        case R::no_complete_seed_or_stale_source:
            reason=QStringLiteral("no complete seed / stale source");break;
        case R::seed_attempt_budget_exceeded:
            reason=QStringLiteral("seed attempt budget exhausted");break;
        case R::invalid_seed_trial:reason=QStringLiteral("invalid seed trial");break;
        case R::invalid_evaluation:reason=QStringLiteral("invalid evaluation");break;
        }
        text+=QStringLiteral("\\nRefinement: %1 | attempts: %2")
            .arg(reason).arg(batch.total_attempted);
    } else {
        text+=QStringLiteral("\\nRefinement: not reached");
    }
    const auto& perf=worker.performance;
    text+=QStringLiteral("\\nNative renders: %1 complete, %2 failed, %3 cache hits")
        .arg(perf.completed).arg(perf.failed).arg(perf.cache_hits);
    text+=QStringLiteral("\\nPreflight rejected: %1 | budget refusals: %2")
        .arg(perf.evaluator_preflight_rejections).arg(perf.budget_refusals);
    const auto elapsed=completed.original_pair_ms+worker.timing.fixed_ms+
        worker.timing.seed_ms+worker.timing.search_ms;
    text+=QStringLiteral("\\nMeasured phases: %1 s (original %2 / fixed %3 / seed %4 / search %5)")
        .arg(static_cast<double>(elapsed)/1000.0,0,'f',1)
        .arg(static_cast<double>(completed.original_pair_ms)/1000.0,0,'f',1)
        .arg(static_cast<double>(worker.timing.fixed_ms)/1000.0,0,'f',1)
        .arg(static_cast<double>(worker.timing.seed_ms)/1000.0,0,'f',1)
        .arg(static_cast<double>(worker.timing.search_ms)/1000.0,0,'f',1);
    return text;
}
QString AutoMatchViewModel::elapsed_time_text()const {
    if(!owner()||!search_clock_.isValid())return {};
    const auto elapsed=search_clock_running_
        ? search_clock_.elapsed() : finished_search_ms_;
    if(elapsed<0)return {};
    const auto minutes=elapsed/60000;
    const auto seconds=(elapsed/1000)%60;
    const auto tenths=(elapsed%1000)/100;
    return QStringLiteral("%1:%2.%3")
        .arg(minutes).arg(seconds,2,10,QLatin1Char('0')).arg(tenths);
}
QString AutoMatchViewModel::verification_note()const {
    if(!owner()||!action_.verification_result())return {};
    const auto& assessment=action_.verification_result()->assessment();
    if(assessment.native_numerical_evidence_passed())
        return QStringLiteral("Numerical evidence passed; formal qualification not granted.");
    if(assessment.canonical_reasons.empty())return QStringLiteral("Verification incomplete.");
    return QString::fromStdString(assessment.canonical_reasons.front().code);
}
QString AutoMatchViewModel::status_text()const {
    if(!owner())return QStringLiteral("UNAVAILABLE");
    using P=Action::Phase;
    switch(action_.phase()) {
    case P::idle:return QStringLiteral("READY");
    case P::searching:return QStringLiteral("SEARCHING");
    case P::cancelling:return QStringLiteral("CANCELLING");
    case P::candidate_ready:return action_.replacing()
        ?QStringLiteral("REPLACEMENT READY"):QStringLiteral("CANDIDATE READY");
    case P::no_correction:return QStringLiteral("NO CORRECTION REQUIRED");
    case P::no_correction_prior_retained:return QStringLiteral("NO CORRECTION / REVIEW REMOVAL");
    case P::no_correction_cleared:return QStringLiteral("NO CORRECTION / MANUAL");
    case P::zero_amount_prior_retained:return QStringLiteral("AMOUNT ZERO / REVIEW SUSPENSION");
    case P::match_suspended:return QStringLiteral("MATCH SUSPENDED");
    case P::kept_current:return QStringLiteral("KEEP CURRENT");
    case P::cancelled:return QStringLiteral("CANCELLED");
    case P::stale:return QStringLiteral("INPUT CHANGED");
    case P::failed:return QStringLiteral("AUTOMATCH UNAVAILABLE");
    case P::materialized:return QStringLiteral("APPLIED");
    case P::verifying:return QStringLiteral("CHECKING RESULT");
    case P::numerical_pass:return QStringLiteral("NUMERICAL CHECK PASSED");
    case P::verification_incomplete:return QStringLiteral("CHECK INCOMPLETE");
    }
    return {};
}
bool AutoMatchViewModel::start(double total,double tonal,double dynamics,double stereo,
    double loudness,double ceiling)
{
    if(!can_start()||can_adopt())return false;
    error_.clear();
    const std::array<double,4> values{tonal,dynamics,stereo,loudness};
    const auto valid=[](double v){return std::isfinite(v)&&v>=0.0&&v<=1.0;};
    if(!valid(total)||!std::all_of(values.begin(),values.end(),valid)||
       !std::isfinite(ceiling)||ceiling< -12.0||ceiling>0.0||
       amount_revision_==std::numeric_limits<std::uint64_t>::max()) {
        error_=QStringLiteral("Invalid AutoMatch settings.");emit changed();return false;
    }
    const std::array<bool,4> requested{tonal>0.0,dynamics>0.0,stereo>0.0,loudness>0.0};
    const auto block=core::FrameCount::create(4096);
    if(!block)return false;
    const a::m08_frozen::AmountConfig amount{total,values,++amount_revision_};
    auto operation=a::begin_native_qt_product_session_on_owner(product_.upstream.project,
        product_,fanout_,selector_,gold_,amount,requested,ceiling,*block.value(),
        new_uuid(),new_uuid(),new_uuid(),true);
    if(!operation) {
        error_=QString::fromStdString(operation.error()->message());emit changed();return false;
    }
    if(!action_.begin(std::move(*operation.value()),[] {return new_uuid();})) {
        error_=QStringLiteral("AutoMatch search could not start.");emit changed();return false;
    }
    search_clock_.start();
    finished_search_ms_=-1;
    search_clock_running_=true;
    timer_.start();emit changed();return true;
}
void AutoMatchViewModel::cancel() {
    if(!can_cancel())return;
    action_.cancel();refresh();
}
bool AutoMatchViewModel::adopt() {
    if(!can_adopt())return false;
    publishing_=true;
    const auto accepted=action_.adopt();
    publishing_=false;
    if(!accepted) {
        error_=action_.error().empty()?QStringLiteral("The candidate is no longer applicable."):
            QString::fromStdString(action_.error());
        refresh();return false;
    }
    // The worker derives hard safety from the actual fresh native
    // execution, exact graph and raw stereo predicates; QML grants no PASS.
    if(action_.phase()==Action::Phase::no_correction_cleared||
       action_.phase()==Action::Phase::match_suspended) {refresh();return true;}
    const auto checking=action_.begin_verification(a::m13_frozen::MandatoryHardPredicate::unavailable,true);
    if(checking)timer_.start();
    refresh();return true;
}
bool AutoMatchViewModel::undo() {
    if(!can_undo())return false;
    publishing_=true;
    const auto result=product_.upstream.project.undo_private_m12_materialization();
    publishing_=false;
    if(!result)error_=QString::fromStdString(result.error()->message());
    else {error_.clear();(void)action_.retire_after_history_transition();}
    refresh();return static_cast<bool>(result);
}
bool AutoMatchViewModel::redo() {
    if(!can_redo())return false;
    publishing_=true;
    const auto result=product_.upstream.project.redo_private_m12_materialization();
    publishing_=false;
    if(!result)error_=QString::fromStdString(result.error()->message());
    else {error_.clear();(void)action_.retire_after_history_transition();}
    refresh();return static_cast<bool>(result);
}
bool AutoMatchViewModel::resume() {
    if(!can_resume())return false;
    publishing_=true;
    const auto result=product_.upstream.project.resume_private_m12_suspension();
    publishing_=false;
    if(!result)error_=QString::fromStdString(result.error()->message());
    else {error_.clear();(void)action_.retire_after_history_transition();}
    refresh();return static_cast<bool>(result);
}
void AutoMatchViewModel::refresh() {
    if(!owner()||publishing_)return;
    action_.poll();
    // Freeze search wall time on the first terminal/search-review phase.
    // M13 verification may run later and must not inflate search duration.
    if(search_clock_running_ && action_.phase()!=Action::Phase::searching
       && action_.phase()!=Action::Phase::cancelling) {
        finished_search_ms_=search_clock_.elapsed();
        search_clock_running_=false;
    }
    if(!action_.busy())timer_.stop();
    emit changed();
}
} // namespace rgsml::app
