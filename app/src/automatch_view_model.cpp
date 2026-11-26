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
    // No GUI-generated PASS for independent mandatory predicates.
    // M13 performs the real fresh analysis; any unavailable predicate
    // remains incomplete until Product supplies its actual authority.
    if(action_.phase()==Action::Phase::no_correction_cleared||
       action_.phase()==Action::Phase::match_suspended) {refresh();return true;}
    const auto checking=action_.begin_verification(a::m13_frozen::MandatoryHardPredicate::unavailable);
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
void AutoMatchViewModel::refresh() {
    if(!owner()||publishing_)return;
    action_.poll();
    if(!action_.busy())timer_.stop();
    emit changed();
}
} // namespace rgsml::app
