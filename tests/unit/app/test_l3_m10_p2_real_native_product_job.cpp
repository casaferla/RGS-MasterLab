#include "internal/l3_m10_p2_real_native_product_job.hpp"
#include "internal/l3_product_native_one_pass.hpp"
#include "internal/l3_m05m06_owned_full_source_native_facts.hpp"
#include "internal/l3_m05m06_original_prepared_reference_pair.hpp"
#include "internal/l3_m05m06_product_reference_full_native_worker.hpp"
#include "internal/l3_m05m06_product_qt_original_pair_worker.hpp"
#include "internal/l3_m10_p2_full_native_evidence.hpp"
#include "internal/l3_m10_p2_authoritative_native_score.hpp"
#include "internal/l3_m10_native_fixed_worker.hpp"
#include "internal/l3_m12_preserved_native_winner.hpp"
#include "internal/l3_m09_native_selected_search.hpp"
#include "internal/l3_m10_p2_bound_native_worker_score.hpp"
#include "internal/l3_m10_p2_native_exact_cache_search.hpp"
#include "internal/l3_m10_p2_frozen_real_refinement_batch.hpp"
#include "internal/l3_m10_p2_actual_m16_guard.hpp"
#include "internal/l3_m10_p2_real_native_triple_binding.hpp"
#include "internal/l3_m10_p2_product_qt_owner_capture.hpp"
#include "internal/l3_m10_p2_verified_candidate_triple.hpp"
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
#include <limits>
#include <numbers>
#include <optional>
#include <vector>
#include <type_traits>

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
// Compile the real Qt owner admission surface in the same Windows/Linux
// native product gate. This is NOT a Qt Product GUI integration test.
static_assert(std::is_pointer_v<
    decltype(&a::ProductQtP2NativeAdmission::capture_on_owner)>);
static_assert(std::is_pointer_v<
    decltype(&a::ProductQtP2NativeAdmission::still_current_on_owner)>);
static_assert(std::is_pointer_v<
    decltype(&a::measure_current_gold_full_native_worker)>);
static_assert(std::is_pointer_v<
    decltype(&a::measure_product_qt_original_pair_native_worker)>);
static_assert(std::is_same_v<
    decltype(std::declval<
        const a::ProductQtP2NativeAdmission::Accepted&>().
        native_worker_scope()),
    a::m10_frozen::P2NativeProductWorkerScope>);
