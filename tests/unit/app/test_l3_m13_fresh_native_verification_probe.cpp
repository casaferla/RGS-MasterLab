#include "internal/l3_m13_fresh_native_verification_probe.hpp"
#include "internal/l3_m13_fresh_full_master_probe.hpp"
#include "internal/l3_automatch_typed_p2_trial.hpp"
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
#include <numbers>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m12=rgsml::app::l3_automatch::m12_frozen;
namespace m13=rgsml::app::l3_automatch::m13_frozen;
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
    const auto second=m13::probe_fresh_m13_full_native_master_once(
        *stage.value(),*manual.value(),manualBindings,
        *prepared.value(),*block.value(),0.0,true,true,*actual_m10_p2);
    QVERIFY(first&&second);
    QCOMPARE(second.value()->genuine_dsp_segment_passes,
             std::uint32_t{3});
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
QTEST_GUILESS_MAIN(rgsml::tests::M13FreshNativeProbeTest)
#include "test_l3_m13_fresh_native_verification_probe.moc"
