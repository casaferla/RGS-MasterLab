#include <rgsml/dsp/parametric_eq_response.hpp>

#include <bit>

#include "../../oracles/parametric_eq/parametric_eq_oracle.hpp"

#include <QtTest/QTest>

#include <cmath>
#include <complex>

namespace rgsml::tests {
namespace {

using rgsml::dsp::EqBandParameters;
using rgsml::dsp::EqFilterType;
using rgsml::dsp::EqResponsePoint;
using rgsml::dsp::EqRouting;
using rgsml::dsp::SlopeDbPerOctave;

class ParametricEqResponseTest final : public QObject {
    Q_OBJECT

private slots:
    void testAllSixFilterTypesAgreementWithOracle();
    void testCascadedPassFilters();
    void testDisabledBandIdentity();
    void testInvalidInputRejection();
};

void ParametricEqResponseTest::testAllSixFilterTypesAgreementWithOracle()
{
    const auto band_id = *core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    const auto sample_rate = *core::SampleRate::create(48000).value();

    // 1. BELL (1 kHz, +6 dB, Q=0.707)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::BELL, EqRouting::STEREO,
            dsp::BellPayload{1000.0, 6.0, 0.707}).value();

        auto res = dsp::evaluate_band_point(band, 1000.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::biquad_transfer_function(
            oracles::ref_constants::BELL_1K_PLUS6_Q0707, 1000.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
        QCOMPARE_LE(std::abs(res.value()->magnitude_db - 20.0 * std::log10(std::abs(oracle_transfer))), 1e-4);
    }

    // 2. NOTCH (1 kHz, Q=12)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::NOTCH, EqRouting::STEREO,
            dsp::NotchPayload{1000.0, 12.0}).value();

        auto res = dsp::evaluate_band_point(band, 1000.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::biquad_transfer_function(
            oracles::ref_constants::NOTCH_1K_Q12, 1000.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
    }

    // 3. LOW_SHELF (100 Hz, +6 dB, S=0.5)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::LOW_SHELF, EqRouting::STEREO,
            dsp::ShelfPayload{100.0, 6.0, 0.5}).value();

        auto res = dsp::evaluate_band_point(band, 100.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::biquad_transfer_function(
            oracles::ref_constants::LOW_SHELF_100_PLUS6_S05, 100.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
    }

    // 4. HIGH_SHELF (10 kHz, +6 dB, S=0.5)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::HIGH_SHELF, EqRouting::STEREO,
            dsp::ShelfPayload{10000.0, 6.0, 0.5}).value();

        auto res = dsp::evaluate_band_point(band, 10000.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::biquad_transfer_function(
            oracles::ref_constants::HIGH_SHELF_10K_PLUS6_S05, 10000.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
    }

    // 5. HIGH_PASS (1 kHz, 12 dB/oct)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::HIGH_PASS, EqRouting::STEREO,
            dsp::PassPayload{1000.0, SlopeDbPerOctave::DB_12}).value();

        auto res = dsp::evaluate_band_point(band, 1000.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::biquad_transfer_function(
            oracles::ref_constants::HP_12_SECTIONS[0], 1000.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
    }

    // 6. LOW_PASS (1 kHz, 12 dB/oct)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::LOW_PASS, EqRouting::STEREO,
            dsp::PassPayload{1000.0, SlopeDbPerOctave::DB_12}).value();

        auto res = dsp::evaluate_band_point(band, 1000.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::biquad_transfer_function(
            oracles::ref_constants::LP_12_SECTIONS[0], 1000.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
    }
}

void ParametricEqResponseTest::testCascadedPassFilters()
{
    const auto band_id = *core::Uuid::parse("10000000-0000-0000-0000-000000000002").value();
    const auto sample_rate = *core::SampleRate::create(48000).value();

    // HP 24 dB/oct (2 biquad sections)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::HIGH_PASS, EqRouting::STEREO,
            dsp::PassPayload{1000.0, SlopeDbPerOctave::DB_24}).value();

        auto res = dsp::evaluate_band_point(band, 1000.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::cascade_transfer_function(
            oracles::ref_constants::HP_24_SECTIONS, 1000.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
    }

    // LP 48 dB/oct (4 biquad sections)
    {
        auto band = *EqBandParameters::create(
            band_id, true, EqFilterType::LOW_PASS, EqRouting::STEREO,
            dsp::PassPayload{1000.0, SlopeDbPerOctave::DB_48}).value();

        auto res = dsp::evaluate_band_point(band, 500.0, sample_rate);
        QVERIFY(res);

        const auto oracle_transfer = oracles::cascade_transfer_function(
            oracles::ref_constants::LP_48_SECTIONS, 500.0, 48000.0);
        QCOMPARE_LE(std::abs(res.value()->transfer_function - oracle_transfer), 1e-6);
    }
}

void ParametricEqResponseTest::testDisabledBandIdentity()
{
    const auto band_id = *core::Uuid::parse("10000000-0000-0000-0000-000000000003").value();
    const auto sample_rate = *core::SampleRate::create(48000).value();

    auto disabled_band = *EqBandParameters::create(
        band_id, false, EqFilterType::BELL, EqRouting::STEREO,
        dsp::BellPayload{1000.0, 18.0, 0.707}).value();

    auto res = dsp::evaluate_band_point(disabled_band, 1000.0, sample_rate);
    QVERIFY(res);
    QCOMPARE(res.value()->transfer_function, std::complex<double>(1.0, 0.0));
    QCOMPARE(res.value()->magnitude_db, 0.0);
    QCOMPARE(res.value()->phase_rad, 0.0);
}

void ParametricEqResponseTest::testInvalidInputRejection()
{
    const auto band_id = *core::Uuid::parse("10000000-0000-0000-0000-000000000004").value();
    const auto sample_rate = *core::SampleRate::create(48000).value();
    auto band = *EqBandParameters::create(
        band_id, true, EqFilterType::BELL, EqRouting::STEREO,
        dsp::BellPayload{1000.0, 0.0, 0.707}).value();

    // Frequency > 0.45 * Fs (48000 * 0.45 = 21600 Hz)
    auto high_freq = dsp::evaluate_band_point(band, 22000.0, sample_rate);
    QVERIFY(!high_freq);

    // Negative frequency
    auto neg_freq = dsp::evaluate_band_point(band, -10.0, sample_rate);
    QVERIFY(!neg_freq);

    // NaN frequency
    auto nan_freq = dsp::evaluate_band_point(band, std::numeric_limits<double>::quiet_NaN(), sample_rate);
    QVERIFY(!nan_freq);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ParametricEqResponseTest)

#include "test_parametric_eq_response.moc"
