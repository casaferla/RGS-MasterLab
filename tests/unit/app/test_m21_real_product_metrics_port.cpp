#include "internal/m21_real_product_metrics_port.hpp"
#include "internal/l2_automatch_evidence_bridge.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace rgsml::tests {

class M21RealProductMetricsPortTest final : public QObject {
    Q_OBJECT
private slots:
    void actualM19M20GoldMinusPreparedWithAuthenticM16TP();
    void mismatchedScopeOrUntrustedOwnerDenied();
    void l2FullTrackPairPreservesM21DeltasAndRateIndependence();
    void l2RejectsMissingStaleAndUnsupportedScopes();
};

void M21RealProductMetricsPortTest::
actualM19M20GoldMinusPreparedWithAuthenticM16TP()
{
    using namespace app::m21_private;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    const auto count=core::FrameCount::create(19200);
    QVERIFY(format&&count);
    auto prepared=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{123},*count.value());
    auto gold=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{123},*count.value());
    QVERIFY(prepared&&gold);
    auto p=prepared.value()->mutable_view().channel(0);
    auto g=gold.value()->mutable_view().channel(0);
    QVERIFY(p&&g);
    for(std::size_t i=0;i<p.value()->size();++i) {
        const auto sine=std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
        (*p.value())[i]=0.25*sine;
        (*g.value())[i]=0.5*sine;
    }
    const auto pl=analysis::measure_basic_level_full_resource(*prepared.value());
    const auto gl=analysis::measure_basic_level_full_resource(*gold.value());
    const auto ps=analysis::measure_tonal_stereo_scope(
        prepared.value()->view(),core::FrameIndex{123},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    const auto gs=analysis::measure_tonal_stereo_scope(
        gold.value()->view(),core::FrameIndex{123},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(pl&&gl&&ps&&gs);
    const auto pImported=import_real_product_metrics(*pl.value(),*ps.value(),
        {Column::prepared,1U,"exact-prepared-owner","Full Track",true});
    const auto gImported=import_real_product_metrics(*gl.value(),*gs.value(),
        {Column::gold,1U,"independent-gold-owner","Full Track",true});
    QVERIFY(pImported.snapshot.has_value());
    QVERIFY(gImported.snapshot.has_value());
    QVERIFY(pImported.problem==RealProductPortProblem::none);
    QVERIFY(gImported.problem==RealProductPortProblem::none);
    QCOMPARE(pImported.snapshot->metrics[3].quality,Quality::low_confidence);
    QVERIFY(pImported.snapshot->metrics[3].unrounded);
    QVERIFY(std::isfinite(*pImported.snapshot->metrics[3].unrounded));
    QVERIFY(pl.value()->true_peak_db_tp.db);
    QVERIFY(std::abs(*pImported.snapshot->metrics[3].unrounded-
                     *pl.value()->true_peak_db_tp.db)<1e-12);
    QVERIFY(!pImported.snapshot->average_spectrum.empty());

    PrivatePublicationGuard guard;
    QVERIFY(guard.expect(Column::prepared,1U));
    QVERIFY(guard.expect(Column::gold,1U));
    QVERIFY(guard.publish(*pImported.snapshot));
    QVERIFY(guard.publish(*gImported.snapshot));
    const auto first=guard.view();
    QVERIFY(first.independent_scopes);
    QCOMPARE(first.prepared_to_gold[1].quality,Quality::present);
    QVERIFY(first.prepared_to_gold[1].unrounded);
    QVERIFY(std::abs(*first.prepared_to_gold[1].unrounded-
                     20.0*std::log10(2.0))<1e-6);
    QCOMPARE(first.prepared_to_gold[3].quality,Quality::low_confidence);
    QVERIFY(first.prepared_to_gold[3].unrounded);
    QVERIFY(gl.value()->true_peak_db_tp.db);
    QVERIFY(std::abs(*first.prepared_to_gold[3].unrounded-(
        *gl.value()->true_peak_db_tp.db-
        *pl.value()->true_peak_db_tp.db))<1e-12);
    QCOMPARE(first.prepared_to_gold[5].quality,Quality::not_applicable);
    QCOMPARE(first.processed_to_gold[1].quality,Quality::missing);

    // Independent PREPARED replacement must invalidate only that slot,
    // never silently retag the GOLD or publish a late previous epoch.
    QVERIFY(guard.expect(Column::prepared,2U));
    QVERIFY(!guard.publish(*pImported.snapshot));
    const auto stale=guard.view();
    QCOMPARE(stale.prepared_to_gold[1].quality,Quality::stale);
    QCOMPARE(stale.columns[1]->lifecycle,Lifecycle::current);
}

void M21RealProductMetricsPortTest::
mismatchedScopeOrUntrustedOwnerDenied()
{
    using namespace app::m21_private;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    const auto count=core::FrameCount::create(19200);
    QVERIFY(format&&count);
    auto owner=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},*count.value());
    QVERIFY(owner);
    auto pcm=owner.value()->mutable_view().channel(0);
    QVERIFY(pcm);
    std::fill(pcm.value()->begin(),pcm.value()->end(),0.3);
    const auto l=analysis::measure_basic_level_full_resource(*owner.value());
    const auto s=analysis::measure_tonal_stereo_scope(
        owner.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::OUTPUT_RATE);
    QVERIFY(l&&s);
    auto noAuthority=import_real_product_metrics(*l.value(),*s.value(),
        {Column::processed,1U,"owner","Full Track",false});
    QVERIFY(!noAuthority.snapshot);
    QCOMPARE(noAuthority.problem,RealProductPortProblem::missing_authority);
    auto mismatch=*s.value();
    mismatch.scope_begin_frame=1;
    auto bad=import_real_product_metrics(*l.value(),mismatch,
        {Column::processed,1U,"owner","Full Track",true});
    QVERIFY(!bad.snapshot);
    QCOMPARE(bad.problem,RealProductPortProblem::mismatched_scope);
}


