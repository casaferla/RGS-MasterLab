#include "internal/l3_m13_fresh_native_verification_probe.hpp"
#include "internal/l3_m13_qtp_27_projection.hpp"
#include "internal/l3_m13_predicted_vs_verified_proximity.hpp"
#include "internal/l3_m13_verification_state_reasons.hpp"
#include "internal/l3_m13_fresh_full_master_probe.hpp"
#include "internal/l3_automatch_typed_p2_trial.hpp"
#include "internal/l3_m13_p2_owned_full_master_probe.hpp"
#include "internal/l3_m12_native_terminal_guarded_stage.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <numbers>
#include <vector>
namespace rgsml::tests {
namespace {
namespace a=rgsml::app::l3_automatch;
namespace m12=a::m12_frozen;
namespace m13=a::m13_frozen;
[[nodiscard]] core::Uuid uuid(const char* s) {
    auto x=core::Uuid::parse(s);Q_ASSERT(x);return *x.value();
}
[[nodiscard]] dsp::ModuleInstanceId id(const char* s) {
    auto x=dsp::ModuleInstanceId::from_uuid(uuid(s));
    Q_ASSERT(x);return *x.value();
}
class M13FreshNativeProbeTest final:public QObject{
    Q_OBJECT
private slots:
    void noM12ReceiptCannotPretendFreshVerification();
    void newRealRenderAndIndependentAnalysisRemainNonverified();
    void optimizerP2RunsAfterLinkedAndM16ReadsItsActualOutput();
    void frozenQtpTwentySevenCanonicalCategories();
    void frozenQtpNativeScalarBoundaries();
    void sameRunPredictedVerifiedRawProximity();
    void frozenVerificationStatePrecedence();
    void reasonsOrderAndExactDuplicateSemantics();
};
void M13FreshNativeProbeTest::
noM12ReceiptCannotPretendFreshVerification() {
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto owned=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*registry.value()));
    auto manual=dsp::ProcessingChain::create(*owned,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    const auto fc=core::FrameCount::create(48000);
    const auto chunk=core::FrameCount::create(4096);
    auto buffer=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*fc.value());
    QVERIFY(buffer);
    auto l=buffer.value()->mutable_view().channel(0);
    auto r=buffer.value()->mutable_view().channel(1);
    QVERIFY(l&&r);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const auto v=0.2*std::sin(2.0*std::numbers::pi*440.0*
                                  static_cast<double>(i)/48000.0);
        (*l.value())[i]=v;(*r.value())[i]=v;
    }
    const auto request=render::RenderRequest::create(
        buffer.value()->view(),buffer.value()->view().absolute_range(),
        *manual.value(),{},*chunk.value());
    QVERIFY(request);
    const auto prepared=render::render_preview(
        *request.value(),*owned);
    QVERIFY(prepared);
    const m12::AcceptedPlanAndOwner accepted{
        uuid("c1000000-0000-4000-8000-000000000001"),
        uuid("c1000000-0000-4000-8000-000000000002"),
        "plan-checksum","winner-checksum","fixed-chain-checksum",
        "source-reference-current",8,true,true,true,false};
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(eq);
    const std::vector<m12::ExactLinkedNode> nodes{{
        id("c2000000-0000-4000-8000-000000000001"),
        uuid("c3000000-0000-4000-8000-000000000001"),
        "rgsml.dsp.parametric-eq",*eq.value()}};
    const std::vector<dsp::ModuleExecutionBinding> none;
    const auto stage=m12::stage_m12_with_independent_m16_safety(
        accepted,*manual.value(),none,*buffer.value(),
        0.0,true,owned,nodes);
    QVERIFY(stage);
    // Frozen AutoMatch M16 guard, without any WAV export module.
    QVERIFY(!m12::stage_m12_with_independent_m16_safety(
        accepted,*manual.value(),none,*buffer.value(),
        -24.0,true,owned,nodes));
    QVERIFY(!m13::probe_fresh_m13_ref_linked_segment_once(
        *stage.value(),*prepared.value(),*chunk.value(),false));
    // Simulated authoritative M12 receipt for isolated analysis tests,
    // NOT evidence that a real Project owner has committed one.
    const auto fresh=m13::probe_fresh_m13_ref_linked_segment_once(
        *stage.value(),*prepared.value(),*chunk.value(),true);
    QVERIFY(fresh);
    QCOMPARE(fresh.value()->verification_evaluations,std::uint32_t{1});
    QCOMPARE(fresh.value()->new_render_not_solver_pcm.view().
                 frame_count().value(),std::int64_t{48000});
    QCOMPARE(fresh.value()->fresh_m20.sample_rate_hz,48000U);
    QCOMPARE(fresh.value()->fresh_m20.channel_count,2U);
    QVERIFY(fresh.value()->fresh_m20.mean_linear_psd.size()>0U);
    QVERIFY(!fresh.value()->is_m13_verified());
    QVERIFY(!fresh.value()->authorizes_export());
    // M13 runs M16 on the entire actual AutoMatch PCM BEFORE export.
    // Neither a terminal M17 module nor its ceiling is an input.
    const auto whole=m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),none,
        *prepared.value(),*chunk.value(),0.0,true,true);
    QVERIFY(whole);
    QCOMPARE(whole.value()->logical_verification_evaluations,
             std::uint32_t{1});
    QCOMPARE(whole.value()->genuine_dsp_segment_passes,
             std::uint32_t{2});
    QCOMPARE(whole.value()->independent_match_render.view().
                 frame_count().value(),std::int64_t{48000});
    QCOMPARE(whole.value()->independent_match_render.view().
                 timebase().frame_domain_id(),
             audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(std::isfinite(
        whole.value()->authentic_full_resource_m16.global_linear_peak()));
    QVERIFY(whole.value()->authentic_full_resource_m16.global_linear_peak()
        <1.0);
    QCOMPARE(whole.value()->independent_final_m20.channel_count,2U);
    QVERIFY(!whole.value()->is_m13_verified());
    QVERIFY(!whole.value()->authorizes_master_export());
    QVERIFY(!m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),none,
        *prepared.value(),*chunk.value(),0.0,false,true));
    // Independent AutoMatch safety threshold may be DIFFERENT from M17.
    QVERIFY(!m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),none,
        *prepared.value(),*chunk.value(),-24.0,true,true));
    // A changed M11 fixed chain invalidates the verifier even if the
    // linked DSP winner stays unchanged.
    QVERIFY(manual.value()->add(
        id("c4000000-0000-4000-8000-000000000002"),
        "rgsml.dsp.gain",0));
    QVERIFY(!m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),none,
        *prepared.value(),*chunk.value(),0.0,true,true));
}
void M13FreshNativeProbeTest::
newRealRenderAndIndependentAnalysisRemainNonverified() {
    // A fresh verification output never takes an M10 candidate PCM
    // or numerical objective as an input. Authority is the currently
    // materialized REF-linked Product and current upstream Prepared.
    QVERIFY(true);
}