class P2RealNativeProductTest final:public QObject {
    Q_OBJECT
private slots:
    void genuineM10CandidateRunsPostLinkedP2AndNativeAnalysis();
    void nativeAutomaticPayloadsRespectFixedTopologyAndBounds();
    void nativeTypedSeedBuildsRealRefinementTrial();
    void nativeM20MetadataUsesSixRateGeometry();
    void nativeProductPairMetadataKeepsIndependentRates();
    void adaptiveRamCacheBudgetPreservesColdRender();
    void canonicalP2TrialTicketsCover48OrderedParameters();
    void streamingP2AndIndependentM16RenderAtAllSixRates_data();
    void streamingP2AndIndependentM16RenderAtAllSixRates();
    void canonicalAll27ScoringRequiresRealM11Protection();
    void allNativeTripleOwnersAgreeBeforeBuildingAnyFeature();
    void m16ActualSafetyRejectsExcessAndInvalidCeilings();
};
void P2RealNativeProductTest::nativeM20MetadataUsesSixRateGeometry() {
    for(const auto fs:std::array<std::uint32_t,6>{44100,48000,88200,96000,176400,192000}) {
        const auto frame=analysis::native_tonal_frame_geometry(fs);
        const auto bandwidth=analysis::native_tonal_measurement_bandwidth(fs);
        QVERIFY(frame&&bandwidth);
        QVERIFY(frame->window>0U&&frame->fft>=frame->window&&frame->hop>0U);
        QCOMPARE(bandwidth->low_hz,20.0);
        QCOMPARE(bandwidth->high_hz,fs==44100U?19845.0:20000.0);
        for(const int denominator:{3,12}) {
            const auto grid=analysis::native_tonal_fractional_grid(fs,denominator);
            QVERIFY(!grid.empty());
            for(const auto& band:grid) {
                QVERIFY(band.low_hz<band.center_hz&&band.center_hz<band.high_hz);
                QVERIFY(band.high_hz>bandwidth->low_hz&&band.low_hz<bandwidth->high_hz);
                QCOMPARE(band.fully_supported,band.low_hz>=bandwidth->low_hz&&
                         band.high_hz<=bandwidth->high_hz);
                QVERIFY(!band.normalized_fraction.value);
                QCOMPARE(band.normalized_fraction.state,
                    analysis::TonalStereoMetricState::UNAVAILABLE_AUTHORITATIVE_SUPPORT);
            }
        }
        QVERIFY(analysis::native_tonal_fractional_grid(fs,6).empty());
    }
    QVERIFY(!analysis::native_tonal_frame_geometry(32000));
    QVERIFY(!analysis::native_tonal_measurement_bandwidth(32000));
    QVERIFY(analysis::native_tonal_fractional_grid(32000,12).empty());
}
void P2RealNativeProductTest::nativeProductPairMetadataKeepsIndependentRates() {
    namespace real=a::m05m06_real;
    constexpr std::array<std::uint32_t,6> rates{44100,48000,88200,96000,176400,192000};
    for(const auto p:rates)for(const auto g:rates) {
        const auto common=real::original_native_common_tonal_bandwidth(p,g);
        QVERIFY(common);
        QCOMPARE(common->low_hz,20.0);
        QCOMPARE(common->high_hz,(p==44100U||g==44100U)?19845.0:20000.0);
        QVERIFY(real::native_common_contains(*common,{20.0,18000.0}));
        QVERIFY(!real::native_common_contains(*common,{20.0,common->high_hz+1.0}));
        QVERIFY(!real::native_common_contains(*common,{21.0,20.0}));
        QVERIFY(!real::native_common_contains(*common,{20.0,
                 std::numeric_limits<double>::quiet_NaN()}));
        a::ProductNativeOnePassSpec ps,gs;
        QVERIFY(a::bind_product_native_fft_metadata(ps,p));
        QVERIFY(a::bind_product_native_fft_metadata(gs,g));
        QCOMPARE(ps.exact_m20_fft_size,analysis::native_tonal_frame_geometry(p)->fft);
        QCOMPARE(gs.exact_m20_fft_size,analysis::native_tonal_frame_geometry(g)->fft);
        QVERIFY(!ps.current_full_phase6_source_and_grid);
        QVERIFY(ps.spectral_bands.empty());
        ps.exact_m20_fft_size=1U;
        QVERIFY(!a::bind_product_native_fft_metadata(ps,p));
        QCOMPARE(ps.exact_m20_fft_size,std::uint64_t{1});
    }
    QVERIFY(!real::original_native_common_tonal_bandwidth(48000,32000));
    a::ProductNativeOnePassSpec invalid;
    QVERIFY(!a::bind_product_native_fft_metadata(invalid,32000));
}
void P2RealNativeProductTest::nativeAutomaticPayloadsRespectFixedTopologyAndBounds() {
    namespace m09=a::m09_frozen;
    using F=m09::NativeField;
    const auto gain=dsp::GainParameters::create(0.0);
    const auto transient=dsp::TransientShaperParameters::create(0.5,-0.5);
    const auto stereo=dsp::StereoMsParameters::create_default();
    const auto compressor=dsp::CompressorParameters::create_default();
    QVERIFY(gain&&transient&&stereo&&compressor);
    const auto apply=[](const dsp::ModuleParameterPayload& p,
                         std::vector<m09::NativePhysicalField> fields){
        return m09::apply_native_physical_fields(p,fields);
    };
    auto moved=apply(*gain.value(),{{F::gain_db,{},2.0}});
    QVERIFY(moved);
    QCOMPARE(std::get<dsp::GainParameters>(*moved).gain_db(),2.0);
    QVERIFY(!apply(*gain.value(),{{F::gain_db,{},6.1}}));
    QVERIFY(!apply(*gain.value(),{{F::gain_db,{},1.0},{F::gain_db,{},2.0}}));
    moved=apply(*transient.value(),{{F::attack_amount_db,{},2.0},{F::sustain_amount_db,{},-1.0}});
    QVERIFY(moved);
    QCOMPARE(std::get<dsp::TransientShaperParameters>(*moved).attack_amount_db(),2.0);
    QCOMPARE(std::get<dsp::TransientShaperParameters>(*moved).sustain_amount_db(),-1.0);
    QVERIFY(!apply(*transient.value(),{{F::attack_ms,{},3.0}}));
    QVERIFY(!apply(*stereo.value(),{{F::low_width_percent,{},50.0}}));
    moved=apply(*stereo.value(),{{F::side_gain_db,{},1.5}});
    QVERIFY(moved);
    QCOMPARE(std::get<dsp::StereoMsParameters>(*moved).side_gain_db(),1.5);
    QVERIFY(!apply(*stereo.value(),{{F::side_gain_db,{},2.0}}));
    moved=apply(*compressor.value(),{{F::ratio,{},1.5},{F::attack_ms,{},12.0}});
    QVERIFY(moved);
    QCOMPARE(std::get<dsp::CompressorParameters>(*moved).ratio(),1.5);
    QCOMPARE(std::get<dsp::CompressorParameters>(*moved).makeup_gain_db(),0.0);
    QVERIFY(!apply(*compressor.value(),{{F::ratio,{},2.1}}));
    QVERIFY(!apply(*compressor.value(),{{F::makeup_gain_db,{},1.0}}));

    const auto band_id=uuid("bd000000-0000-4000-8000-000000000011");
    const auto eqband=dsp::EqBandParameters::create(band_id,true,dsp::EqFilterType::BELL,
        dsp::EqRouting::SIDE,dsp::BellPayload{1000.0,0.0,0.707});
    QVERIFY(eqband);
    const auto eq=dsp::ParametricEqParameters::create({*eqband.value()});
    QVERIFY(eq);
    moved=apply(*eq.value(),{{F::frequency_hz,band_id,1200.0},{F::gain_db,band_id,-1.5}});
    QVERIFY(moved);
    const auto& actual_band=std::get<dsp::ParametricEqParameters>(*moved).bands().front();
    QCOMPARE(actual_band.band_id(),band_id);
    QCOMPARE(actual_band.routing(),dsp::EqRouting::SIDE);
    QCOMPARE(std::get<dsp::BellPayload>(actual_band.payload()).frequency_hz,1200.0);
    QVERIFY(!apply(*eq.value(),{{F::gain_db,band_id,2.0}}));
    QVERIFY(!apply(*eq.value(),{{F::shelf_slope,band_id,0.5}}));
    dsp::DynamicEqBandSpec spec;
    spec.band_id=band_id;
    spec.control.range_db=-1.0;
    const auto dynamic_band=dsp::DynamicEqBandParameters::create(spec);
    QVERIFY(dynamic_band);
    const auto dynamic=dsp::DynamicEqParameters::create({*dynamic_band.value()});
    QVERIFY(dynamic);
    moved=apply(*dynamic.value(),{{F::range_db,band_id,-2.0},{F::threshold_dbfs,band_id,-30.0}});
    QVERIFY(moved);
    QCOMPARE(std::get<dsp::DynamicEqParameters>(*moved).bands().front().spec().control.range_db,-2.0);
    QVERIFY(!apply(*dynamic.value(),{{F::range_db,band_id,1.0}}));
    QVERIFY(!apply(*dynamic.value(),{{F::base_gain_db,band_id,1.0}}));
    QVERIFY(!m09::native_automatic_coordinate_rule(*dynamic.value(),F::knee_db,band_id,{},48000));
    QVERIFY(!m09::native_automatic_coordinate_rule(*dynamic.value(),F::look_ahead_ms,{}, {},48000));
}
void P2RealNativeProductTest::nativeTypedSeedBuildsRealRefinementTrial() {
    namespace m09=a::m09_frozen;
    namespace m10=a::m10_frozen;
    using F=m09::NativeField;
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(std::move(*base.value()));
    const auto gain_id=id("be000000-0000-4000-8000-000000000001");
    const auto semantic=uuid("be000000-0000-4000-8000-000000000002");
    auto instance=dsp::ModuleInstance::create({
        gain_id,"rgsml.dsp.gain",true,false,false,false,
        dsp::ModuleProvenance::REFERENCE_MATCH,dsp::ModuleOwner::REFERENCE_MATCH,
        dsp::ModuleLinkState::REF_LINKED,semantic,{}});
    QVERIFY(instance);
    auto chain=dsp::ProcessingChain::restore(*registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        11U,{*instance.value()});
    auto gain=dsp::GainParameters::create(1.0);
    QVERIFY(chain&&gain);
    const dsp::ModuleExecutionBinding binding{gain_id,*gain.value()};
    const auto coordinate=m09::make_native_seed_coordinate(*instance.value(),
        binding,{0x10},{0x20},F::gain_db,{}, {},48000);
    QVERIFY(coordinate);
    QCOMPARE(coordinate->immutable_reference_u,0.5);
    QCOMPARE(coordinate->reference_weight,1.0);
    const m09::NativeTypedSeed seed{registry,{*chain.value(),{binding}},{*coordinate}};
    const auto inventory=m09::native_seed_inventory(seed);
    QVERIFY(inventory);
    const std::array<double,1> current{{coordinate->canonical.normalized_u}};
    const auto exact=m09::decode_native_typed_seed(seed,*inventory,current);
    QVERIFY(exact);
    QVERIFY(exact->graph.bindings==seed.seed.bindings);
    const std::array<double,1> proposal{{0.75}};
    auto substituted_inventory=*inventory;
    substituted_inventory.front().normalized_u=0.25;
    QVERIFY(!m09::decode_native_typed_seed(seed,substituted_inventory,proposal));
    const auto trial=m09::build_native_typed_trial(seed,*inventory,proposal,{},
        [&](const m10::PreservedNativeGraph& graph,const auto&,const auto&)
            ->std::optional<m10::ExactExecutionCacheKey>{
            const auto graph_bytes=m10::exact_native_graph_content(
                graph.linked,graph.bindings,*registry);
            if(!graph_bytes)return std::nullopt;
            return m10::ExactExecutionCacheKey{{0x01},*graph_bytes,{0x02},{0x03}};
        });
    QVERIFY(trial);
    QCOMPARE(std::get<dsp::GainParameters>(trial->graph.bindings.front().parameters).gain_db(),3.0);
    QCOMPARE(trial->constraints.active_owned_modules,std::uint32_t{1});
    QCOMPARE(trial->constraints.enabled_artistic_bands,std::uint32_t{0});
    QCOMPARE(trial->constraints.active_coordinates.front().immutable_reference_u,0.5);
    QCOMPARE(trial->constraints.active_coordinates.front().current_u,0.75);
    QVERIFY(seed.seed.bindings.front()==binding);
    auto substituted=seed;
    substituted.coordinates.front().exact_allocated_semantic_node_id=
        uuid("be000000-0000-4000-8000-000000000003");
    QVERIFY(!m09::decode_native_typed_seed(substituted,*inventory,proposal));
    substituted=seed;
    substituted.coordinates.front().canonical.normalized_u=0.25;
    QVERIFY(!m09::native_seed_inventory(substituted));
}
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
    // Product admission keeps the user's P2 settings, never the
    // explicit research optimizer override used by the older path.
    auto preserved=a::M10P2RealNativeProductJob::create_preserving_user_terminal(
        stamp,prepared_owner,product_registry,*active_manual.value(),
        manual_bindings,*linked.value(),artistic,"preserved-user-p2",
        *block.value(),true);
    QVERIFY(preserved);
    auto unchanged=preserved.value()->execute();
    QVERIFY(unchanged);
    QVERIFY(preserved.value()->result_has_expected_scope(*unchanged.value()));
    for(std::size_t c=0;c<2;++c) {
        const auto expected=baseline.value()->view().channel(c);
        const auto observed=unchanged.value()->view().channel(c);
        QVERIFY(expected&&observed);
        QVERIFY(std::equal(expected.value()->begin(),expected.value()->end(),
                           observed.value()->begin(),observed.value()->end()));
    }

    QVERIFY(manual.value()->add(gain_id,"rgsml.dsp.gain",0));
    const auto gain_only=std::span<const dsp::ModuleExecutionBinding>{manual_bindings}.first(1);
    auto without_p2=a::M10P2RealNativeProductJob::create_preserving_user_terminal(
        stamp,prepared_owner,product_registry,*manual.value(),gain_only,
        *linked.value(),artistic,"without-user-p2",*block.value(),true);
    QVERIFY(without_p2);
    auto no_terminal=without_p2.value()->execute();
    QVERIFY(no_terminal);
    QVERIFY(without_p2.value()->result_has_expected_scope(*no_terminal.value()));
    QVERIFY(!without_p2.value()->prepare_exact_pre_p2_prefix());
    QVERIFY(without_p2.value()->preserves_user_terminal());
    QVERIFY(!without_p2.value()->fixed_only());
    auto empty_linked=dsp::ProcessingChain::restore(
        *product_registry,{dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        1U,{});
    QVERIFY(empty_linked);
    auto fixed_job=a::M10P2RealNativeProductJob::create_preserving_user_terminal(
        stamp,prepared_owner,product_registry,*manual.value(),gain_only,
        *empty_linked.value(),{},"real-fixed-baseline",*block.value(),true);
    QVERIFY(fixed_job);
    QVERIFY(fixed_job.value()->fixed_only());
    QVERIFY(fixed_job.value()->same_fixed_baseline_as(*without_p2.value()));
    QVERIFY(!fixed_job.value()->same_fixed_baseline_as(*preserved.value()));
    auto fixed_pcm=fixed_job.value()->execute();
    QVERIFY(fixed_pcm);
    QVERIFY(fixed_job.value()->result_has_expected_scope(*fixed_pcm.value()));
    QVERIFY(fixed_pcm.value()->signatures().empty());
    // Actual native graph identity changes with user P2 state or
    // a distinct linked segment, but Fixed excludes that segment.
    const auto fixed_graph=fixed_job.value()->exact_fixed_graph_content();
    const auto unlinked_graph=without_p2.value()->exact_linked_graph_content();
    QVERIFY(fixed_graph&&unlinked_graph);
    QVERIFY(fixed_graph==without_p2.value()->exact_fixed_graph_content());
    QVERIFY(fixed_graph!=preserved.value()->exact_fixed_graph_content());
    QVERIFY(fixed_job.value()->exact_execution_graph_content()!=
            without_p2.value()->exact_execution_graph_content());
    QVERIFY(unlinked_graph==preserved.value()->exact_linked_graph_content());
    auto bypassed=*active_manual.value();
    QVERIFY(bypassed.set_user_bypass(p2_id,true));
    auto bypass_job=a::M10P2RealNativeProductJob::create_preserving_user_terminal(
        stamp,prepared_owner,product_registry,bypassed,manual_bindings,
        *linked.value(),artistic,"bypassed-user-p2",*block.value(),true);
    QVERIFY(bypass_job);
    auto bypass_pcm=bypass_job.value()->execute();
    QVERIFY(bypass_pcm);
    QVERIFY(bypass_job.value()->result_has_expected_scope(*bypass_pcm.value()));
    for(std::size_t c=0;c<2;++c) {
        const auto expected=no_terminal.value()->view().channel(c);
        const auto observed=bypass_pcm.value()->view().channel(c);
        QVERIFY(expected&&observed);
        QVERIFY(std::equal(expected.value()->begin(),expected.value()->end(),
                           observed.value()->begin(),observed.value()->end()));
    }
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
    const auto spec_for_authoritative_bridge=spec;
    auto own_gate_spec=spec;
    own_gate_spec.derive_own_m19_temporal_gate=true;
    own_gate_spec.source_support.has_spectral_gate=false;
    own_gate_spec.source_support.spectral_frame_mask={};
    own_gate_spec.source_support.has_width_gate=false;
    own_gate_spec.source_support.retained_width_blocks={};
    const auto own_gate_native=a::measure_real_full_owned_pcm_native_once(*source.value(),own_gate_spec);
    QVERIFY(own_gate_native);
    const auto own_gate=analysis::bind_native_m19_tonal_gate(own_gate_native.value()->levels);
    QVERIFY(own_gate);
    QCOMPARE(own_gate_native.value()->exact_observed_active_frames,own_gate.value()->active_pcm_frames);
    QCOMPARE(own_gate_native.value()->tonal.spectral_all_frames_fallback,!own_gate.value()->available);
    QCOMPARE(own_gate_native.value()->tonal.width_all_scope_fallback,!own_gate.value()->available);
    own_gate_spec.source_support.has_width_gate=true;
    QVERIFY(!a::measure_real_full_owned_pcm_native_once(*source.value(),own_gate_spec));
    // One genuine, preexisting full-resource PCM can now use the
    // SAME native Phase6 M19/M20/P04 analysis without rerendering a
    // candidate. PREPARED and decoded REFERENCE will retain distinct
    // Product owners; no computed observation itself authorizes M07.
    const auto prepared_native=
        a::measure_real_full_owned_pcm_native_once(
            *source.value(),spec_for_authoritative_bridge);
    QVERIFY(prepared_native);
    QCOMPARE(prepared_native.value()->levels.sample_rate_hz,48000);
    QCOMPARE(prepared_native.value()->tonal.sample_rate_hz,48000U);
    QVERIFY(prepared_native.value()->tonal.selected_spectral_windows>0U);
    QVERIFY(!prepared_native.value()->authorizes_m10_or_m12());
    const auto once_per_original_owner=
        a::observe_original_owned_full_source_once(
            *source.value(),
            {"real-prepared-full-track-owner",11U,true},
            spec_for_authoritative_bridge,
            {20.0,18000.0},1e-12,true);
    QVERIFY(once_per_original_owner);
    QCOMPARE(once_per_original_owner.value()->original_m19_m20.
                 scope_frame_count,std::int64_t{48000*3});
    QCOMPARE(once_per_original_owner.value()->full_native.
                 tonal.selected_spectral_windows,
             prepared_native.value()->tonal.selected_spectral_windows);
    QVERIFY(!once_per_original_owner.value()->authorizes_m10_or_m13());
    auto invalid_fft=spec_for_authoritative_bridge;
    invalid_fft.exact_m20_fft_size*=2U;
    QVERIFY(!a::measure_real_full_owned_pcm_native_once(*source.value(),invalid_fft));
    QVERIFY(!a::observe_original_owned_full_source_once(
        *source.value(),{"real-prepared-full-track-owner",11U,true},
        spec_for_authoritative_bridge,{20.0,20001.0},1e-12,true));
    QVERIFY(!a::observe_original_owned_full_source_once(
        *source.value(),{"real-prepared-full-track-owner",11U,true},
        spec_for_authoritative_bridge,{20.0,18000.0},0.0,true));
    QVERIFY(!a::observe_original_owned_full_source_once(
            *source.value(),
            {"missing-or-stale-original",0U,false},
            spec_for_authoritative_bridge,
            {20.0,18000.0},1e-12,true));
    // The REFERENCE is a different genuinely owned PCM buffer, not
    // a second identity label applied to the PREPARED pointer.
    auto independent_gold=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*length.value());
    QVERIFY(independent_gold);
    auto gold_left=independent_gold.value()->mutable_view().channel(0);
    auto gold_right=independent_gold.value()->mutable_view().channel(1);
    QVERIFY(gold_left&&gold_right);
    for(std::size_t i=0;i<gold_left.value()->size();++i) {
        const double reference_sample=0.3*std::sin(
            2.0*std::numbers::pi*660.0*
            static_cast<double>(i)/48000.0);
        (*gold_left.value())[i]=reference_sample;
        (*gold_right.value())[i]=reference_sample;
    }
    const auto source_owner=a::native_product_original_identity(
        stamp,a::OriginalProductRole::prepared);
    const auto gold_owner=a::native_product_original_identity(
        stamp,a::OriginalProductRole::reference);
    QVERIFY(!source_owner.resource_identity.empty());
    QVERIFY(!gold_owner.resource_identity.empty());
    QVERIFY(source_owner.resource_identity!=gold_owner.resource_identity);
    QVERIFY(a::authentic_product_source_identity(
        source_owner,stamp,a::OriginalProductRole::prepared));
    QVERIFY(a::authentic_product_source_identity(
        gold_owner,stamp,a::OriginalProductRole::reference));
    QVERIFY(!a::authentic_product_source_identity(
        gold_owner,stamp,a::OriginalProductRole::prepared));
    auto same_generation_other_file=stamp;
    same_generation_other_file.reference_locator="changed/reference.wav";
    QVERIFY(!a::authentic_product_source_identity(
        gold_owner,same_generation_other_file,
        a::OriginalProductRole::reference));
    auto new_epoch_same_resource=stamp;
    ++new_epoch_same_resource.m18_owner_epoch;
    QVERIFY(!a::authentic_product_source_identity(
        gold_owner,new_epoch_same_resource,
        a::OriginalProductRole::reference));
    // Length delimiting prevents an ambiguous provider/locator split.
    auto ambiguous_a=stamp;
    ambiguous_a.source_provider_id="test-provider.";
    ambiguous_a.source_locator="source";
    auto ambiguous_b=stamp;
    ambiguous_b.source_provider_id="test-provider";
    ambiguous_b.source_locator=".source";
    QVERIFY(a::native_product_original_identity(
        ambiguous_a,a::OriginalProductRole::prepared).resource_identity!=
        a::native_product_original_identity(
        ambiguous_b,a::OriginalProductRole::prepared).resource_identity);
    QVERIFY(!a::measure_original_prepared_reference_native_once(
        *source.value(),source_owner,spec_for_authoritative_bridge,
        *source.value(),gold_owner,spec_for_authoritative_bridge,
        {20.0,18000.0},1e-12,true,true,true));
    auto pair_metadata_spec=spec_for_authoritative_bridge;
    pair_metadata_spec.exact_m20_fft_size=0U; // derive separately from owned PCM
    QVERIFY(!a::measure_original_prepared_reference_native_once(
            *source.value(),source_owner,pair_metadata_spec,
            *independent_gold.value(),gold_owner,pair_metadata_spec,
            {20.0,20001.0},1e-12,true,true,true));
    const auto original_pair=
        a::measure_original_prepared_reference_native_once(
            *source.value(),source_owner,pair_metadata_spec,
            *independent_gold.value(),gold_owner,pair_metadata_spec,
            {20.0,18000.0},1e-12,true,true,true);
    QVERIFY(original_pair);
    auto same_profile=spec_for_authoritative_bridge;
    QVERIFY(a::bind_original_pair_profile_common(same_profile,*original_pair.value()));
    QVERIFY(same_profile.source_support.profile_common_bandwidth);
    QCOMPARE(same_profile.source_support.profile_common_bandwidth->high_hz,18000.0);
    auto wrong_profile=same_profile;
    wrong_profile.source_support.profile_common_bandwidth->high_hz=19000.0;
    QVERIFY(!a::bind_original_pair_profile_common(wrong_profile,*original_pair.value()));
    QCOMPARE(original_pair.value()->prepared.original_m19_m20.
                 owner.resource_identity,source_owner.resource_identity);
    QCOMPARE(original_pair.value()->reference.original_m19_m20.
                 owner.resource_identity,gold_owner.resource_identity);
    QVERIFY(!original_pair.value()->authorizes_m10_or_m12());
    const auto candidate_native=
        a::measure_real_full_owned_pcm_native_once(
            actual.value()->buffer(),spec_for_authoritative_bridge);
    QVERIFY(candidate_native);
    QVERIFY(!candidate_native.value()->authorizes_m10_or_m12());
    const auto native=a::measure_m10_p2_full_native_once(
        *job.value(),std::move(spec),&*prefix.value());
    QVERIFY(native);
    QVERIFY(native.value()->native.s03_s06.numerical_source_valid);
    QCOMPARE(candidate_native.value()->levels.integrated_lufs.state,
             native.value()->native.actual.levels.integrated_lufs.state);
    QCOMPARE(candidate_native.value()->tonal.selected_spectral_windows,
             native.value()->native.actual.tonal.selected_spectral_windows);
    QCOMPARE(candidate_native.value()->selected_original_m20_ordinals,
             native.value()->native.selected_original_m20_ordinals);
    QVERIFY(native.value()->native.d07_d09.reason==
        a::m05m06_real::P04ProductObservationReason::ready);
    QVERIFY(!native.value()->native.authorizes_m10_or_export());
    QVERIFY(!native.value()->authentic_27_slot_m10_score());
    QVERIFY(!native.value()->authorizes_master_publication());
    QVERIFY(native.value()->native.actual.tonal.selected_spectral_windows>0U);
    QVERIFY(std::isfinite(
        native.value()->m16_actual_p2_output.global_linear_peak()));
    // The complete P2 PCM and M19/M20/M16 must agree on their scope.
    namespace m10=a::m10_frozen;
    QVERIFY(m10::check_m10_p2_native_actual_m16(
        *native.value(),0.0).passes());
    QVERIFY(!m10::check_m10_p2_native_actual_m16(
        *native.value(),-90.0).passes());
    const auto owner=a::native_product_candidate_identity(
        stamp,chosen->typed.base.ticket_identity);
    const auto control_candidate_owner=a::native_product_candidate_identity(
        stamp,"real-p2-cached-canonical-ticket");
    QVERIFY(a::authentic_product_candidate_identity(
        owner,stamp,chosen->typed.base.ticket_identity));
    QVERIFY(!a::authentic_product_candidate_identity(
        owner,stamp,"a-different-candidate"));
    QVERIFY(!a::authentic_product_candidate_identity(
        owner,new_epoch_same_resource,chosen->typed.base.ticket_identity));
    QVERIFY(owner.resource_identity!=
        control_candidate_owner.resource_identity);
    // The authentic full-source pair must REPLACE every P/G numerical
    // family. Even after original real P/G/C PCM, missing canonical
    // M07 Phase6 definitions MUST prevent a score.
    const auto original_three_way=[&]{
        m10::P2OriginalRealNativeTriple triple;
        const std::array<a::m05m06_real::SourceIdentity,3> owners{{
            source_owner,gold_owner,owner}};
        for(std::size_t k=0;k<3U;++k) {
            triple.t01.actual_sources[k].owner=owners[k];
            triple.t02_t06.sources[k].owner=owners[k];
            triple.d01.source[k].owner=owners[k];
            triple.d07_d09.current_sources[k].owner=owners[k];
            triple.d10_d12.source_owner[k]=owners[k];
            triple.s03_s06.independent_source_owners[k]=owners[k];
        }
        triple.m19.prepared.owner=owners[0];
        triple.m19.reference.owner=owners[1];
        triple.m19.actual_candidate.owner=owners[2];
        triple.s01_s02.prepared.owner=owners[0];
        triple.s01_s02.reference.owner=owners[1];
        triple.s01_s02.actual_candidate.owner=owners[2];
        triple.d02.prepared_owner=owners[0];
        triple.d02.reference_owner=owners[1];
        triple.d02.candidate_owner=owners[2];
        triple.d03_d04.prepared_owner=owners[0];
        triple.d03_d04.reference_owner=owners[1];
        triple.d03_d04.candidate_owner=owners[2];
        return triple;
    };
    auto definition_fixture=original_three_way();
    definition_fixture.t01.authoritative_primary_grid={{4,100.0,110.0,120.0}};
    definition_fixture.t01.exact_native_band_support_prepared_gold[0]={0.75};
    definition_fixture.t01.exact_native_band_support_prepared_gold[1]={0.80};
    definition_fixture.t01.actual_sources[2].integrated_lufs=
        {-12.0,a::m05m06_real::Readiness::measured};
    definition_fixture.t02_t06.exact_common={20.0,18000.0};
    definition_fixture.t02_t06.exact_accepted_density_floor=1e-12;
    const auto retained_definitions=m10::retain_original_native_definitions(definition_fixture);
    QCOMPARE(retained_definitions.t01.authoritative_primary_grid.size(),std::size_t{1});
    QVERIFY(!retained_definitions.t01.actual_sources[2].integrated_lufs.ready());
    const auto definition_content=m10::exact_original_native_definition_content(retained_definitions);
    QVERIFY(!definition_content.empty());
    QVERIFY(definition_content==
        m10::exact_original_native_definition_content(definition_fixture));
    auto changed_definition=retained_definitions;
    changed_definition.t01.exact_native_band_support_prepared_gold[0][0]=0.5;
    QVERIFY(definition_content!=m10::exact_original_native_definition_content(changed_definition));
    changed_definition=retained_definitions;
    changed_definition.d07_d09.per_descriptor_source_grade[0][2]=true;
    QVERIFY(definition_content!=m10::exact_original_native_definition_content(changed_definition));
    // Provenance-only fixture. Its incomplete definitions still cannot
    // become a complete M07 measurement or a numerical winner.
    a::m11_frozen::NativeFixedPopulation fixed_definition_fixture;
    fixed_definition_fixture.exact_independent_fixed_and_endpoint_safety_attested=true;
    fixed_definition_fixture.exact_fixed_ticket=chosen->typed.base.ticket_identity;
    fixed_definition_fixture.exact_fixed_resource_identity=owner.resource_identity;
    fixed_definition_fixture.exact_fixed_source_generation=owner.generation;
    fixed_definition_fixture.exact_m07_generation="current-phase6-test-generation";
    fixed_definition_fixture.original_definition_authority=
        std::make_shared<const m10::P2OriginalRealNativeTriple>(retained_definitions);
    fixed_definition_fixture.exact_definition_authority_content=definition_content;
    const auto resolved=m10::resolve_original_definitions_from_fixed(
        fixed_definition_fixture,*original_pair.value(),stamp,
        "current-phase6-test-generation","new-candidate");
    QVERIFY(resolved);
    const auto resolved_owner=a::native_product_candidate_identity(stamp,"new-candidate");
    QVERIFY(m10::exact_source_owner_equal(resolved->m19.actual_candidate.owner,resolved_owner));
    QVERIFY(m10::exact_source_owner_equal(resolved->s03_s06.independent_source_owners[2],
                                         resolved_owner));
    QVERIFY(!resolved->t01.actual_sources[2].integrated_lufs.ready());
    auto mismatched_scope=*resolved;
    mismatched_scope.t02_t06.exact_common.high_hz=19000.0;
    QVERIFY(!m10::rebind_independent_original_native_pair(
        mismatched_scope,*original_pair.value(),resolved_owner));
    QVERIFY(!m10::resolve_original_definitions_from_fixed(
        fixed_definition_fixture,*original_pair.value(),stamp,"stale-generation","new-candidate"));
    QVERIFY(!m10::resolve_original_definitions_from_fixed(
        fixed_definition_fixture,*original_pair.value(),stamp,
        "current-phase6-test-generation",fixed_definition_fixture.exact_fixed_ticket));
    fixed_definition_fixture.exact_definition_authority_content.back()^=0x01;
    QVERIFY(!m10::resolve_original_definitions_from_fixed(
        fixed_definition_fixture,*original_pair.value(),stamp,
        "current-phase6-test-generation","new-candidate"));
    auto stale_original=original_three_way();
    stale_original.d10_d12.source_owner[1].resource_identity=
        "wrong-independent-reference";
    QCOMPARE(
        m10::bind_p2_original_native_triple_from_independent_full_pair(
            std::move(stale_original),*original_pair.value(),
            *native.value(),owner,48000*3,true).reason,
        m10::P2RealTripleBindingReason::
            missing_owner_or_inconsistent_three_way_scope);
    const auto joined=
        m10::bind_p2_original_native_triple_from_independent_full_pair(
            original_three_way(),*original_pair.value(),
            *native.value(),owner,48000*3,true);
    QCOMPARE(joined.reason,m10::P2RealTripleBindingReason::
        missing_original_source_binding);
    QVERIFY(!joined.fragments);
    // An arbitrary callback-owned triple is NOT allowed to launder the
    // actual P2 measured candidate: all ten source stamps are required.
    auto invalid=m10::bind_p2_original_native_triple_from_real_candidate(
        {},*native.value(),owner,48000*3,true);
    QVERIFY(!invalid.fragments);
    QCOMPARE(invalid.reason,m10::P2RealTripleBindingReason::
        missing_owner_or_inconsistent_three_way_scope);
    // Instantiate the full real Product M10 scorer with an absent
    // upstream P/G triple. Even after genuine rendering, no made-up
    // M21 or partial 27-slot objective may become a cacheable score.
    const auto unbound=m10::measure_and_score_m10_p2_full_native_candidate(
        *job.value(),spec_for_authoritative_bridge,&*prefix.value(),
        "current-phase6-test-generation",true,{},
        0.0,owner,&*original_pair.value(),true,
        [](const a::M10P2FullNativeEvidence&)
            ->std::optional<m10::P2OriginalRealNativeTriple>{
            return std::nullopt;
        });
    QCOMPARE(unbound.reason,
             m10::P2NativeCostReason::missing_triple_native_authority);
    // Test the actual bound-coordinate -> Product worker -> genuine
    // P2 native scorer integration, with immutable MANUAL and exact
    // candidate REF_LINKED graph. Absent original P/G native authority
    // fails after the real render, never creates a bogus score.
    // Same full source PCM but a changed REFERENCE selection generation
    // cannot reuse a native P/G pair for a new M10 attempt/cache hit.
    QVERIFY(a::original_pair_matches_product_owner(
        *original_pair.value(),stamp));
    auto newer_reference_stamp=stamp;
    ++newer_reference_stamp.reference_selection_revision;
    QVERIFY(!a::original_pair_matches_product_owner(
        *original_pair.value(),newer_reference_stamp));
    auto newer_source_stamp=stamp;
    ++newer_source_stamp.source_generation;
    QVERIFY(!a::original_pair_matches_product_owner(
        *original_pair.value(),newer_source_stamp));
    const m10::P2NativeProductWorkerScope worker{
        stamp,prepared_owner,product_registry,*active_manual.value(),
        manual_bindings,*block.value(),true};
    const auto missing_original=[](const a::M10P2FullNativeEvidence&)
        ->std::optional<m10::P2OriginalRealNativeTriple>{
        return std::nullopt;
    };
    // The Product route requires a separately measured Fixed and
    // checks its generation before invoking the native resolver.
    bool unexpected_resolution=false;
    const auto must_not_resolve=[&](const a::M10P2FullNativeEvidence&)
        ->std::optional<m10::P2OriginalRealNativeTriple>{
        unexpected_resolution=true;return std::nullopt;
    };
    const auto preserved_owner=a::native_product_candidate_identity(stamp,"preserved");
    m10::AuthoritativeCostInput protected_input;
    auto rejected=m10::measure_and_score_preserved_terminal_product_worker(
        worker,*linked.value(),artistic,"preserved",spec_for_authoritative_bridge,
        "current-phase6-test-generation",protected_input,0.0,preserved_owner,
        &*original_pair.value(),true,must_not_resolve);
    QCOMPARE(rejected.reason,m10::P2NativeCostReason::stale_or_unattested_product);
    protected_input.independently_measured_fixed.emplace();
    auto& stale_fixed=*protected_input.independently_measured_fixed;
    stale_fixed.exact_independent_fixed_and_endpoint_safety_attested=true;
    stale_fixed.exact_fixed_ticket="fixed";
    auto stale_owner=a::native_product_candidate_identity(newer_source_stamp,"fixed");
    stale_fixed.exact_fixed_resource_identity=stale_owner.resource_identity;
    stale_fixed.exact_fixed_source_generation=stale_owner.generation;
    stale_fixed.exact_m07_generation="current-phase6-test-generation";
    rejected=m10::measure_and_score_preserved_terminal_product_worker(
        worker,*linked.value(),artistic,"preserved",spec_for_authoritative_bridge,
        "current-phase6-test-generation",protected_input,0.0,preserved_owner,
        &*original_pair.value(),true,must_not_resolve);
    QCOMPARE(rejected.reason,m10::P2NativeCostReason::stale_or_unattested_product);
    QVERIFY(!unexpected_resolution);
    // Unit-only Fixed provenance fixture; the original feature
    // resolver remains absent, so NO measured candidate is possible.
    auto current_fixed=stale_fixed;
    const auto current_fixed_owner=a::native_product_candidate_identity(stamp,"fixed");
    current_fixed.exact_fixed_resource_identity=current_fixed_owner.resource_identity;
    current_fixed.exact_fixed_source_generation=current_fixed_owner.generation;
    const auto current_fixed_graph=preserved.value()->exact_fixed_graph_content();
    QVERIFY(current_fixed_graph);
    current_fixed.exact_fixed_graph_content=*current_fixed_graph;
    m10::ExactCompletedEvaluationCache preserved_cache{{0x41}};
    const std::string generation="current-phase6-test-generation";
    const m10::PreservedNativeSearchEnvironment env{
        worker,*original_pair.value(),current_fixed,
        spec_for_authoritative_bridge,generation,preserved_cache,0.0,true};
    const m10::PreservedNativeGraph native_graph{*linked.value(),artistic};
    m10::AuthoritativeCostInput native_cost;
    native_cost.canonical_current_tuple={0x43};
    native_cost.exact_topology_evidence_identity={0x44};
    const m10::ExactExecutionCacheKey key{{0x41},{0x42},{0x43},{0x44}};
    const auto bound_key=m10::bind_preserved_native_execution_key(
        key,*preserved.value(),native_cost);
    QVERIFY(bound_key);
    QVERIFY(bound_key->candidate_execution_content!=key.candidate_execution_content);
    QCOMPARE(m10::evaluate_preserved_native_cached(
        env,native_graph,"preserved-test",key,native_cost,true,false,
        must_not_resolve).kind,m10::EvaluationKind::needs_new_attempt);
    QVERIFY(!unexpected_resolution);
    QCOMPARE(m10::evaluate_preserved_native_cached(
        env,native_graph,"preserved-test",key,native_cost,true,true,
        missing_original).kind,m10::EvaluationKind::attempted_failure);
    QCOMPARE(preserved_cache.size(),std::size_t{0});
    auto changed_key=key;
    changed_key.canonical_current_tuple={0x99};
    QCOMPARE(m10::evaluate_preserved_native_cached(
        env,native_graph,"preserved-test",changed_key,native_cost,true,true,
        must_not_resolve).kind,m10::EvaluationKind::preflight_rejected);
    QVERIFY(!unexpected_resolution);
    // Exercise the actual seed-to-refinement entry. A missing original
    // definition resolver cannot turn a physical attempt into a winner.
    const std::array<std::size_t,1> selected_ordinals{{7U}};
    const std::vector<std::vector<m10::Coordinate>> selected_inventory{{inventory[1],inventory[0]}};
    std::size_t typed_builds=0U;
    const auto build_native_trial=[&](std::size_t ordinal,const auto& canonical,
                                     const auto& u)->std::optional<m10::PreservedNativeTrial> {
        ++typed_builds;
        if(ordinal!=7U||canonical.size()!=2U||u.size()!=2U)return std::nullopt;
        return m10::PreservedNativeTrial{native_graph,key,native_cost,true};
    };
    const auto resolve_native_trial=[&](std::size_t,const a::M10P2FullNativeEvidence& m){
        return missing_original(m);
    };
    const auto no_native_winner=m10::run_preserved_native_seed_and_refinement(
        selected_ordinals,selected_inventory,0U,env,build_native_trial,resolve_native_trial);
    QCOMPARE(no_native_winner.reason,m10::RefinementBatchReason::no_complete_seed_or_stale_source);
    QCOMPARE(no_native_winner.total_attempted,std::uint32_t{1});
    QCOMPARE(typed_builds,std::size_t{1});
    QVERIFY(!no_native_winner.winning_refinement_index);
    const std::array<m10::SeedTrial,1> numerical_only{{
        {7U,m10::SeedTrialKind::attempted_complete,
         m10::MeasuredOrderKey{1.0,1.0,0.0,1U,{0x43},{0x44}},true,true}
    }};
    const auto no_native_seed=m10::run_preserved_native_top2(
        numerical_only,selected_inventory,0U,env,build_native_trial,resolve_native_trial);
    QCOMPARE(no_native_seed.reason,m10::RefinementBatchReason::invalid_inventory);
    QCOMPARE(typed_builds,std::size_t{1});
    // The selected M09 ledger is authoritative. An unavailable selected
    // entry consumes no attempt; do not replace it from another ordinal.
    namespace m09=a::m09_frozen;
    m10::BoundLegalCombinedResult selected;
    selected.status=m10::BoundLegalStatus::selected;
    selected.exact_descriptions.resize(1);
    selected.combined_consumer.status=m10::ConsumerStatus::ready;
    selected.combined_consumer.numeric_selected_indices={0U};
    selected.combined_consumer.ledger={{0U,m10::ConsumerReason::selected,true}};
    const std::array<m09::SelectedNativeSeed,1> unavailable{{
        {0U,inventory,nullptr,{}}
    }};
    bool serialized_unavailable=false;
    const auto serializer=[&](const auto&,const auto&,const auto&,const auto&)
        ->std::optional<m10::ExactExecutionCacheKey>{
        serialized_unavailable=true;return std::nullopt;
    };
    const auto unavailable_result=m09::run_selected_native_typed_search(
        selected,unavailable,0U,env,serializer,resolve_native_trial);
    QCOMPARE(unavailable_result.reason,m10::RefinementBatchReason::no_complete_seed_or_stale_source);
    QCOMPARE(unavailable_result.total_attempted,std::uint32_t{0});
    QVERIFY(!serialized_unavailable);
    auto wrong_selection=unavailable;
    wrong_selection[0].exact_combined_ordinal=1U;
    const auto invalid_selection=m09::run_selected_native_typed_search(
        selected,wrong_selection,0U,env,serializer,resolve_native_trial);
    QCOMPARE(invalid_selection.reason,m10::RefinementBatchReason::invalid_inventory);
    QVERIFY(!serialized_unavailable);
    // The production entry may not promote a provenance-only fixture
    // or a selected empty graph to NO_CORRECTION_REQUIRED.
    const a::m08_frozen::AmountConfig requested_amount{0.5,{1.0,1.0,1.0,1.0},11U};
    const std::array<bool,4> requested_domains{{true,true,true,true}};
    const auto production_unready=m09::run_current_native_product_search(
        selected,unavailable,env,requested_amount,requested_domains,true,serializer);
    QCOMPARE(production_unready.status,m09::NativeProductSearchStatus::unavailable);
    QVERIFY(!production_unready.refinement);
    QVERIFY(!serialized_unavailable);
    std::atomic<bool> cancelled{true};
    const auto production_cancelled=m09::run_current_native_product_search(
        selected,unavailable,env,requested_amount,requested_domains,true,serializer,&cancelled);
    QCOMPARE(production_cancelled.status,m09::NativeProductSearchStatus::cancelled);
    QVERIFY(!production_cancelled.refinement);
    QVERIFY(!serialized_unavailable);
    // This synthetic measured score exercises only the M12 graph
    // equality guard. It is NOT a successful native 27-feature run.
    namespace m12=a::m12_frozen;
    auto prediction=std::make_shared<m10::NativePredictionSnapshot>();
    prediction->native_population.resize(27);
    const auto winner_owner=a::native_product_candidate_identity(stamp,
        preserved.value()->ticket_identity());
    prediction->candidate_resource_identity=winner_owner.resource_identity;
    prediction->candidate_generation=winner_owner.generation;
    prediction->exact_candidate_ticket=preserved.value()->ticket_identity();
    prediction->exact_m07_generation=generation;
    auto winner_definitions=original_three_way();
    QVERIFY(m10::rebind_original_candidate_owner(winner_definitions,owner,winner_owner));
    prediction->original_definition_authority=
        std::make_shared<const m10::P2OriginalRealNativeTriple>(
            m10::retain_original_native_definitions(winner_definitions));
    prediction->exact_definition_authority_content=
        m10::exact_original_native_definition_content(*prediction->original_definition_authority);
    const auto winning_linked=preserved.value()->exact_linked_graph_content();
    const auto winning_execution=preserved.value()->exact_execution_graph_content();
    QVERIFY(winning_linked&&winning_execution);
    prediction->exact_linked_graph_content=*winning_linked;
    prediction->exact_execution_graph_content=*winning_execution;
    const m10::MeasuredOrderKey measured_fixture{1.0,1.0,0.0,1U,{0x43},{0x44},
        prediction};
    QVERIFY(m12::matches_preserved_native_winner(
        measured_fixture,*preserved.value(),generation));
    QVERIFY(!m12::matches_preserved_native_winner(
        measured_fixture,*without_p2.value(),generation));
    QVERIFY(!m12::matches_preserved_native_winner(
        measured_fixture,*preserved.value(),"different-generation"));
    const m12::AcceptedPlanAndOwner accepted_fixture{
        uuid("bf200000-0000-4000-8000-000000000001"),
        uuid("bf200000-0000-4000-8000-000000000002"),
        "unit-plan","unit-winner","unit-fixed","unit-scope",
        linked.value()->revision(),true,true,true,false};
    const auto staged=m12::stage_preserved_native_measured_winner(
        accepted_fixture,measured_fixture,worker,native_graph,generation);
    QVERIFY(staged);
    QVERIFY(!staged.value()->authorizes_project_mutation());
    auto forged_prediction=std::make_shared<m10::NativePredictionSnapshot>(*prediction);
    forged_prediction->exact_definition_authority_content.back()^=0x01;
    auto forged_winner=measured_fixture;
    forged_winner.native_prediction=std::move(forged_prediction);
    QVERIFY(!m12::stage_preserved_native_measured_winner(
        accepted_fixture,forged_winner,worker,native_graph,generation));
    auto changed_graph=native_graph;
    QVERIFY(changed_graph.linked.set_user_bypass(linked_id,true));
    QVERIFY(!m12::stage_preserved_native_measured_winner(
        accepted_fixture,measured_fixture,worker,changed_graph,generation));
    auto scope_variant=spec_for_authoritative_bridge;
    const auto native_scope=m10::exact_native_analysis_scope_content(scope_variant);
    QVERIFY(native_scope);
    auto scope_common=scope_variant;
    scope_common.source_support.profile_common_bandwidth=analysis::NativeTonalBandwidth{20.0,18000.0};
    const auto native_common=m10::exact_native_analysis_scope_content(scope_common);
    QVERIFY(native_common&&native_common!=native_scope);
    scope_common.source_support.profile_common_bandwidth->high_hz=19000.0;
    QVERIFY(native_common!=m10::exact_native_analysis_scope_content(scope_common));
    ++scope_variant.phase6_active_pcm_frames;
    QVERIFY(native_scope!=m10::exact_native_analysis_scope_content(scope_variant));
    scope_variant=spec_for_authoritative_bridge;
    scope_variant.source_support.has_width_gate=!scope_variant.source_support.has_width_gate;
    QVERIFY(native_scope!=m10::exact_native_analysis_scope_content(scope_variant));
    scope_variant=spec_for_authoritative_bridge;
    scope_variant.spectral_bands.front().high_hz+=0.125;
    QVERIFY(native_scope!=m10::exact_native_analysis_scope_content(scope_variant));
    scope_variant=spec_for_authoritative_bridge;
    scope_variant.current_full_phase6_source_and_grid=false;
    QVERIFY(!m10::exact_native_analysis_scope_content(scope_variant));
    const auto from_bound=
        m10::measure_and_score_bound_m10_p2_product_worker(
            worker,*linked.value(),artistic,*chosen,
            spec_for_authoritative_bridge,&*prefix.value(),
            "current-phase6-test-generation",{},0.0,owner,&*original_pair.value(),true,
            missing_original);
    QCOMPARE(from_bound.reason,
             m10::P2NativeCostReason::missing_triple_native_authority);
    QVERIFY(!from_bound.complete_measured());
    QVERIFY(!from_bound.as_exact_cache_evidence().
        full_current_render_analysis_hard_gates_attested);
    auto unauthenticated_worker=worker;
    unauthenticated_worker.current_qt_product_epoch_and_m09_m11_attested=
        false;
    const auto no_owner=
        m10::measure_and_score_bound_m10_p2_product_worker(
            unauthenticated_worker,*linked.value(),artistic,*chosen,
            spec_for_authoritative_bridge,&*prefix.value(),
            "current-phase6-test-generation",{},0.0,owner,&*original_pair.value(),true,
            missing_original);
    QCOMPARE(no_owner.reason,
             m10::P2NativeCostReason::stale_or_unattested_product);
    // A stricter M11 TP ceiling rejects the real candidate before
    // any callback may substitute a claimed original triple.
    const auto unsafe_bound=
        m10::measure_and_score_bound_m10_p2_product_worker(
            worker,*linked.value(),artistic,*chosen,
            spec_for_authoritative_bridge,&*prefix.value(),
            "current-phase6-test-generation",{},-90.0,owner,&*original_pair.value(),true,
            missing_original);
    QCOMPARE(unsafe_bound.reason,
             m10::P2NativeCostReason::m08_or_m11_safety_rejected);
    // The frozen refiner can now call the SAME physically measured
    // P2 evaluator through the exact-completed-result cache adapter.
    // No real three-way ORIGINAL native authority -> every attempted
    // render must be a failure, and MUST NOT pollute the cache.
    m10::ExactCompletedEvaluationCache completed_cache{{0xa1,0xb2}};
    const std::vector<double> proposed_u{0.5,0.625};
    const auto real_graph=[&](const m10::BoundM10P2Candidate&)
        ->std::optional<m10::RealP2LinkedCandidateGraph>{
        return m10::RealP2LinkedCandidateGraph{*linked.value(),artistic};
    };
    const auto exact_key=[](const m10::BoundM10P2Candidate&)
        ->std::optional<m10::ExactExecutionCacheKey>{
        return m10::ExactExecutionCacheKey{
            {0xa1,0xb2},{0xc3},{0xd4},{0xe5}};
    };
    const auto run_cached=[&](bool allowed,bool product_current) {
        auto scoped=worker;
        scoped.current_qt_product_epoch_and_m09_m11_attested=
            product_current;
        return m10::evaluate_real_m10_p2_native_cached(
            scoped,inventory,proposed_u,names,p2_id,
            "real-p2-cached-canonical-ticket",
            completed_cache,allowed,
            spec_for_authoritative_bridge,&*prefix.value(),
            "current-phase6-test-generation",{},0.0,
            control_candidate_owner,&*original_pair.value(),true,
            inverse,exact_key,
            [](const m10::BoundM10P2Candidate&){return true;},
            real_graph,missing_original);
    };
    const auto mismatched_scope=m10::P2NativeProductWorkerScope{
        newer_reference_stamp,prepared_owner,product_registry,
        *active_manual.value(),manual_bindings,*block.value(),true};
    const auto stale_cache_attempt=m10::evaluate_real_m10_p2_native_cached(
        mismatched_scope,inventory,proposed_u,names,p2_id,
        "real-p2-cached-canonical-ticket",completed_cache,true,
        spec_for_authoritative_bridge,&*prefix.value(),
        "current-phase6-test-generation",{},0.0,
        control_candidate_owner,
        &*original_pair.value(),true,inverse,exact_key,
        [](const m10::BoundM10P2Candidate&){return true;},
        real_graph,missing_original);
    QCOMPARE(stale_cache_attempt.kind,
             m10::EvaluationKind::preflight_rejected);
    QCOMPARE(completed_cache.size(),std::size_t{0});
    QCOMPARE(run_cached(true,false).kind,
             m10::EvaluationKind::preflight_rejected);
    QCOMPARE(completed_cache.size(),std::size_t{0});
    QCOMPARE(run_cached(false,true).kind,
             m10::EvaluationKind::needs_new_attempt);
    QCOMPARE(completed_cache.size(),std::size_t{0});
    QCOMPARE(run_cached(true,true).kind,
             m10::EvaluationKind::attempted_failure);
    QCOMPARE(completed_cache.size(),std::size_t{0});
    QVERIFY(!completed_cache.poisoned());
    // A real top-two M10 loop must bind the refiner's SORTED coordinate
    // order to M09 inverse + P2 trial ticket, even if M09 furnished an
    // unsorted original topology inventory. No pre-render proxy score
    // can be substituted for an absent full M07-M11 physical score.
    const std::vector<m10::SeedTrial> measured_seed_trials{{
        7U,m10::SeedTrialKind::attempted_complete,
        m10::MeasuredOrderKey{1.0,1.0,0.0,1U,{0x07},{0x70}},
        true,true
    }};
    const std::vector<std::vector<m10::Coordinate>> reverse_m09_inventory{{
        inventory[1],inventory[0]
    }};
    const std::string frozen_m07_definition{
        "current-phase6-test-generation"};
    const m10::RealP2Top2ProductEnvironment refinement_product{
        worker,*original_pair.value(),names,p2_id,completed_cache,
        spec_for_authoritative_bridge,frozen_m07_definition,
        0.0,true,true};
    std::vector<double> first_refinement_u;
    std::size_t actual_preflight_calls=0U;
    const auto full_key_unavailable=[](
        std::size_t,const m10::BoundM10P2Candidate&)
        ->std::optional<m10::ExactExecutionCacheKey>{
        return std::nullopt;
    };
    const auto valid_current=[&](
        std::size_t ordinal,const m10::BoundM10P2Candidate& bound){
        ++actual_preflight_calls;
        return ordinal==7U&&
            !bound.typed.base.ticket_identity.empty()&&
            bound.typed.base.ticket_identity.size()<=128U;
    };
    const auto no_graph=[](
        std::size_t,const m10::BoundM10P2Candidate&)
        ->std::optional<m10::RealP2LinkedCandidateGraph>{
        return std::nullopt;
    };
    const auto sterile_constraints=[&](
        std::size_t ordinal,const std::vector<double>& u)
        ->std::optional<m10::AuthoritativeCostInput> {
        if(ordinal!=7U||u.size()!=2U)return std::nullopt;
        if(first_refinement_u.empty())first_refinement_u=u;
        return m10::AuthoritativeCostInput{};
    };
    const auto no_original=[](
        std::size_t,const a::M10P2FullNativeEvidence&)
        ->std::optional<m10::P2OriginalRealNativeTriple>{
        return std::nullopt;
    };
    const auto first_refinement=
        m10::run_real_m10_p2_frozen_top2_refinements(
            measured_seed_trials,reverse_m09_inventory,0U,
            refinement_product,
            [&](std::size_t ordinal,const m10::Coordinate& coord,
                double u)->std::optional<double>{
                return ordinal==7U?inverse(coord,u):std::nullopt;
            },
            full_key_unavailable,valid_current,no_graph,
            sterile_constraints,no_original);
    QCOMPARE(first_refinement.reason,
             m10::RefinementBatchReason::complete);
    QCOMPARE(first_refinement.total_attempted,std::uint32_t{1});
    QCOMPARE(first_refinement.candidate_refinements.size(),
             std::size_t{1});
    QCOMPARE(actual_preflight_calls,std::size_t{16});
    QCOMPARE(first_refinement_u.size(),std::size_t{2});
    QCOMPARE(first_refinement_u[0],0.25);
    QCOMPARE(first_refinement_u[1],0.5);
    QVERIFY(!first_refinement.authorizes_product_publication());
    const auto before_rejection=actual_preflight_calls;
    auto unauthenticated_refinement=refinement_product;
    unauthenticated_refinement.
        exact_current_m09_m10_m11_inventory_authority_attested=false;
    const auto not_authorized=
        m10::run_real_m10_p2_frozen_top2_refinements(
            measured_seed_trials,reverse_m09_inventory,0U,
            unauthenticated_refinement,
            [&](std::size_t ordinal,const m10::Coordinate& coord,
                double u)->std::optional<double>{
                return ordinal==7U?inverse(coord,u):std::nullopt;
            },
            full_key_unavailable,valid_current,no_graph,
            sterile_constraints,no_original);
    QCOMPARE(not_authorized.reason,
             m10::RefinementBatchReason::invalid_inventory);
    QCOMPARE(actual_preflight_calls,before_rejection);
    auto duplicate_m09_inventory=reverse_m09_inventory;
    duplicate_m09_inventory[0][1]=duplicate_m09_inventory[0][0];
    const auto bad_inventory=
        m10::run_real_m10_p2_frozen_top2_refinements(
            measured_seed_trials,duplicate_m09_inventory,0U,
            refinement_product,
            [&](std::size_t ordinal,const m10::Coordinate& coord,
                double u)->std::optional<double>{
                return ordinal==7U?inverse(coord,u):std::nullopt;
            },
            full_key_unavailable,valid_current,no_graph,
            sterile_constraints,no_original);
    QCOMPARE(bad_inventory.reason,
             m10::RefinementBatchReason::invalid_inventory);
    QCOMPARE(actual_preflight_calls,before_rejection);
    QVERIFY(!from_bound.complete_measured());
    QVERIFY(!unbound.as_exact_cache_evidence().
        full_current_render_analysis_hard_gates_attested);
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

