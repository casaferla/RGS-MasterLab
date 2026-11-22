#include "internal/l3_m06_p04_native_event_selection.hpp"
#include <QtTest/QTest>
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace rgsml::tests {
namespace {
namespace p=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] p::P04RawFluxPopulation fixture() {
    p::P04RawFluxPopulation r;
    r.sample_rate_hz=48000;
    r.native_window_frames=960;
    r.canonical_fft_size=1024;
    r.reason=p::P04NativeReason::ready;
    for(std::uint64_t j=0;j<1200U;++j)
        r.canonical.push_back({
            j,j*240U,2U*j*240U+960U,
            j==0U?std::optional<double>{}:std::optional<double>{0.0}});
    return r;
}
class P04EventSelectionTest final:public QObject {
    Q_OBJECT
private slots:
    void fixedMADZeroBackgroundFindsOnlyRawOnset();
    void NmsRankZThenEarlierAndRefractoryThirtyMilliseconds();
    void rejectsBrokenAbsoluteFrameIdentity();
    void cancelDetectorLeavesNoCandidateEventPopulation();
};
void P04EventSelectionTest::
fixedMADZeroBackgroundFindsOnlyRawOnset(){
    auto r=fixture();
    r.canonical[600].raw_positive_spectral_flux=0.5;
    const auto out=p::select_p04_canonical_native_events(r);
    QCOMPARE(out.reason,p::P04SelectionReason::ready);
    QCOMPARE(out.canonical_events.size(),std::size_t{1});
    QCOMPARE(out.canonical_events[0].canonical_source_frame_ordinal,
             std::uint64_t{600});
    QCOMPARE(out.canonical_events[0].onset_strength_raw_flux,0.5);
    QVERIFY(out.canonical_events[0].robust_z>=6.0);
    QVERIFY(!out.authorizes_m07());
}
void P04EventSelectionTest::
NmsRankZThenEarlierAndRefractoryThirtyMilliseconds(){
    auto r=fixture();
    r.canonical[400].raw_positive_spectral_flux=0.5;
    r.canonical[405].raw_positive_spectral_flux=0.9;
    r.canonical[411].raw_positive_spectral_flux=0.6;
    r.canonical[800].raw_positive_spectral_flux=0.8;
    const auto out=p::select_p04_canonical_native_events(r);
    QCOMPARE(out.reason,p::P04SelectionReason::ready);
    // 400 and 405 are 25ms apart (outside local ±15ms, inside
    // refractory 30ms), so only stronger 405 survives. 411 is
    // precisely 30ms from 405 and may survive if a local maximum.
    QCOMPARE(out.canonical_events.size(),std::size_t{3});
    QCOMPARE(out.canonical_events[0].canonical_source_frame_ordinal,
             std::uint64_t{405});
    QCOMPARE(out.canonical_events[1].canonical_source_frame_ordinal,
             std::uint64_t{411});
    QCOMPARE(out.canonical_events[2].canonical_source_frame_ordinal,
             std::uint64_t{800});
}
void P04EventSelectionTest::
cancelDetectorLeavesNoCandidateEventPopulation() {
    const auto native=fixture();
    std::atomic<bool> cancelled{true};
    const auto result=p::select_p04_canonical_native_events(
        native,&cancelled);
    QCOMPARE(result.reason,p::P04SelectionReason::cancelled);
    QVERIFY(result.canonical_events.empty());
}
void P04EventSelectionTest::
rejectsBrokenAbsoluteFrameIdentity(){
    auto r=fixture();
    r.canonical[3].absolute_start_frame=0;
    const auto rejected=p::select_p04_canonical_native_events(r);
    QCOMPARE(rejected.reason,
             p::P04SelectionReason::
                 unqualified_or_invalid_flux_population);
    // The same apparently monotonic population must be rejected
    // when a single native sample is shifted off the frozen rational
    // 200 Hz grid, even if its timestamp/center is shifted together.
    r=fixture();
    ++r.canonical[3].absolute_start_frame;
    r.canonical[3].event_center_twice_frames+=2U;
    const auto shifted=p::select_p04_canonical_native_events(r);
    QCOMPARE(shifted.reason,p::P04SelectionReason::
                 unqualified_or_invalid_flux_population);
    r=fixture();
    r.canonical[41].raw_positive_spectral_flux.reset();
    QVERIFY(p::select_p04_canonical_native_events(r).canonical_events.empty());
    QCOMPARE(p::select_p04_canonical_native_events(r).reason,
             p::P04SelectionReason::
                 unqualified_or_invalid_flux_population);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::P04EventSelectionTest)
#include "test_l3_m06_p04_native_event_selection.moc"
