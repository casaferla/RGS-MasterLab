#include "internal/m21_real_product_metrics_port.hpp"
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
    void actualM19M20GoldMinusPreparedAndNoUnqualifiedTP();
    void mismatchedScopeOrUntrustedOwnerDenied();
};

void M21RealProductMetricsPortTest::
actualM19M20GoldMinusPreparedAndNoUnqualifiedTP()
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
    QCOMPARE(pImported.snapshot->metrics[3].quality,Quality::missing);
    QVERIFY(!pImported.snapshot->metrics[3].unrounded);
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
    QCOMPARE(first.prepared_to_gold[3].quality,Quality::missing);
    QVERIFY(!first.prepared_to_gold[3].unrounded);
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

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M21RealProductMetricsPortTest)
#include "test_m21_real_product_metrics_port.moc"