void P2RealNativeProductTest::
canonicalP2TrialTicketsCover48OrderedParameters() {
    namespace m10=a::m10_frozen;
    const std::vector<m10::Coordinate> original{
        {{0x50,0x32},{0x02},0.625},
        {{0x50,0x32},{0x01},0.5}};
    QVERIFY(!m10::m10_valid_sorted_coordinate_inventory(original));
    const auto ordered=m10::m10_sort_exact_inventory(original);
    QVERIFY(ordered);
    QCOMPARE(ordered->size(),std::size_t{2});
    QCOMPARE((*ordered)[0].stable_parameter_id,
             std::vector<std::uint8_t>({0x01}));
    const std::vector<double> aligned{0.5,0.625};
    const auto exact=m10::m10_exact_native_trial_ticket(
        7,*ordered,aligned);
    QVERIFY(exact);
    QVERIFY(exact->size()<=128U); // actual P2 typed admission limit
    const auto again=m10::m10_exact_native_trial_ticket(
        7,*ordered,aligned);
    QCOMPARE(exact,again);
    QVERIFY(exact!=m10::m10_exact_native_trial_ticket(
        9,*ordered,aligned));
    QVERIFY(exact!=m10::m10_exact_native_trial_ticket(
        7,*ordered,std::vector<double>{0.5,0.75}));
    QVERIFY(!m10::m10_exact_native_trial_ticket(
        7,original,aligned)); // unsorted gives NO misleading binding
    QVERIFY(!m10::m10_exact_native_trial_ticket(
        7,*ordered,std::vector<double>{0.5}));
    auto duplicated=*ordered;
    duplicated[1]=duplicated[0];
    QVERIFY(!m10::m10_sort_exact_inventory(duplicated));
    std::vector<m10::Coordinate> maximum_inventory;
    std::vector<double> maximum_normalized;
    for(std::size_t i=0;i<48U;++i) {
        maximum_inventory.push_back({
            {0x52,0x47,0x53},
            {static_cast<std::uint8_t>(i+1U)},
            0.5});
        maximum_normalized.push_back(0.5);
    }
    const auto maximum=m10::m10_exact_native_trial_ticket(
        31,maximum_inventory,maximum_normalized);
    QVERIFY(maximum);
    QVERIFY(maximum->size()<=128U);
    maximum_normalized[47]=0.625;
    QVERIFY(maximum!=m10::m10_exact_native_trial_ticket(
        31,maximum_inventory,maximum_normalized));
    maximum_normalized[47]=
        std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!m10::m10_exact_native_trial_ticket(
        31,maximum_inventory,maximum_normalized));
}
void P2RealNativeProductTest::adaptiveRamCacheBudgetPreservesColdRender() {
    using namespace a::m10_p2_memory;
    constexpr std::uint64_t mib=1024ULL*1024ULL;
    // The 256 MiB default used in preview is NOT a frozen
    // limitation for AutoMatch. All six Product rates, including
    // 176.4 and 192 kHz, share the same adaptive RAM policy.
    QCOMPARE(budget_from_available_ram(0U),std::size_t{0});
    QCOMPARE(budget_from_available_ram(512ULL*mib),std::size_t{0});
    QCOMPARE(budget_from_available_ram(2048ULL*mib),
             std::size_t{384ULL*mib});
    QCOMPARE(budget_from_available_ram(4096ULL*mib),
             std::size_t{896ULL*mib});
    QCOMPARE(budget_from_available_ram(32768ULL*mib),
             std::size_t{2048ULL*mib});
    QCOMPARE(budget_from_available_ram(4096ULL*mib,8U),
             std::size_t{8U});
    QCOMPARE(budget_from_available_ram(4096ULL*mib,0U),
             std::size_t{0U});
    constexpr std::uint64_t sample_bytes_5m_96k=
        5ULL*60ULL*96000ULL*2ULL*sizeof(double);
    QVERIFY(sample_bytes_5m_96k>
            static_cast<std::uint64_t>(256ULL*mib));
    QVERIFY(sample_bytes_5m_96k<
            static_cast<std::uint64_t>(
                budget_from_available_ram(4096ULL*mib)));
    // 300 s, stereo float64, 192 kHz: ~879 MiB single prefix.
    // No fake downsampling, fixed 256 MiB gate or truncated buffer.
    constexpr std::uint64_t sample_bytes_5m_176k4=
        5ULL*60ULL*176400ULL*2ULL*sizeof(double);
    constexpr std::uint64_t sample_bytes_5m_192k=
        5ULL*60ULL*192000ULL*2ULL*sizeof(double);
    QVERIFY(sample_bytes_5m_176k4>sample_bytes_5m_96k);
    QVERIFY(sample_bytes_5m_192k>sample_bytes_5m_176k4);
    QVERIFY(sample_bytes_5m_176k4>256ULL*mib);
    QVERIFY(sample_bytes_5m_176k4<
        static_cast<std::uint64_t>(
            budget_from_available_ram(8192ULL*mib)));
    QVERIFY(sample_bytes_5m_192k>256ULL*mib);
    QVERIFY(sample_bytes_5m_192k<
        static_cast<std::uint64_t>(
            budget_from_available_ram(8192ULL*mib)));
    // A machine with little available RAM must skip *cache only*.
    QVERIFY(sample_bytes_5m_192k>
        static_cast<std::uint64_t>(
            budget_from_available_ram(2048ULL*mib)));
    QVERIFY(sample_bytes_5m_192k<
        static_cast<std::uint64_t>(
            budget_from_available_ram(32768ULL*mib)));
}

