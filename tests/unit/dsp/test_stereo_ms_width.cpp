#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <rgsml/dsp/stereo_ms_width.hpp>

#include <QtTest/QTest>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using rgsml::core::ErrorCode;

class StereoMsWidthTest final : public QObject {
    Q_OBJECT
private slots:
    void neutralAndExactMutePreserveStoredFields();
    void frozenCommonGainSlicesAndRangeTruthfulness();
    void positiveEditsPreserveCommonGainWithoutClamps();
    void invalidOutOfDomainAndSubnormalDoNotMuteOrMutate();
};

void StereoMsWidthTest::neutralAndExactMutePreserveStoredFields()
{
    auto neutral = StereoMsParameters::create_default();
    QVERIFY(neutral);
    const auto initial = stereo_ms_width_coordinates(*neutral.value());
    QCOMPARE(initial.common_gain_db, 0.0);
    QCOMPARE(initial.current_width_percent, 100.0);
    auto muted = stereo_ms_edit_width_preserving_common_gain(
        *neutral.value(), 0.0);
    QVERIFY(muted);
    QVERIFY(muted.value()->side_muted());
    QCOMPARE(muted.value()->side_gain_db(), 0.0);
    QCOMPARE(muted.value()->mid_gain_db(), 0.0);
    QCOMPARE(stereo_ms_width_coordinates(*muted.value()).current_width_percent,
             0.0);

    // Non-effective stored fields MUST survive Width mute and reactivation.
    auto source = StereoMsParameters::create(
        -3.0, 6.0, false, MonoBassMode::LR24, 300.0, 25.0);
    QVERIFY(source);
    auto zero = stereo_ms_edit_width_preserving_common_gain(*source.value(), 0.0);
    QVERIFY(zero);
    QVERIFY(zero.value()->side_muted());
    QCOMPARE(zero.value()->side_gain_db(), 6.0);
    QCOMPARE(zero.value()->mid_gain_db(), -3.0);
    QCOMPARE(zero.value()->mono_bass_mode(), MonoBassMode::LR24);
    QCOMPARE(zero.value()->mono_bass_cutoff_hz(), 300.0);
    QCOMPARE(zero.value()->low_band_width_percent(), 25.0);

    auto wide = stereo_ms_edit_width_preserving_common_gain(*zero.value(), 100.0);
    QVERIFY(wide);
    QVERIFY(!wide.value()->side_muted());
    QCOMPARE(wide.value()->mono_bass_mode(), MonoBassMode::LR24);
    QCOMPARE(wide.value()->mono_bass_cutoff_hz(), 300.0);
    QCOMPARE(wide.value()->low_band_width_percent(), 25.0);
    QVERIFY(std::abs(stereo_ms_width_coordinates(*wide.value()).common_gain_db
                     - 1.5) < 1e-14);
    QCOMPARE(stereo_ms_width_coordinates(*wide.value()).current_width_percent,
             100.0);
}

