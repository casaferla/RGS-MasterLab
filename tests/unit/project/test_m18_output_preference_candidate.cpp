// Candidate-only M18 JSON/seed projection: not activated in ProjectSession.
// Exact uint64 numeric JSON must NEVER travel via IEEE floating point.
#include <rgsml/project/m18_output_preference_candidate.hpp>
#include <rgsml/project/master_output_rate_preference.hpp>
#include <QtTest>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace rgsml::tests {
class M18OutputPreferenceCandidateTest final : public QObject {
    Q_OBJECT
private slots:
    void legacyOutputIsAbsentNotAnImplicitFormat();
    void exactUnsignedSeedsAndUnknownPreservation();
    void float32NeverRetainsPcmDitherSeed();
    void failClosedForMalformedAndPartialValues();
};

void M18OutputPreferenceCandidateTest::legacyOutputIsAbsentNotAnImplicitFormat()
{
    using project::M18OutputPreferenceCandidateCodec;
    const auto empty=project::OpaqueJsonValue::empty_object();
    const auto observed=M18OutputPreferenceCandidateCodec::read_candidate(empty);
    QVERIFY(observed);
    QVERIFY(!observed.value()->has_value());
    const auto legacy=project::MasterOutputRatePreference::with_target(
        empty,96000);
    QVERIFY(legacy);
    const auto observedLegacy=M18OutputPreferenceCandidateCodec::read_candidate(
        *legacy.value());
    QVERIFY(observedLegacy);
    QVERIFY(!observedLegacy.value()->has_value());
    QVERIFY(project::MasterOutputRatePreference::only_known_fields(
        *legacy.value()));
}

void M18OutputPreferenceCandidateTest::exactUnsignedSeedsAndUnknownPreservation()
{
    using project::M18OutputPreferenceCandidateCodec;
    using project::M18OutputEncodingCandidate;
    using project::M18OutputPreferenceCandidate;
    const auto future=project::OpaqueJsonValue::parse(
        "{\"targetMasterOutputRateHz\":192000,"
        "\"futureOutput\":{\"nested\":[1,{\"replay\":true}]},"
        "\"thirdParty\":\"do not discard\"}");
    QVERIFY(future);
    for(const std::uint64_t seed:{
        UINT64_C(0),UINT64_C(1),UINT64_C(9007199254740993),
        std::numeric_limits<std::uint64_t>::max()}) {
        const M18OutputPreferenceCandidate selected{
            M18OutputEncodingCandidate::PCM24,seed};
        const auto saved=M18OutputPreferenceCandidateCodec::with_candidate(
            *future.value(),selected);
        QVERIFY(saved);
        const auto loaded=M18OutputPreferenceCandidateCodec::read_candidate(
            *saved.value());
        QVERIFY(loaded);
        QVERIFY(loaded.value()->has_value());
        QCOMPARE(loaded.value()->value(),selected);
        const auto json=saved.value()->canonical_utf8();
        QVERIFY(json.find(std::to_string(seed))!=std::string::npos);
        QVERIFY(json.find("futureOutput")!=std::string::npos);
        QVERIFY(json.find("thirdParty")!=std::string::npos);
        const auto rate=project::MasterOutputRatePreference::read(*saved.value());
        QVERIFY(rate);
        QVERIFY(rate.value()->has_value());
        QCOMPARE(rate.value()->value(),std::int64_t{192000});
        // Until final key approval, Project/Render must treat this as
        // UNKNOWN future output rather than silently granting export.
        QVERIFY(!project::MasterOutputRatePreference::only_known_fields(
            *saved.value()));
    }
}

