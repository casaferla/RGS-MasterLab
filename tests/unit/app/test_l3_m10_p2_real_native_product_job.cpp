#include "internal/l3_m10_p2_real_native_product_job.hpp"
#include "internal/l3_product_native_one_pass.hpp"
#include "internal/l3_m05m06_owned_full_source_native_facts.hpp"
#include "internal/l3_m05m06_original_prepared_reference_pair.hpp"
#include "internal/l3_m05m06_product_reference_full_native_worker.hpp"
#include "internal/l3_m05m06_product_qt_original_pair_worker.hpp"
#include "internal/l3_m10_p2_full_native_evidence.hpp"
#include "internal/l3_m10_p2_authoritative_native_score.hpp"
#include "internal/l3_native_automatch_worker.hpp"
#include "internal/l3_m12_preserved_native_winner.hpp"
#include "internal/l3_m09_native_selected_search.hpp"
#include "internal/l3_m09_native_role_graph.hpp"
#include "internal/l3_m09_native_loudness_seed.hpp"
#include "internal/l3_m09_native_micro_seed.hpp"
#include "internal/l3_m09_native_macro_seed.hpp"
#include "internal/l3_m09_native_stereo_seed.hpp"
#include "internal/l3_m09_native_eq_atom.hpp"
#include "internal/l3_m09_native_tonal_seed.hpp"
#include "internal/l3_m09_native_dynamic_seed.hpp"
#include "internal/l3_m09_native_family_seed_bank.hpp"
#include "internal/l3_m09_native_domain_stream.hpp"
#include "internal/l3_m09_native_fixed_seed_geometry.hpp"
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
    void nativeRoleAssemblyCreatesActualTypedSeed();
    void nativeFamilyBankDoesNotSubstituteUnavailableRoles();
    void nativeDomainSelectionKeepsUnavailableNumericWinners();
    void nativeSelectedFamilyChecksExactSeedIdentity();
    void nativeLoudnessSeedUsesFixedIntegratedAndIndependentTp();
    void nativeMicroSeedUsesFixedEventTargetsAndRealC00Payload();
    void nativeMacroSeedUsesMeasuredRmsMembershipAndCoherentGroups();
    void nativeStereoSeedUsesFixedWidthAndCutoffEvidence();
    void nativeSideSeedUsesSelectedWidthGridWithoutInterpolation();
    void nativeEqAtomUsesRealBoundedFrequencyResponse();
    void nativeEqPrefixesPreserveStoredOrder();
    void nativeDynamicClustersUseRetainedEnergyAndC01Parameters();
    void nativeM20MetadataUsesSixRateGeometry();
    void nativeSeedGeometryRejectsSubstitutedStereoGrid();
    void nativeProductPairMetadataKeepsIndependentRates();
    void adaptiveRamCacheBudgetPreservesColdRender();
    void canonicalP2TrialTicketsCover48OrderedParameters();
    void streamingP2AndIndependentM16RenderAtAllSixRates_data();
    void streamingP2AndIndependentM16RenderAtAllSixRates();
    void canonicalAll27ScoringRequiresRealM11Protection();
    void allNativeTripleOwnersAgreeBeforeBuildingAnyFeature();
    void m16ActualSafetyRejectsExcessAndInvalidCeilings();
};
void P2RealNativeProductTest::nativeSeedGeometryRejectsSubstitutedStereoGrid() {
    namespace m09=a::m09_frozen;
    for(const auto rate:std::array<std::uint32_t,6>{44100,48000,88200,96000,176400,192000}) {
        for(const int denominator:{3,12}) {
            const auto grid=analysis::native_tonal_fractional_grid(rate,denominator);
            std::vector<a::m05m06_real::NativeFrameBand> bands;
            std::vector<int> indices;
            for(const auto& band:grid) {
                if(!band.fully_supported)continue;
                bands.push_back({band.band_index,band.low_hz,band.high_hz});
                indices.push_back(band.band_index);
            }
            const auto matched=m09::match_exact_native_stereo_grid(bands,indices,rate,denominator);
            QVERIFY(matched);
            QCOMPARE(matched->size(),bands.size());
            QVERIFY(!m09::match_exact_native_stereo_grid(bands,indices,rate,denominator==3?12:3));
            auto wrong=bands;wrong.front().high_hz+=0.01;
            QVERIFY(!m09::match_exact_native_stereo_grid(wrong,indices,rate,denominator));
        }
    }
}
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
void P2RealNativeProductTest::nativeEqPrefixesPreserveStoredOrder() {
    namespace m09=a::m09_frozen;
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const std::array<m09::NativeEqFitCell,2> active{{{100.0,3.0,0.5},{8000.0,-3.0,0.5}}};
    const std::array<m09::NativeEqAtom,2> dictionary{{
        {uuid("bd000000-0000-4000-8000-000000000034"),m09::AtomFilter::bell,8000.0,1.0},
        {uuid("bd000000-0000-4000-8000-000000000035"),m09::AtomFilter::bell,100.0,1.0}}};
    const auto prefixes=m09::fit_native_eq_prefixes(active,dictionary,*rate.value(),false,0.01);
    QCOMPARE(prefixes.size(),std::size_t{2});
    QCOMPARE(prefixes.back().payload.bands().size(),std::size_t{2});
    QCOMPARE(std::get<dsp::BellPayload>(prefixes.back().payload.bands().front().payload()).frequency_hz,100.0);
    QCOMPARE(std::get<dsp::BellPayload>(prefixes.back().payload.bands().back().payload()).frequency_hz,8000.0);
    auto duplicate=dictionary;
    duplicate[1].band_id=duplicate[0].band_id;
    QVERIFY(m09::fit_native_eq_prefixes(active,duplicate,*rate.value(),false,0.01).empty());
}
void P2RealNativeProductTest::nativeDynamicClustersUseRetainedEnergyAndC01Parameters() {
    namespace m09=a::m09_frozen;
    std::vector<m09::SpectralSeedCell> cells;
    for(int i=0;i<7;++i) {
        const double lo=100.0+10.0*i;
        cells.push_back({i!=3,true,true,true,lo+5.0,lo,lo+10.0,
            i<3?3.0:-1.0,0.5,1.0,i<3?2.0:-1.0,0.5});
    }
    const auto ranked=m09::rank_native_spectral_clusters(cells);
    QCOMPARE(ranked.size(),std::size_t{2});
    QCOMPARE(ranked.front().first,std::size_t{0});
    QCOMPARE(ranked.back().first,std::size_t{4});
    a::NativeSpectralSeedCarrier carrier;
    for(int i=0;i<7;++i)carrier.grid.push_back({i,100.0+10.0*i,110.0+10.0*i});
    carrier.ordinals={0,1,2,3};
    carrier.energy.assign(4,std::vector<std::optional<double>>(7,0.01));
    carrier.energy.back()[0]=std::nullopt;
    const auto threshold=m09::native_fixed_cluster_threshold(carrier,0,3);
    QVERIFY(threshold.p75_dbfs);
    QCOMPARE(threshold.common_valid_frames,std::size_t{3});
    QVERIFY(std::abs(*threshold.p75_dbfs-10.0*std::log10(0.03))<1e-12);
    for(const auto fs:std::array<std::uint32_t,6>{44100,48000,88200,96000,176400,192000}) {
        const auto band=m09::native_dynamic_seed(ranked.front().seed,threshold,
            uuid("bd000000-0000-4000-8000-000000000036"),true,false,fs,2U);
        QVERIFY(band);
        QVERIFY(band->payload.spec().filter_type==dsp::DynamicEqFilterType::LOW_SHELF);
        QCOMPARE(band->payload.spec().control.range_db,2.0);
        QCOMPARE(band->payload.spec().control.rms_time_constant_ms,50.0);
        QVERIFY(band->parameter_limited);
    }
    carrier.ordinals.back()=1;
    QVERIFY(!m09::native_fixed_cluster_threshold(carrier,0,3).p75_dbfs);
}
void P2RealNativeProductTest::nativeEqAtomUsesRealBoundedFrequencyResponse() {
    namespace m09=a::m09_frozen;
    const m09::NativeEqAtom atom{uuid("bd000000-0000-4000-8000-000000000033"),
        m09::AtomFilter::bell,1000.0,0.707};
    for(const auto fs:std::array<std::uint32_t,6>{44100,48000,88200,96000,176400,192000}) {
        const auto rate=core::SampleRate::create(fs);
        QVERIFY(rate);
        const std::array<m09::NativeEqFitCell,1> cells{{{1000.0,8.0,1.0}}};
        const auto stereo=m09::fit_native_eq_atom(atom,cells,*rate.value(),false);
        const auto side=m09::fit_native_eq_atom(atom,cells,*rate.value(),true);
        QVERIFY(stereo&&side);
        QCOMPARE(std::get<dsp::BellPayload>(stereo->payload.payload()).gain_db,3.0);
        QCOMPARE(std::get<dsp::BellPayload>(side->payload.payload()).gain_db,1.5);
        QVERIFY(m09::usable_greedy_atom(stereo->fit));
        QVERIFY(m09::usable_greedy_atom(side->fit));
        QVERIFY(std::abs(stereo->fit.atom.exact_canonical_response_at_limited_gain.front()-3.0)<1e-8);
        QVERIFY(std::abs(side->fit.atom.exact_canonical_response_at_limited_gain.front()-1.5)<1e-8);
        const std::array<m09::NativeEqFitCell,1> unavailable{{{21000.0,8.0,1.0}}};
        QVERIFY(!m09::fit_native_eq_atom(atom,unavailable,*rate.value(),false));
    }
}
void P2RealNativeProductTest::nativeSideSeedUsesSelectedWidthGridWithoutInterpolation() {
    namespace m09=a::m09_frozen;
    namespace m07=a::m07_frozen;
    namespace m08=a::m08_frozen;
    a::m11_frozen::NativeFixedPopulation fixed;
    fixed.exact_independent_fixed_and_endpoint_safety_attested=true;
    for(const auto& binding:m07::kBindings) {
        const bool side=binding.binding_id=="rgsml.reference-match.binding.stereo.s03";
        m08::AuthenticatedPairSlot slot{std::string(binding.binding_id),
            std::string(binding.source_feature_id),"fixture-only",true,{}};
        const auto n=side?3U:binding.component_share==m07::ComponentShare::thirds?3U:
            binding.component_share==m07::ComponentShare::configured_n?2U:1U;
        for(unsigned i=0;i<n;++i) {
            slot.configured.push_back({true,1.0,{0.0,m08::Endpoint::ordinary},
                {side?0.5:0.0,m08::Endpoint::ordinary},0.0,true});
            if(side) {
                slot.configured.back().candidate_native.value=1.0;
                slot.configured.back().candidate_native.selector=std::to_string(i);
            }
        }
        fixed.measured_fixed.push_back(std::move(slot));
    }
    const m08::AmountConfig amount{1.0,{1.0,1.0,1.0,1.0},1U};
    const std::array<bool,4> requested{false,false,true,false};
    std::vector<a::m05m06_real::T01ConfiguredBand> grid;
    for(int i=0;i<3;++i)grid.push_back({i,1000.0*std::exp2((i-0.5)/3.0),
        1000.0*std::exp2(i/3.0),1000.0*std::exp2((i+0.5)/3.0)});
    const auto pop=m09::native_side_fit_population(fixed,amount,requested,grid,3);
    QVERIFY(pop);
    unsigned next_band=1U;
    const auto allocate=[&](const m09::NativeEqDictionaryIdentity&)->std::optional<core::Uuid>{
        core::Uuid::Bytes bytes{};
        bytes[0]=0xbdU;bytes[15]=static_cast<std::uint8_t>(next_band++);
        return core::Uuid{bytes};
    };
    const auto full_dictionary=m09::derive_native_eq_dictionary(grid,*pop,20.0,20000.0,
        *core::SampleRate::create(48000).value(),true,allocate);
    QVERIFY(full_dictionary);
    QCOMPARE(full_dictionary->size(),std::size_t{12});
    auto missing=*pop;missing.eligible_grid[1].reset();
    const auto split_dictionary=m09::derive_native_eq_dictionary(grid,missing,20.0,20000.0,
        *core::SampleRate::create(48000).value(),true,allocate);
    QVERIFY(split_dictionary);
    QCOMPARE(split_dictionary->size(),std::size_t{8});
    for(const auto& atom:*split_dictionary)
        QVERIFY(atom.frequency_hz!=grid[1].center_hz);
    const auto automatic_bank=m09::derive_native_automatic_seed_bank(fixed,amount,requested,
        {},grid,3,20.0,20000.0,*core::SampleRate::create(48000).value(),2U,0.0,
        false,false,{},[&](auto role,const auto& key)->std::optional<core::Uuid>{
            if(role!=m09::IntentRole::stereo_side_eq)return {};
            return allocate(key);
        });
    QVERIFY(automatic_bank);
    QVERIFY(!automatic_bank->side.empty());
    QVERIFY(automatic_bank->tonal.empty());
    QVERIFY(automatic_bank->exact_side_grid.size()==grid.size());
    QCOMPARE(pop->active.size(),std::size_t{3});
    QVERIFY(std::abs(pop->active.front().residual_db-10.0*std::log10(2.0))<1e-12);
    QVERIFY(!m09::native_side_fit_population(fixed,amount,requested,grid,12));
    const std::array<m09::NativeEqAtom,1> dictionary{{
        {uuid("bd000000-0000-4000-8000-000000000053"),m09::AtomFilter::bell,grid[1].center_hz,0.707}}};
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto prefixes=m09::derive_native_side_prefixes(fixed,amount,requested,grid,3,dictionary,*rate.value());
    QVERIFY(!prefixes.empty());
    QVERIFY(prefixes.front().payload.bands().front().routing()==dsp::EqRouting::SIDE);
    const auto response=m09::exact_prefix_cell_response(prefixes.front(),grid,1,"1");
    QVERIFY(response&&*response>0.0);
    QVERIFY(!m09::exact_prefix_cell_response(prefixes.front(),grid,1,"0"));
    m09::NativeFamilySeedBank bank;
    bank.side=prefixes;bank.exact_side_grid=grid;
    m09::AbstractFamilyChoice family;family.stereo_side_eq_prefix=1U;
    const std::array<m09::NativeRoleCoordinateAuthority,1> keys{{
        {m09::IntentRole::stereo_side_eq,{0x41}}}};
    const std::array<m09::NativeBandCoordinateAuthority,1> bands{{
        {m09::IntentRole::stereo_side_eq,dictionary.front().band_id,{0x42},
         {grid.front().low_hz,grid.back().high_hz}}}};
    const auto requests=m09::derive_native_family_coordinate_requests(bank,family,keys,bands,48000);
    QVERIFY(requests);
    QCOMPARE(requests->size(),std::size_t{3});
    QVERIFY(!m09::derive_native_family_coordinate_requests(bank,family,keys,{},48000));
    const auto automatic_authorities=m09::derive_native_family_band_authorities(bank,family,
        20.0,20000.0,[&](auto role,const auto& band)->std::optional<m09::ExactBytes>{
            if(role!=m09::IntentRole::stereo_side_eq||band!=dictionary.front().band_id)return {};
            return m09::ExactBytes{0x42};
        });
    QVERIFY(automatic_authorities);
    QCOMPARE(automatic_authorities->size(),std::size_t{1});
    QCOMPARE(automatic_authorities->front().bounds.lower,grid.front().low_hz);
    QCOMPARE(automatic_authorities->front().bounds.upper,grid.back().high_hz);
    auto split_bank=bank;
    split_bank.side.front().exact_eligible_grid.front().reset();
    const auto split_bounds=m09::native_eq_band_run_bounds(split_bank.side.front(),grid,
        dictionary.front().band_id,20.0,20000.0);
    QVERIFY(split_bounds);
    QCOMPARE(split_bounds->lower,grid[1].low_hz);
    split_bank.side.front().exact_eligible_grid[1].reset();
    QVERIFY(!m09::native_eq_band_run_bounds(split_bank.side.front(),grid,
        dictionary.front().band_id,20.0,20000.0));
    for(auto& slot:fixed.measured_fixed)if(slot.binding_id=="rgsml.reference-match.binding.stereo.s03")
        slot.configured[1].candidate_native.endpoint=m08::Endpoint::mono_side_muted;
    QVERIFY(!m09::native_side_fit_population(fixed,amount,requested,grid,3));
}
void P2RealNativeProductTest::nativeStereoSeedUsesFixedWidthAndCutoffEvidence() {
    namespace m09=a::m09_frozen;
    namespace m07=a::m07_frozen;
    namespace m08=a::m08_frozen;
    a::m11_frozen::NativeFixedPopulation fixed;
    fixed.exact_independent_fixed_and_endpoint_safety_attested=true;
    for(const auto& binding:m07::kBindings) {
        const bool broad=binding.binding_id=="rgsml.reference-match.binding.stereo.s01";
        const bool low=binding.binding_id=="rgsml.reference-match.binding.stereo.s05";
        m08::AuthenticatedPairSlot slot{std::string(binding.binding_id),
            std::string(binding.source_feature_id),"fixture-only",true,{}};
        const auto n=binding.component_share==m07::ComponentShare::thirds?3U:
            binding.component_share==m07::ComponentShare::configured_n?2U:1U;
        for(unsigned i=0;i<n;++i) {
            slot.configured.push_back({true,1.0,{0.0,m08::Endpoint::ordinary},
                {broad?-0.5:low?-1.5:0.0,m08::Endpoint::ordinary},0.0,true});
            auto& native=slot.configured.back().candidate_native;
            if(broad||low) {
                native.value=1.0;
                native.selector=broad?"broadband-width":std::array<const char*,3>{"80","120","200"}[i];
            }
        }
        fixed.measured_fixed.push_back(std::move(slot));
    }
    const m08::AmountConfig amount{1.0,{1.0,1.0,1.0,1.0},1U};
    const std::array<bool,4> requested{false,false,true,false};
    const auto broad=m09::derive_native_stereo_seed(fixed,amount,requested,1U,true,true);
    QVERIFY(broad);
    QVERIFY(broad->payload.mono_bass_mode()==dsp::MonoBassMode::OFF);
    QVERIFY(std::abs(broad->payload.mid_gain_db()-5.0*std::log10(2.0))<1e-12);
    QVERIFY(std::abs(broad->payload.side_gain_db()+5.0*std::log10(2.0))<1e-12);
    const auto low=m09::derive_native_stereo_seed(fixed,amount,requested,3U,true,true);
    QVERIFY(low);
    QVERIFY(low->payload.mono_bass_mode()==dsp::MonoBassMode::LR12);
    QCOMPARE(low->payload.mono_bass_cutoff_hz(),120.0);
    QVERIFY(std::abs(low->payload.low_band_width_percent()-50.0)<1e-12);
    QVERIFY(!m09::derive_native_stereo_seed(fixed,amount,requested,3U,true,false));
    QVERIFY(!m09::derive_native_stereo_seed(fixed,amount,requested,3U,false,true));
    fixed.measured_fixed[22].configured[1].candidate_native.selector="80";
    QVERIFY(!m09::derive_native_stereo_seed(fixed,amount,requested,3U,true,true));
}
void P2RealNativeProductTest::nativeMacroSeedUsesMeasuredRmsMembershipAndCoherentGroups() {
    namespace m09=a::m09_frozen;
    namespace m07=a::m07_frozen;
    namespace m08=a::m08_frozen;
    namespace native=a::m05m06_real;
    a::m11_frozen::NativeFixedPopulation fixed;
    fixed.exact_independent_fixed_and_endpoint_safety_attested=true;
    native::D03D04NativeSeries series;
    series.problem=native::NativeWindowProblem::none;
    native::NativeMembershipIdentity membership{"D03-RMS-grid400-hop100/1"};
    for(std::int64_t i=0;i<5;++i) {
        const auto start=i*4800;
        series.complete_native_windows.push_back({start,-20.0+4.0*static_cast<double>(i),{}});
        membership.append(static_cast<std::uint64_t>(start));
        membership.append(1U);
    }
    series.valid_rms_windows=5U;
    const auto selectors=native::extract_d03_d04_type7(series);
    QVERIFY(selectors.d03_type7_rms_p95_minus_p10_db);
    fixed.measured_fixed_native_windows=series;
    for(const auto& binding:m07::kBindings) {
        const bool lra=binding.binding_id=="rgsml.reference-match.binding.dynamics.d02";
        const bool rms=binding.binding_id=="rgsml.reference-match.binding.dynamics.d03";
        const double measured=rms?*selectors.d03_type7_rms_p95_minus_p10_db:lra?10.0:0.0;
        m08::AuthenticatedPairSlot slot{std::string(binding.binding_id),
            std::string(binding.source_feature_id),"fixture-only",true,{}};
        const auto n=binding.component_share==m07::ComponentShare::thirds?3U:
            binding.component_share==m07::ComponentShare::configured_n?2U:1U;
        for(unsigned i=0;i<n;++i)
            slot.configured.push_back({true,1.0,{measured,m08::Endpoint::ordinary},
                {measured-((lra||rms)?4.0:0.0),m08::Endpoint::ordinary},measured,true});
        if(rms) {
            auto& observation=slot.configured.front().candidate_native;
            observation.value=measured;
            observation.valid_count=5U;
            observation.configured_count=5U;
            observation.selector="P95-P10";
            observation.population_identity=membership.finish();
        }
        fixed.measured_fixed.push_back(std::move(slot));
    }
    const m08::AmountConfig amount{1.0,{1.0,1.0,1.0,1.0},1U};
    const std::array<bool,4> requested{false,true,false,false};
    const auto seed=m09::derive_native_macro_seed(fixed,amount,requested,m09::MacroMode::linked_max,false);
    QVERIFY(seed);
    QCOMPARE(seed->payload.threshold_dbfs(),-11.0);
    QCOMPARE(seed->payload.ratio(),2.0);
    QCOMPARE(seed->payload.makeup_gain_db(),0.0);
    QVERIFY(seed->payload.channel_link()==dsp::CompressorChannelLink::LINKED_MAX);
    QVERIFY(!m09::derive_native_macro_seed(fixed,amount,requested,m09::MacroMode::stereo_mean,false));
    const auto stereo=m09::derive_native_macro_seed(fixed,amount,requested,m09::MacroMode::stereo_mean,true);
    QVERIFY(stereo&&stereo->payload.channel_link()==dsp::CompressorChannelLink::LINKED_MEAN);
    fixed.measured_fixed_native_windows->complete_native_windows.front().absolute_start_frame=1;
    QVERIFY(!m09::derive_native_macro_seed(fixed,amount,requested,m09::MacroMode::linked_max,false));
    fixed.measured_fixed_native_windows.reset();
    QVERIFY(!m09::derive_native_macro_seed(fixed,amount,requested,m09::MacroMode::linked_max,false));
}
void P2RealNativeProductTest::nativeMicroSeedUsesFixedEventTargetsAndRealC00Payload() {
    namespace m09=a::m09_frozen;
    namespace m07=a::m07_frozen;
    namespace m08=a::m08_frozen;
    a::m11_frozen::NativeFixedPopulation fixed;
    fixed.exact_independent_fixed_and_endpoint_safety_attested=true;
    for(const auto& binding:m07::kBindings) {
        const bool active=binding.binding_id=="rgsml.reference-match.binding.dynamics.d07"||
            binding.binding_id=="rgsml.reference-match.binding.dynamics.d08";
        m08::AuthenticatedPairSlot slot{std::string(binding.binding_id),
            std::string(binding.source_feature_id),"fixture-only",true,{}};
        const auto n=binding.component_share==m07::ComponentShare::thirds?3U:
            binding.component_share==m07::ComponentShare::configured_n?2U:1U;
        for(unsigned i=0;i<n;++i)
            slot.configured.push_back({true,1.0,{0.0,m08::Endpoint::ordinary},
                {active?4.0:0.0,m08::Endpoint::ordinary},0.0,true});
        fixed.measured_fixed.push_back(std::move(slot));
    }
    const m08::AmountConfig amount{1.0,{1.0,1.0,1.0,1.0},1U};
    const std::array<bool,4> requested{false,true,false,false};
    const auto micro=m09::derive_native_micro_seed(fixed,amount,requested,true);
    QVERIFY(micro.limited_db);
    QCOMPARE(*micro.limited_db,3.0);
    const auto attack=m09::native_micro_variant_payload(micro,m09::MicroMode::attack);
    const auto sustain=m09::native_micro_variant_payload(micro,m09::MicroMode::sustain);
    const auto balanced=m09::native_micro_variant_payload(micro,m09::MicroMode::balanced);
    QVERIFY(attack&&sustain&&balanced);
    QCOMPARE(attack->attack_amount_db(),3.0);
    QCOMPARE(attack->sustain_amount_db(),0.0);
    QCOMPARE(sustain->attack_amount_db(),0.0);
    QCOMPARE(sustain->sustain_amount_db(),-3.0);
    QCOMPARE(balanced->attack_amount_db(),1.5);
    QCOMPARE(balanced->sustain_amount_db(),-1.5);
    QVERIFY(!m09::derive_native_micro_seed(fixed,amount,requested,false).limited_db);
    QVERIFY(!m09::native_micro_variant_payload(micro,m09::MicroMode::none));
}
void P2RealNativeProductTest::nativeLoudnessSeedUsesFixedIntegratedAndIndependentTp() {
    namespace m09=a::m09_frozen;
    namespace m07=a::m07_frozen;
    namespace m08=a::m08_frozen;
    a::m11_frozen::NativeFixedPopulation fixed;
    fixed.exact_independent_fixed_and_endpoint_safety_attested=true;
    fixed.measured_m16_linear_peak=std::pow(10.0,-7.0/20.0);
    for(const auto& binding:m07::kBindings) {
        const bool integrated=binding.binding_id=="rgsml.reference-match.binding.loudness.l01";
        m08::AuthenticatedPairSlot slot{std::string(binding.binding_id),
            std::string(binding.source_feature_id),"fixture-only",true,{}};
        const auto n=binding.component_share==m07::ComponentShare::thirds?3U:
            binding.component_share==m07::ComponentShare::configured_n?2U:1U;
        for(unsigned i=0;i<n;++i)
            slot.configured.push_back({true,1.0,{-20.0,m08::Endpoint::ordinary},
                {integrated?-10.0:-20.0,m08::Endpoint::ordinary},-20.0,true});
        fixed.measured_fixed.push_back(std::move(slot));
    }
    m08::AmountConfig amount{1.0,{1.0,1.0,1.0,1.0},1U};
    const std::array<bool,4> requested{false,false,false,true};
    const auto full=m09::derive_native_loudness_gain_seed(fixed,amount,requested,-0.9);
    QVERIFY(full);
    QCOMPARE(full->raw_gain_db,10.0);
    QCOMPARE(full->payload.gain_db(),6.0);
    QVERIFY(full->seed_parameter_limited);
    QVERIFY(!full->terminal_branch_requested);
    const auto limited=m09::derive_native_loudness_gain_seed(fixed,amount,requested,-2.0);
    QVERIFY(limited&&limited->terminal_branch_requested);
    amount.global=0.5;
    const auto half=m09::derive_native_loudness_gain_seed(fixed,amount,requested,0.0);
    QVERIFY(half);
    QCOMPARE(half->payload.gain_db(),5.0);
    QVERIFY(!half->seed_parameter_limited);
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(std::move(*base.value()));
    m09::NativeFamilySeedBank bank;
    bank.gain=*half;
    m09::AbstractFamilyChoice family;
    family.loudness=m09::LoudnessMode::gain;
    const std::array<m09::NativeRoleAllocation,1> allocations{{
        {m09::IntentRole::loudness_gain,id("bd000000-0000-4000-8000-000000000061"),
         uuid("bd000000-0000-4000-8000-000000000062")}}};
    const std::array<m09::NativeRoleCoordinateRequest,1> coordinates{{
        {m09::IntentRole::loudness_gain,{0x31},{0x32},m09::NativeField::gain_db,{},{}}}};
    const auto typed=m09::build_native_family_seed(registry,bank,family,allocations,coordinates,1U,48000);
    QVERIFY(typed);
    const auto coverage=m09::native_gain_coverage(fixed,amount,requested,*typed);
    QVERIFY(coverage);
    QVERIFY((*coverage)[24].covered.front());
    auto none=*coverage;
    none[24].covered.front()=false;
    const auto initial=m09::native_covered_target_loss(fixed,amount,requested,none);
    const auto covered=m09::native_covered_target_loss(fixed,amount,requested,*coverage);
    QVERIFY(initial&&covered);
    QVERIFY(*initial>0.0);
    QCOMPARE(*covered,0.0);
    auto invalid=*coverage;
    invalid[25].covered.front()=true;
    QVERIFY(!m09::native_covered_target_loss(fixed,amount,requested,invalid));
    const m09::CandidateCarrier identity{{0x31},{},{0x32},{0x33},{"fixture"}};
    const auto bound=m09::bind_native_family_candidate(registry,bank,family,allocations,coordinates,1U,
        48000,identity,fixed,amount,requested,{});
    QVERIFY(bound&&bound->legal.pre_render_proxy.is_bound());
    const std::array<m09::NativeRoleCoordinateAuthority,1> role_keys{{
        {m09::IntentRole::loudness_gain,{0x31}}}};
    const auto auto_inventory=m09::derive_native_family_coordinate_requests(bank,family,role_keys,{},48000);
    QVERIFY(auto_inventory);
    QCOMPARE(auto_inventory->size(),std::size_t{1});
    QVERIFY(auto_inventory->front().field==m09::NativeField::gain_db);
    const auto generated=m09::bind_native_family_candidate_with_inventory(registry,bank,family,
        allocations,role_keys,{},1U,48000,identity,fixed,amount,requested,{});
    QVERIFY(generated);
    const m09::NativeFamilyBindingEnvironment binding_env{registry,bank,fixed,amount,
        1U,48000,20.0,20000.0,{}};
    m09::NativeFamilyBindingAuthority authority;
    authority.disposition=m09::NativeDomainDisposition::legal;
    authority.identity=identity;
    authority.allocations.assign(allocations.begin(),allocations.end());
    authority.role_keys.assign(role_keys.begin(),role_keys.end());
    authority.exact_reason="fixture-exact";
    authority.available_currently=false;
    const auto resolve_band=[](auto,const auto&)->std::optional<m09::ExactBytes>{return {};};
    const auto domain=m09::bind_native_domain_entry(binding_env,3U,family,authority,resolve_band);
    QVERIFY(domain.legal&&domain.legal->proxy.is_bound());
    QVERIFY(!domain.legal->available_currently);
    authority.available_currently=true;
    const auto combined_entry=m09::bind_native_family_entry(binding_env,family,authority,requested,resolve_band);
    QVERIFY(combined_entry.candidate);
    QVERIFY(combined_entry.candidate->legal.availability_currently);
    authority.role_keys.clear();
    const auto unresolved=m09::bind_native_family_entry(binding_env,family,authority,requested,resolve_band);
    QVERIFY(unresolved.disposition==m09::NativeDomainDisposition::unresolved);
    QVERIFY(!unresolved.candidate);
    QCOMPARE(generated->recipe.exact_inventory.front().normalized_u,
        bound->recipe.exact_inventory.front().normalized_u);
    QVERIFY(!m09::derive_native_family_coordinate_requests(bank,family,{},{},48000));
    QVERIFY(!bound->recipe.exact_description.seed_exact_content.empty());
    const m09::AbstractFamilyChoice no_action;
    const m09::CandidateCarrier none_identity{{0x30},{},{0x32},{0x33},{"fixture"}};
    const auto no_seed=m09::bind_native_family_candidate(registry,bank,no_action,{},{},1U,
        48000,none_identity,fixed,amount,requested,{});
    QVERIFY(no_seed);
    std::vector<m09::BoundNativeFamilyCandidate> all{*no_seed,*bound,*bound};
    QVERIFY(!m09::select_bound_native_family_batch({0x33},all,false));
    const auto batch=m09::select_bound_native_family_batch({0x33},all,true);
    QVERIFY(batch);
    QCOMPARE(batch->selected.exact_descriptions.size(),std::size_t{2});
    QCOMPARE(batch->recipes.size(),std::size_t{2});
    QCOMPARE(batch->recipes.front().ordinal,std::size_t{1});
    QVERIFY(batch->recipes.front().family.loudness==m09::LoudnessMode::gain);
    const auto selected_native=m09::bind_selected_native_families(batch->selected,batch->recipes,
        registry,bank,1U,48000,[&](const auto& graph,const auto& inventory){
            return m09::exact_family_seed_content(graph,*registry,inventory);
        });
    QVERIFY(selected_native);
    all.back().recipe.exact_description.dependency_tuple={0xff};
    QVERIFY(!m09::select_bound_native_family_batch({0x33},all,true));
    fixed.measured_fixed[24].configured.front().current_candidate_z=-15.0;
    QVERIFY(!m09::derive_native_loudness_gain_seed(fixed,amount,requested,0.0));
    fixed.measured_fixed[24].configured.front().current_candidate_z=-20.0;
    fixed.measured_m16_linear_peak.reset();
    QVERIFY(!m09::derive_native_loudness_gain_seed(fixed,amount,requested,0.0));
}
void P2RealNativeProductTest::nativeSelectedFamilyChecksExactSeedIdentity() {
    namespace m09=a::m09_frozen;
    namespace m10=a::m10_frozen;
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(std::move(*base.value()));
    const auto gain=dsp::GainParameters::create(0.0);
    QVERIFY(gain);
    m09::NativeFamilySeedBank bank;
    bank.gain=m09::NativeLoudnessGainSeed{*gain.value(),0.0,-3.0,false,false};
    m09::SelectedNativeFamilyRecipe recipe;
    recipe.exact_description={{0x11},{0x12},{0x13},{0x14},{"fixture"}};
    recipe.family.loudness=m09::LoudnessMode::gain;
    recipe.allocations.push_back({m09::IntentRole::loudness_gain,
        id("bd000000-0000-4000-8000-000000000051"),
        uuid("bd000000-0000-4000-8000-000000000052")});
    recipe.coordinates.push_back({m09::IntentRole::loudness_gain,
        {0x21},{0x22},m09::NativeField::gain_db,{},{}});
    recipe.exact_inventory.push_back({{0x21},{0x22},0.5});
    m10::BoundLegalCombinedResult selected;
    selected.status=m10::BoundLegalStatus::selected;
    selected.exact_descriptions.push_back(recipe.exact_description);
    selected.combined_consumer.status=m10::ConsumerStatus::ready;
    selected.combined_consumer.numeric_selected_indices={0U};
    selected.combined_consumer.ledger={{0U,m10::ConsumerReason::selected,false}};
    const std::array<m09::SelectedNativeFamilyRecipe,1> recipes{recipe};
    const auto matching=[](const auto&,const auto&)->std::optional<m09::ExactBytes>{
        return m09::ExactBytes{0x12};};
    const auto mismatching=[](const auto&,const auto&)->std::optional<m09::ExactBytes>{
        return m09::ExactBytes{0x99};};
    const auto bound=m09::bind_selected_native_families(selected,recipes,registry,bank,1U,48000,matching);
    QVERIFY(bound&&bound->front().typed);
    const auto& typed=*bound->front().typed;
    const auto inventory=m09::native_seed_inventory(typed);
    QVERIFY(inventory);
    const auto bytes=m09::exact_family_seed_content(typed.seed,*registry,*inventory);
    QVERIFY(bytes);
    auto exact_description=recipe.exact_description;
    exact_description.seed_exact_content=*bytes;
    const std::array<double,1> u{0.5},v{0.75};
    const auto key=m09::exact_family_trial_key(exact_description,typed.seed,*registry,*inventory,u);
    QVERIFY(key);
    const auto decoded=m09::decode_native_typed_seed(typed,*inventory,v);
    QVERIFY(decoded);
    const auto changed=m09::exact_family_trial_key(exact_description,decoded->graph,*registry,*inventory,v);
    QVERIFY(changed);
    QVERIFY(changed->canonical_current_tuple!=key->canonical_current_tuple);
    // Structural reconstruction fixture only: this is not a measured
    // Product prediction and cannot pass the independent M12 owner gate.
    m10::RefinementBatchResult refinement;
    refinement.reason=m10::RefinementBatchReason::complete;
    refinement.winning_refinement_index=0U;
    m10::RefinedSeed refined;
    refined.exact_combined_ordinal=0U;
    refined.numerical.coordinates=*inventory;
    refined.numerical.coordinates.front().normalized_u=v.front();
    refined.numerical.winner.canonical_current_tuple=changed->canonical_current_tuple;
    refined.numerical.winner.topology_evidence_identity=changed->topology_evidence_identity;
    const auto ticket=m10::m10_exact_native_trial_ticket(0U,*inventory,v);
    QVERIFY(ticket);
    auto prediction=std::make_shared<m10::NativePredictionSnapshot>();
    prediction->exact_candidate_ticket=*ticket;
    const auto graph_bytes=m10::exact_native_graph_content(decoded->graph.linked,decoded->graph.bindings,*registry);
    QVERIFY(graph_bytes);
    prediction->exact_linked_graph_content=*graph_bytes;
    refined.numerical.winner.native_prediction=prediction;
    refinement.candidate_refinements.push_back(refined);
    auto reconstruction_selection=selected;
    reconstruction_selection.exact_descriptions.front()=exact_description;
    const std::array<m09::SelectedNativeSeed,1> reconstruction_seeds{{
        {0U,*inventory,&typed,{}}}};
    const auto serialize=[&](const auto& description,const auto& graph,const auto& coords,const auto& values){
        return m09::exact_family_trial_key(description,graph,*registry,coords,values);
    };
    const auto reconstructed=m09::reconstruct_selected_native_winner(reconstruction_selection,
        reconstruction_seeds,refinement,serialize);
    QVERIFY(reconstructed);
    QCOMPARE(reconstructed->immutable_seed_inventory.front().normalized_u,0.5);
    QCOMPARE(reconstructed->winning_values.front(),0.75);
    QCOMPARE(std::get<dsp::GainParameters>(reconstructed->graph.bindings.front().parameters).gain_db(),3.0);
    refinement.candidate_refinements.front().numerical.coordinates.front().normalized_u=0.625;
    QVERIFY(!m09::reconstruct_selected_native_winner(reconstruction_selection,
        reconstruction_seeds,refinement,serialize));

    QCOMPARE(changed->topology_evidence_identity,key->topology_evidence_identity);
    QCOMPARE(changed->immutable_generation_and_policy,key->immutable_generation_and_policy);
    auto exact_recipes=recipes;
    exact_recipes.front().exact_description=exact_description;
    auto exact_selected=selected;
    exact_selected.exact_descriptions.front()=exact_description;
    const auto codec=[&](const auto& graph,const auto& coords){
        return m09::exact_family_seed_content(graph,*registry,coords);};
    QVERIFY(m09::bind_selected_native_families(exact_selected,exact_recipes,registry,bank,1U,48000,codec));
    QVERIFY(!m09::bind_selected_native_families(selected,recipes,registry,bank,1U,48000,mismatching));
    selected.combined_consumer.ledger.front().selected_but_unavailable=true;
    const auto absent=m09::bind_selected_native_families(selected,recipes,registry,bank,1U,48000,mismatching);
    QVERIFY(absent&&!absent->front().typed);
    auto stale=recipes;
    stale.front().exact_description.dependency_tuple={0xff};
    QVERIFY(!m09::bind_selected_native_families(selected,stale,registry,bank,1U,48000,matching));
}
void P2RealNativeProductTest::nativeDomainSelectionKeepsUnavailableNumericWinners() {
    namespace m09=a::m09_frozen;
    namespace m10=a::m10_frozen;
    m09::NativeFamilySeedBank bank;
    const auto streams=m09::enumerate_native_domain_families(bank,true);
    QVERIFY(streams);
    QCOMPARE((*streams)[0].size(),std::size_t{1});
    QCOMPARE((*streams)[1].size(),std::size_t{12});
    QCOMPARE((*streams)[2].size(),std::size_t{8});
    QCOMPARE((*streams)[3].size(),std::size_t{4});
    const std::array<bool,4> requested{false,true,false,false};
    std::size_t ordinal=0;
    const auto selected=m09::select_native_domain_families(*streams,requested,
        [&](std::size_t d,const auto& family){
            m09::NativeDomainEntry e;e.family=family;
            if(d!=1U)return e;
            const auto i=ordinal++;
            e.disposition=m09::NativeDomainDisposition::legal;
            e.exact_reason="fixture-bound";
            m09::ProxyOperands proxy;
            proxy.exact_m05_m08_and_m10_operands_attested=true;
            proxy.frozen_covered_target_loss=static_cast<double>(i);
            proxy.m10_seed_normalized_parameter_displacement=0.0;
            proxy.m10_topology_complexity=0.0;
            proxy.canonical_unsigned_topology_key={static_cast<std::uint8_t>(i+1U)};
            e.legal=m10::ConsumerInput{m09::make_pre_render_proxy(proxy),i!=0U};
            return e;
        });
    QVERIFY(selected.ready);
    QCOMPARE(selected.numeric[1].numeric_selected_indices.size(),std::size_t{8});
    QVERIFY(selected.numeric[1].ledger.front().selected_but_unavailable);
    const auto combined=m09::enumerate_retained_domain_cartesian(selected);
    QVERIFY(combined);
    QCOMPARE(combined->size(),std::size_t{8});
    QVERIFY(m09::same_abstract_family(combined->front(),{}));
    auto unresolved=m09::bind_retained_native_cartesian(selected,[](const auto& family){
        return m09::NativeCombinedEntry{family,m09::NativeDomainDisposition::unresolved,{},"missing-fixture"};
    });
    QVERIFY(!unresolved.complete);
    QCOMPARE(unresolved.ledger.size(),std::size_t{8});
    QVERIFY(unresolved.legal.empty());
    const auto incomplete=m09::select_native_domain_families(*streams,requested,[](auto,const auto& family){
        return m09::NativeDomainEntry{family,m09::NativeDomainDisposition::unresolved,{},"missing-fixture"};
    });
    QVERIFY(!incomplete.ready);
    QVERIFY(!m09::enumerate_retained_domain_cartesian(incomplete));
}
void P2RealNativeProductTest::nativeFamilyBankDoesNotSubstituteUnavailableRoles() {
    namespace m09=a::m09_frozen;
    const auto gain=dsp::GainParameters::create(2.0);
    QVERIFY(gain);
    m09::NativeFamilySeedBank bank;
    bank.gain=m09::NativeLoudnessGainSeed{*gain.value(),2.0,-3.0,false,false};
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(std::move(*base.value()));
    m09::AbstractFamilyChoice family;
    family.loudness=m09::LoudnessMode::gain;
    const std::array<m09::NativeRoleAllocation,1> allocations{{
        {m09::IntentRole::loudness_gain,id("bd000000-0000-4000-8000-000000000041"),
         uuid("bd000000-0000-4000-8000-000000000042")}}};
    const std::array<m09::NativeRoleCoordinateRequest,1> coordinates{{
        {m09::IntentRole::loudness_gain,{0x11},{0x12},m09::NativeField::gain_db,{},{}}}};
    const auto seed=m09::build_native_family_seed(registry,bank,family,allocations,coordinates,1U,48000);
    QVERIFY(seed);
    QCOMPARE(seed->seed.linked.instances().size(),std::size_t{1});
    bank.gain.reset();
    QVERIFY(!m09::build_native_family_seed(registry,bank,family,allocations,coordinates,1U,48000));
    family.loudness=m09::LoudnessMode::terminal_limiter_only;
    QVERIFY(!m09::native_family_role_payload(bank,family,m09::IntentRole::terminal_limiter));
    family.tonal_eq_prefix=1U;
    QVERIFY(!m09::native_family_role_payload(bank,family,m09::IntentRole::tonal_eq));
}
void P2RealNativeProductTest::nativeRoleAssemblyCreatesActualTypedSeed() {
    namespace m09=a::m09_frozen;
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    auto registry=std::make_shared<const dsp::ModuleRegistry>(std::move(*base.value()));
    m09::AbstractFamilyChoice family;
    family.loudness=m09::LoudnessMode::gain;
    const auto gain=dsp::GainParameters::create(2.0);
    QVERIFY(gain);
    const std::vector<m09::NativeRoleSeed> roles{{
        m09::IntentRole::loudness_gain,id("bd000000-0000-4000-8000-000000000021"),
        uuid("bd000000-0000-4000-8000-000000000022"),*gain.value()}};
    const std::vector<m09::NativeRoleCoordinateRequest> coordinates{{
        m09::IntentRole::loudness_gain,{0x01},{0x02},m09::NativeField::gain_db,{},{}}};
    for(const auto rate:std::array<std::int64_t,6>{44100,48000,88200,96000,176400,192000}) {
        const auto seed=m09::assemble_native_artistic_typed_seed(registry,family,roles,coordinates,1U,rate);
        QVERIFY(seed);
        QCOMPARE(seed->seed.linked.instances().size(),std::size_t{1});
        QVERIFY(seed->seed.linked.instances().front().owner()==dsp::ModuleOwner::REFERENCE_MATCH);
        const auto inventory=m09::native_seed_inventory(*seed);
        QVERIFY(inventory);
        const std::array<double,1> proposal{0.75};
        const auto actual=m09::decode_native_typed_seed(*seed,*inventory,proposal);
        QVERIFY(actual);
        QCOMPARE(std::get<dsp::GainParameters>(actual->graph.bindings.front().parameters).gain_db(),3.0);
    }
    QVERIFY(!m09::assemble_native_artistic_typed_seed(registry,family,roles,coordinates,1U,32000));
    auto wrong=family;
    wrong.loudness=m09::LoudnessMode::gain_with_terminal_limiter;
    QVERIFY(!m09::assemble_native_artistic_typed_seed(registry,wrong,roles,coordinates,1U,48000));
    auto wrong_roles=roles;
    wrong_roles.front().role=m09::IntentRole::macro_compressor;
    QVERIFY(!m09::assemble_native_artistic_typed_seed(registry,family,wrong_roles,coordinates,1U,48000));
    auto wrong_coordinates=coordinates;
    wrong_coordinates.front().role=m09::IntentRole::tonal_eq;
    QVERIFY(!m09::assemble_native_artistic_typed_seed(registry,family,roles,wrong_coordinates,1U,48000));
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
    QVERIFY(!own_gate_native.value()->spectral_seed_carrier);
    auto retain_gate_spec=own_gate_spec;
    retain_gate_spec.retain_native_spectral_energy=true;
    const auto retained_native=a::measure_real_full_owned_pcm_native_once(*source.value(),retain_gate_spec);
    QVERIFY(retained_native);
    QVERIFY(retained_native.value()->spectral_seed_carrier);
    const auto& carrier=*retained_native.value()->spectral_seed_carrier;
    QCOMPARE(carrier.ordinals,retained_native.value()->selected_original_m20_ordinals);
    QCOMPARE(carrier.energy.size(),carrier.ordinals.size());
    QCOMPARE(carrier.grid.size(),retain_gate_spec.spectral_bands.size());
    QCOMPARE(retained_native.value()->tonal.mean_linear_psd,own_gate_native.value()->tonal.mean_linear_psd);
    for(const auto& row:carrier.energy)QCOMPARE(row.size(),carrier.grid.size());
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
        triple.t02_t06.exact_common={20.0,18000.0};
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
    auto mismatched_profile_definition=*resolved;
    mismatched_profile_definition.t02_t06.exact_common.high_hz=19000.0;
    QVERIFY(!m10::rebind_independent_original_native_pair(
        mismatched_profile_definition,*original_pair.value(),resolved_owner));
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
    std::size_t fixed_definition_requests=0U;
    const auto absent_fixed_definitions=[&](const a::M10P2FullNativeEvidence&)
        ->std::optional<m10::P2OriginalRealNativeTriple>{
        ++fixed_definition_requests;return {};
    };
    const auto independent_fixed=m10::measure_current_product_fixed(worker,*original_pair.value(),
        spec_for_authoritative_bridge,"current-phase6-test-generation","fixed-before-m09",
        1U,true,true,absent_fixed_definitions);
    QVERIFY(!independent_fixed); // real render cannot invent absent definitions
    QCOMPARE(fixed_definition_requests,std::size_t{1});
    auto stale_worker=worker;
    stale_worker.current_qt_product_epoch_and_m09_m11_attested=false;
    QVERIFY(!m10::measure_current_product_fixed(stale_worker,*original_pair.value(),
        spec_for_authoritative_bridge,"current-phase6-test-generation","fixed-before-m09",
        1U,true,true,absent_fixed_definitions));
    QCOMPARE(fixed_definition_requests,std::size_t{1});
    const a::NativeAutoMatchWorkerInputs native_worker_input{
        worker,*original_pair.value(),spec_for_authoritative_bridge,
        "current-phase6-test-generation",{0x41},"fixed-worker-entry",1U,
        {1.0,{1.0,1.0,1.0,1.0},1U},{false,false,false,true},0.0,
        true,true,false,false,{},{}};
    bool unexpected_generation=false;
    const auto allocate_band=[&](auto,const auto&)->std::optional<core::Uuid>{
        unexpected_generation=true;return {};
    };
    const auto resolve_authority=[&](const auto&,auto,const auto&){
        unexpected_generation=true;return a::m09_frozen::NativeFamilyBindingAuthority{};
    };
    const auto resolve_band_identity=[&](auto,const auto&)->std::optional<a::m09_frozen::ExactBytes>{
        unexpected_generation=true;return {};
    };
    std::atomic<bool> worker_cancelled_flag{true};
    const auto cancelled_worker=a::run_native_automatch_worker(native_worker_input,
        absent_fixed_definitions,allocate_band,resolve_authority,resolve_band_identity,&worker_cancelled_flag);
    QVERIFY(cancelled_worker.phase==a::NativeAutoMatchWorkerPhase::cancelled);
    QCOMPARE(fixed_definition_requests,std::size_t{1});
    const auto unready_worker=a::run_native_automatch_worker(native_worker_input,
        absent_fixed_definitions,allocate_band,resolve_authority,resolve_band_identity);
    QVERIFY(unready_worker.phase==a::NativeAutoMatchWorkerPhase::unavailable_fixed);
    QCOMPARE(fixed_definition_requests,std::size_t{2});
    QVERIFY(!unexpected_generation);
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
    const std::vector<m10::Coordinate> empty;
    QVERIFY(m10::m10_valid_sorted_coordinate_inventory(empty));
    const auto sorted_empty=m10::m10_sort_exact_inventory(empty);
    QVERIFY(sorted_empty&&sorted_empty->empty());
    const auto none=m10::m10_exact_native_trial_ticket(7,empty,{});
    QVERIFY(none);
    QVERIFY(none!=exact);
    QVERIFY(none!=m10::m10_exact_native_trial_ticket(9,empty,{}));
    QVERIFY(!m10::m10_exact_native_trial_ticket(7,empty,std::vector<double>{0.5}));
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
