#include <rgsml/project/m18_master_wav_preference.hpp>
#include <rgsml/project/master_output_rate_preference.hpp>
#include <QtTest>
#include <cstdint>
#include <limits>
#include <string>
#include <optional>

namespace rgsml::tests {

class M18MasterWavPreferenceTest final : public QObject {
    Q_OBJECT
private slots:
    void legacyOutputNeverImpliesApprovedWavDelivery();
    void pcm24FullUint64AndMasterRateRoundtrip();
    void float32RemovesSeedAndPreservesUnknown();
    void malformedPartialAndDraftKeysFailClosed();
};

void M18MasterWavPreferenceTest::
legacyOutputNeverImpliesApprovedWavDelivery()
{
    using project::M18MasterWavPreferenceCodec;
    const auto empty=project::OpaqueJsonValue::empty_object();
    const auto absent=M18MasterWavPreferenceCodec::read(empty);
    QVERIFY(absent);
    QVERIFY(!absent.value()->has_value());
    QVERIFY(project::MasterOutputRatePreference::only_known_fields(empty));
    const auto withRate=project::MasterOutputRatePreference::with_target(
        empty,192000);
    QVERIFY(withRate);
    const auto noChoice=M18MasterWavPreferenceCodec::read(*withRate.value());
    QVERIFY(noChoice);
    QVERIFY(!noChoice.value()->has_value());
    QVERIFY(project::MasterOutputRatePreference::only_known_fields(
        *withRate.value()));
}

void M18MasterWavPreferenceTest::
pcm24FullUint64AndMasterRateRoundtrip()
{
    using project::M18MasterWavPreferenceCodec;
    using project::M18MasterWavPreference;
    using project::M18MasterWavEncoding;
    const auto withRate=project::MasterOutputRatePreference::with_target(
        project::OpaqueJsonValue::empty_object(),88200);
    QVERIFY(withRate);
    for(const std::uint64_t seed:{
        UINT64_C(0),UINT64_C(1),UINT64_C(9007199254740993),
        std::numeric_limits<std::uint64_t>::max()}) {
        const M18MasterWavPreference choice{
            M18MasterWavEncoding::PCM24_TPDF,seed};
        const auto saved=M18MasterWavPreferenceCodec::with_choice(
            *withRate.value(),choice);
        QVERIFY(saved);
        const auto read=M18MasterWavPreferenceCodec::read(*saved.value());
        QVERIFY(read);
        QVERIFY(read.value()->has_value());
        QCOMPARE(read.value()->value(),choice);
        QVERIFY(saved.value()->canonical_utf8().find(std::to_string(seed))
            !=std::string::npos);
        QVERIFY(project::MasterOutputRatePreference::only_known_fields(
            *saved.value()));
        const auto rate=project::MasterOutputRatePreference::read(
            *saved.value());
        QVERIFY(rate);
        QVERIFY(rate.value()->has_value());
        QCOMPARE(rate.value()->value(),std::int64_t{88200});
    }
}

void M18MasterWavPreferenceTest::
float32RemovesSeedAndPreservesUnknown()
{
    using project::M18MasterWavPreferenceCodec;
    using project::M18MasterWavEncoding;
    using project::M18MasterWavPreference;
    auto unknown=project::OpaqueJsonValue::parse(
        "{\"targetMasterOutputRateHz\":48000,"
        "\"futureOutput\":{\"nested\":[1,{\"x\":true}]}}");
    QVERIFY(unknown);
    const auto pcm=M18MasterWavPreferenceCodec::with_choice(
        *unknown.value(),
        {M18MasterWavEncoding::PCM24_TPDF,
         std::numeric_limits<std::uint64_t>::max()});
    QVERIFY(pcm);
    const M18MasterWavPreference f32{
        M18MasterWavEncoding::IEEE_FLOAT32,std::nullopt};
    const auto floatSaved=M18MasterWavPreferenceCodec::with_choice(
        *pcm.value(),f32);
    QVERIFY(floatSaved);
    const auto json=floatSaved.value()->canonical_utf8();
    QVERIFY(json.find("finalMasterPcm24TpdfSeed")==std::string::npos);
    QVERIFY(json.find("futureOutput")!=std::string::npos);
    QVERIFY(json.find("targetMasterOutputRateHz")!=std::string::npos);
    const auto reloaded=M18MasterWavPreferenceCodec::read(
        *floatSaved.value());
    QVERIFY(reloaded);
    QCOMPARE(reloaded.value()->value(),f32);
    QVERIFY(!project::MasterOutputRatePreference::only_known_fields(
        *floatSaved.value()));
    const auto clean=M18MasterWavPreferenceCodec::with_choice(
        project::OpaqueJsonValue::empty_object(),f32);
    QVERIFY(clean);
    QVERIFY(project::MasterOutputRatePreference::only_known_fields(
        *clean.value()));
    QVERIFY(!M18MasterWavPreferenceCodec::with_choice(
        project::OpaqueJsonValue::empty_object(),
        {M18MasterWavEncoding::PCM24_TPDF,std::nullopt}));
    QVERIFY(!M18MasterWavPreferenceCodec::with_choice(
        project::OpaqueJsonValue::empty_object(),
        {M18MasterWavEncoding::IEEE_FLOAT32,12U}));
}

void M18MasterWavPreferenceTest::
malformedPartialAndDraftKeysFailClosed()
{
    using project::M18MasterWavPreferenceCodec;
    using project::OpaqueJsonValue;
    for(const char* malformed:{
        "[]","null",
        "{\"finalMasterPcm24TpdfSeed\":1}",
        "{\"finalMasterWavEncoding\":\"PCM24_TPDF\"}",
        "{\"finalMasterWavEncoding\":\"PCM24_TPDF\",\"finalMasterPcm24TpdfSeed\":-1}",
        "{\"finalMasterWavEncoding\":\"PCM24_TPDF\",\"finalMasterPcm24TpdfSeed\":1.0}",
        "{\"finalMasterWavEncoding\":\"PCM24_TPDF\",\"finalMasterPcm24TpdfSeed\":null}",
        "{\"finalMasterWavEncoding\":\"PCM24_TPDF\",\"finalMasterPcm24TpdfSeed\":true}",
        "{\"finalMasterWavEncoding\":\"PCM24_TPDF\",\"finalMasterPcm24TpdfSeed\":\"42\"}",
        "{\"finalMasterWavEncoding\":\"IEEE_FLOAT32\",\"finalMasterPcm24TpdfSeed\":1}",
        "{\"finalMasterWavEncoding\":\"IEEE_FLOAT16\"}"}) {
        const auto source=OpaqueJsonValue::parse(malformed);
        QVERIFY(source);
        QVERIFY(!M18MasterWavPreferenceCodec::read(*source.value()));
    }
    const auto experimental=OpaqueJsonValue::parse(
        "{\"m18DraftTargetEncoding\":\"PCM24\","
        "\"m18DraftDitherSeed\":7}");
    QVERIFY(experimental);
    const auto absent=M18MasterWavPreferenceCodec::read(
        *experimental.value());
    QVERIFY(absent);
    QVERIFY(!absent.value()->has_value());
    QVERIFY(!project::MasterOutputRatePreference::only_known_fields(
        *experimental.value()));
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18MasterWavPreferenceTest)
#include "test_m18_master_wav_preference.moc"