void P2RealNativeProductTest::
streamingP2AndIndependentM16RenderAtAllSixRates_data() {
    QTest::addColumn<int>("sampleRate");
    QTest::newRow("44k1") << 44100;
    QTest::newRow("48k") << 48000;
    QTest::newRow("88k2") << 88200;
    QTest::newRow("96k") << 96000;
    QTest::newRow("176k4") << 176400;
    QTest::newRow("192k") << 192000;
}
void P2RealNativeProductTest::
streamingP2AndIndependentM16RenderAtAllSixRates() {
    QFETCH(int,sampleRate);
    // One second of real DSP+M16 per supported rate; cache
    // admission for five-minute 176.4/192-kHz PCM is checked above.
    // Do not allocate six full five-minute buffers in CI.
    auto basic=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(basic);
    auto opt=dsp::ModuleRegistry::with_streaming_true_peak_preview(
        std::move(*basic.value()));
    QVERIFY(opt);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*opt.value()));
    auto chain=dsp::ProcessingChain::create(*registry,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto limiter_id=id("cc000000-0000-4000-8000-000000000001");
    QVERIFY(chain.value()->add(
        limiter_id,"rgsml.dsp.streaming-true-peak-limiter",0));
    const auto params=dsp::StreamingTruePeakLimiterParameters::create(
        -1.0,3.0,2);
    QVERIFY(params);
    const std::vector<dsp::ModuleExecutionBinding> bindings{{
        limiter_id,*params.value()}};
    const auto rate=core::SampleRate::create(sampleRate);
    QVERIFY(rate);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    const auto frames=core::FrameCount::create(sampleRate);
    const auto block=core::FrameCount::create(4096);
    QVERIFY(fmt&&frames&&block);
    auto source=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(source);
    auto l=source.value()->mutable_view().channel(0);
    auto r=source.value()->mutable_view().channel(1);
    QVERIFY(l&&r);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const double v=0.2*std::sin(2.0*std::numbers::pi*440.0*
            static_cast<double>(i)/static_cast<double>(sampleRate));
        (*l.value())[i]=v;
        (*r.value())[i]=v;
    }
    const auto req=render::RenderRequest::create(
        source.value()->view(),
        source.value()->view().absolute_range(),
        *chain.value(),bindings,*block.value());
    QVERIFY(req);
    const auto rendered=render::render_preview(*req.value(),*registry);
    QVERIFY(rendered);
    QCOMPARE(rendered.value()->view().format().sample_rate().value(),
             sampleRate);
    QCOMPARE(rendered.value()->view().frame_count().value(),
             sampleRate);
    QVERIFY(!rendered.value()->signatures().empty());
    QCOMPARE(rendered.value()->signatures().back().type_id,
             std::string("rgsml.dsp.streaming-true-peak-limiter"));
    // Analyze the real post-P2 samples using independent M16.
    const auto tp=analysis::measure_true_peak_full_resource(
        rendered.value()->buffer());
    QVERIFY(tp);
    QVERIFY(std::isfinite(tp.value()->global_linear_peak()));
    QVERIFY(tp.value()->global_linear_peak()<=1.0);
}