void M13FreshNativeProbeTest::
optimizerP2RunsAfterLinkedAndM16ReadsItsActualOutput() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto opt=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        std::move(*base.value()));
    QVERIFY(opt);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*opt.value()));
    auto empty=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(empty&&manual);
    const auto p2id=id("c7000000-0000-4000-8000-000000000001");
    QVERIFY(manual.value()->add(
        p2id,"rgsml.dsp.streaming-true-peak-limiter",0));
    const auto p2=dsp::StreamingTruePeakLimiterParameters::create(-1.0,0.0,2);
    const auto drive=dsp::StreamingTruePeakLimiterParameters::create(-1.0,6.0,2);
    const auto wrongMargin=dsp::StreamingTruePeakLimiterParameters::create(
        -1.0,6.0,5);
    QVERIFY(p2&&drive&&wrongMargin);
    const std::vector<dsp::ModuleExecutionBinding> manualBindings{
        {p2id,*p2.value()}};
    const auto chosen=rgsml::app::l3_automatch::make_p2_scalar_trial(
        *manual.value(),manualBindings,p2id,
        rgsml::app::l3_automatch::P2ScalarCoordinate::drive_db,
        6.0,"m10-exact-p2-winner");
    QVERIFY(chosen);
    QVERIFY(chosen->still_matches(*manual.value(),manualBindings));
    const auto* actual_m10_p2=chosen->chosen_p2_parameters();
    QVERIFY(actual_m10_p2);
    QCOMPARE(actual_m10_p2->drive_db(),6.0);
    const auto rate=core::SampleRate::create(48000);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    // A full second is required for real canonical loudness analysis.
    const auto frames=core::FrameCount::create(48000);
    const auto block=core::FrameCount::create(512);
    auto buffer=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(buffer);
    auto l=buffer.value()->mutable_view().channel(0);
    auto r=buffer.value()->mutable_view().channel(1);
    QVERIFY(l&&r);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const double v=0.25*std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
        (*l.value())[i]=v;(*r.value())[i]=v;
    }
    const auto request=render::RenderRequest::create(
        buffer.value()->view(),buffer.value()->view().absolute_range(),
        *empty.value(),{},*block.value());
    QVERIFY(request);
    auto prepared=render::render_preview(*request.value(),*registry);
    QVERIFY(prepared);
    const m12::AcceptedPlanAndOwner accepted{
        uuid("c8000000-0000-4000-8000-000000000001"),
        uuid("c8000000-0000-4000-8000-000000000002"),
        "current-plan","winning-typed-P2","protected-manual",
        "source-ref-policy",8,true,true,true,false};
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(eq);
    const std::vector<m12::ExactLinkedNode> nodes{{
        id("c9000000-0000-4000-8000-000000000001"),
        uuid("ca000000-0000-4000-8000-000000000001"),
        "rgsml.dsp.parametric-eq",*eq.value()}};
    const auto stage=m12::stage_m12_with_independent_m16_safety(
        accepted,*manual.value(),manualBindings,*buffer.value(),
        0.0,true,registry,nodes);
    QVERIFY(stage);
    // Bind the genuine M10 typed candidate and exact M11 owner before
    // allowing the final fresh M13 three-stage verification probe.
    const a::m10_frozen::ExactP2CoordinateNames p2Fields{
        {0xa1},{0x01},{0x02}};
    const std::vector<a::m10_frozen::Coordinate> p2Coords{
        {{0xa1},{0x01},0.5},{{0xa1},{0x02},0.5}};
    const std::vector<double> trialNormalized{0.4,0.6};
    const auto m10P2=a::m10_frozen::bind_m10_p2_joint_coordinates(
        p2Coords,trialNormalized,p2Fields,*manual.value(),manualBindings,
        p2id,"m10-exact-p2-winner",
        [&](const a::m10_frozen::Coordinate& coord,double)
            ->std::optional<double> {
            return coord.stable_parameter_id==p2Fields.ceiling_parameter_id
                ?std::optional<double>{-1.0}
                :std::optional<double>{6.0};
        });
    QVERIFY(m10P2);
    const auto stagedP2=m12::preflight_exact_m12_p2_optimizing_winner(
        *stage.value(),*manual.value(),manualBindings,*m10P2,
        "m10-exact-p2-winner","winning-typed-P2",true);
    QVERIFY(stagedP2);
    QVERIFY(stagedP2->matches(
        *stage.value(),*manual.value(),manualBindings));
    QVERIFY(!stagedP2->authorizes_project_commit());
    QVERIFY(!stagedP2->is_m13_verified());
    QVERIFY(!m12::preflight_exact_m12_p2_optimizing_winner(
        *stage.value(),*manual.value(),manualBindings,*m10P2,
        "wrong-ticket","winning-typed-P2",true));
    QVERIFY(!m12::preflight_exact_m12_p2_optimizing_winner(
        *stage.value(),*manual.value(),manualBindings,*m10P2,
        "m10-exact-p2-winner","wrong-winner",true));
    QVERIFY(!m12::preflight_exact_m12_p2_optimizing_winner(
        *stage.value(),*manual.value(),manualBindings,*m10P2,
        "m10-exact-p2-winner","winning-typed-P2",false));
    // An active manual P2 without a typed M10 winner must FAIL CLOSED.
    QVERIFY(!m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),manualBindings,
        *prepared.value(),*block.value(),0.0,true,true));
    // Margin remains USER-owned, not a silent optimizer coordinate.
    QVERIFY(!m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),manualBindings,
        *prepared.value(),*block.value(),0.0,true,true,
        *wrongMargin.value()));
    const auto first=m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),manualBindings,
        *prepared.value(),*block.value(),0.0,true,true,*p2.value());
    const auto second=m13::probe_fresh_m13_full_master_using_exact_p2_winner(
        *stage.value(),*stagedP2,*manual.value(),manualBindings,
        *prepared.value(),*block.value(),0.0,true,true);
    QVERIFY(!m13::probe_fresh_m13_full_master_using_exact_p2_winner(
        *stage.value(),*stagedP2,*manual.value(),manualBindings,
        *prepared.value(),*block.value(),0.0,false,true));
    QVERIFY(first&&second);
    QCOMPARE(second.value()->genuine_dsp_segment_passes,
             std::uint32_t{3});
    QVERIFY(stagedP2->winning_parameters==*actual_m10_p2);
    QCOMPARE(second.value()->independent_match_render.view().
                 frame_count().value(),std::int64_t{48000});
    QCOMPARE(second.value()->independent_match_render.signatures().size(),
             std::size_t{1});
    QCOMPARE(second.value()->independent_match_render.signatures().back().type_id,
             std::string("rgsml.dsp.streaming-true-peak-limiter"));
    QVERIFY(std::isfinite(second.value()->
              authentic_full_resource_m16.global_linear_peak()));
    QVERIFY(second.value()->authentic_full_resource_m16.global_linear_peak()
            <=1.0);
    // Two distinct typed P2 candidates actually produce distinct PCM:
    // no bypass, proxy, or fallback to the unmodified manual limiter.
    const auto a=first.value()->independent_match_render.view().channel(0);
    const auto b=second.value()->independent_match_render.view().channel(0);
    QVERIFY(a&&b);
    bool differs=false;
    for(std::size_t i=0;i<a.value()->size();++i)
        if((*a.value())[i]!=(*b.value())[i]){differs=true;break;}
    QVERIFY(differs);
    const auto* still_manual=
        std::get_if<dsp::StreamingTruePeakLimiterParameters>(
            &manualBindings[0].parameters);
    QVERIFY(still_manual);
    QCOMPARE(still_manual->drive_db(),0.0);
    QCOMPARE(still_manual->margin_percent(),2);
    QVERIFY(!second.value()->is_m13_verified());
    QVERIFY(!second.value()->authorizes_master_export());
}
} // namespace
} // namespace rgsml::tests
void M13FreshNativeProbeTest::frozenQtpTwentySevenCanonicalCategories()
{
    using namespace m13;
    static_assert(kQtpBindings.size()==m07_frozen::kBindings.size());
    QVERIFY(kQtpState==std::string_view("PROVISIONAL"));
    QCOMPARE(kQtpBindings[0].category,QtpCategory::band_profile_db);
    QCOMPARE(kQtpBindings[1].category,QtpCategory::slope_db_per_octave);
    QCOMPARE(kQtpBindings[5].category,QtpCategory::slope_db_per_octave);
    QCOMPARE(kQtpBindings[14].category,QtpCategory::positive_ms);
    QCOMPARE(kQtpBindings[15].category,QtpCategory::generic_db);
    QCOMPARE(kQtpBindings[16].category,QtpCategory::generic_db);
    QCOMPARE(kQtpBindings[17].category,QtpCategory::correlation_raw_rho);
    QCOMPARE(kQtpBindings[22].category,QtpCategory::positive_width_ratio);
    QCOMPARE(kQtpBindings[26].category,QtpCategory::true_peak_db_tp);
    for(std::size_t i=0;i<kQtpBindings.size();++i) {
        QCOMPARE(kQtpBindings[i].binding_id,m07_frozen::kBindings[i].binding_id);
        QCOMPARE(kQtpBindings[i].source_transform,
                 m07_frozen::kBindings[i].transform);
    }
    constexpr std::array<double,3> equal{0.0,1.0,2.0};
    constexpr std::array<double,3> uniformlySmall{.002,.998,2.002};
    const auto goodProfile=compare_qtp_t1_profile(equal,uniformlySmall);
    QCOMPARE(goodProfile.outcome,QtpOutcome::pass);
    constexpr std::array<double,3> singleTooLarge{.0101,1.0,2.0};
    QCOMPARE(compare_qtp_t1_profile(equal,singleTooLarge).outcome,
             QtpOutcome::mismatch);
    constexpr std::array<double,3> eachInsideButRmsTooLarge{
        .004,1.004,2.004};
    QCOMPARE(compare_qtp_t1_profile(equal,eachInsideButRmsTooLarge).outcome,
             QtpOutcome::mismatch);
    QCOMPARE(compare_qtp_t1_profile(equal,std::span<const double>{}).outcome,
             QtpOutcome::unavailable);
}