void M21RealProductMetricsPortTest::
l2FullTrackPairPreservesM21DeltasAndRateIndependence()
{
    using namespace app::m21_private;
    using namespace app::l2_minimal;
    Snapshot current;
    current.column=Column::processed;
    current.generation_token=51;
    current.resource_identity="actual-processed-realization-51";
    current.analysis_scope_label="Full Track";
    current.scope_frames=48000;
    current.sample_rate_hz=48000;
    current.lifecycle=Lifecycle::current;
    current.metrics[0]={-16.,Quality::present,{}};
    current.metrics[1]={-17.,Quality::present,{}};
    current.metrics[3]={-1.,Quality::low_confidence,{}};

    Snapshot reference;
    reference.column=Column::gold;
    reference.generation_token=18;
    reference.resource_identity="selected-independent-reference-18";
    reference.analysis_scope_label="Full Track";
    reference.scope_frames=88200;
    reference.sample_rate_hz=44100; // NEVER require an SRC to compare levels
    reference.lifecycle=Lifecycle::current;
    reference.metrics[0]={-14.,Quality::present,{}};
    reference.metrics[1]={-15.,Quality::present,{}};
    reference.metrics[3]={-2.,Quality::low_confidence,{}};

    PrivatePublicationGuard auth;
    QVERIFY(auth.expect(Column::processed,51));
    QVERIFY(auth.expect(Column::gold,18));
    QVERIFY(auth.publish(current));
    QVERIFY(auth.publish(reference));
    const auto live=auth.view();
    const auto result=make_evidence_pair(live,Column::processed);
    QVERIFY(result.evidence);
    QVERIFY(result.problem==PairProblem::none);
    QCOMPARE(result.evidence->usable_count,std::size_t{2});
    QCOMPARE(result.evidence->candidate_sample_rate_hz,std::uint32_t{48000});
    QCOMPARE(result.evidence->reference_sample_rate_hz,std::uint32_t{44100});
    QVERIFY(result.evidence->usable_metrics[0]);
    QCOMPARE(result.evidence->usable_metrics[0]->candidate,-16.);
    QCOMPARE(result.evidence->usable_metrics[0]->reference,-14.);
    QCOMPARE(result.evidence->usable_metrics[0]->reference_minus_candidate,2.);
    QCOMPARE(result.evidence->usable_metrics[1]->reference_minus_candidate,2.);
    QVERIFY(!result.evidence->usable_metrics[3]); // no low-confidence TP objective
    QVERIFY(matches_current(*result.evidence,auth.view()));

    // A new processed candidate invalidates the old receipt but preserves
    // the reference; future L3 searches need fresh measured PCM each time.
    QVERIFY(auth.expect(Column::processed,52));
    QVERIFY(!matches_current(*result.evidence,auth.view()));
    current.generation_token=52;
    current.resource_identity="actual-processed-realization-52";
    current.metrics[0].unrounded=-15.;
    QVERIFY(auth.publish(current));
    const auto next=make_evidence_pair(auth.view(),Column::processed);
    QVERIFY(next.evidence);
    QCOMPARE(next.evidence->usable_metrics[0]->reference_minus_candidate,1.);
    QVERIFY(!matches_current(*result.evidence,auth.view()));
    QVERIFY(matches_current(*next.evidence,auth.view()));

    // A new independent REFERENCE invalidates the candidate/reference pair
    // even if the processed PCM has not changed.
    QVERIFY(auth.expect(Column::gold,19));
    QVERIFY(!matches_current(*next.evidence,auth.view()));
}

void M21RealProductMetricsPortTest::
l2RejectsMissingStaleAndUnsupportedScopes()
{
    using namespace app::m21_private;
    using namespace app::l2_minimal;
    Comparison view{};
    QVERIFY(make_evidence_pair(view,Column::processed).problem
        ==PairProblem::missing_observation);
    QVERIFY(make_evidence_pair(view,Column::gold).problem
        ==PairProblem::invalid_column);
    Snapshot p;
    p.column=Column::prepared;
    p.generation_token=1;
    p.resource_identity="genuine-prepared";
    p.analysis_scope_label="Full Track";
    p.scope_frames=500;
    p.sample_rate_hz=48000;
    p.lifecycle=Lifecycle::current;
    p.metrics[1]={-20.,Quality::present,{}};
    Snapshot g=p;
    g.column=Column::gold;
    g.resource_identity="genuine-reference";
    view.columns[0]=p;
    view.columns[1]=g;
    QVERIFY(make_evidence_pair(view,Column::prepared).evidence);
    view.columns[1]->lifecycle=Lifecycle::stale;
    QVERIFY(make_evidence_pair(view,Column::prepared).problem
        ==PairProblem::stale_owner);
    view.columns[1]->lifecycle=Lifecycle::current;
    view.columns[0]->analysis_scope_label="Region";
    QVERIFY(make_evidence_pair(view,Column::prepared).problem
        ==PairProblem::unsupported_scope);
    view.columns[0]->analysis_scope_label="Full Track";
    view.columns[0]->resource_identity.clear();
    QVERIFY(make_evidence_pair(view,Column::prepared).problem
        ==PairProblem::invalid_identity);
    view.columns[0]->resource_identity="genuine-prepared";
    view.columns[0]->metrics[1]={{},Quality::missing,"Absent"};
    QVERIFY(make_evidence_pair(view,Column::prepared).problem
        ==PairProblem::no_usable_metrics);
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M21RealProductMetricsPortTest)
#include "test_m21_real_product_metrics_port.moc"