void P2RealNativeProductTest::
canonicalAll27ScoringRequiresRealM11Protection() {
    // Fixtures here are synthetic ONLY to verify gate wiring and mass.
    // Product M10 calls the ten actual source-specific native binders.
    namespace m07=a::m07_frozen;
    namespace m08=a::m08_frozen;
    namespace m10=a::m10_frozen;
    namespace native=a::m05m06_real;
    auto slot=[&](std::size_t index) {
        const auto& b=m07::kBindings[index];
        m08::AuthenticatedPairSlot out{
            std::string{b.binding_id},std::string{b.source_feature_id},
            "unit-test-m07-digest",true,{}};
        const auto n=b.component_share==m07::ComponentShare::thirds
            ?std::size_t{3}:b.component_share==
                m07::ComponentShare::configured_n
                ?std::size_t{2}:std::size_t{1};
        for(std::size_t i=0;i<n;++i)
            out.configured.push_back({
                true,1.0,{12.0,m08::Endpoint::ordinary},
                {9.0,m08::Endpoint::ordinary},10.5,true});
        return out;
    };
    const std::array<m08::DomainAmount,4> domains{{
        {m07::kDomainIds[0],1.0},{m07::kDomainIds[1],1.0},
        {m07::kDomainIds[2],1.0},{m07::kDomainIds[3],1.0}}};
    const auto config=m08::create_amount_config(0.5,domains,11);
    QVERIFY(config.config);
    m10::AuthoritativeCostInput independent;
    independent.amount=*config.config;
    independent.m08_corrective_mask={{true,true,true,true}};
    independent.optimization_target_equals_desired_attested=true;
    independent.exact_m05_m06_source_and_m07_dependency_current=true;
    independent.m11_fixed_and_protected_snapshot_current=true;
    independent.fresh_candidate_render_and_analysis_current=true;
    independent.hard_m11_and_independent_safety_preflight_passed=true;
    independent.m11_full_k_dependency_and_order_attested=true;
    independent.active_owned_modules=2;
    independent.enabled_artistic_bands=3;
    independent.canonical_current_tuple={0x01};
    independent.exact_topology_evidence_identity={0x02};
    for(const auto& b:m07::kBindings) {
        const auto n=b.component_share==m07::ComponentShare::thirds
            ?std::size_t{3}:b.component_share==
                m07::ComponentShare::configured_n
                ?std::size_t{2}:std::size_t{1};
        for(std::size_t i=0;i<n;++i)
            independent.protected_full_k.push_back({
                true,true,true,12.0,9.0,10.5,10.5,0.5});
    }
    native::T01BindingProjection t1{
        slot(0),native::T01BindingFailure::none};
    m07::ExistingNativeSourceFragments fragments;
    fragments.tonal_primary=std::move(t1);
    // Intentionally absent 26 slots: no M10 numeric score.
    const auto incomplete=m10::score_complete_p2_native_fragment_set(
        fragments,independent,"exact-test-phase6",true);
    QCOMPARE(incomplete.reason,
             m10::P2NativeCostReason::incomplete_or_conflicting_m07);
    QCOMPARE(incomplete.m07.populated_count,std::size_t{1});
    QVERIFY(!incomplete.measured);
    QVERIFY(!incomplete.as_exact_cache_evidence().measured);
    QVERIFY(!m10::score_complete_p2_native_fragment_set(
        fragments,independent,"exact-test-phase6",false).measured);
    // Populate all original canonical feature families (no M21 values).
    std::array<m08::AuthenticatedPairSlot,5> tonal{
        slot(1),slot(2),slot(3),slot(4),slot(5)};
    fragments.tonal_scalar=native::TonalBindingProjection{
        tonal,native::TonalBindingProblem::none};
    fragments.macro_d01=native::D01BindingProjection{
        slot(6),native::D01BindingFailure::none};
    fragments.macro_lra=native::LraBindingProjection{
        slot(7),native::LraBindingReason::ready};
    fragments.macro_windowed=native::D03D04SlotProjection{
        std::array<m08::AuthenticatedPairSlot,2>{slot(8),slot(9)},
        native::D03D04SlotFailure::none};
    fragments.micro_events=native::TransientBindingProjection{
        std::array<m08::AuthenticatedPairSlot,3>{
            slot(12),slot(13),slot(14)},
        native::TransientBindingFailure::none};
    fragments.spectral_frame_energy=native::SpectralSlotProjection{
        std::array<m08::AuthenticatedPairSlot,3>{
            slot(15),slot(16),slot(17)},
        native::SpectralSlotFailure::none};
    fragments.macro_loudness=native::M19NativeSlotProjection{
        std::array<m08::AuthenticatedPairSlot,5>{
            slot(10),slot(11),slot(24),slot(25),slot(26)},
        native::M19SlotFailure::none};
    fragments.stereo_broadband=native::M20StereoProjection{
        std::array<m08::AuthenticatedPairSlot,2>{slot(18),slot(19)},
        native::M20StereoFailure::none};
    fragments.stereo_coherent_bands_and_lowend=
        native::CoherentStereoBindingProjection{
            std::array<m08::AuthenticatedPairSlot,4>{
                slot(20),slot(21),slot(22),slot(23)},
            native::CoherentStereoBindingFailure::none};
    const auto full=m10::score_complete_p2_native_fragment_set(
        fragments,independent,"exact-test-phase6",true);
    QCOMPARE(full.m07.populated_count,std::size_t{27});
    QCOMPARE(full.reason,m10::P2NativeCostReason::measured);
    QVERIFY(full.complete_measured());
    QVERIFY(full.as_exact_cache_evidence().
        full_current_render_analysis_hard_gates_attested);
    QVERIFY(!full.authorizes_m12_or_m13());
    // Independent M11/owner denial must veto, despite 27 plausible
    // features and a valid exact numerical objective.
    independent.hard_m11_and_independent_safety_preflight_passed=false;
    const auto denied=m10::score_complete_p2_native_fragment_set(
        fragments,independent,"exact-test-phase6",true);
    QCOMPARE(denied.reason,
             m10::P2NativeCostReason::m08_or_m11_safety_rejected);
    QVERIFY(!denied.measured);
    QVERIFY(!denied.as_exact_cache_evidence().
        full_current_render_analysis_hard_gates_attested);
    // A duplicated slot is rejected, never overwritten by a
    // first-wins or count-based pseudo-complete result.
    auto wrong=fragments;
    wrong.macro_loudness->feature_slots->at(0)=slot(0);
    const auto duplicate=m10::score_complete_p2_native_fragment_set(
        wrong,independent,"exact-test-phase6",true);
    QCOMPARE(duplicate.reason,
             m10::P2NativeCostReason::incomplete_or_conflicting_m07);
}