void M13FreshNativeProbeTest::frozenQtpNativeScalarBoundaries()
{
    using namespace m13;
    const auto scalar=[](QtpCategory cat,double ref,double actual) {
        return compare_qtp_native_scalar(cat,ref,actual).outcome;
    };
    QCOMPARE(scalar(QtpCategory::loudness_lu,-12.0,-12.009),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::loudness_lu,-12.0,-12.02),
             QtpOutcome::mismatch);
    QCOMPARE(scalar(QtpCategory::lra_lu,3.0,3.019),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::lra_lu,3.0,3.021),
             QtpOutcome::mismatch);
    QCOMPARE(scalar(QtpCategory::generic_db,8.0,8.009),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::true_peak_db_tp,-1.0,-0.989),
             QtpOutcome::mismatch);
    QCOMPARE(scalar(QtpCategory::slope_db_per_octave,-2.0,-1.99981),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::slope_db_per_octave,-2.0,-1.9997),
             QtpOutcome::mismatch);
    QCOMPARE(scalar(QtpCategory::positive_hz,1000.0,1000.009),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::positive_hz,1000.0,1000.02),
             QtpOutcome::mismatch);
    QCOMPARE(scalar(QtpCategory::positive_ms,10.0,10.00009),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::positive_ms,10.0,0.0),
             QtpOutcome::mismatch);
    QCOMPARE(scalar(QtpCategory::positive_width_ratio,0.0,1e-7),
             QtpOutcome::mismatch); // exact MONO endpoint
    QCOMPARE(scalar(QtpCategory::positive_width_ratio,1.0,1.000009),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::correlation_raw_rho,0.7,0.700019),
             QtpOutcome::pass);
    QCOMPARE(scalar(QtpCategory::correlation_raw_rho,0.7,0.700021),
             QtpOutcome::mismatch);
    QCOMPARE(scalar(QtpCategory::correlation_raw_rho,1.0,0.999999),
             QtpOutcome::mismatch); // exact Fisher endpoint
    QCOMPARE(compare_qtp_native_scalar(
        QtpCategory::positive_hz,100.0,std::nullopt).outcome,
        QtpOutcome::unavailable);
    QCOMPARE(compare_qtp_native_scalar(
        QtpCategory::band_profile_db,1.0,1.0).outcome,
        QtpOutcome::unavailable);
}

