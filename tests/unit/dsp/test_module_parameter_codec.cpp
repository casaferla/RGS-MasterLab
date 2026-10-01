#include <rgsml/core/error.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <QtTest/QTest>

#include <cmath>
#include <string>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using rgsml::core::Uuid;

class ModuleParameterCodecTest final : public QObject {
    Q_OBJECT

private slots:
    void gainRoundTripAndBoundaries();
    void gainRejections();
    void gainNegativeZeroCanonicalization();
    void eqRoundTripAllFilterTypes();
    void eqRejectionsAndMalformedJson();
    void genericModuleParameterCodecApi();
};

void ModuleParameterCodecTest::gainRoundTripAndBoundaries()
{
    for (const double expected_db : {-24.0, -12.5, 0.0, 3.1415926535, 24.0}) {
        auto params = GainParameters::create(expected_db);
        QVERIFY(params);

        auto encoded = encode_gain_parameters_json(*params.value());
        QVERIFY(encoded);
        QVERIFY(!encoded.value()->empty());

        auto decoded = decode_gain_parameters_json(*encoded.value());
        QVERIFY(decoded);
        QCOMPARE(decoded.value()->gain_db(), params.value()->gain_db());
        QCOMPARE(*decoded.value(), *params.value());
    }
}

void ModuleParameterCodecTest::gainRejections()
{
    // Out of range [-24, +24]
    QVERIFY(!decode_gain_parameters_json("{\"gainDb\": -24.1}"));
    QVERIFY(!decode_gain_parameters_json("{\"gainDb\": 24.1}"));

    // Invalid JSON syntax / structural malformation
    QVERIFY(!decode_gain_parameters_json("invalid json"));
    QVERIFY(!decode_gain_parameters_json("[0.0]"));
    QVERIFY(!decode_gain_parameters_json("{}"));

    // Wrong field type
    QVERIFY(!decode_gain_parameters_json("{\"gainDb\": \"0.0\"}"));
    QVERIFY(!decode_gain_parameters_json("{\"gainDb\": null}"));
    QVERIFY(!decode_gain_parameters_json("{\"gainDb\": true}"));

    // Strict schema: no extra keys allowed
    QVERIFY(!decode_gain_parameters_json("{\"gainDb\": 0.0, \"extra\": 1}"));
}

void ModuleParameterCodecTest::gainNegativeZeroCanonicalization()
{
    auto neg_zero = GainParameters::create(-0.0);
    QVERIFY(neg_zero);

    auto encoded = encode_gain_parameters_json(*neg_zero.value());
    QVERIFY(encoded);
    QCOMPARE(*encoded.value(), std::string("{\"gainDb\":0.0}"));

    auto decoded_from_neg = decode_gain_parameters_json("{\"gainDb\": -0.0}");
    QVERIFY(decoded_from_neg);
    QCOMPARE(decoded_from_neg.value()->gain_db(), 0.0);
    QVERIFY(!std::signbit(decoded_from_neg.value()->gain_db()));
}

void ModuleParameterCodecTest::eqRoundTripAllFilterTypes()
{
    const auto id1 = *Uuid::parse("00000000-0000-4000-8000-000000000001").value();
    const auto id2 = *Uuid::parse("00000000-0000-4000-8000-000000000002").value();
    const auto id3 = *Uuid::parse("00000000-0000-4000-8000-000000000003").value();
    const auto id4 = *Uuid::parse("00000000-0000-4000-8000-000000000004").value();
    const auto id5 = *Uuid::parse("00000000-0000-4000-8000-000000000005").value();
    const auto id6 = *Uuid::parse("00000000-0000-4000-8000-000000000006").value();

    std::vector<EqBandParameters> bands;

    // Band 1: BELL Stereo
    bands.push_back(*EqBandParameters::create(
        id1, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{100.0, -3.5, 1.414}).value());

    // Band 2: NOTCH Mid
    bands.push_back(*EqBandParameters::create(
        id2, false, EqFilterType::NOTCH, EqRouting::MID, NotchPayload{500.0, 5.0}).value());

    // Band 3: LOW_SHELF Side
    bands.push_back(*EqBandParameters::create(
        id3, true, EqFilterType::LOW_SHELF, EqRouting::SIDE, ShelfPayload{80.0, 4.0, 0.75}).value());

    // Band 4: HIGH_SHELF Left
    bands.push_back(*EqBandParameters::create(
        id4, true, EqFilterType::HIGH_SHELF, EqRouting::LEFT, ShelfPayload{8000.0, -2.0, 0.90}).value());

    // Band 5: HIGH_PASS Right
    bands.push_back(*EqBandParameters::create(
        id5, true, EqFilterType::HIGH_PASS, EqRouting::RIGHT, PassPayload{30.0, SlopeDbPerOctave::DB_24}).value());

    // Band 6: LOW_PASS Stereo
    bands.push_back(*EqBandParameters::create(
        id6, false, EqFilterType::LOW_PASS, EqRouting::STEREO, PassPayload{18000.0, SlopeDbPerOctave::DB_48}).value());

    auto orig_eq = ParametricEqParameters::create(bands);
    QVERIFY(orig_eq);

    auto encoded = encode_parametric_eq_parameters_json(*orig_eq.value());
    QVERIFY(encoded);
    QVERIFY(!encoded.value()->empty());

    auto decoded = decode_parametric_eq_parameters_json(*encoded.value());
    QVERIFY(decoded);
    QCOMPARE(*decoded.value(), *orig_eq.value());
}

