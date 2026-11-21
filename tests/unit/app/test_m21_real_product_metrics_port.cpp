#include "internal/m21_real_product_metrics_port.hpp"
#include "internal/l2_automatch_evidence_bridge.hpp"
#include "internal/l3_automatch_offline_search.hpp"
#include "internal/l3_automatch_typed_gain_trial.hpp"
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/dynamic_eq_module.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/core/uuid.hpp>
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
#include <utility>

namespace rgsml::tests {

class M21RealProductMetricsPortTest final : public QObject {
    Q_OBJECT
private slots:
    void actualM19M20GoldMinusPreparedWithAuthenticM16TP();
    void mismatchedScopeOrUntrustedOwnerDenied();
    void l2FullTrackPairPreservesM21DeltasAndRateIndependence();
    void l2RejectsMissingStaleAndUnsupportedScopes();
    void l3OfflineSearchScoresCurrentRealM21ProducerEvidence();
    void l3OfflineSearchRejectsStaleOrUntrustedCandidateEvidence();
    void l3OfflineBudgetCapAndAttemptedFailures();
    void l3ActualGainCandidateRunsExistingRendererAndM19M20();
    void l3C01FullPrefixCandidateMeasuresRealM21Evidence();
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


void M21RealProductMetricsPortTest::
l3OfflineSearchScoresCurrentRealM21ProducerEvidence()
{
    using namespace app::m21_private;
    using namespace app::l3_automatch;
    Snapshot candidate;
    candidate.column=Column::processed;
    candidate.generation_token=7;
    candidate.resource_identity="processed-7";
    candidate.analysis_scope_label="Full Track";
    candidate.scope_frames=48000;
    candidate.sample_rate_hz=48000;
    candidate.lifecycle=Lifecycle::current;
    candidate.metrics[0]={-16.,Quality::present,{}};
    candidate.metrics[1]={-17.,Quality::present,{}};
    Snapshot reference=candidate;
    reference.column=Column::gold;
    reference.generation_token=3;
    reference.resource_identity="gold-reference-3";
    reference.sample_rate_hz=44100;
    reference.scope_frames=88200;
    reference.metrics[0]={-14.,Quality::present,{}};
    reference.metrics[1]={-15.,Quality::present,{}};

    PrivatePublicationGuard producer;
    QVERIFY(producer.expect(Column::processed,7));
    QVERIFY(producer.expect(Column::gold,3));
    QVERIFY(producer.publish(candidate));
    QVERIFY(producer.publish(reference));
    auto search=OfflineSearch::begin(998,producer.view(),
        {{"typed-gain-eq-a"},{"typed-gain-eq-b"}},
        {{Metric::integrated_lufs,1.,2.},{Metric::full_scope_rms,1.,1.}});
    QVERIFY(search);
    QCOMPARE(search->best().weighted_error,4.);
    QVERIFY(!search->best().typed_dsp_fingerprint);
    QVERIFY(!search->complete());
    auto ticket1=search->next();
    QVERIFY(ticket1);
    QVERIFY(!search->next()); // one in-flight candidate maximum
    QCOMPARE(ticket1->ordinal,std::size_t{0});
    QCOMPARE(ticket1->typed_dsp_fingerprint,std::string{"typed-gain-eq-a"});

    QVERIFY(producer.expect(Column::processed,8));
    candidate.generation_token=8;
    candidate.resource_identity="processed-8";
    candidate.metrics[0].unrounded=-15.;
    candidate.metrics[1].unrounded=-16.;
    QVERIFY(producer.publish(candidate));
    QVERIFY(search->complete(*ticket1,producer.view())==Submission::improved);
    QCOMPARE(search->best().weighted_error,1.);
    QVERIFY(search->best().typed_dsp_fingerprint);
    QCOMPARE(*search->best().typed_dsp_fingerprint,
        std::string{"typed-gain-eq-a"});

    auto ticket2=search->next();
    QVERIFY(ticket2);
    QCOMPARE(ticket2->ordinal,std::size_t{1});
    QVERIFY(producer.expect(Column::processed,9));
    candidate.generation_token=9;
    candidate.resource_identity="processed-9";
    candidate.metrics[0].unrounded=-18.;
    candidate.metrics[1].unrounded=-19.;
    QVERIFY(producer.publish(candidate));
    QVERIFY(search->complete(*ticket2,producer.view())==Submission::accepted);
    QCOMPARE(search->best().weighted_error,1.);
    QCOMPARE(search->attempts(),std::size_t{2});
    QVERIFY(search->complete());
    QVERIFY(!search->next());
    QVERIFY(search->complete(*ticket2,producer.view())
        ==Submission::no_pending_ticket);
}

void M21RealProductMetricsPortTest::
l3OfflineSearchRejectsStaleOrUntrustedCandidateEvidence()
{
    using namespace app::m21_private;
    using namespace app::l3_automatch;
    Snapshot processed;
    processed.column=Column::processed;
    processed.generation_token=4;
    processed.resource_identity="processed-4";
    processed.analysis_scope_label="Full Track";
    processed.scope_frames=48000;
    processed.sample_rate_hz=48000;
    processed.lifecycle=Lifecycle::current;
    processed.metrics[1]={-18.,Quality::present,{}};
    Snapshot gold=processed;
    gold.column=Column::gold;
    gold.generation_token=6;
    gold.resource_identity="reference-6";
    gold.metrics[1].unrounded=-16.;
    PrivatePublicationGuard producer;
    QVERIFY(producer.expect(Column::processed,4));
    QVERIFY(producer.expect(Column::gold,6));
    QVERIFY(producer.publish(processed));
    QVERIFY(producer.publish(gold));
    const auto initial=producer.view();
    QVERIFY(!OfflineSearch::begin(0,initial,{{"a"}},
        {{Metric::full_scope_rms,1.,1.}}));
    QVERIFY(!OfflineSearch::begin(5,initial,{{"a"},{"a"}},
        {{Metric::full_scope_rms,1.,1.}}));
    QVERIFY(!OfflineSearch::begin(5,initial,{{"a"}},
        {{Metric::full_scope_rms,0.,1.}}));
    QVERIFY(!OfflineSearch::begin(5,initial,{{"a"}},
        {{Metric::true_peak,1.,1.}})); // no invented TP objective
    auto search=OfflineSearch::begin(5,initial,{{"typed-compressor"}},
        {{Metric::full_scope_rms,1.,1.}});
    QVERIFY(search);
    const auto ticket=search->next();
    QVERIFY(ticket);
    auto fake=*ticket;
    ++fake.run_id;
    QVERIFY(search->complete(fake,initial)==Submission::mismatched_ticket);
    QVERIFY(!search->cancelled());
    QVERIFY(producer.expect(Column::gold,7));
    gold.generation_token=7;
    gold.resource_identity="reference-replaced-7";
    QVERIFY(producer.publish(gold));
    QVERIFY(search->complete(*ticket,producer.view())
        ==Submission::stale_or_invalid_product);
    QVERIFY(search->cancelled());
    QVERIFY(!search->next());
    QVERIFY(search->complete(*ticket,producer.view())==Submission::cancelled);
}


void M21RealProductMetricsPortTest::
l3OfflineBudgetCapAndAttemptedFailures()
{
    using namespace app::m21_private;
    using namespace app::l3_automatch;
    // This is a budget accounting test, not 800 audio executions.
    Snapshot processed;
    processed.column=Column::processed;
    processed.generation_token=100;
    processed.resource_identity="owner-authenticated-processed";
    processed.analysis_scope_label="Full Track";
    processed.scope_frames=48000;
    processed.sample_rate_hz=48000;
    processed.lifecycle=Lifecycle::current;
    processed.metrics[1]={-18.0,Quality::present,{}};
    auto gold=processed;
    gold.column=Column::gold;
    gold.generation_token=2;
    gold.resource_identity="independent-reference";
    gold.metrics[1].unrounded=-16.0;
    PrivatePublicationGuard guard;
    QVERIFY(guard.expect(Column::processed,100));
    QVERIFY(guard.expect(Column::gold,2));
    QVERIFY(guard.publish(processed));
    QVERIFY(guard.publish(gold));
    std::vector<Proposal> proposals;
    for(std::size_t i=0;i<800;++i)
        proposals.push_back(Proposal{"offline-candidate-"+std::to_string(i)});
    const std::vector<Objective> rms{{
        Metric::full_scope_rms,1.0,1.0}};
    auto oversize=proposals;
    oversize.push_back(Proposal{"not-admitted-801"});
    QVERIFY(!OfflineSearch::begin(5,guard.view(),std::move(oversize),rms));
    auto run=OfflineSearch::begin(5,guard.view(),std::move(proposals),rms);
    QVERIFY(run);
    QCOMPARE(run->best().weighted_error,4.0);
    auto first=run->next();
    QVERIFY(first);
    auto wrong=*first;
    ++wrong.ordinal;
    QVERIFY(run->attempted_evaluation_failed(wrong)==Submission::mismatched_ticket);
    QCOMPARE(run->attempts(),std::size_t{0});
    QCOMPARE(run->next().has_value(),false); // one pending ticket
    QVERIFY(run->attempted_evaluation_failed(*first)
        ==Submission::attempted_evaluation_failed);
    QCOMPARE(run->attempts(),std::size_t{1});
    QVERIFY(!run->best().typed_dsp_fingerprint);
    for(std::size_t ordinal=1;ordinal<800;++ordinal) {
        auto ticket=run->next();
        QVERIFY(ticket);
        QCOMPARE(ticket->ordinal,ordinal);
        QVERIFY(run->attempted_evaluation_failed(*ticket)
            ==Submission::attempted_evaluation_failed);
    }
    QCOMPARE(run->attempts(),std::size_t{800});
    QVERIFY(run->complete());
    QVERIFY(!run->next());
    QCOMPARE(run->best().weighted_error,4.0);
    QVERIFY(!run->best().typed_dsp_fingerprint);
}

void M21RealProductMetricsPortTest::
l3ActualGainCandidateRunsExistingRendererAndM19M20()
{
    using namespace app::m21_private;
    using namespace app::l3_automatch;
    const auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto uuid=core::Uuid::parse(
        "aad00000-0000-4000-8000-000000000001");
    QVERIFY(uuid);
    const auto gainId=dsp::ModuleInstanceId::from_uuid(*uuid.value());
    QVERIFY(gainId);
    QVERIFY(chain.value()->add(*gainId.value(),"rgsml.dsp.gain",0));
    const auto initialGain=dsp::GainParameters::create(0.);
    QVERIFY(initialGain);
    const std::vector<dsp::ModuleExecutionBinding> baseline{
        {*gainId.value(),*initialGain.value()}};
    auto candidate=make_gain_trial(*chain.value(),baseline,*gainId.value(),
        6.020599913279624,"typed-actual-gain-6db");
    QVERIFY(candidate);
    QVERIFY(candidate->still_matches(*chain.value(),baseline));
    QCOMPARE(candidate->candidate_bindings.size(),std::size_t{1});
    QVERIFY(candidate->candidate_bindings!=baseline);
    QVERIFY(!make_gain_trial(*chain.value(),baseline,*gainId.value(),
        0.0,"noop"));
    QVERIFY(!make_gain_trial(*chain.value(),baseline,*gainId.value(),
        25.0,"out-of-range"));
    QVERIFY(!make_gain_trial(*chain.value(),baseline,*gainId.value(),
        4.0,""));
    const auto rate=core::SampleRate::create(48000);
    const auto count=core::FrameCount::create(48000);
    const auto block=core::FrameCount::create(4096);
    QVERIFY(rate&&count&&block);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto source=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    auto reference=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    QVERIFY(source&&reference);
    const auto a=source.value()->mutable_view().channel(0);
    const auto b=reference.value()->mutable_view().channel(0);
    QVERIFY(a&&b);
    for(std::size_t i=0;i<a.value()->size();++i) {
        const auto tone=std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.);
        (*a.value())[i]=0.15*tone;
        (*b.value())[i]=0.30*tone;
    }
    // The *same* canonical renderer that MasteringPreviewController
    // invokes: this is not a second DSP implementation or a fake score.
    const auto originalRequest=render::RenderRequest::create(
        source.value()->view(),source.value()->view().absolute_range(),
        *chain.value(),baseline,*block.value());
    QVERIFY(originalRequest);
    const auto trialRequest=render::RenderRequest::create(
        source.value()->view(),source.value()->view().absolute_range(),
        *chain.value(),candidate->candidate_bindings,*block.value());
    QVERIFY(trialRequest);
    const auto rendered0=render::render_preview(
        *originalRequest.value(),*registry.value());
    const auto rendered1=render::render_preview(
        *trialRequest.value(),*registry.value());
    QVERIFY(rendered0&&rendered1);
    QVERIFY(candidate->still_matches(*chain.value(),baseline));
    const auto baseLevels=analysis::measure_basic_level_full_resource(
        rendered0.value()->buffer());
    const auto gainLevels=analysis::measure_basic_level_full_resource(
        rendered1.value()->buffer());
    const auto refLevels=analysis::measure_basic_level_full_resource(
        *reference.value());
    QVERIFY(baseLevels&&gainLevels&&refLevels);
    const auto baseStereo=analysis::measure_tonal_stereo_scope(
        rendered0.value()->view(),core::FrameIndex{0},*count.value(),
        rendered0.value()->view().timebase().frame_domain_id());
    const auto gainStereo=analysis::measure_tonal_stereo_scope(
        rendered1.value()->view(),core::FrameIndex{0},*count.value(),
        rendered1.value()->view().timebase().frame_domain_id());
    const auto refStereo=analysis::measure_tonal_stereo_scope(
        reference.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(baseStereo&&gainStereo&&refStereo);
    const auto baselineMetrics=import_real_product_metrics(
        *baseLevels.value(),*baseStereo.value(),
        {Column::processed,1U,"real-gain-render-0","Full Track",true});
    const auto trialMetrics=import_real_product_metrics(
        *gainLevels.value(),*gainStereo.value(),
        {Column::processed,2U,"real-gain-render-1","Full Track",true});
    const auto referenceMetrics=import_real_product_metrics(
        *refLevels.value(),*refStereo.value(),
        {Column::gold,1U,"real-reference-a","Full Track",true});
    QVERIFY(baselineMetrics.snapshot);
    QVERIFY(trialMetrics.snapshot);
    QVERIFY(referenceMetrics.snapshot);
    PrivatePublicationGuard producer;
    QVERIFY(producer.expect(Column::gold,1U));
    QVERIFY(producer.expect(Column::processed,1U));
    QVERIFY(producer.publish(*referenceMetrics.snapshot));
    QVERIFY(producer.publish(*baselineMetrics.snapshot));
    auto search=OfflineSearch::begin(81U,producer.view(),
        {{candidate->ticket_identity}},
        {{Metric::full_scope_rms,1.,1.}});
    QVERIFY(search);
    const auto t=search->next();
    QVERIFY(t);
    QVERIFY(candidate->matches_ticket(*t));
    QVERIFY(producer.expect(Column::processed,2U));
    QVERIFY(producer.publish(*trialMetrics.snapshot));
    QVERIFY(search->complete(*t,producer.view())==Submission::improved);
    QVERIFY(search->complete());
    QVERIFY(search->best().typed_dsp_fingerprint);
    QCOMPARE(*search->best().typed_dsp_fingerprint,
        candidate->ticket_identity);
    QVERIFY(search->best().weighted_error
        <0.001); // +6.020599913 dB gain against exact doubled tone

    // Modifying the actual source chain invalidates a prepared immutable
    // trial: Product must capture a new baseline, not render a stale trial.
    QVERIFY(chain.value()->set_user_bypass(*gainId.value(),true));
    QVERIFY(!candidate->still_matches(*chain.value(),baseline));
}

void M21RealProductMetricsPortTest::
l3C01FullPrefixCandidateMeasuresRealM21Evidence()
{
    using namespace app::m21_private;
    using namespace app::l3_automatch;
    // Direct IModule from true frame-zero prefix. This is real C01 PCM,
    // NOT a claim that the Product renderer/registry already supports
    // Dynamic EQ random access or a certified finite pre-context.
    dsp::ModuleDescriptorSpec d{};
    d.type_id="rgsml.dsp.dynamic-eq";
    d.display_name_key="rgsml.dsp.dynamic-eq.display-name";
    d.categories={dsp::ModuleCategory::FILTER_EQ};
    d.allowed_stages={dsp::ProcessingStage::MASTER};
    d.allowed_segments={dsp::ChainSegment::MANUAL};
    d.duplicable=true;
    d.bypassable=true;
    d.placement_class=dsp::PlacementClass::INLINE_CHAIN;
    d.algorithm_version="1.0.0";
    d.parameter_schema_id="rgsml.dsp.dynamic-eq.parameters/1.0.0";
    auto desc=dsp::ModuleDescriptor::create(std::move(d));
    QVERIFY(desc);
    dsp::DynamicEqBandSpec band{};
    band.band_id=*core::Uuid::parse(
        "01234567-89ab-4cde-8123-456789abcdef").value();
    band.shape=dsp::DynamicEqBellShape{440.0,0.707};
    band.control.base_gain_db=0.0;
    band.control.range_db=6.0;
    band.control.threshold_dbfs=-80.0;
    band.control.ratio=20.0;
    band.control.attack_ms=0.1;
    auto typed=dsp::DynamicEqBandParameters::create(band);
    QVERIFY(typed);
    auto params=dsp::DynamicEqParameters::create({*typed.value()});
    QVERIFY(params);
    auto module=dsp::DynamicEqModule::create(*desc.value(),*params.value());
    QVERIFY(module);
    const auto rate=core::SampleRate::create(48000);
    const auto count=core::FrameCount::create(48000);
    const auto block=core::FrameCount::create(4096);
    QVERIFY(rate&&count&&block);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    const dsp::DspProcessSpec processing{
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        *block.value()};
    QVERIFY(module.value()->get()->prepare(processing));
    auto source=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    auto candidate=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    auto reference=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    QVERIFY(source&&candidate&&reference);
    auto x=source.value()->mutable_view().channel(0);
    auto ref=reference.value()->mutable_view().channel(0);
    QVERIFY(x&&ref);
    for(std::size_t i=0;i<x.value()->size();++i) {
        const double tone=std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
        (*x.value())[i]=0.2*tone;
        (*ref.value())[i]=0.4*tone; // independent target, not candidate PCM
    }
    for(std::int64_t begin=0;begin<48000;begin+=4096) {
        const auto n=std::min<std::int64_t>(4096,48000-begin);
        const auto size=core::FrameCount::create(n);
        const auto range=core::FrameRange::create(
            core::FrameIndex{begin},core::FrameIndex{begin+n});
        QVERIFY(size&&range);
        auto source_part=source.value()->view().subview(
            core::FrameIndex{begin},*size.value());
        auto candidate_part=candidate.value()->mutable_view().subview(
            core::FrameIndex{begin},*size.value());
        QVERIFY(source_part&&candidate_part);
        QVERIFY(module.value()->get()->process(
            *source_part.value(),*candidate_part.value(),
            dsp::DspProcessContext{*range.value(),begin==0,begin+n==48000}));
    }
    const auto levels_base=analysis::measure_basic_level_full_resource(
        *source.value());
    const auto levels_trial=analysis::measure_basic_level_full_resource(
        *candidate.value());
    const auto levels_ref=analysis::measure_basic_level_full_resource(
        *reference.value());
    const auto stereo_base=analysis::measure_tonal_stereo_scope(
        source.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    const auto stereo_trial=analysis::measure_tonal_stereo_scope(
        candidate.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    const auto stereo_ref=analysis::measure_tonal_stereo_scope(
        reference.value()->view(),core::FrameIndex{0},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(levels_base&&levels_trial&&levels_ref);
    QVERIFY(stereo_base&&stereo_trial&&stereo_ref);
    const auto baseMetrics=import_real_product_metrics(
        *levels_base.value(),*stereo_base.value(),
        {Column::processed,1U,"c01-baseline-source","Full Track",true});
    const auto candidateMetrics=import_real_product_metrics(
        *levels_trial.value(),*stereo_trial.value(),
        {Column::processed,2U,"c01-full-prefix-processed","Full Track",true});
    const auto referenceMetrics=import_real_product_metrics(
        *levels_ref.value(),*stereo_ref.value(),
        {Column::gold,1U,"independent-c01-reference","Full Track",true});
    QVERIFY(baseMetrics.snapshot);
    QVERIFY(candidateMetrics.snapshot);
    QVERIFY(referenceMetrics.snapshot);
    PrivatePublicationGuard producer;
    QVERIFY(producer.expect(Column::processed,1U));
    QVERIFY(producer.expect(Column::gold,1U));
    QVERIFY(producer.publish(*baseMetrics.snapshot));
    QVERIFY(producer.publish(*referenceMetrics.snapshot));
    auto search=OfflineSearch::begin(140U,producer.view(),
        {{"c01-full-prefix-BELL-RANGE-6"}},
        {{Metric::full_scope_rms,1.0,1.0}});
    QVERIFY(search);
    const auto ticket=search->next();
    QVERIFY(ticket);
    QVERIFY(producer.expect(Column::processed,2U));
    QVERIFY(producer.publish(*candidateMetrics.snapshot));
    QVERIFY(search->complete(*ticket,producer.view())==Submission::improved);
    QVERIFY(search->best().typed_dsp_fingerprint.has_value());
    QCOMPARE(*search->best().typed_dsp_fingerprint,
             std::string{"c01-full-prefix-BELL-RANGE-6"});
    QVERIFY(search->best().weighted_error<0.5);
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M21RealProductMetricsPortTest)
#include "test_m21_real_product_metrics_port.moc"
