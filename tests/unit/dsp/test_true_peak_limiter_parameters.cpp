#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/dsp/module_parameter_payload.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>

#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;

class TruePeakLimiterParametersTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultAndBoundaries();
    void invalidNumbersRejectRatherThanClamp();
    void strictJsonGrammarAndDuplicateKeys();
    void preservesExactCeilingInGenericPayload();
    void negativeZeroIsCanonical();
};

void TruePeakLimiterParametersTest::defaultAndBoundaries()
{
    const auto defaults = TruePeakLimiterParameters::create_default();
    QVERIFY(defaults);
    QCOMPARE(defaults.value()->ceiling_db_tp(), -1.0);

    for (const double x : {-12.0, -6.0, -1.0, -0.1, 0.0}) {
        const auto params = TruePeakLimiterParameters::create(x);
        QVERIFY(params);
        QCOMPARE(params.value()->ceiling_db_tp(), x);
        const auto encoded = encode_true_peak_limiter_parameters_json(*params.value());
        QVERIFY(encoded);
        const auto decoded = decode_true_peak_limiter_parameters_json(*encoded.value());
        QVERIFY(decoded);
        QCOMPARE(decoded.value()->ceiling_db_tp(), x);
    }
}

void TruePeakLimiterParametersTest::invalidNumbersRejectRatherThanClamp()
{
    const auto below = TruePeakLimiterParameters::create(
        std::nextafter(-12.0, -std::numeric_limits<double>::infinity()));
    const auto above = TruePeakLimiterParameters::create(
        std::nextafter(0.0, std::numeric_limits<double>::infinity()));
    QVERIFY(!below);
    QVERIFY(!above);
    QCOMPARE(below.error()->code(), core::ErrorCode::OutOfRange);
    QCOMPARE(above.error()->code(), core::ErrorCode::OutOfRange);

    for (const double invalid : {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()}) {
        auto params = TruePeakLimiterParameters::create(invalid);
        QVERIFY(!params);
        QCOMPARE(params.error()->code(), core::ErrorCode::InvalidArgument);
    }
}

void TruePeakLimiterParametersTest::strictJsonGrammarAndDuplicateKeys()
{
    // Other codecs intentionally retain their existing v1.0 behavior.
    // The M17 strict schema uniquely rejects duplicate-object JSON keys.
    for (const auto* invalid : {
        "", "null", "[]", "{}", "{",
        R"({"ceilingDbTp":null})",
        R"({"ceilingDbTp":"-1"})",
        R"({"ceilingDbTp":false})",
        R"({"ceilingDbTp":[]})",
        R"({"ceilingDbTp":{}})",
        R"({"ceilingDbTp":-13})",
        R"({"ceilingDbTp":0.001})",
        R"({"ceilingDbTp":-1,"unknown":0})",
        R"({"ceilingDbTp":-6,"ceilingDbTp":-1})",
        R"({"ceilingDbTp":-1,"ceilingDbTp":-1})",
        R"({"ceilingDbTp":1e999})"}) {
        QVERIFY2(!decode_true_peak_limiter_parameters_json(invalid), invalid);
    }

    auto proper = decode_true_peak_limiter_parameters_json(
        R"({"ceilingDbTp":-6.000000000000001})");
    QVERIFY(proper);
    QCOMPARE(
        std::bit_cast<std::uint64_t>(proper.value()->ceiling_db_tp()),
        std::bit_cast<std::uint64_t>(-6.000000000000001));
}

void TruePeakLimiterParametersTest::preservesExactCeilingInGenericPayload()
{
    constexpr auto kSchema = "rgsml.dsp.true-peak-limiter.parameters/1.0.0";
    const double x = std::nextafter(-1.0, 0.0);
    const auto input = TruePeakLimiterParameters::create(x);
    QVERIFY(input);
    const ModuleParameterPayload payload{*input.value()};
    const auto encoded = encode_module_parameters_json(payload);
    QVERIFY(encoded);
    const auto decoded = decode_module_parameters_json(kSchema, *encoded.value());
    QVERIFY(decoded);
    const auto* limiter = std::get_if<TruePeakLimiterParameters>(decoded.value());
    QVERIFY(limiter != nullptr);
    QCOMPARE(
        std::bit_cast<std::uint64_t>(limiter->ceiling_db_tp()),
        std::bit_cast<std::uint64_t>(x));
    QVERIFY(!decode_module_parameters_json(
        "rgsml.dsp.true-peak-limiter.parameters/2.0.0", *encoded.value()));
}

void TruePeakLimiterParametersTest::negativeZeroIsCanonical()
{
    const auto result = TruePeakLimiterParameters::create(-0.0);
    QVERIFY(result);
    QVERIFY(!std::signbit(result.value()->ceiling_db_tp()));
    const auto encoded = encode_true_peak_limiter_parameters_json(*result.value());
    QVERIFY(encoded);
    QCOMPARE(*encoded.value(), std::string(R"({"ceilingDbTp":0.0})"));
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterParametersTest)
#include "test_true_peak_limiter_parameters.moc"
