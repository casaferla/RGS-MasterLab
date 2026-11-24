#include "internal/l3_m05m06_real_analysis_port.hpp"
#include "internal/l3_m05_real_tonal_statistics.hpp"
#include "internal/l3_m06_real_short_term_shape.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
class M05M06RealPortTest final:public QObject {
    Q_OBJECT
private slots:
    void actualM19M16M20FullResourceAndFrozenFeatureReadiness();
    void pairedT1RequiresSameSupportedCanonicalPopulation();
    void malformedOwnerScopeAndUnavailableCannotFabricateM10();
    void realM20PsdT3T5AndIndependentFlatDensityOracle();
    void realNativeM19ShortTermP50PairedSupportAndNoReanalysis();
    void frozenD01Type7AndThreeComponentMassNativeOracles();
    void frozenT2T6ExternalFloorAndUnweightedOlsOracle();
    void nativePopulationRequiresExactSelectedOrdinals();
};
[[nodiscard]] core::Result<audio::AudioBuffer> example(
    double scale,double harmonic,int seconds=1) {
    auto fs=core::SampleRate::create(48000);
    auto format=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    auto frames=core::FrameCount::create(48000*seconds);
    auto source=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    if(!source)return source;
    auto l=source.value()->mutable_view().channel(0);
    auto r=source.value()->mutable_view().channel(1);
    for(std::size_t i=0;i<l.value()->size();++i) {
        const double t=static_cast<double>(i);
        const double a=std::sin(
            2.0*std::numbers::pi*440.0*t/48000.0);
        const double b=std::sin(
            2.0*std::numbers::pi*1100.0*t/48000.0);
        (*l.value())[i]=scale*(0.7*a+harmonic*b);
        (*r.value())[i]=scale*(0.64*a-0.8*harmonic*b);
    }
    return source;
}
void M05M06RealPortTest::nativePopulationRequiresExactSelectedOrdinals()
{
    auto pcm=example(0.4,0.15);
    QVERIFY(pcm);
    const auto levels=analysis::measure_basic_level_full_resource(*pcm.value());
    const auto tonal=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},pcm.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(levels&&tonal);
    const auto without=real::import_actual_full_source(
        *levels.value(),*tonal.value(),{"native-test",1,true});
    QVERIFY(without.source);
    QVERIFY(!without.source->full_pcm_population_identity.empty());
    QVERIFY(without.source->selected_spectral_population_identity.empty());
    auto fixture=*tonal.value();
    // Synthetic exact ordinals exercise only the import contract.
    fixture.complete_spectral_windows=4;
    fixture.selected_spectral_windows=2;
    fixture.spectral_all_frames_fallback=false;
    const std::array<std::uint64_t,2> a{{0,2}},b{{1,3}},bad{{2,2}};
    const auto first=real::import_actual_full_source(
        *levels.value(),fixture,{"native-test",1,true},a);
    const auto second=real::import_actual_full_source(
        *levels.value(),fixture,{"native-test",2,true},b);
    QVERIFY(first.source&&second.source);
    QCOMPARE(first.source->full_pcm_population_identity,
             second.source->full_pcm_population_identity);
    QVERIFY(!first.source->selected_spectral_population_identity.empty());
    QVERIFY(first.source->selected_spectral_population_identity!=
            second.source->selected_spectral_population_identity);
    QVERIFY(!real::import_actual_full_source(
        *levels.value(),fixture,{"native-test",1,true},bad).source);
}
void M05M06RealPortTest::
actualM19M16M20FullResourceAndFrozenFeatureReadiness() {
    auto p=example(0.40,0.15),g=example(0.57,0.30);
    QVERIFY(p&&g);
    auto lp=analysis::measure_basic_level_full_resource(*p.value());
    auto lg=analysis::measure_basic_level_full_resource(*g.value());
    QVERIFY(lp&&lg);
    auto tp=analysis::measure_tonal_stereo_scope(
        p.value()->view(),core::FrameIndex{0},
        p.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    auto tg=analysis::measure_tonal_stereo_scope(
        g.value()->view(),core::FrameIndex{0},
        g.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(tp&&tg);
    const auto source=real::import_actual_full_source(
        *lp.value(),*tp.value(),{"SOURCE-real-prepared",7,true});
    const auto gold=real::import_actual_full_source(
        *lg.value(),*tg.value(),{"REFERENCE-independent-gold",9,true});
    QVERIFY(source.source&&gold.source);
    QCOMPARE(source.problem,real::ImportProblem::none);
    QVERIFY(source.source->integrated_lufs.ready());
    QVERIFY(gold.source->integrated_lufs.ready());
    QVERIFY(source.source->broadband_width.ready());
    QVERIFY(gold.source->broadband_width.ready());
    QVERIFY(source.source->k_weighting_bank_member_id.size()>0U);
    QVERIFY(source.source->k_weighting_bank_sha256.size()>0U);
    QCOMPARE(source.source->scope_frame_count,std::int64_t{48000});
    QCOMPARE(source.source->sample_rate_hz,std::int64_t{48000});
    QCOMPARE(source.source->channels,std::size_t{2});
    const auto signedGain=
        *gold.source->integrated_lufs.unrounded-
        *source.source->integrated_lufs.unrounded;
    QVERIFY(signedGain>1.0);
    // M16's public gate may still say SUPPORTED_UNQUALIFIED. Preserve
    // that status; a numeric dBTP alone is not frozen M05 eligibility.
    if(lp.value()->true_peak_db_tp.state==
       analysis::BasicLevelValueState::M16_SUPPORTED_UNQUALIFIED) {
        QVERIFY(source.source->true_peak_db_tp.state==
                real::Readiness::unqualified);
        QVERIFY(!source.source->true_peak_db_tp.ready());
    } else if(lp.value()->true_peak_db_tp.state==
              analysis::BasicLevelValueState::AVAILABLE) {
        QVERIFY(source.source->true_peak_db_tp.ready());
    }
    const auto paired=real::pair_actual_sources(
        *source.source,*gold.source);
    QVERIFY(paired.pair);
    QVERIFY(paired.pair->integrated_lufs.ready());
    QVERIFY(paired.pair->broadband_width.ready());
    QVERIFY(paired.pair->broadband_width_z.ready());
    QVERIFY(paired.pair->global_crest_db.ready());
    QVERIFY(paired.pair->correlation_pair_support);
    QVERIFY(*paired.pair->correlation_pair_support>=0.0);
    QVERIFY(*paired.pair->correlation_pair_support<=1.0);
    QVERIFY(!paired.pair->full_m05m06_objective_ready());
    // Without externally supplied Phase6 eligibility, M20 correctly
    // refuses all T1 normalized fractions. No percentage synthesis.
    for(const auto& b:paired.pair->profile_1_12)
        QVERIFY(!b.power_fraction_db.ready());
    QVERIFY(!paired.pair->profile_1_12.empty());
    QVERIFY(paired.pair->profile_1_12.front().canonical_index<0);
}
void M05M06RealPortTest::
pairedT1RequiresSameSupportedCanonicalPopulation() {
    auto p=example(0.4,0.15),g=example(0.57,0.30);
    QVERIFY(p&&g);
    auto lp=analysis::measure_basic_level_full_resource(*p.value());
    auto lg=analysis::measure_basic_level_full_resource(*g.value());
    QVERIFY(lp&&lg);
    // This test supplies an explicitly synthetic Phase6 eligibility
    // fixture. Production will consume the independently authenticated
    // eligibility vector. Tests MUST NOT claim source qualification.
    auto measured=analysis::measure_tonal_stereo_scope(
        p.value()->view(),core::FrameIndex{0},
        p.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(measured);
    std::vector<std::uint8_t> eligibility(
        measured.value()->primary_bands_1_12.size(),0);
    std::size_t eligible=0;
    for(std::size_t i=0;i<eligibility.size();++i) {
        const auto& b=measured.value()->primary_bands_1_12[i];
        if(b.fully_supported&&
           ((b.center_hz>400&&b.center_hz<500)
            ||(b.center_hz>1000&&b.center_hz<1200))) {
            eligibility[i]=1;
            ++eligible;
        }
    }
    QVERIFY(eligible>=2);
    analysis::TonalStereoTrustedSupport supplied;
    supplied.authoritative_band_eligibility=eligibility;
    auto tp=analysis::measure_tonal_stereo_scope(
        p.value()->view(),core::FrameIndex{0},
        p.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,supplied);
    auto tg=analysis::measure_tonal_stereo_scope(
        g.value()->view(),core::FrameIndex{0},
        g.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,supplied);
    QVERIFY(tp&&tg);
    auto pa=real::import_actual_full_source(
        *lp.value(),*tp.value(),{"p-fixture",10,true});
    auto ga=real::import_actual_full_source(
        *lg.value(),*tg.value(),{"g-fixture",11,true});
    QVERIFY(pa.source&&ga.source);
    auto pair=real::pair_actual_sources(*pa.source,*ga.source);
    QVERIFY(pair.pair);
    std::size_t available=0;
    for(const auto& band:pair.pair->profile_1_12) {
        if(!band.power_fraction_db.ready())continue;
        ++available;
        const double fracP=std::pow(
            10.0,*band.power_fraction_db.candidate.unrounded/10.0);
        const double fracG=std::pow(
            10.0,*band.power_fraction_db.reference.unrounded/10.0);
        QVERIFY(fracP>0.0&&fracG>0.0);
    }
    QCOMPARE(available,eligible);
    QVERIFY(!pair.pair->full_m05m06_objective_ready());
    auto altered=*ga.source;
    for(auto& band:altered.profile_1_12) {
        if(band.power_fraction_db.ready()) {
            band.power_fraction_db={{},real::Readiness::missing};
            break;
        }
    }
    const auto rejected=real::pair_actual_sources(
        *pa.source,altered);
    QVERIFY(!rejected.pair);
    QCOMPARE(rejected.problem,
             real::PairProblem::incompatible_measurement_geometry);
}
void M05M06RealPortTest::
realM20PsdT3T5AndIndependentFlatDensityOracle() {
    analysis::TonalStereoAnalysisResult constant;
    constant.sample_rate_hz=48000;
    constant.complete_spectral_windows=1;
    constant.spectrum_state=analysis::TonalStereoMetricState::AVAILABLE;
    constant.mean_linear_psd.assign(2049,1.0); // Nfft=4096
    const auto uniform=real::observe_m05_t3t5(
        constant,{100.0,10000.0});
    QVERIFY(uniform);
    // Constant power/Hz: E=9900, continuous centroid=5050Hz;
    // quantiles 100+0.85*9900=8515 and 100+0.95*9900=9505 Hz.
    QVERIFY(std::abs(uniform->energy-9900.0)<1e-9);
    QVERIFY(std::abs(uniform->centroid_hz-5050.0)<1e-9);
    QVERIFY(std::abs(uniform->rolloff85_hz-8515.0)<1e-9);
    QVERIFY(std::abs(uniform->rolloff95_hz-9505.0)<1e-9);
    QVERIFY(std::abs(uniform->centroid_log2_hz-
                     std::log2(5050.0))<1e-12);
    QVERIFY(!real::observe_m05_t3t5(
        constant,{100.0,20100.0}));
    QVERIFY(!real::observe_m05_t3t5(
        constant,{10000.0,100.0}));
    auto broken=constant;
    broken.mean_linear_psd[20]=-1.0;
    QVERIFY(!real::observe_m05_t3t5(
        broken,{100.0,10000.0}));
    broken=constant;
    broken.mean_linear_psd.assign(2049,0.0);
    QVERIFY(!real::observe_m05_t3t5(
        broken,{100.0,10000.0})); // no E>=1e-12

    auto realPcm=example(0.45,0.2);
    QVERIFY(realPcm);
    const auto measured=analysis::measure_tonal_stereo_scope(
        realPcm.value()->view(),core::FrameIndex{0},
        realPcm.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(measured);
    const auto observed=real::observe_m05_t3t5(
        *measured.value(),{100.0,10000.0});
    QVERIFY(observed);
    QVERIFY(observed->centroid_hz>=100.0);
    QVERIFY(observed->centroid_hz<=10000.0);
    QVERIFY(observed->rolloff85_hz<=observed->rolloff95_hz);
    // Numerical observation alone never qualifies the source for
    // the M05 >=2s reference-grade/eligible M07 producer.
}
void M05M06RealPortTest::
frozenT2T6ExternalFloorAndUnweightedOlsOracle() {
    auto pcm=example(0.41,0.22);
    QVERIFY(pcm);
    auto actual=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{0},
        pcm.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(actual);
    auto synthetic=*actual.value();
    // Independent analytical density oracle on authentic M20
    // 1/12 geometry. Exactly 2.5 dB per log2 octave is inserted;
    // this is not an assertion of source-grade qualification.
    for(auto& band:synthetic.primary_bands_1_12) {
        const double x=std::log2(band.center_hz/1000.0);
        const double density=std::pow(10.0,(2.5*x)/10.0);
        band.measured_linear_energy=
            density*(band.high_hz-band.low_hz);
    }
    const auto tilt=real::observe_m05_t2_or_t6(
        synthetic,{20.0,20000.0},1e-12,false);
    const auto high=real::observe_m05_t2_or_t6(
        synthetic,{20.0,20000.0},1e-12,true);
    QVERIFY(tilt&&high);
    QVERIFY(std::abs(tilt->slope_db_per_octave-2.5)<1e-11);
    QVERIFY(std::abs(high->slope_db_per_octave-2.5)<1e-11);
    QVERIFY(tilt->configured_bands>high->configured_bands);
    QCOMPARE(tilt->above_floor_bands,tilt->configured_bands);
    QCOMPARE(high->above_floor_bands,high->configured_bands);
    QVERIFY(!real::observe_m05_t2_or_t6(
        synthetic,{20.0,20000.0},0.0,false));
    QVERIFY(!real::observe_m05_t2_or_t6(
        synthetic,{200.0,20000.0},1e-12,false));
    QVERIFY(!real::observe_m05_t2_or_t6(
        synthetic,{20.0,15000.0},1e-12,true));
    auto insufficient=synthetic;
    std::size_t count=0;
    for(auto& band:insufficient.primary_bands_1_12) {
        if(band.center_hz>=8000.0&&band.center_hz<=16000.0
           &&count++%2==0)
            band.measured_linear_energy=0.0;
    }
    QVERIFY(!real::observe_m05_t2_or_t6(
        insufficient,{20.0,20000.0},1e-12,true));
}
void M05M06RealPortTest::
realNativeM19ShortTermP50PairedSupportAndNoReanalysis() {
    auto prepared=example(0.28,0.15,10);
    auto reference=example(0.45,0.15,10);
    QVERIFY(prepared&&reference);
    const auto lp=analysis::measure_basic_level_full_resource(
        *prepared.value());
    const auto lg=analysis::measure_basic_level_full_resource(
        *reference.value());
    QVERIFY(lp&&lg);
    const auto ps=analysis::measure_tonal_stereo_scope(
        prepared.value()->view(),core::FrameIndex{0},
        prepared.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    const auto gs=analysis::measure_tonal_stereo_scope(
        reference.value()->view(),core::FrameIndex{0},
        reference.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(ps&&gs);
    auto p=real::import_actual_full_source(
        *lp.value(),*ps.value(),{"native-prepared-10s",71,true});
    auto g=real::import_actual_full_source(
        *lg.value(),*gs.value(),{"native-reference-10s",72,true});
    QVERIFY(p.source&&g.source);
    QVERIFY(p.source->short_term_p50_lufs.ready());
    QVERIFY(g.source->short_term_p50_lufs.ready());
    QCOMPARE(p.source->short_term_complete_windows,std::uint64_t{71});
    QCOMPARE(p.source->short_term_valid_windows,std::uint64_t{71});
    QCOMPARE(*p.source->short_term_native_support,1.0);
    const auto pair=real::pair_actual_sources(*p.source,*g.source);
    QVERIFY(pair.pair);
    QVERIFY(pair.pair->short_term_p50_lufs.ready());
    QCOMPARE(*pair.pair->short_term_pair_support,1.0);
    const auto d01=real::pair_d01(*p.source,*g.source);
    QVERIFY(d01);
    QCOMPARE(d01->pair_support,1.0);
    for(std::size_t i=0;i<3;++i)
        QVERIFY(std::abs(d01->candidate.relative_to_p50_lu[i]-
                         d01->reference.relative_to_p50_lu[i])<1e-9);
    const double expected=20.0*std::log10(0.45/0.28);
    const double actual=
        *g.source->short_term_p50_lufs.unrounded-
        *p.source->short_term_p50_lufs.unrounded;
    QVERIFY(std::abs(actual-expected)<1e-9);
    QVERIFY(!pair.pair->full_m05m06_objective_ready());
    // A zero valid observation is not made available by shortening
    // the configured window denominator or substituting -70 LUFS.
    auto corrupted=*p.source;
    corrupted.short_term_p50_lufs={};
    QVERIFY(!corrupted.short_term_p50_lufs.ready());
}
void M05M06RealPortTest::
frozenD01Type7AndThreeComponentMassNativeOracles() {
    const std::vector<double> sorted{0,1,2,3,4,5,6,7,8,9};
    QCOMPARE(*real::type7_percentile(sorted,0.10),0.9);
    QCOMPARE(*real::type7_percentile(sorted,0.50),4.5);
    QCOMPARE(*real::type7_percentile(sorted,0.90),8.1);
    QCOMPARE(*real::type7_percentile(sorted,0.95),8.55);
    QVERIFY(!real::type7_percentile(sorted,-0.01));
    QVERIFY(!real::type7_percentile(sorted,1.01));
    real::SourceReadings a;
    a.owner={"D01-independent-prepared",1,true};
    a.short_term_complete_windows=10;
    a.short_term_valid_windows=10;
    a.short_term_native_support=1.0;
    a.short_term_p50_lufs={4.5,real::Readiness::measured};
    for(std::size_t i=0;i<sorted.size();++i)
        a.short_term_3s_grid100.push_back({
            static_cast<std::int64_t>(i*4800),
            {sorted[i],real::Readiness::measured}});
    const auto shape=real::observe_d01(a);
    QVERIFY(shape);
    QVERIFY(std::abs(shape->relative_to_p50_lu[0]+3.6)<1e-12);
    QVERIFY(std::abs(shape->relative_to_p50_lu[1]-3.6)<1e-12);
    QVERIFY(std::abs(shape->relative_to_p50_lu[2]-4.05)<1e-12);
    auto g=a;
    g.owner={"D01-independent-reference",2,true};
    for(auto& w:g.short_term_3s_grid100)
        *w.lufs.unrounded+=6.0;
    g.short_term_p50_lufs={10.5,real::Readiness::measured};
    const auto pair=real::pair_d01(a,g);
    QVERIFY(pair);
    QCOMPARE(pair->pair_support,1.0);
    QVERIFY(!pair->authorizes_m10());
    for(std::size_t i=0;i<3;++i)
        QVERIFY(std::abs(pair->candidate.relative_to_p50_lu[i]-
                         pair->reference.relative_to_p50_lu[i])<1e-12);
    auto corrupt=g;
    corrupt.short_term_complete_windows=9; // wrong denominator
    QVERIFY(!real::observe_d01(corrupt));
    corrupt=g;
    corrupt.short_term_p50_lufs={0.0,real::Readiness::measured};
    QVERIFY(!real::observe_d01(corrupt));
    QVERIFY(!real::pair_d01(a,a)); // one owner cannot self-reference
}
void M05M06RealPortTest::
malformedOwnerScopeAndUnavailableCannotFabricateM10() {
    using real::Readiness;
    const analysis::BasicLevelValue unqualified{
        -1.25,analysis::BasicLevelValueState::M16_SUPPORTED_UNQUALIFIED};
    const auto t=real::from_m19(unqualified);
    QVERIFY(t.state==Readiness::unqualified);
    QVERIFY(!t.unrounded);
    const analysis::TonalStereoValue tenPercent{
        0.1,analysis::TonalStereoMetricState::AVAILABLE};
    const auto transformed=real::power_fraction_to_db(tenPercent);
    const auto zeroWidth=real::width_mono_endpoint_log2(
        {0.0,Readiness::measured});
    QVERIFY(zeroWidth.ready());
    QCOMPARE(*zeroWidth.unrounded,-24.0);
    QVERIFY(real::width_mono_endpoint_log2({
        std::exp2(-25.0),Readiness::measured}).state==
        Readiness::invalid);
    const auto wide=real::width_mono_endpoint_log2({
        2.0,Readiness::measured});
    QVERIFY(wide.ready());
    QCOMPARE(*wide.unrounded,1.0);
    const auto rhoZero=real::correlation_fisher_z({
        0.0,Readiness::measured});
    QVERIFY(rhoZero.ready());
    QCOMPARE(*rhoZero.unrounded,0.0);
    const auto rhoPlus=real::correlation_fisher_z({
        1.0,Readiness::measured});
    const auto rhoMinus=real::correlation_fisher_z({
        -1.0,Readiness::measured});
    QVERIFY(rhoPlus.ready()&&rhoMinus.ready());
    QVERIFY(std::abs(*rhoPlus.unrounded+
                     *rhoMinus.unrounded)<1e-12);
    QVERIFY(std::abs(*rhoPlus.unrounded-
                     std::atanh(1.0-1e-6))<1e-12);
    QVERIFY(real::correlation_fisher_z({
        1.001,Readiness::measured}).state==Readiness::invalid);
    const auto emptyPlr=real::plr_from_existing_m19(
        {{},Readiness::unqualified},{-12.0,Readiness::measured});
    QVERIFY(emptyPlr.state==Readiness::unqualified);
    QVERIFY(!emptyPlr.unrounded);
    const auto realPlr=real::plr_from_existing_m19(
        {-1.0,Readiness::measured},{-12.0,Readiness::measured});
    QVERIFY(realPlr.ready());
    QCOMPARE(*realPlr.unrounded,11.0);
    QVERIFY(transformed.ready());
    QVERIFY(std::abs(*transformed.unrounded+10.0)<1e-12);
    QVERIFY(real::power_fraction_to_db({0.0,
        analysis::TonalStereoMetricState::AVAILABLE}).state==
        Readiness::invalid);
    QVERIFY(real::power_fraction_to_db({2.0,
        analysis::TonalStereoMetricState::AVAILABLE}).state==
        Readiness::invalid);
    auto p=example(0.45,0.2);
    QVERIFY(p);
    auto l=analysis::measure_basic_level_full_resource(*p.value());
    auto t20=analysis::measure_tonal_stereo_scope(
        p.value()->view(),core::FrameIndex{0},
        p.value()->view().frame_count(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(l&&t20);
    QVERIFY(!real::import_actual_full_source(
        *l.value(),*t20.value(),{"same",0,true}).source);
    QVERIFY(!real::import_actual_full_source(
        *l.value(),*t20.value(),{"same",1,false}).source);
    auto mismatch=*t20.value();
    mismatch.scope_begin_frame=1;
    const auto stale=real::import_actual_full_source(
        *l.value(),mismatch,{"source",1,true});
    QVERIFY(!stale.source);
    QCOMPARE(stale.problem,real::ImportProblem::scope_mismatch);
    auto source=real::import_actual_full_source(
        *l.value(),*t20.value(),{"same",1,true});
    QVERIFY(source.source);
    const auto alias=real::pair_actual_sources(
        *source.source,*source.source);
    QVERIFY(!alias.pair);
    QCOMPARE(alias.problem,real::PairProblem::invalid_owner);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M05M06RealPortTest)
#include "test_l3_m05m06_real_analysis_port.moc"
