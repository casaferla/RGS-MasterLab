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
    AuditionSourceSelector& selector,const GoldSelectionViewModel& gold,QObject* parent)
    :QObject(parent),product_(product),fanout_(fanout),selector_(selector),gold_(gold),
     action_(product,fanout,selector,gold)
{
    timer_.setInterval(100);
    connect(&timer_,&QTimer::timeout,this,&AutoMatchViewModel::refresh);
    connect(&product_.upstream.source,&SourceSelectionViewModel::sourceChanged,
        this,[this] {
            if(transfer_busy()||transfer_ready())discardTransfer();
            refresh();
        },Qt::QueuedConnection);
    connect(&gold_,&GoldSelectionViewModel::changed,this,&AutoMatchViewModel::refresh,Qt::QueuedConnection);
    connect(&selector_,&AuditionSourceSelector::changed,this,&AutoMatchViewModel::refresh,Qt::QueuedConnection);
    connect(&product_.upstream.project,&ProjectSessionViewModel::changed,
        this,&AutoMatchViewModel::refresh,Qt::QueuedConnection);
}
AutoMatchViewModel::~AutoMatchViewModel()
{
    if(transfer_cancel_)transfer_cancel_->store(true,std::memory_order_release);
    if(selector_.private_transfer_audition_active())(void)selector_.end_private_transfer_audition();
}
bool AutoMatchViewModel::transfer_busy()const{return owner()&&transfer_worker_.valid();}
bool AutoMatchViewModel::transfer_ready()const{return owner()&&static_cast<bool>(transfer_candidate_);}
bool AutoMatchViewModel::can_begin_transfer()const
{
    return owner()&&!busy()&&!action_.can_adopt()&&!transfer_ready()&&
        product_.upstream.project.private_m12_match_state()==ProjectSessionViewModel::M12CurrentMatchState::active;
}
QString AutoMatchViewModel::transfer_text()const{return owner()?transfer_text_:QString{};}
bool AutoMatchViewModel::beginTransfer(const QString& instanceId)
{
    if(!can_begin_transfer())return false;
    auto id=core::Uuid::parse(instanceId.toStdString());
    if(!id)return false;
    auto capture=product_.upstream.project.capture_private_manual_match_transfer(*id.value());
    if(!capture){
        transfer_text_=QString::fromStdString(capture.error()->message());emit changed();return false;
    }
    try {
        auto cancel=std::make_shared<std::atomic<bool>>(false);
        auto frozen=*capture.value();
        auto task=std::async(std::launch::async,[frozen,cancel]{
            return a::ManualMatchTransferAudition::create(frozen,
                *core::FrameCount::create(4096).value(),cancel.get());
        });
        transfer_cancel_=std::move(cancel);
        transfer_worker_=std::move(task);
        transfer_reference_revision_=gold_.selection_revision();
        transfer_text_=QStringLiteral("Rendering transfer candidate…");
        timer_.start();emit changed();return true;
    }catch(...) {
        transfer_text_=QStringLiteral("Transfer render could not start.");emit changed();return false;
    }
}
bool AutoMatchViewModel::transfer_listening()const
{return owner()&&selector_.private_transfer_audition_active();}
bool AutoMatchViewModel::listenTransfer()
{
    if(!transfer_ready()||busy()||transfer_listening())return false;
    const auto candidate=transfer_candidate_;
    const auto& capture=candidate->capture();
    if(gold_.selection_revision()!=transfer_reference_revision_||
       !product_.upstream.project.private_manual_match_transfer_still_current(capture)){
        discardTransfer();return false;
    }
    if(selector_.active_target()!=AuditionTarget::PREPARED) {
        const auto selected=selector_.switch_to(AuditionTarget::PREPARED);
        if(!selected){transfer_text_=QString::fromStdString(selected.error()->message());emit changed();return false;}
    }
    if(!product_.upstream.project.private_manual_match_transfer_still_current(capture)){
        discardTransfer();return false;
    }
    const auto pcm=std::shared_ptr<const render::RenderResult>(candidate,&candidate->rendered());
    const auto result=selector_.begin_private_transfer_audition(pcm,capture.native.exact_prior_owner->exact_prepared_owner);
    transfer_text_=result?QStringLiteral("Listening to transfer candidate / NOT VERIFIED"):
        QString::fromStdString(result.error()->message());
    emit changed();return static_cast<bool>(result);
}
bool AutoMatchViewModel::stopTransferListening()
{
    if(!owner())return false;
    const auto result=selector_.end_private_transfer_audition();
    transfer_text_=result?QStringLiteral("Candidate ready. PREPARED restored."):
        QString::fromStdString(result.error()->message());
    emit changed();return static_cast<bool>(result);
}
void AutoMatchViewModel::discardTransfer()
{
    if(!owner()||publishing_)return;
    if(transfer_listening()&&!stopTransferListening())return;
    if(transfer_cancel_)transfer_cancel_->store(true,std::memory_order_release);
    transfer_candidate_.reset();
    transfer_text_=QStringLiteral("Transfer discarded.");
    emit changed();
}
bool AutoMatchViewModel::applyTransfer()
{
    if(!transfer_ready()||busy()||action_.can_adopt())return false;
    if(transfer_listening()&&!stopTransferListening())return false;
    if(gold_.selection_revision()!=transfer_reference_revision_){discardTransfer();return false;}
    publishing_=true;
    const auto candidate=transfer_candidate_;
    const auto& capture=candidate->capture();
    const a::ManualMatchTransferConfirmation confirmation{capture.product_epoch,capture.project.identity.sha256};
    const auto result=product_.upstream.project.apply_private_manual_match_transfer(candidate,confirmation);
    publishing_=false;
    transfer_candidate_.reset();
    if(!result)transfer_text_=QString::fromStdString(result.error()->message());
    else {
        transfer_text_=QStringLiteral("Transfer applied / not verified.");
        (void)action_.retire_after_history_transition();
    }
    refresh();return static_cast<bool>(result);
}
void AutoMatchViewModel::poll_transfer()
{
    using namespace std::chrono_literals;
    if((transfer_busy()||transfer_ready())&&
       gold_.selection_revision()!=transfer_reference_revision_) {
        if(transfer_cancel_)transfer_cancel_->store(true,std::memory_order_release);
        if(transfer_listening()&&!stopTransferListening())return;
        transfer_candidate_.reset();
        transfer_text_=QStringLiteral("Transfer expired: reference changed.");
    }
    if(transfer_candidate_&&!product_.upstream.project.private_manual_match_transfer_owner_current(
        transfer_candidate_->capture())) {
        if(transfer_listening()&&!stopTransferListening())return;
        transfer_candidate_.reset();
        transfer_text_=QStringLiteral("Transfer expired: project changed.");
    }
    if(transfer_worker_.valid()&&transfer_worker_.wait_for(0ms)==std::future_status::ready) {
        try {
            auto done=transfer_worker_.get();
            if(transfer_cancel_&&transfer_cancel_->load(std::memory_order_acquire))
                transfer_text_=QStringLiteral("Transfer discarded.");
            else if(!done)transfer_text_=QString::fromStdString(done.error()->message());
            else if(!product_.upstream.project.private_manual_match_transfer_still_current(done.value()->capture()))
                transfer_text_=QStringLiteral("Transfer expired: project changed.");
            else {
                transfer_candidate_=std::make_shared<const a::ManualMatchTransferAudition>(std::move(*done.value()));
                const auto& graph=transfer_candidate_->capture().native;
                const auto node=graph.manual_after.proposed_manual_chain.find_instance(graph.manual_instance);
                const auto type=node&&node.value()->get().module_type_id()=="rgsml.dsp.dynamic-eq"
                    ?QStringLiteral("Dynamic EQ"):QStringLiteral("Transient Shaper");
                transfer_text_=QStringLiteral("%1 candidate rendered. Apply to move it into MANUAL; processing order changes and fresh verification is required.").arg(type);
            }
        }catch(...) {transfer_text_=QStringLiteral("Transfer render failed.");}
        transfer_cancel_.reset();
    }
}
bool AutoMatchViewModel::owner()const{return QThread::currentThread()==thread();}
bool AutoMatchViewModel::busy()const{return owner()&&(publishing_||action_.busy()||transfer_busy());}
bool AutoMatchViewModel::can_start()const {
    return owner()&&!busy()&&!transfer_ready()&&product_.upstream.source.has_source()&&gold_.has_gold()&&
        selector_.prepared_available()&&product_.upstream.project.private_m12_match_state()!=
            ProjectSessionViewModel::M12CurrentMatchState::manual_transfer;
}
bool AutoMatchViewModel::can_adopt()const {
    return owner()&&!busy()&&action_.can_adopt();
}
bool AutoMatchViewModel::can_cancel()const {
    return owner()&&!publishing_&&(action_.phase()==Action::Phase::searching||
        action_.can_adopt()||action_.phase()==Action::Phase::verifying);
}
bool AutoMatchViewModel::can_undo()const {
    return owner()&&!busy()&&!transfer_ready()&&!action_.can_adopt()&&
        (product_.upstream.project.can_undo_private_transferred_manual_edit()||
         product_.upstream.project.can_undo_private_manual_match_transfer()||
         product_.upstream.project.can_undo_private_m12_materialization());
}
bool AutoMatchViewModel::can_redo()const {
    return owner()&&!busy()&&!transfer_ready()&&!action_.can_adopt()&&
        (product_.upstream.project.can_redo_private_transferred_manual_edit()||
         product_.upstream.project.can_redo_private_manual_match_transfer()||
         product_.upstream.project.can_redo_private_m12_materialization());
}
bool AutoMatchViewModel::can_resume()const {
    return owner()&&!busy()&&!transfer_ready()&&!action_.can_adopt()&&
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
    case S::manual_transfer:return QStringLiteral("Manual transfer / not verified");
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
        text+=QStringLiteral("\nRefinement: %1 | attempts: %2")
            .arg(reason).arg(batch.total_attempted);
    } else {
        text+=QStringLiteral("\nRefinement: not reached");
    }
    const auto& perf=worker.performance;
    text+=QStringLiteral("\nNative renders: %1 complete, %2 failed, %3 cache hits")
        .arg(perf.completed).arg(perf.failed).arg(perf.cache_hits);
    text+=QStringLiteral("\nPreflight rejected: %1 | budget refusals: %2")
        .arg(perf.evaluator_preflight_rejections).arg(perf.budget_refusals);
    static constexpr const char* reasons[]={"measured","stale/unattested","render/analysis","triple authority","M07 incomplete","M08/M11 rejected"};
    for(std::size_t i=0;i<perf.native_cost_reasons.size();++i) {
        if(!perf.native_cost_reasons[i])continue;
        text+=QStringLiteral("\n%1: %2").arg(QString::fromLatin1(reasons[i])).arg(perf.native_cost_reasons[i]);
        if(!perf.first_native_failure_details[i].empty())
            text+=QStringLiteral(" (%1)").arg(QString::fromStdString(perf.first_native_failure_details[i]));
    }
    const auto elapsed=completed.original_pair_ms+worker.timing.fixed_ms+
        worker.timing.seed_ms+worker.timing.search_ms;
    text+=QStringLiteral("\nMeasured phases: %1 s (original %2 / fixed %3 / seed %4 / search %5)")
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
    if(!owner()||product_.upstream.project.private_m12_match_state()==
       ProjectSessionViewModel::M12CurrentMatchState::manual_transfer||!action_.verification_result())return {};
    const auto& assessment=action_.verification_result()->assessment();
    if(assessment.native_numerical_evidence_passed())
        return QStringLiteral("Numerical evidence passed; formal qualification not granted.");
    if(assessment.canonical_reasons.empty())return QStringLiteral("Verification incomplete.");
    return QString::fromStdString(assessment.canonical_reasons.front().code);
}
QString AutoMatchViewModel::status_text()const {
    if(!owner())return QStringLiteral("UNAVAILABLE");
    if(transfer_busy())return QStringLiteral("RENDERING TRANSFER");
    if(transfer_ready())return QStringLiteral("TRANSFER READY");
    if(product_.upstream.project.private_m12_match_state()==ProjectSessionViewModel::M12CurrentMatchState::manual_transfer)
        return QStringLiteral("MANUAL TRANSFER / NOT VERIFIED");
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
    case P::failed:return QStringLiteral("SEARCH FAILED");
    case P::materialized:return QStringLiteral("APPLIED");
    case P::verifying:return QStringLiteral("CHECKING RESULT");
    case P::numerical_pass:return QStringLiteral("NUMERICAL CHECK PASSED");
    case P::verification_incomplete:return QStringLiteral("CHECK INCOMPLETE");
    }
    return {};
}
bool AutoMatchViewModel::startProduct(double total,double tonal,double dynamics,double stereo,
    double loudness)
{
    // Never read this from the M17 Export TP Limiter or the solver-owned
    // streaming limiter. The final independent M16 check remains active.
    constexpr double independentProductM16CeilingDbTp=0.0;
    return start(total,tonal,dynamics,stereo,loudness,independentProductM16CeilingDbTp);
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
    auto& project=product_.upstream.project;
    const auto result=project.can_undo_private_transferred_manual_edit()
        ?project.undo_private_transferred_manual_edit()
        :project.can_undo_private_manual_match_transfer()
            ?project.undo_private_manual_match_transfer():project.undo_private_m12_materialization();
    publishing_=false;
    if(!result)error_=QString::fromStdString(result.error()->message());
    else {error_.clear();(void)action_.retire_after_history_transition();}
    refresh();return static_cast<bool>(result);
}
bool AutoMatchViewModel::redo() {
    if(!can_redo())return false;
    publishing_=true;
    auto& project=product_.upstream.project;
    const auto result=project.can_redo_private_transferred_manual_edit()
        ?project.redo_private_transferred_manual_edit()
        :project.can_redo_private_manual_match_transfer()
            ?project.redo_private_manual_match_transfer():project.redo_private_m12_materialization();
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
    poll_transfer();
    // Freeze search wall time on the first terminal/search-review phase.
    // M13 verification may run later and must not inflate search duration.
    if(search_clock_running_ && action_.phase()!=Action::Phase::searching
       && action_.phase()!=Action::Phase::cancelling) {
        finished_search_ms_=search_clock_.elapsed();
        search_clock_running_=false;
    }
    if(!action_.busy()&&!transfer_busy())timer_.stop();
    emit changed();
}
} // namespace rgsml::app