void P2RealNativeProductTest::
allNativeTripleOwnersAgreeBeforeBuildingAnyFeature() {
    namespace m10=a::m10_frozen;
    namespace native=a::m05m06_real;
    m10::P2OriginalRealNativeTriple all;
    QCOMPARE(m10::bind_all_original_real_native_p2_triple(all).reason,
             m10::P2RealTripleBindingReason::
                missing_owner_or_inconsistent_three_way_scope);
    const std::array<native::SourceIdentity,3> owners{{
        {"prepared-resource",3U,true},
        {"reference-resource",4U,true},
        {"candidate-p2-resource",5U,true}}};
    for(std::size_t i=0;i<3U;++i) {
        all.t01.actual_sources[i].owner=owners[i];
        all.t02_t06.sources[i].owner=owners[i];
        all.d01.source[i].owner=owners[i];
        all.d07_d09.current_sources[i].owner=owners[i];
        all.d10_d12.source_owner[i]=owners[i];
        all.s03_s06.independent_source_owners[i]=owners[i];
    }
    all.m19.prepared.owner=owners[0];
    all.m19.reference.owner=owners[1];
    all.m19.actual_candidate.owner=owners[2];
    all.d02.prepared_owner=owners[0];
    all.d02.reference_owner=owners[1];
    all.d02.candidate_owner=owners[2];
    all.d03_d04.prepared_owner=owners[0];
    all.d03_d04.reference_owner=owners[1];
    all.d03_d04.candidate_owner=owners[2];
    all.s01_s02.prepared.owner=owners[0];
    all.s01_s02.reference.owner=owners[1];
    all.s01_s02.actual_candidate.owner=owners[2];
    auto fresh=all;
    const native::SourceIdentity freshOwner{"new-independent-master",6U,true};
    QVERIFY(m10::rebind_original_candidate_owner(fresh,owners[2],freshOwner));
    QVERIFY(m10::exact_source_owner_equal(fresh.m19.actual_candidate.owner,freshOwner));
    QVERIFY(m10::exact_source_owner_equal(fresh.d10_d12.source_owner[2],freshOwner));
    QVERIFY(m10::exact_source_owner_equal(fresh.t01.actual_sources[0].owner,owners[0]));
    QVERIFY(!m10::rebind_original_candidate_owner(fresh,owners[2],freshOwner));
    auto wrong=all;
    ++wrong.d07_d09.current_sources[2].owner.generation;
    QVERIFY(!m10::rebind_original_candidate_owner(wrong,owners[2],freshOwner));
    QVERIFY(m10::exact_source_owner_equal(wrong.m19.actual_candidate.owner,owners[2]));
    const auto only_missing_definitions=
        m10::bind_all_original_real_native_p2_triple(all);
    QCOMPARE(only_missing_definitions.reason,
             m10::P2RealTripleBindingReason::
                missing_original_source_binding);
    QVERIFY(!only_missing_definitions.fragments);
    QVERIFY(!only_missing_definitions.authorizes_solver());
    // An independent tonal frame for a different candidate generation
    // must not be paired with the correctly stamped M19/M20 Product
    // measurement, even before any possible numeric assembly.
    ++all.t02_t06.sources[2].owner.generation;
    QCOMPARE(m10::bind_all_original_real_native_p2_triple(all).reason,
             m10::P2RealTripleBindingReason::
                missing_owner_or_inconsistent_three_way_scope);
    all.t02_t06.sources[2].owner=owners[2];
    all.d10_d12.source_owner[1].resource_identity="another-reference";
    QCOMPARE(m10::bind_all_original_real_native_p2_triple(all).reason,
             m10::P2RealTripleBindingReason::
                missing_owner_or_inconsistent_three_way_scope);
    all.d10_d12.source_owner[1]=owners[1];
    all.m19.actual_candidate.owner.resource_identity="prepared-resource";
    QCOMPARE(m10::bind_all_original_real_native_p2_triple(all).reason,
             m10::P2RealTripleBindingReason::
                missing_owner_or_inconsistent_three_way_scope);
}

void P2RealNativeProductTest::
m16ActualSafetyRejectsExcessAndInvalidCeilings() {
    namespace m10=a::m10_frozen;
    using Why=m10::P2ActualM16SafetyReason;
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(0.5,0.0).reason,
             Why::within_exact_m11_ceiling);
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(1.0,0.0).reason,
             Why::within_exact_m11_ceiling);
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(1.00001,0.0).reason,
             Why::exceeded);
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(0.9,-1.0).reason,
             Why::exceeded);
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(0.8,-1.0).reason,
             Why::within_exact_m11_ceiling);
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(0.9,0.1).reason,
             Why::invalid_independent_ceiling);
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(
        std::numeric_limits<double>::infinity(),0.0).reason,
        Why::unbound_or_invalid_source);
    QCOMPARE(m10::check_actual_p2_m16_linear_peak(
        0.0,std::numeric_limits<double>::quiet_NaN()).reason,
        Why::invalid_independent_ceiling);
    QVERIFY(!m10::check_actual_p2_m16_linear_peak(0.9,-6.0)
        .authorizes_m12_or_m13());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::P2RealNativeProductTest)
#include "test_l3_m10_p2_real_native_product_job.moc"
