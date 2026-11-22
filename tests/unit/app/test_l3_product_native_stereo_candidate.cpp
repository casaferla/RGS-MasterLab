#include "internal/l3_product_offline_trial_job.hpp"
#include "internal/l3_product_native_one_pass.hpp"
#include "internal/l3_automatch_typed_gain_trial.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <QtTest/QTest>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>
namespace rgsml::tests {
namespace {
namespace a=rgsml::app::l3_automatch;
[[nodiscard]] dsp::ModuleInstanceId test_id() {
    const auto uuid=core::Uuid::parse(
        "b1000000-0000-4000-8000-000000000001");
    Q_ASSERT(uuid);
    auto id=dsp::ModuleInstanceId::from_uuid(*uuid.value());
    Q_ASSERT(id);
    return *id.value();
}
class ProductNativeStereoTest final:public QObject {
    Q_OBJECT
private slots:
    void realStereoRenderSameM20FftDeliversS03ToS06();
};
void ProductNativeStereoTest::realStereoRenderSameM20FftDeliversS03ToS06() {
    auto registryValue=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registryValue);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*registryValue.value()));
    const dsp::ProcessingChainContext context{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL};
    auto empty=dsp::ProcessingChain::create(*registry,context);
    auto live=dsp::ProcessingChain::create(*registry,context);
    QVERIFY(empty&&live);
    const auto id=test_id();
    QVERIFY(live.value()->add(id,"rgsml.dsp.gain",0));
    const auto gain=dsp::GainParameters::create(0.0);
    QVERIFY(gain);
    const std::vector<dsp::ModuleExecutionBinding> baseline{
        {id,*gain.value()}};
    const auto trial=a::make_gain_trial(
        *live.value(),baseline,id,1.0,"stereo-candidate-real");
    QVERIFY(trial);
    const auto fs=core::SampleRate::create(48000);
    const auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    const auto frames=core::FrameCount::create(48000*3);
    const auto block=core::FrameCount::create(4096);
    QVERIFY(fmt&&frames&&block);
    auto buffer=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(buffer);
    auto left=buffer.value()->mutable_view().channel(0);
    auto right=buffer.value()->mutable_view().channel(1);
    QVERIFY(left&&right);
    for(std::size_t i=0;i<left.value()->size();++i) {
        const double v=0.2*std::sin(
            2.0*std::numbers::pi*50.0*
            static_cast<double>(i)/48000.0)
            +0.1*std::sin(2.0*std::numbers::pi*440.0*
            static_cast<double>(i)/48000.0);
        (*left.value())[i]=v;(*right.value())[i]=v;
    }
    auto request=render::RenderRequest::create(
        buffer.value()->view(),buffer.value()->view().absolute_range(),
        *empty.value(),{},*block.value());
    QVERIFY(request);
    auto prepared=render::render_preview(*request.value(),*registry);
    QVERIFY(prepared);
    auto owned=std::make_shared<const render::RenderResult>(
        std::move(*prepared.value()));
    const a::OfflineProductOwnerStamp stamp{
        1,2,3,"native-test","input-owner","native-test","reference-owner"};
    const auto job=a::OfflineProductTrialJob::create(
        stamp,owned,registry,*live.value(),baseline,*trial,*block.value());
    QVERIFY(job);
    constexpr std::array<bool,3> exact_mono{{true,true,true}};
    const auto native=job.value()->evaluate_with_native_stereo_s03_s06(
        {{0,20,80},{1,80,200},{2,200,1000}},4096,
        true,true,48000*3,true,exact_mono,{{true,true,true}});
    QVERIFY(native);
    const auto& source=native.value()->native_s03_s06;
    QVERIFY(source.numerical_source_valid);
    QVERIFY(source.native_selected_frames>=20U);
    QCOMPARE(source.multiband.size(),std::size_t{3});
    QCOMPARE(source.low_end_width.size(),std::size_t{3});
    for(const auto& band:source.multiband) {
        QVERIFY(band.width.transformed_z);
        QVERIFY(band.width.actual_zero_side_endpoint);
        QVERIFY(band.correlation.raw_ratio_or_rho);
    }
    for(const auto& low:source.low_end_width)
        QVERIFY(low.transformed_z);
    QVERIFY(job.value()->result_has_expected_scope(
        native.value()->measurement.rendered));
    QVERIFY(!native.value()->authorizes_export_or_plan());
    QVERIFY(!job.value()->evaluate_with_native_stereo_s03_s06(
        {{0,20,80}},4096,false,true,48000*3,true));
    const auto no_low=job.value()->evaluate_with_native_stereo_s03_s06(
        {{0,20,80}},4096,true,false,48000*3,true);
    QVERIFY(no_low);
    QVERIFY(!no_low.value()->native_s03_s06.low_end_width[0].transformed_z);
    // Real single Product candidate: one render and one M19/M20 pass
    // with BOTH native spectral/stereo observers plus distinct P04.
    a::ProductNativeOnePassSpec spec;
    spec.current_full_phase6_source_and_grid=true;
    spec.exact_m20_fft_size=4096;
    spec.phase6_active_pcm_frames=48000U*3U;
    spec.exact_stereo_reference_grade=true;
    spec.exact_m19_low_end_gate_same_as_m20_stft=true;
    spec.exact_low_end_zero_side={{true,true,true}};
    spec.spectral_bands={{0,20,80},{1,80,200},{2,200,1000}};
    spec.stereo_bands={{0,20,80},{1,80,200},{2,200,1000}};
    const auto unified=a::measure_real_product_candidate_once(
        *job.value(),std::move(spec));
    QVERIFY(unified);
    QVERIFY(job.value()->result_has_expected_scope(
        unified.value()->actual.rendered));
    QCOMPARE(unified.value()->actual.tonal.mean_linear_psd,
             native.value()->measurement.tonal.mean_linear_psd);
    QCOMPARE(unified.value()->selected_original_m20_ordinals.size(),
             static_cast<std::size_t>(
               unified.value()->actual.tonal.selected_spectral_windows));
    QCOMPARE(unified.value()->s03_s06.native_selected_frames,
             unified.value()->actual.tonal.selected_spectral_windows);
    QVERIFY(unified.value()->s03_s06.numerical_source_valid);
    QCOMPARE(unified.value()->d10_d12.canonical_band_results.size(),
             std::size_t{3});
    QVERIFY(unified.value()->d07_d09.actual_flux.canonical.size()>300U);
    QCOMPARE(unified.value()->d07_d09.reason,
             a::m05m06_real::P04ProductObservationReason::ready);
    QVERIFY(!unified.value()->authorizes_m10_or_export());
    std::atomic<bool> cancelled{true};
    a::ProductNativeOnePassSpec cancel_spec;
    cancel_spec.current_full_phase6_source_and_grid=true;
    cancel_spec.exact_m20_fft_size=4096;
    cancel_spec.spectral_bands={{0,20,80}};
    QVERIFY(!a::measure_real_product_candidate_once(
        *job.value(),std::move(cancel_spec),&cancelled));
    QVERIFY(job.value()->still_current(
        stamp,owned,*live.value(),baseline));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::ProductNativeStereoTest)
#include "test_l3_product_native_stereo_candidate.moc"
