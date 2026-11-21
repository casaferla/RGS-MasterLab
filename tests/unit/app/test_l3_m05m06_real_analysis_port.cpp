#include "internal/l3_m05m06_real_analysis_port.hpp"
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
};
[[nodiscard]] core::Result<audio::AudioBuffer> example(
    double scale,double harmonic) {
    auto fs=core::SampleRate::create(48000);
    auto format=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    auto frames=core::FrameCount::create(48000);
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
