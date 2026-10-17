#include "internal/true_peak_limiter_state.hpp"
#include "../../oracles/true_peak_limiter/direct_oracle.hpp"

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
#include <string>

namespace rgsml::tests {
namespace {

using dsp::internal::TruePeakLimiterDetectorState;
using dsp::internal::LimiterDetectorQuarter;
using m17_oracle::Event;

class TruePeakLimiterCoreTest final : public QObject {
    Q_OBJECT
private slots:
    void frozenAssetsMatchIndependentSources();
    void signedTimelineAndFirstFrameImpulse();
    void directOracleMatchesAllPhaseAndTailEvents();
    void nonfiniteAndFinalizationAreAtomic();
};

void TruePeakLimiterCoreTest::frozenAssetsMatchIndependentSources()
{
    using namespace dsp::internal;
    for (std::size_t i=0;i<48;++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(kLimiterAnchorPhaseMajor[i]),
                 std::bit_cast<std::uint64_t>(
                     analysis::internal::kTruePeakItu4xPhaseMajor[i]));
    }
    for (std::size_t i=0;i<16;++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(kLimiterMidpointFir16[i]),
                 std::bit_cast<std::uint64_t>(m17_oracle::kReferenceMidpoint[i]));
    }
    QCOMPARE(std::string(kLimiterAnchorAssetSha256),
             std::string(analysis::internal::kTruePeakItu4xAssetSha256));
}

void TruePeakLimiterCoreTest::signedTimelineAndFirstFrameImpulse()
{
    TruePeakLimiterDetectorState d{1};
    const auto real = d.push_programme_frame(4.0,0.0);
    QVERIFY(real);
    QCOMPARE((*real.value())[0].midpoint.tick, std::int64_t{-62});
    QVERIFY(!(*real.value())[0].preceding_anchor.valid);
    QCOMPARE((*real.value())[3].midpoint.tick, std::int64_t{-56});
    const auto tail = d.finalize_detector();
    QVERIFY(tail);
    QCOMPARE((*tail.value()).size(), std::size_t{59});
    QCOMPARE((*tail.value())[58].midpoint.tick, std::int64_t{62});
    QCOMPARE(d.programme_frames(), std::uint64_t{1});
    QCOMPARE(d.next_quarter_index(), std::int64_t{63});

    // First-frame anchor j=13 has physical tick -21 and is above
    // -12 dBTP; it changes q/density before controller tick 0.
    bool found = false;
    for (const auto& q : *tail.value()) {
        if (q.preceding_anchor.valid && q.preceding_anchor.tick == -21) {
            QCOMPARE(q.preceding_anchor.peak, 4.0*730.0/8192.0);
            found = true;
        }
    }
    QVERIFY(found);
}

void TruePeakLimiterCoreTest::directOracleMatchesAllPhaseAndTailEvents()
{
    const std::array<std::vector<double>,4> inputs{{
        {1.0}, {0.0,-2.0}, {0.0,0.0,0.1,0.0,-0.75,0.0,0.0},
        {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.9,0.0,0.0,0.0,
         0.0,0.0,0.0,-0.5}
    }};
    for (const auto& left : inputs) {
        for (const std::uint32_t channels : {1U,2U}) {
            std::vector<double> right(left.size(),+0.0);
            if (channels==2) {
                for (std::size_t i=0;i<right.size();++i)
                    right[i] = -left[i]*0.375;
            }
            TruePeakLimiterDetectorState d{channels};
            std::vector<Event> real;
            auto collect=[&real](const LimiterDetectorQuarter& q) {
                if (q.preceding_anchor.valid)
                    real.push_back(Event{
                        q.preceding_anchor.tick,q.preceding_anchor.peak});
                if (q.midpoint.valid)
                    real.push_back(Event{q.midpoint.tick,q.midpoint.peak});
            };
            for (std::size_t i=0;i<left.size();++i) {
                const auto result=d.push_programme_frame(left[i],right[i]);
                QVERIFY(result);
                for(const auto& q:*result.value())collect(q);
            }
            const auto tail=d.finalize_detector();
            QVERIFY(tail);
            for (const auto& q:*tail.value())collect(q);
            const auto reference=m17_oracle::direct_events(left,
                channels==2?std::span<const double>(right)
                           :std::span<const double>{});
            QCOMPARE(real.size(),reference.size());
            for(std::size_t i=0;i<real.size();++i) {
                QCOMPARE(real[i].tick,reference[i].tick);
                QCOMPARE(real[i].peak,reference[i].peak);
                if(i>0)QVERIFY(real[i].tick>real[i-1].tick);
            }
            QCOMPARE(real.front().tick,std::int64_t{-62});
            QCOMPARE(real.back().tick,
                static_cast<std::int64_t>(8*left.size()+54));
        }
    }
}

void TruePeakLimiterCoreTest::nonfiniteAndFinalizationAreAtomic()
{
    TruePeakLimiterDetectorState d{2};
    QVERIFY(!d.finalize_detector());
    QVERIFY(!d.push_programme_frame(
        1.0,std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(d.programme_frames(),std::uint64_t{0});
    QCOMPARE(d.next_quarter_index(),std::int64_t{0});
    QVERIFY(d.push_programme_frame(1.0,-1.0));
    QVERIFY(!d.push_programme_frame(
        std::numeric_limits<double>::infinity(),0.0));
    QCOMPARE(d.programme_frames(),std::uint64_t{1});
    QCOMPARE(d.next_quarter_index(),std::int64_t{4});
    QVERIFY(d.finalize_detector());
    QVERIFY(!d.finalize_detector());
    QVERIFY(!d.push_programme_frame(0.0,0.0));
    d.reset();
    QCOMPARE(d.next_quarter_index(),std::int64_t{0});
    QVERIFY(d.push_programme_frame(0.0,0.0));
}

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterCoreTest)
#include "test_true_peak_limiter_core.moc"