void ModuleParameterCodecTest::eqRejectionsAndMalformedJson()
{
    // Syntax errors
    QVERIFY(!decode_parametric_eq_parameters_json("not json"));

    // Root not object
    QVERIFY(!decode_parametric_eq_parameters_json("[]"));

    // Root missing 'bands' or containing extra keys
    QVERIFY(!decode_parametric_eq_parameters_json("{}"));
    QVERIFY(!decode_parametric_eq_parameters_json("{\"bands\":[], \"extra\":1}"));

    // 'bands' not an array
    QVERIFY(!decode_parametric_eq_parameters_json("{\"bands\":{}}"));

    // Band count out of range (0 or >6)
    QVERIFY(!decode_parametric_eq_parameters_json("{\"bands\":[]}"));

    // Invalid band element type
    QVERIFY(!decode_parametric_eq_parameters_json("{\"bands\":[123]}"));

    // Missing common fields
    QVERIFY(!decode_parametric_eq_parameters_json(
        "{\"bands\":[{\"bandId\":\"00000000-0000-4000-8000-000000000001\",\"enabled\":true}]}'"));

    // Extra field in band object
    QVERIFY(!decode_parametric_eq_parameters_json(
        "{\"bands\":[{\"bandId\":\"00000000-0000-4000-8000-000000000001\",\"enabled\":true,\"filterType\":\"BELL\",\"routing\":\"STEREO\",\"frequencyHz\":1000.0,\"gainDb\":0.0,\"q\":0.707,\"unknownField\":123}]}"));

    // Out of range parameter values in band
    // Frequency < 20
    QVERIFY(!decode_parametric_eq_parameters_json(
        "{\"bands\":[{\"bandId\":\"00000000-0000-4000-8000-000000000001\",\"enabled\":true,\"filterType\":\"BELL\",\"routing\":\"STEREO\",\"frequencyHz\":10.0,\"gainDb\":0.0,\"q\":0.707}]}"));

    // Gain > +18
    QVERIFY(!decode_parametric_eq_parameters_json(
        "{\"bands\":[{\"bandId\":\"00000000-0000-4000-8000-000000000001\",\"enabled\":true,\"filterType\":\"BELL\",\"routing\":\"STEREO\",\"frequencyHz\":1000.0,\"gainDb\":20.0,\"q\":0.707}]}"));

    // Invalid slope integer (10)
    QVERIFY(!decode_parametric_eq_parameters_json(
        "{\"bands\":[{\"bandId\":\"00000000-0000-4000-8000-000000000001\",\"enabled\":true,\"filterType\":\"HIGH_PASS\",\"routing\":\"STEREO\",\"frequencyHz\":1000.0,\"slopeDbPerOctave\":10}]}"));

    // Duplicate band ID
    QVERIFY(!decode_parametric_eq_parameters_json(
        "{\"bands\":["
        "{\"bandId\":\"00000000-0000-4000-8000-000000000001\",\"enabled\":true,\"filterType\":\"BELL\",\"routing\":\"STEREO\",\"frequencyHz\":1000.0,\"gainDb\":0.0,\"q\":0.707},"
        "{\"bandId\":\"00000000-0000-4000-8000-000000000001\",\"enabled\":true,\"filterType\":\"BELL\",\"routing\":\"STEREO\",\"frequencyHz\":2000.0,\"gainDb\":0.0,\"q\":0.707}"
        "]}"));
}

void ModuleParameterCodecTest::genericModuleParameterCodecApi()
{
    // Generic Gain payload
    ModuleParameterPayload gain_payload{*GainParameters::create(-6.0).value()};
    auto encoded_gain = encode_module_parameters_json(gain_payload);
    QVERIFY(encoded_gain);

    auto decoded_gain = decode_module_parameters_json("rgsml.dsp.gain.parameters/1.0.0", *encoded_gain.value());
    QVERIFY(decoded_gain);
    QVERIFY(std::holds_alternative<GainParameters>(*decoded_gain.value()));
    QCOMPARE(std::get<GainParameters>(*decoded_gain.value()).gain_db(), -6.0);

    // Generic EQ payload
    ModuleParameterPayload eq_payload{*ParametricEqParameters::create_legacy_default().value()};
    auto encoded_eq = encode_module_parameters_json(eq_payload);
    QVERIFY(encoded_eq);

    auto decoded_eq = decode_module_parameters_json("rgsml.dsp.parametric-eq.parameters/1.0.0", *encoded_eq.value());
    QVERIFY(decoded_eq);
    QVERIFY(std::holds_alternative<ParametricEqParameters>(*decoded_eq.value()));

    // Invalid schema ID rejection
    QVERIFY(!decode_module_parameters_json("rgsml.dsp.unknown.schema/1.0.0", "{}"));
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ModuleParameterCodecTest)

#include "test_module_parameter_codec.moc"
