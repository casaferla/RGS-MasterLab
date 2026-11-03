#include <rgsml/project/master_output_rate_preference.hpp>
#include <QtTest>
namespace rgsml::tests {
class MasterOutputRateCodecTest final : public QObject {
Q_OBJECT
private slots:
    void legacyAndSixRates() {
        using rgsml::project::MasterOutputRatePreference;
        const auto empty=rgsml::project::OpaqueJsonValue::empty_object();
        auto missing=MasterOutputRatePreference::read(empty);
        QVERIFY(missing);QVERIFY(!missing.value()->has_value());
        QVERIFY(MasterOutputRatePreference::only_known_fields(empty));
        for(const std::int64_t rate:{44100,48000,88200,96000,176400,192000}) {
            auto encoded=MasterOutputRatePreference::with_target(empty,rate);
            QVERIFY(encoded);
            auto decoded=MasterOutputRatePreference::read(*encoded.value());
            QVERIFY(decoded);QVERIFY(decoded.value()->has_value());
            QCOMPARE(decoded.value()->value(),rate);
            QVERIFY(MasterOutputRatePreference::only_known_fields(*encoded.value()));
        }
        QVERIFY(!MasterOutputRatePreference::with_target(empty,384000));
        QVERIFY(!MasterOutputRatePreference::with_target(empty,32000));
    }
    void rejectMalformedAndPreserveUnknown() {
        using rgsml::project::MasterOutputRatePreference;
        using rgsml::project::OpaqueJsonValue;
        for(const char* malformed:{
            "{\"targetMasterOutputRateHz\":32000}",
            "{\"targetMasterOutputRateHz\":\"48000\"}",
            "{\"targetMasterOutputRateHz\":48000.0}",
            "{\"targetMasterOutputRateHz\":null}",
            "{\"targetMasterOutputRateHz\":[]}"}) {
            auto value=OpaqueJsonValue::parse(malformed);QVERIFY(value);
            QVERIFY(!MasterOutputRatePreference::read(*value.value()));
            QVERIFY(!MasterOutputRatePreference::with_target(*value.value(),96000));
            QVERIFY(!MasterOutputRatePreference::only_known_fields(*value.value()));
        }
        auto future=OpaqueJsonValue::parse("{\"futureOutput\":{\"rev\":4}}");
        QVERIFY(future);
        QVERIFY(!MasterOutputRatePreference::only_known_fields(*future.value()));
        auto plus=MasterOutputRatePreference::with_target(*future.value(),192000);
        QVERIFY(plus);
        auto encoded=plus.value()->canonical_utf8();
        QVERIFY(encoded.find("futureOutput")!=std::string::npos);
        QVERIFY(!MasterOutputRatePreference::only_known_fields(*plus.value()));
        auto recovered=MasterOutputRatePreference::read(*plus.value());
        QVERIFY(recovered);QCOMPARE(recovered.value()->value(),std::int64_t{192000});
        auto invalid=OpaqueJsonValue::parse("[]");QVERIFY(invalid);
        QVERIFY(!MasterOutputRatePreference::with_target(*invalid.value(),44100));
    }
};
}
QTEST_GUILESS_MAIN(rgsml::tests::MasterOutputRateCodecTest)
#include "test_master_output_rate_codec.moc"
