// M21 PRIVATE read-only semantic/state test. NOT official Gate S/M.
#include "internal/m21_metrics_comparison_candidate.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
using namespace rgsml::app::m21_private;
static Snapshot snap(Column c,std::uint64_t gen,double lufs,std::uint32_t fs) {
    Snapshot s{};
    s.column=c;s.generation_token=gen;s.lifecycle=Lifecycle::current;
    s.resource_identity=c==Column::gold?"EXTERNAL_GOLD":"SOURCE_DERIVED";
    s.analysis_scope_label=c==Column::gold?"Region":"Full Track";
    s.absolute_scope_begin=c==Column::gold?500000:3333;
    s.scope_frames=fs*3ULL;s.sample_rate_hz=fs;
    for(auto& m:s.metrics)m={{},Quality::missing,"Producer not available"};
    s.metrics[0]={lufs,Quality::present,{}};
    s.metrics[1]={-20.,Quality::present,{}};
    s.metrics[2]={-3.,Quality::present,{}};
    s.metrics[3]={{},Quality::missing,"M16 TP authority not linked"};
    s.metrics[4]={17.,Quality::present,{}};
    s.metrics[5]={{},Quality::not_applicable,"Mono"};
    s.metrics[6]={{},Quality::not_applicable,"Mono"};
    s.metrics[7]={{},Quality::not_applicable,"Mono"};
    s.metrics[8]={{},Quality::not_applicable,"Mono"};
    s.profile_denominator=12;
    s.tonal_profile={{100,true,{3.123456,Quality::present,{}}},
                     {101,false,{{},Quality::missing,"No common support"}}};
    s.average_spectrum={{100.,{1.e-6,Quality::present,{}}}};
    return s;
}
int main(){
    static_assert(kMetricSpecs.size()==9);
    assert(std::string(kColumnNames[0])=="PREPARED");
    assert(std::string(kColumnNames[1])=="GOLD");
    assert(std::string(kColumnNames[2])=="PROCESSED");
    assert(std::string(kMetricSpecs[0].delta_unit)=="LU");
    assert(std::string(kMetricSpecs[3].absolute_unit)=="dBTP");
    for(const std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}){
        PrivatePublicationGuard guard;
        auto p=snap(Column::prepared,1,-18.,fs);
        auto g=snap(Column::gold,6,-13.,fs);
        auto x=snap(Column::processed,9,-14.,fs);
        assert(guard.expect(Column::prepared,1));
        assert(guard.expect(Column::gold,6));
        assert(guard.expect(Column::processed,9));
        assert(guard.publish(p));
        assert(guard.publish(g));
        assert(guard.publish(x));
        auto v=guard.view();
        assert(v.independent_scopes);
        assert(v.columns[0]->analysis_scope_label=="Full Track");
        assert(v.columns[1]->analysis_scope_label=="Region");
        assert(v.columns[1]->absolute_scope_begin!=v.columns[0]->absolute_scope_begin);
        assert(v.prepared_to_gold[0].quality==Quality::present);
        assert(*v.prepared_to_gold[0].unrounded==5.);
        assert(*v.processed_to_gold[0].unrounded==1.);
        assert(v.prepared_to_gold[3].quality==Quality::missing);
        assert(!v.prepared_to_gold[3].unrounded);
        assert(v.prepared_to_gold[5].quality==Quality::not_applicable);
        assert(!v.prepared_to_gold[5].unrounded);
        auto gg=g;gg.metrics[0].unrounded=-25.;
        assert(guard.publish(gg));
        v=guard.view();
        assert(*v.prepared_to_gold[0].unrounded==-7.); // Gold < Current
        assert(*v.processed_to_gold[0].unrounded==-11.);
        gg.metrics[0].unrounded=-18.;
        assert(guard.publish(gg));
        v=guard.view();
        assert(*v.prepared_to_gold[0].unrounded==0.); // Gold == Current
        gg.metrics[0].unrounded=-13.;
        assert(guard.publish(gg));
        // Unrounded canonical values, never difference of rounded strings.
        p.metrics[0].unrounded=-18.444;
        gg.metrics[0].unrounded=-13.445;
        assert(guard.publish(p)&&guard.publish(gg));
        v=guard.view();
        assert(std::abs(*v.prepared_to_gold[0].unrounded-4.999)<1e-12);
        // Per-band exact IDs; missing/out-of-common support NOT a fake zero.
        auto td=tonal_delta(&p,&gg,100);
        assert(td.quality==Quality::present && *td.unrounded==0.);
        td=tonal_delta(&p,&gg,101);
        assert(td.quality==Quality::incompatible && !td.unrounded);
        td=tonal_delta(&p,&gg,199);
        assert(td.quality==Quality::missing && !td.unrounded);
        auto qg=gg;qg.tonal_profile[0].normalized_shape_db.unrounded=4.5;
        assert(*tonal_delta(&p,&qg,100).unrounded>1.3);
        qg.profile_denominator=3;
        td=tonal_delta(&p,&qg,100);
        assert(td.quality==Quality::incompatible && !td.unrounded);
        // Low support visible while valid numerical delta is still marked.
        qg=gg;qg.metrics[0].quality=Quality::low_confidence;
        assert(guard.publish(qg));
        v=guard.view();
        assert(v.prepared_to_gold[0].quality==Quality::low_confidence);
        assert(v.prepared_to_gold[0].reason.find("LOW_CONFIDENCE")!=std::string::npos);
        // Production generator MUST supply exact epochs. Reject asynchronous
        // result after independent Mastering Chain generation changed.
        assert(guard.expect(Column::processed,10));
        assert(!guard.publish(x));
        v=guard.view();
        assert(v.columns[2]->lifecycle==Lifecycle::stale);
        assert(v.processed_to_gold[0].quality==Quality::stale);
        assert(v.prepared_to_gold[0].quality==Quality::low_confidence);
        auto newx=x;newx.generation_token=10;
        assert(guard.publish(newx));
        assert(guard.view().processed_to_gold[0].quality==Quality::low_confidence);
        // Prep change stales prepared only; Gold is preserved.
        assert(guard.expect(Column::prepared,2));
        v=guard.view();
        assert(v.columns[0]->lifecycle==Lifecycle::stale);
        assert(v.columns[1]->lifecycle==Lifecycle::current);
        assert(v.prepared_to_gold[0].quality==Quality::stale);
        assert(!guard.publish(p));
        p.generation_token=2;assert(guard.publish(p));
        // Gold change stales only Gold; all deltas must be stale.
        assert(guard.expect(Column::gold,7));
        v=guard.view();
        assert(v.columns[0]->lifecycle==Lifecycle::current);
        assert(v.columns[1]->lifecycle==Lifecycle::stale);
        assert(v.prepared_to_gold[0].quality==Quality::stale);
        assert(v.processed_to_gold[0].quality==Quality::stale);
        // Fake present numbers or nonfinite samples fail closed.
        auto bad=gg;bad.generation_token=7;
        bad.metrics[0].unrounded=std::numeric_limits<double>::quiet_NaN();
        assert(!guard.publish(bad));
        bad.metrics[0].unrounded=-13.;
        assert(guard.publish(bad));
        bad.metrics[0].unrounded=std::numeric_limits<double>::infinity();
        assert(!guard.publish(bad));
        assert(!guard.expect(Column::gold,6)); // no epoch rollback
        // Missing current Gold must disable every Gold-relative delta.
        auto no_gold=std::array<std::optional<Snapshot>,3>{p,std::nullopt,newx};
        v=compose(no_gold);
        assert(!v.prepared_to_gold[0].unrounded);
        assert(!v.processed_to_gold[0].unrounded);
        // No current Processed must remain absent (never copied from Prep).
        v=compose({p,gg,std::nullopt});
        assert(v.processed_to_gold[0].quality==Quality::missing);
        assert(!v.processed_to_gold[0].unrounded);
        // Distinct static SOURCE/GOLD spectrum passed through unchanged.
        assert(v.columns[0]->average_spectrum[0].power_density.unrounded==1.e-6);
    }
    std::cout<<"M21_PRIVATE_PREPRODUCTION_PASS_NOT_GATE_S_OR_M\n";
}