void M18OutputPreferenceCandidateTest::float32NeverRetainsPcmDitherSeed()
{
    using project::M18OutputPreferenceCandidateCodec;
    using project::M18OutputEncodingCandidate;
    using project::M18OutputPreferenceCandidate;
    const auto empty=project::OpaqueJsonValue::empty_object();
    const auto pcm=M18OutputPreferenceCandidateCodec::with_candidate(
        empty,{M18OutputEncodingCandidate::PCM24,UINT64_MAX});
    QVERIFY(pcm);
    const M18OutputPreferenceCandidate floatChoice{
        M18OutputEncodingCandidate::IEEE_FLOAT32,std::nullopt};
    const auto floatSaved=M18OutputPreferenceCandidateCodec::with_candidate(
        *pcm.value(),floatChoice);
    QVERIFY(floatSaved);
    QVERIFY(floatSaved.value()->canonical_utf8().find("m18DraftDitherSeed")
        ==std::string::npos);
    const auto loaded=M18OutputPreferenceCandidateCodec::read_candidate(
        *floatSaved.value());
    QVERIFY(loaded);
    QVERIFY(loaded.value()->has_value());
    QCOMPARE(loaded.value()->value(),floatChoice);
    const auto restored=M18OutputPreferenceCandidateCodec::with_candidate(
        *floatSaved.value(),{M18OutputEncodingCandidate::PCM24,42U});
    QVERIFY(restored);
    const auto round=M18OutputPreferenceCandidateCodec::read_candidate(
        *restored.value());
    QVERIFY(round);
    QCOMPARE(round.value()->value().dither_seed.value(),std::uint64_t{42});
    QVERIFY(!M18OutputPreferenceCandidateCodec::with_candidate(
        empty,{M18OutputEncodingCandidate::PCM24,std::nullopt}));
    QVERIFY(!M18OutputPreferenceCandidateCodec::with_candidate(
        empty,{M18OutputEncodingCandidate::IEEE_FLOAT32,UINT64_C(7)}));
}

void M18OutputPreferenceCandidateTest::failClosedForMalformedAndPartialValues()
{
    using project::M18OutputPreferenceCandidateCodec;
    using project::OpaqueJsonValue;
    for(const char* malformed:{
        "[]","null",
        "{\"m18DraftDitherSeed\":7}",
        "{\"m18DraftTargetEncoding\":\"PCM24\"}",
        "{\"m18DraftTargetEncoding\":\"PCM24\",\"m18DraftDitherSeed\":null}",
        "{\"m18DraftTargetEncoding\":\"PCM24\",\"m18DraftDitherSeed\":-1}",
        "{\"m18DraftTargetEncoding\":\"PCM24\",\"m18DraftDitherSeed\":1.0}",
        "{\"m18DraftTargetEncoding\":\"PCM24\",\"m18DraftDitherSeed\":true}",
        "{\"m18DraftTargetEncoding\":\"PCM24\",\"m18DraftDitherSeed\":\"1\"}",
        "{\"m18DraftTargetEncoding\":\"FLOAT32\",\"m18DraftDitherSeed\":0}",
        "{\"m18DraftTargetEncoding\":\"FLOAT16\"}",
        "{\"m18DraftTargetEncoding\":0}"}) {
        const auto doc=OpaqueJsonValue::parse(malformed);
        QVERIFY(doc);
        QVERIFY(!M18OutputPreferenceCandidateCodec::read_candidate(*doc.value()));
        QVERIFY(!M18OutputPreferenceCandidateCodec::with_candidate(
            *doc.value(),{project::M18OutputEncodingCandidate::PCM24,1U}));
    }
    // Values above uint64 maximum may be rejected by strict project SAX
    // before this candidate codec sees them; either layer must fail closed.
    const auto overflowing=OpaqueJsonValue::parse(
        "{\"m18DraftTargetEncoding\":\"PCM24\","
        "\"m18DraftDitherSeed\":18446744073709551616}");
    if(overflowing)
        QVERIFY(!M18OutputPreferenceCandidateCodec::read_candidate(
            *overflowing.value()));
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18OutputPreferenceCandidateTest)
#include "test_m18_output_preference_candidate.moc"
