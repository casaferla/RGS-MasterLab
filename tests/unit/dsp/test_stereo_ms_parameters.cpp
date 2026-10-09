#include <rgsml/core/error.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace rgsml::tests {
namespace {

using rgsml::dsp::MonoBassMode;
using rgsml::dsp::StereoMsParameters;
using rgsml::core::ErrorCode;

class StereoMsParametersTest final : public QObject {
    Q_OBJECT

private slots:
    void neutralDefaultsAndNoIndependentWidthField();
    void inclusiveRangesAndAdjacentRejections();
    void rejectsNonFiniteAndUnknownMode();
    void retainsNonEffectiveStoredValues();
    void canonicalSignedZero();
};

void StereoMsParametersTest::neutralDefaultsAndNoIndependentWidthField()
{
    const auto p = StereoMsParameters::create_default();
    QVERIFY(p);
    QCOMPARE(p.value()->mid_gain_db(), 0.0);
    QCOMPARE(p.value()->side_gain_db(), 0.0);
    QVERIFY(!p.value()->side_muted());
    QVERIFY(p.value()->mono_bass_mode() == MonoBassMode::OFF);
    QCOMPARE(p.value()->mono_bass_cutoff_hz(), 120.0);
    QCOMPARE(p.value()->low_band_width_percent(), 100.0);
    // There is intentionally no stored widthPercent field; Width is derived.
    const auto from_fields = StereoMsParameters::create();
    QVERIFY(from_fields);
    QVERIFY(*p.value() == *from_fields.value());
}

void StereoMsParametersTest::inclusiveRangesAndAdjacentRejections()
{
    for (const double mid : {-12.0, 0.0, 12.0}) {
        for (const double side : {-24.0, 0.0, 12.0}) {
            for (const double cutoff : {40.0, 120.0, 300.0}) {
                for (const double low : {0.0, 25.0, 50.0, 75.0, 100.0}) {
                    for (const auto mode : {MonoBassMode::OFF,
                                            MonoBassMode::LR12,
                                            MonoBassMode::LR24}) {
                        auto p = StereoMsParameters::create(
                            mid, side, false, mode, cutoff, low);
                        QVERIFY(p);
                        QCOMPARE(p.value()->mid_gain_db(), mid);
                        QCOMPARE(p.value()->side_gain_db(), side);
                        QCOMPARE(p.value()->mono_bass_cutoff_hz(), cutoff);
                        QCOMPARE(p.value()->low_band_width_percent(), low);
                    }
                }
            }
        }
    }

    const auto below = [](double x) {
        return std::nextafter(x, -std::numeric_limits<double>::infinity());
    };
    const auto above = [](double x) {
        return std::nextafter(x, std::numeric_limits<double>::infinity());
    };
    for (double mid : {below(-12.0), above(12.0)}) {
        auto p = StereoMsParameters::create(mid);
        QVERIFY(!p);
        QCOMPARE(p.error()->code(), ErrorCode::OutOfRange);
    }
    for (double side : {below(-24.0), above(12.0)}) {
        auto p = StereoMsParameters::create(0.0, side);
        QVERIFY(!p);
        QCOMPARE(p.error()->code(), ErrorCode::OutOfRange);
    }
    for (double cutoff : {below(40.0), above(300.0)}) {
        auto p = StereoMsParameters::create(
            0.0, 0.0, false, MonoBassMode::LR12, cutoff);
        QVERIFY(!p);
        QCOMPARE(p.error()->code(), ErrorCode::OutOfRange);
    }
    for (double low : {below(0.0), above(100.0)}) {
        auto p = StereoMsParameters::create(
            0.0, 0.0, false, MonoBassMode::LR24, 120.0, low);
        QVERIFY(!p);
        QCOMPARE(p.error()->code(), ErrorCode::OutOfRange);
    }
}

void StereoMsParametersTest::rejectsNonFiniteAndUnknownMode()
{
    const std::array invalids{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()};
    for (double val : invalids) {
        const auto p1 = StereoMsParameters::create(val);
        const auto p2 = StereoMsParameters::create(0.0, val);
        const auto p3 = StereoMsParameters::create(
            0.0, 0.0, false, MonoBassMode::LR12, val);
        const auto p4 = StereoMsParameters::create(
            0.0, 0.0, false, MonoBassMode::LR24, 120.0, val);
        QVERIFY(!p1);
        QVERIFY(!p2);
        QVERIFY(!p3);
        QVERIFY(!p4);
        QCOMPARE(p1.error()->code(), ErrorCode::InvalidArgument);
        QCOMPARE(p2.error()->code(), ErrorCode::InvalidArgument);
        QCOMPARE(p3.error()->code(), ErrorCode::InvalidArgument);
        QCOMPARE(p4.error()->code(), ErrorCode::InvalidArgument);
    }
    const auto invalid_mode = StereoMsParameters::create(
        0.0, 0.0, false, static_cast<MonoBassMode>(255));
    QVERIFY(!invalid_mode);
    QCOMPARE(invalid_mode.error()->code(), ErrorCode::InvalidArgument);
}

void StereoMsParametersTest::retainsNonEffectiveStoredValues()
{
    // OFF mode and Side mute affect future effective audio, not persistence.
    const auto off = StereoMsParameters::create(
        -6.0, 9.0, false, MonoBassMode::OFF, 300.0, 25.0);
    const auto side_muted = StereoMsParameters::create(
        -6.0, 9.0, true, MonoBassMode::LR24, 40.0, 0.0);
    QVERIFY(off);
    QVERIFY(side_muted);
    QVERIFY(off.value()->mono_bass_mode() == MonoBassMode::OFF);
    QCOMPARE(off.value()->mono_bass_cutoff_hz(), 300.0);
    QCOMPARE(off.value()->low_band_width_percent(), 25.0);
    QVERIFY(side_muted.value()->side_muted());
    QVERIFY(side_muted.value()->mono_bass_mode() == MonoBassMode::LR24);
    QCOMPARE(side_muted.value()->side_gain_db(), 9.0);
    QCOMPARE(side_muted.value()->mono_bass_cutoff_hz(), 40.0);
    QCOMPARE(side_muted.value()->low_band_width_percent(), 0.0);
}

void StereoMsParametersTest::canonicalSignedZero()
{
    const auto p = StereoMsParameters::create(
        -0.0, -0.0, false, MonoBassMode::LR12, 120.0, -0.0);
    QVERIFY(p);
    QCOMPARE(std::bit_cast<std::uint64_t>(p.value()->mid_gain_db()),
             UINT64_C(0));
    QCOMPARE(std::bit_cast<std::uint64_t>(p.value()->side_gain_db()),
             UINT64_C(0));
    QCOMPARE(std::bit_cast<std::uint64_t>(
        p.value()->low_band_width_percent()), UINT64_C(0));
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsParametersTest)

#include "test_stereo_ms_parameters.moc"
