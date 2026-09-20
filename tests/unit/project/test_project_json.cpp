#include "canonical_json.hpp"

#include <rgsml/project/project_snapshot.hpp>

#include <QtTest/QtTest>

#include <bit>
#include <cstdint>
#include <limits>

namespace {

using namespace rgsml;

[[nodiscard]] core::Uuid id(const char* text)
{
    auto parsed = core::Uuid::parse(text);
    Q_ASSERT(parsed);
    return *parsed.value();
}

[[nodiscard]] project::ProjectDocument sample()
{
    project::ProjectDocument doc;
    doc.projectId = id("a1111111-1111-4111-8111-111111111111");
    doc.displayName = "écho";
    doc.sourceResourceId = id("a2222222-2222-4222-8222-222222222222");
    doc.resources.push_back({doc.sourceResourceId, "AUDIO",
        {{"rgsml.windows.local-file", "C:/test/source.wav", "source.wav"}}, std::nullopt});
    return doc;
}

class ProjectJsonTest final : public QObject {
    Q_OBJECT
private slots:
    void source_roundtrip();
    void region_gold_chain_roundtrip();
    void strict_rejections();
    void numeric_exactness();
    void future_sections_and_gain_flags_roundtrip();
    void schema_and_revision_overflow_rejections();
};

void ProjectJsonTest::source_roundtrip()
{
    const auto original = sample();
    auto encoded = project::internal::write_project_json(original);
    QVERIFY(encoded);
    QVERIFY(encoded.value()->find('\n') == std::string::npos);
    QVERIFY(encoded.value()->find("\xEF\xBB\xBF") == std::string::npos);
    QVERIFY(encoded.value()->starts_with("{\"analyses\":"));
    auto parsed = project::internal::parse_project_json(*encoded.value());
    QVERIFY(parsed);
    QCOMPARE(parsed.value()->projectId.to_string(), original.projectId.to_string());
    QCOMPARE(parsed.value()->displayName, original.displayName);
    auto again = project::internal::write_project_json(*parsed.value());
    QVERIFY(again);
    QCOMPARE(*again.value(), *encoded.value());
}

void ProjectJsonTest::region_gold_chain_roundtrip()
{
    auto doc = sample();
    const auto goldId = id("a3333333-3333-4333-8333-333333333333");
    const auto refId = id("a4444444-4444-4444-8444-444444444444");
    doc.resources.push_back({goldId, "AUDIO",
        {{"rgsml.windows.local-file", "C:/test/gold.wav", "gold.wav"}}, std::nullopt});
    doc.references.activeReferenceId = refId;
    doc.references.items.push_back({refId, "GOLD", goldId});
    doc.timeSelections.auditionRegions.push_back({
        id("a5555555-5555-4555-8555-555555555555"), doc.sourceResourceId,
        "SOURCE", 9007199254740993LL, 9007199254740994LL});
    project::Chain chain;
    chain.chainId = id("a6666666-6666-4666-8666-666666666666");
    chain.stage = "MASTER";
    chain.segment = "MANUAL";
    chain.revision = std::numeric_limits<std::uint64_t>::max();
    project::Module module;
    module.instanceId = id("a7777777-7777-4777-8777-777777777777");
    module.typeId = "future.unknown.module";
    module.parameters = *project::OpaqueJsonValue::parse(
        "{\"z\":[3,1],\"gainDb\":0.33333333333333331}").value();
    chain.modules.push_back(module);
    doc.chains.push_back(chain);
    doc.pipeline.masteringChainId = chain.chainId;
    doc.extensions = *project::OpaqueJsonValue::parse("{\"future\":{\"x\":true}}").value();
    auto encoded = project::internal::write_project_json(doc);
    QVERIFY(encoded);
    auto parsed = project::internal::parse_project_json(*encoded.value());
    QVERIFY(parsed);
    QCOMPARE(parsed.value()->timeSelections.auditionRegions.front().startFrame,
             9007199254740993LL);
    QCOMPARE(parsed.value()->chains.front().revision,
             std::numeric_limits<std::uint64_t>::max());
    QCOMPARE(parsed.value()->chains.front().modules.front().parameters.canonical_utf8(),
             module.parameters.canonical_utf8());
    QCOMPARE(parsed.value()->extensions.canonical_utf8(), doc.extensions.canonical_utf8());
}

void ProjectJsonTest::strict_rejections()
{
    QVERIFY(!project::OpaqueJsonValue::parse("{\"a\":1,\"a\":2}"));
    QVERIFY(!project::OpaqueJsonValue::parse("[1,]"));
    QVERIFY(!project::OpaqueJsonValue::parse("\xEF\xBB\xBF{}"));
    QVERIFY(!project::OpaqueJsonValue::parse("1e309"));
    QVERIFY(!project::OpaqueJsonValue::parse("18446744073709551616"));
    QVERIFY(!project::OpaqueJsonValue::parse(std::string(65, '[') + "0" + std::string(65, ']')));
    auto encoded = project::internal::write_project_json(sample());
    QVERIFY(encoded);
    auto wrong = *encoded.value();
    const auto pos = wrong.find("\"formatId\":\"rgsml-project\"");
    QVERIFY(pos != std::string::npos);
    wrong.replace(pos, std::string("\"formatId\":\"rgsml-project\"").size(),
                  "\"formatId\":\"wrong\"");
    QVERIFY(!project::internal::parse_project_json(wrong));
}

void ProjectJsonTest::numeric_exactness()
{
    auto value = project::OpaqueJsonValue::parse("-0.0");
    QVERIFY(value);
    QCOMPARE(value.value()->canonical_utf8(), std::string("-0.0"));
    auto hard = project::OpaqueJsonValue::parse("0.33333333333333331");
    QVERIFY(hard);
    auto again = project::OpaqueJsonValue::parse(hard.value()->canonical_utf8());
    QVERIFY(again);
    QCOMPARE(again.value()->canonical_utf8(), hard.value()->canonical_utf8());
}

void ProjectJsonTest::future_sections_and_gain_flags_roundtrip()
{
    auto doc = sample();
    project::Chain chain;
    chain.chainId = id("a6666666-6666-4666-8666-666666666666");
    chain.stage = "MASTER";
    chain.segment = "MANUAL";
    chain.revision = 9007199254740993ULL;
    project::Module gain;
    gain.instanceId = id("a7777777-7777-4777-8777-777777777777");
    gain.typeId = "rgsml.dsp.gain";
    gain.enabled = false;
    gain.userBypass = true;
    gain.controllerSuspended = true;
    gain.domainSuspended = true;
    gain.algorithmVersion = "1.0.0";
    gain.parameterSchemaId = "rgsml.dsp.gain.parameters/1.0.0";
    gain.parameters = *project::OpaqueJsonValue::parse(
        "{\"gainDb\":0.33333333333333331}").value();
    chain.modules.push_back(gain);
    doc.chains.push_back(chain);
    doc.pipeline.masteringChainId = chain.chainId;
    doc.analyses = *project::OpaqueJsonValue::parse("[{\"unknown\":9}]").value();
    doc.plans = *project::OpaqueJsonValue::parse("[{\"mode\":\"future\"}]").value();
    doc.referenceMatch = *project::OpaqueJsonValue::parse("{\"future\":true}").value();
    doc.masteringDNA = *project::OpaqueJsonValue::parse("{\"signature\":[1,2]}").value();
    doc.output = *project::OpaqueJsonValue::parse("{\"futureOutput\":{}}").value();
    doc.derivedArtifacts = *project::OpaqueJsonValue::parse("[{\"id\":1}]").value();
    doc.history = *project::OpaqueJsonValue::parse("[{\"event\":\"future\"}]").value();
    auto encoded = project::internal::write_project_json(doc);
    QVERIFY(encoded);
    auto parsed = project::internal::parse_project_json(*encoded.value());
    QVERIFY(parsed);
    QCOMPARE(parsed.value()->chains.front().revision, 9007199254740993ULL);
    const auto& restored = parsed.value()->chains.front().modules.front();
    QVERIFY(!restored.enabled);
    QVERIFY(restored.userBypass && restored.controllerSuspended && restored.domainSuspended);
    QCOMPARE(restored.parameters.canonical_utf8(), gain.parameters.canonical_utf8());
    QCOMPARE(parsed.value()->analyses.canonical_utf8(), doc.analyses.canonical_utf8());
    QCOMPARE(parsed.value()->plans.canonical_utf8(), doc.plans.canonical_utf8());
    QCOMPARE(parsed.value()->referenceMatch.canonical_utf8(), doc.referenceMatch.canonical_utf8());
    QCOMPARE(parsed.value()->masteringDNA.canonical_utf8(), doc.masteringDNA.canonical_utf8());
    QCOMPARE(parsed.value()->output.canonical_utf8(), doc.output.canonical_utf8());
    QCOMPARE(parsed.value()->derivedArtifacts.canonical_utf8(),
             doc.derivedArtifacts.canonical_utf8());
    QCOMPARE(parsed.value()->history.canonical_utf8(), doc.history.canonical_utf8());
}

void ProjectJsonTest::schema_and_revision_overflow_rejections()
{
    auto encoded = project::internal::write_project_json(sample());
    QVERIFY(encoded);
    auto unknown = *encoded.value();
    const auto resourcePos = unknown.find("\"resourceType\":\"AUDIO\"");
    QVERIFY(resourcePos != std::string::npos);
    unknown.insert(resourcePos, "\"typo\":true,");
    QVERIFY(!project::internal::parse_project_json(unknown));
    QVERIFY(!project::OpaqueJsonValue::parse("18446744073709551616"));
    QVERIFY(!project::OpaqueJsonValue::parse("-9223372036854775809"));
    QVERIFY(!project::OpaqueJsonValue::parse("{\"a\":1,\"a\":2}"));
}

}  // namespace

QTEST_GUILESS_MAIN(ProjectJsonTest)
#include "test_project_json.moc"