void StereoMsWidthTest::frozenCommonGainSlicesAndRangeTruthfulness()
{
    // Independent golden d=[Side-Mid] boundaries from the accepted
    // rectangular hard gain domain, evaluated on the five critical slices.
    struct Expected final { double m,s,c,dmin,dmax; };
    constexpr std::array<Expected, 5> slices{{
        {-12.0,-24.0,-18.0,-12.0,-12.0},
        {  0.0,-12.0, -6.0,-36.0, 12.0},
        {  0.0,  0.0,  0.0,-24.0, 24.0},
        {  9.0,  9.0,  9.0, -6.0,  6.0},
        { 12.0, 12.0, 12.0,  0.0,  0.0}
    }};
    for (const auto& v : slices) {
        auto p = StereoMsParameters::create(v.m,v.s);
        QVERIFY(p);
        const auto x = stereo_ms_width_coordinates(*p.value());
        QCOMPARE(x.common_gain_db,v.c);
        QVERIFY(std::abs(
            x.minimum_positive_width_percent
            - 100.0 * std::pow(10.0,v.dmin/20.0)) < 1e-12);
        QVERIFY(std::abs(
            x.maximum_positive_width_percent
            - 100.0 * std::pow(10.0,v.dmax/20.0)) < 1e-11);
        // Displaying existing legal states may NEVER be rejected just
        // because analytical extrema rounded outward to binary64.
        auto same = stereo_ms_edit_width_preserving_common_gain(
            *p.value(),x.current_width_percent);
        QVERIFY(same);
        QCOMPARE(*same.value(),*p.value());
    }
    auto c9 = StereoMsParameters::create(9.0,9.0);
    QVERIFY(c9);
    const auto limit = stereo_ms_width_coordinates(*c9.value());
    QVERIFY(limit.maximum_positive_width_percent < 200.0);
    QVERIFY(!stereo_ms_edit_width_preserving_common_gain(*c9.value(),200.0));

    // Valid current Width >200% is visible, not clamped to slider range.
    auto expanded = StereoMsParameters::create(0.0,12.0);
    QVERIFY(expanded);
    const auto x = stereo_ms_width_coordinates(*expanded.value());
    QVERIFY(x.current_width_percent > 200.0);
    QVERIFY(x.current_width_percent < 500.0);
}

void StereoMsWidthTest::positiveEditsPreserveCommonGainWithoutClamps()
{
    auto neutral = StereoMsParameters::create_default();
    QVERIFY(neutral);
    for (const double width : {50.0,100.0,200.0,400.0}) {
        auto edited = stereo_ms_edit_width_preserving_common_gain(
            *neutral.value(), width);
        QVERIFY(edited);
        QVERIFY(!edited.value()->side_muted());
        const auto x = stereo_ms_width_coordinates(*edited.value());
        QVERIFY(std::abs(x.common_gain_db) < 1e-14);
        QVERIFY(std::abs(x.current_width_percent-width) < 2e-11);
        QVERIFY(edited.value()->mid_gain_db() >= -12.0);
        QVERIFY(edited.value()->side_gain_db() <= 12.0);
    }
    auto wide = stereo_ms_edit_width_preserving_common_gain(
        *neutral.value(), 200.0);
    QVERIFY(wide);
    QVERIFY(std::abs(wide.value()->mid_gain_db() + 3.010299956639812) < 1e-12);
    QVERIFY(std::abs(wide.value()->side_gain_db() - 3.010299956639812) < 1e-12);
}

void StereoMsWidthTest::invalidOutOfDomainAndSubnormalDoNotMuteOrMutate()
{
    auto source = StereoMsParameters::create(
        9.0,9.0,false,MonoBassMode::LR12,40.0,0.0);
    QVERIFY(source);
    constexpr double inf = std::numeric_limits<double>::infinity();
    const std::array requests{
        -1.0,-inf,inf,std::numeric_limits<double>::quiet_NaN(),
        200.0,400.0,1e-300,std::numeric_limits<double>::denorm_min()};
    for (const double width : requests) {
        const auto result = stereo_ms_edit_width_preserving_common_gain(
            *source.value(),width);
        QVERIFY(!result);
        QVERIFY(result.error()->code()==ErrorCode::InvalidArgument
                || result.error()->code()==ErrorCode::OutOfRange);
        // Immutable source stays valid and unchanged after every rejection.
        QCOMPARE(source.value()->mid_gain_db(),9.0);
        QCOMPARE(source.value()->side_gain_db(),9.0);
        QVERIFY(!source.value()->side_muted());
        QCOMPARE(source.value()->mono_bass_cutoff_hz(),40.0);
        QCOMPARE(source.value()->low_band_width_percent(),0.0);
    }
    // -0 is the exact mute request; no finite approximation is needed.
    const auto minus_zero = stereo_ms_edit_width_preserving_common_gain(
        *source.value(), -0.0);
    QVERIFY(minus_zero);
    QVERIFY(minus_zero.value()->side_muted());
    QCOMPARE(minus_zero.value()->side_gain_db(),9.0);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsWidthTest)
#include "test_stereo_ms_width.moc"