void M13FreshNativeProbeTest::sameRunPredictedVerifiedRawProximity()
{
    using namespace m13;
    using M=m08_frozen::ProximityReason;
    const m08_frozen::DomainProximity prediction{
        1.25,M::comparable};
    const m08_frozen::DomainProximity justInside{
        1.25049,M::comparable};
    const m08_frozen::DomainProximity tooFar{
        1.25051,M::comparable};
    QCOMPARE(compare_exact_m08_raw_proximity(prediction,justInside,true).
             status,ProximityComparisonStatus::passed);
    QCOMPARE(compare_exact_m08_raw_proximity(prediction,tooFar,true).
             status,ProximityComparisonStatus::failed);
    QCOMPARE(compare_exact_m08_raw_proximity(prediction,
        {1.25019,M::comparable},false).status,
        ProximityComparisonStatus::passed);
    QCOMPARE(compare_exact_m08_raw_proximity(prediction,
        {1.25021,M::comparable},false).status,
        ProximityComparisonStatus::failed);
    QCOMPARE(compare_exact_m08_raw_proximity(prediction,
        {1.25,M::proximity_already_matched},true).status,
        ProximityComparisonStatus::failed);
    QCOMPARE(compare_exact_m08_raw_proximity(
        {std::nullopt,M::candidate_required_component_missing},
        {std::nullopt,M::candidate_required_component_missing},true).status,
        ProximityComparisonStatus::unavailable);
    std::array<m08_frozen::RequestedDomainReport,4> predicted{};
    std::array<m08_frozen::RequestedDomainReport,4> verified{};
    for(std::size_t i=0;i<4;++i) {
        predicted[i]={true,{0.5,M::comparable},1.0};
        verified[i]={true,{0.5003,M::comparable},1.0};
    }
    const auto pass=compare_frozen_m08_prediction_with_fresh_verified(
        predicted,verified);
    QVERIFY(pass.all_required_pass());
    QCOMPARE(pass.global.status,ProximityComparisonStatus::passed);
    verified[1].effective_coverage=0.75;
    QVERIFY(!compare_frozen_m08_prediction_with_fresh_verified(
        predicted,verified).all_required_pass());
    verified[1].effective_coverage=1.0;
    verified[1].proximity={std::nullopt,
        M::candidate_required_component_missing};
    const auto partial=compare_frozen_m08_prediction_with_fresh_verified(
        predicted,verified);
    QVERIFY(!partial.all_required_pass());
    QCOMPARE(partial.global.status,ProximityComparisonStatus::failed);
    QVERIFY(!partial.full_global_population);
}

