#include "internal/l3_m10_p2_real_native_product_job.hpp"
#include "internal/l3_product_native_one_pass.hpp"
#include "internal/l3_m10_p2_full_native_evidence.hpp"
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <vector>

namespace rgsml::tests {
namespace {
namespace a=rgsml::app::l3_automatch;
[[nodiscard]] core::Uuid uuid(const char* str) {
    const auto value=core::Uuid::parse(str);Q_ASSERT(value);
    return *value.value();
}
[[nodiscard]] dsp::ModuleInstanceId id(const char* str) {
    const auto value=dsp::ModuleInstanceId::from_uuid(uuid(str));
    Q_ASSERT(value);return *value.value();
}
class P2RealNativeProductTest final:public QObject {
    Q_OBJECT
private slots:
    void genuineM10CandidateRunsPostLinkedP2AndNativeAnalysis();
};
void P2RealNativeProductTest::
genuineM10CandidateRunsPostLinkedP2AndNativeAnalysis() {
    auto registry_value=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry_value);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*registry_value.value()));
    auto manual=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    auto empty=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual&&empty);
    const auto gain_id=id("ba000000-0000-4000-8000-000000000001");
    const auto p2_id=id("ba000000-0000-4000-8000-000000000002");
    // Live registry is factory-less for P2, but frozen Product opt-in
    // in the job attaches the genuine existing P2 implementation.
    // A manual graph requires the opt-in descriptor at creation.
    auto registry_with_p2=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        *registry);
    QVERIFY(registry_with_p2);
    auto product_registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*registry_with_p2.value()));
    auto active_manual=dsp::ProcessingChain::create(*product_registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(active_manual);
    QVERIFY(active_manual.value()->add(gain_id,"rgsml.dsp.gain",0));
    QVERIFY(active_manual.value()->add(p2_id,
        "rgsml.dsp.streaming-true-peak-limiter",1));
    auto gain=dsp::GainParameters::create(0.0);
    auto base_p2=dsp::StreamingTruePeakLimiterParameters::create(
        -1.0,0.0,2);
    QVERIFY(gain&&base_p2);
    const std::vector<dsp::ModuleExecutionBinding> manual_bindings{
        {gain_id,*gain.value()},{p2_id,*base_p2.value()}};
    auto eq=dsp::ParametricEqParameters::create_legacy_default();
    QVERIFY(eq);
    const auto linked_id=id("bb000000-0000-4000-8000-000000000001");
    auto linked_instance=dsp::ModuleInstance::create({
        linked_id,"rgsml.dsp.parametric-eq",true,false,false,false,
        dsp::ModuleProvenance::REFERENCE_MATCH,
        dsp::ModuleOwner::REFERENCE_MATCH,
        dsp::ModuleLinkState::REF_LINKED,
        uuid("bc000000-0000-4000-8000-000000000001"),{}});
    QVERIFY(linked_instance);
    auto linked=dsp::ProcessingChain::restore(
        *product_registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        1U,{*linked_instance.value()});
    QVERIFY(linked);
    const std::vector<dsp::ModuleExecutionBinding> artistic{
        {linked_id,*eq.value()}};
    const a::m10_frozen::ExactP2CoordinateNames names{
        {0x50,0x32},{0x01},{0x02}};
    const std::vector<a::m10_frozen::Coordinate> inventory{
        {{0x50,0x32},{0x01},0.5},{{0x50,0x32},{0x02},0.5}};
    const auto inverse=[&](const a::m10_frozen::Coordinate& c,double u)
        ->std::optional<double> {
        // Explicit unit-test M09 inverse, not a production policy.
        return c.stable_parameter_id==names.ceiling_parameter_id
            ?std::optional<double>{-1.0}
            :std::optional<double>{-24.0+48.0*u};
    };
    const auto chosen=a::m10_frozen::bind_m10_p2_joint_coordinates(
        inventory,std::vector<double>{0.5,0.625},
        names,*active_manual.value(),manual_bindings,
        p2_id,"real-p2-product-test",inverse);
    const auto alternative=a::m10_frozen::bind_m10_p2_joint_coordinates(
        inventory,std::vector<double>{0.5,0.5},
        names,*active_manual.value(),manual_bindings,
        p2_id,"real-p2-product-control",inverse);
    QVERIFY(chosen&&alternative);
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    const auto length=core::FrameCount::create(48000*3);
    const auto block=core::FrameCount::create(4096);
    QVERIFY(fs&&fmt&&length&&block);
    auto source=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*length.value());
    QVERIFY(source);
    auto left=source.value()->mutable_view().channel(0);
    auto right=source.value()->mutable_view().channel(1);
    QVERIFY(left&&right);
    for(std::size_t i=0;i<left.value()->size();++i) {
        const double v=0.4*std::sin(
            2.0*std::numbers::pi*440.0*
            static_cast<double>(i)/48000.0);
        (*left.value())[i]=v;(*right.value())[i]=v;
    }
    auto request=render::RenderRequest::create(
        source.value()->view(),source.value()->view().absolute_range(),
        *empty.value(),{},*block.value());
    QVERIFY(request);
    auto prepared=render::render_preview(*request.value(),*registry);
    QVERIFY(prepared);
    auto prepared_owner=std::make_shared<const render::RenderResult>(
        std::move(*prepared.value()));
    const a::OfflineProductOwnerStamp stamp{
        10U,11U,12U,"test-provider","source","test-provider","reference"};
    const auto create=[&](const a::m10_frozen::BoundM10P2Candidate& p2,
                          bool attested=true) {
        return a::M10P2RealNativeProductJob::create(
            stamp,prepared_owner,product_registry,*active_manual.value(),
            manual_bindings,*linked.value(),artistic,p2,*block.value(),
            attested);
    };
    QVERIFY(!create(*chosen,false));
    auto job=create(*chosen);
    auto control=create(*alternative);
    QVERIFY(job&&control);
    QVERIFY(job.value()->identical_upstream_to(*control.value()));
    QVERIFY(job.value()->still_current(
        stamp,prepared_owner,*active_manual.value(),manual_bindings,
        *linked.value(),artistic));
    QVERIFY(!job.value()->authorizes_publication());
    auto actual=job.value()->execute();
    auto baseline=control.value()->execute();
    QVERIFY(actual&&baseline);
    QVERIFY(job.value()->result_has_expected_scope(*actual.value()));
    // Reject a prefix whose REF_LINKED graph revision has changed.
    auto another_linked=dsp::ProcessingChain::restore(
        *product_registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        2U,{*linked_instance.value()});
    QVERIFY(another_linked);
    const auto stale_job=a::M10P2RealNativeProductJob::create(
        stamp,prepared_owner,product_registry,*active_manual.value(),
        manual_bindings,*another_linked.value(),artistic,*chosen,
        *block.value(),true);
    QVERIFY(stale_job);
    const auto prefix_for_original=job.value()->prepare_exact_pre_p2_prefix();
    QVERIFY(prefix_for_original);
    QVERIFY(!stale_job.value()->execute_with_exact_pre_p2_prefix(
        *prefix_for_original.value()));
    // An overly tight RAM budget rejects only caching, never normal
    // P2 Product rendering or the real native analysis path.
    QVERIFY(!job.value()->prepare_exact_pre_p2_prefix(8U));
    const auto prefix=job.value()->prepare_exact_pre_p2_prefix();
    QVERIFY(prefix);
    QVERIFY(!prefix.value()->authorizes_m10_score());
    const auto hot=control.value()->execute_with_exact_pre_p2_prefix(
        *prefix.value());
    QVERIFY(hot);
    const auto cold_view=baseline.value()->view().channel(0);
    const auto hot_view=hot.value()->view().channel(0);
    QVERIFY(cold_view&&hot_view);
    QCOMPARE(cold_view.value()->size(),hot_view.value()->size());
    for(std::size_t i=0;i<cold_view.value()->size();++i)
        QCOMPARE((*cold_view.value())[i],(*hot_view.value())[i]);
    QVERIFY(!actual.value()->signatures().empty());
    QCOMPARE(actual.value()->signatures().back().type_id,
             std::string("rgsml.dsp.streaming-true-peak-limiter"));
    auto v=actual.value()->view().channel(0);
    auto b=baseline.value()->view().channel(0);
    QVERIFY(v&&b);
    bool differs=false;
    for(std::size_t i=0;i<v.value()->size();++i)
        if((*v.value())[i]!=(*b.value())[i]){
            differs=true;break;
        }
    QVERIFY(differs);
    // The M16 analytical True Peak must come from the *actual*
    // output of P2, not M17 or the pre-P2 source.
    const auto m16=analysis::measure_true_peak_full_resource(
        actual.value()->buffer());
    QVERIFY(m16);
    QVERIFY(std::isfinite(m16.value()->global_linear_peak()));
    QVERIFY(m16.value()->global_linear_peak()<=1.0);
    a::ProductNativeOnePassSpec spec;
    spec.current_full_phase6_source_and_grid=true;
    spec.exact_m20_fft_size=4096;
    spec.phase6_active_pcm_frames=48000U*3U;
    spec.exact_stereo_reference_grade=true;
    spec.exact_m19_low_end_gate_same_as_m20_stft=true;
    spec.exact_low_end_zero_side={{true,true,true}};
    spec.spectral_bands={{0,20,80},{1,80,200},{2,200,1000}};
    spec.stereo_bands={{0,20,80},{1,80,200},{2,200,1000}};
    const auto native=a::measure_m10_p2_full_native_once(
        *job.value(),std::move(spec),&*prefix.value());
    QVERIFY(native);
    QVERIFY(native.value()->native.s03_s06.numerical_source_valid);
    QVERIFY(native.value()->native.d07_d09.reason==
        a::m05m06_real::P04ProductObservationReason::ready);
    QVERIFY(!native.value()->native.authorizes_m10_or_export());
    QVERIFY(!native.value()->authentic_27_slot_m10_score());
    QVERIFY(!native.value()->authorizes_master_publication());
    QVERIFY(native.value()->native.actual.tonal.selected_spectral_windows>0U);
    QVERIFY(std::isfinite(
        native.value()->m16_actual_p2_output.global_linear_peak()));
    const auto* retained=std::get_if<
        dsp::StreamingTruePeakLimiterParameters>(
        &manual_bindings.back().parameters);
    QVERIFY(retained);
    QCOMPARE(retained->drive_db(),0.0);
    QVERIFY(!job.value()->still_current(
        {11U,11U,12U,"test-provider","source","test-provider","reference"},
        prepared_owner,*active_manual.value(),manual_bindings,
        *linked.value(),artistic));
    QVERIFY(active_manual.value()->set_user_bypass(p2_id,true));
    QVERIFY(!job.value()->still_current(
        stamp,prepared_owner,*active_manual.value(),manual_bindings,
        *linked.value(),artistic));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::P2RealNativeProductTest)
#include "test_l3_m10_p2_real_native_product_job.moc"
