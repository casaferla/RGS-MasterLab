#include "internal/l3_m09_p03_stereo_low_seed.hpp"
#include <QtTest/QTest>
#include <cmath>
#include <limits>

namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
[[nodiscard]] m09::LowSeedEvidence valid() {
    return {true,true,false,true,false,false,0.0,-1.0,0.0,0.0,true};
}
class M09StereoP03Test final:public QObject {
    Q_OBJECT
private slots:
    void sevenDiscreteModesDoNotCollapseActiveHundredToOff();
    void rawAndCombinedIdealLowLaneMath();
    void legalBoundAndMissingFixedOrGainAuthority();
    void endpointAndMonoSafetyNeverInferredFromNumericZero();
};
void M09StereoP03Test::
sevenDiscreteModesDoNotCollapseActiveHundredToOff() {
    QCOMPARE(m09::kLowBassAlternatives.size(),std::size_t{7});
    QCOMPARE(m09::kLowBassAlternatives.front().cutoff_hz,
             std::uint16_t{120});
    QVERIFY(!m09::kLowBassAlternatives.front().active());
    for(std::size_t i=1;i<7;++i) {
        QVERIFY(m09::kLowBassAlternatives[i].active());
        QCOMPARE(m09::kLowBassAlternatives[i].stored_width_percent,100.0);
    }
    QVERIFY(m09::structural_low_alternatives_valid());
}
void M09StereoP03Test::rawAndCombinedIdealLowLaneMath() {
    auto e=valid();
    const auto standalone=m09::derive_low_width_seed(e,false);
    QCOMPARE(standalone.reason,m09::LowSeedReason::ready);
    QVERIFY(standalone.raw_percent);
    QVERIFY(std::abs(*standalone.raw_percent-50.0)<1e-12);
    QCOMPARE(*standalone.legal_percent,*standalone.raw_percent);
    e.bounded_mid_gain_db=0.0;
    e.bounded_side_gain_db=6.020599913279624;
    const auto combined=m09::derive_low_width_seed(e,true);
    QCOMPARE(combined.reason,m09::LowSeedReason::ready);
    QVERIFY(combined.raw_percent);
    QVERIFY(std::abs(*combined.raw_percent-25.0)<1e-9);
    QVERIFY(!combined.authorizes_stereo_safety_or_publication());
}
void M09StereoP03Test::legalBoundAndMissingFixedOrGainAuthority() {
    auto e=valid();
    e.optimization_target_low_width_log2=1.0;
    const auto limited=m09::derive_low_width_seed(e,false);
    QCOMPARE(limited.reason,m09::LowSeedReason::limited_to_legal_range);
    QCOMPARE(*limited.raw_percent,200.0);
    QCOMPARE(*limited.legal_percent,100.0);
    e.exact_insertion_baseline_and_m11_ownership=false;
    QCOMPARE(m09::derive_low_width_seed(e,false).reason,
             m09::LowSeedReason::missing_exact_insertion_baseline);
    e.exact_insertion_baseline_and_m11_ownership=true;
    e.gains_frozen_schema_legal=false;
    QCOMPARE(m09::derive_low_width_seed(e,true).reason,
             m09::LowSeedReason::missing_bounded_gain_authority);
}
void M09StereoP03Test::
endpointAndMonoSafetyNeverInferredFromNumericZero() {
    auto e=valid();
    e.observed_zero_side=true;
    QCOMPARE(m09::derive_low_width_seed(e,true).reason,
             m09::LowSeedReason::observed_zero_side_cannot_widen);
    e.both_observed_and_target_mono=true;
    e.target_requires_positive_side=false;
    QCOMPARE(m09::derive_low_width_seed(e,false).reason,
             m09::LowSeedReason::already_mono_satisfied);
    e.global_exact_side_mute=true;
    e.both_observed_and_target_mono=false;
    QCOMPARE(m09::derive_low_width_seed(e,false).reason,
             m09::LowSeedReason::exact_global_side_mute_separate_branch);
    e.global_exact_side_mute=false;
    e.observed_zero_side=false;
    e.stereo_input=false;
    QCOMPARE(m09::derive_low_width_seed(e,false).reason,
             m09::LowSeedReason::one_channel_not_applicable);
    e.stereo_input=true;
    e.optimization_target_low_width_log2=
        -std::numeric_limits<double>::infinity();
    QCOMPARE(m09::derive_low_width_seed(e,false).reason,
             m09::LowSeedReason::nonfinite_or_illegal_operand);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09StereoP03Test)
#include "test_l3_m09_p03_stereo_low_seed.moc"