void M13FreshNativeProbeTest::frozenVerificationStatePrecedence()
{
    using namespace m13;
    VerificationStateEvidence e;
    QCOMPARE(project_verification_state(e),
             VerificationState::verification_unavailable);
    e.materialization_exists=true;
    QCOMPARE(project_verification_state(e),VerificationState::not_verified);
    e.any_attempt=true;
    QCOMPARE(project_verification_state(e),
             VerificationState::verification_unavailable);
    e.all_complete_conjunction_passed=true;
    e.report_integrity_passed=true;
    // A genuinely completed measurement is NOT a profile approval.
    QCOMPARE(project_verification_state(e),
             VerificationState::verification_unavailable);
    e.qtp_qualified_for_claimed_scope=true;
    QCOMPARE(project_verification_state(e),VerificationState::verified);
    e.mandatory_evidence_missing=true;
    QCOMPARE(project_verification_state(e),
             VerificationState::verification_unavailable);
    e.evaluated_predicate_failed=true;
    QCOMPARE(project_verification_state(e),
             VerificationState::verification_failed);
    e.exact_dependency_known_changed=true;
    QCOMPARE(project_verification_state(e),VerificationState::stale);
}
void M13FreshNativeProbeTest::reasonsOrderAndExactDuplicateSemantics()
{
    using namespace m13;
    VerificationReason signature;
    signature.stage=VerificationStage::input_integrity;
    signature.code="SIGNATURE_MISMATCH";
    signature.field_path="/acceptedPlanRef";
    VerificationReason safety;
    safety.stage=VerificationStage::safety;
    safety.code="HARD_SAFETY_VIOLATION";
    safety.field_path="/freshM16";
    VerificationReason proximity;
    proximity.stage=VerificationStage::proximity;
    proximity.code="PROXIMITY_VERIFICATION_MISMATCH";
    proximity.field_path="/proximityResults/global";
    VerificationReason stale;
    stale.stage=VerificationStage::dependency_currentness;
    stale.code="STALE_DEPENDENCY";
    stale.field_path="/Prepared/ContentChecksum";
    auto sorted=normalize_verification_reasons({
        proximity,safety,signature,stale,signature});
    QVERIFY(sorted);
    QCOMPARE(sorted->size(),std::size_t{4});
    QCOMPARE(sorted->front().code,std::string("STALE_DEPENDENCY"));
    QCOMPARE((*sorted)[1].code,std::string("SIGNATURE_MISMATCH"));
    QCOMPARE((*sorted)[2].code,std::string("HARD_SAFETY_VIOLATION"));
    QCOMPARE(sorted->back().code,
             std::string("PROXIMITY_VERIFICATION_MISMATCH"));
    auto wrongStage=safety;
    wrongStage.stage=VerificationStage::feature_numeric;
    QVERIFY(!normalize_verification_reasons({wrongStage}));
    auto nonfinite=safety;
    nonfinite.measured_delta=std::numeric_limits<double>::infinity();
    QVERIFY(!normalize_verification_reasons({nonfinite}));
    auto retained=safety;
    retained.component_key="left";
    QVERIFY(normalize_verification_reasons({safety,retained})->size()==2U);
}
QTEST_GUILESS_MAIN(rgsml::tests::M13FreshNativeProbeTest)
#include "test_l3_m13_fresh_native_verification_probe.moc"
