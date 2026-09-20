#include <rgsml/project/project_snapshot.hpp>

#include <QtTest/QtTest>

namespace {

[[nodiscard]] rgsml::core::Uuid id(const char* text)
{
    auto parsed = rgsml::core::Uuid::parse(text);
    Q_ASSERT(parsed);
    return *parsed.value();
}

[[nodiscard]] rgsml::project::ProjectDocument valid_document()
{
    rgsml::project::ProjectDocument doc;
    doc.projectId = id("a1111111-1111-4111-8111-111111111111");
    doc.displayName = "Test";
    doc.sourceResourceId = id("a2222222-2222-4222-8222-222222222222");
    doc.resources.push_back({doc.sourceResourceId, "AUDIO",
        {{"rgsml.windows.local-file", "C:/test/source.wav", "source.wav"}}, std::nullopt});
    return doc;
}

class ProjectSnapshotTest final : public QObject {
    Q_OBJECT
private slots:
    void source_only();
    void invalid_ids_and_foreign_keys();
    void region_half_open();
    void optional_paths();
};

void ProjectSnapshotTest::source_only()
{
    auto result = rgsml::project::ProjectSnapshot::create(valid_document());
    QVERIFY(result);
    QCOMPARE(result.value()->document().resources.size(), 1U);
}

void ProjectSnapshotTest::invalid_ids_and_foreign_keys()
{
    auto doc = valid_document();
    doc.projectId = {};
    QVERIFY(!rgsml::project::ProjectSnapshot::create(doc));
    doc = valid_document();
    doc.resources.push_back(doc.resources.front());
    QVERIFY(!rgsml::project::ProjectSnapshot::create(doc));
    doc = valid_document();
    doc.sourceResourceId = id("a3333333-3333-4333-8333-333333333333");
    QVERIFY(!rgsml::project::ProjectSnapshot::create(doc));
}

void ProjectSnapshotTest::region_half_open()
{
    auto doc = valid_document();
    doc.timeSelections.auditionRegions.push_back({
        id("a4444444-4444-4444-8444-444444444444"), doc.sourceResourceId,
        "SOURCE", 1, 2});
    QVERIFY(rgsml::project::ProjectSnapshot::create(doc));
    doc.timeSelections.auditionRegions.front().endFrameExclusive = 1;
    QVERIFY(!rgsml::project::ProjectSnapshot::create(doc));
}

void ProjectSnapshotTest::optional_paths()
{
    auto doc = valid_document();
    QVERIFY(rgsml::project::ProjectSnapshot::create(doc,
        {{"artifacts/item.bin", "application/octet-stream", {std::byte{0x42}}}}));
    QVERIFY(!rgsml::project::ProjectSnapshot::create(doc,
        {{"artifacts/../item.bin", "application/octet-stream", {std::byte{0x42}}}}));
}

}  // namespace

QTEST_GUILESS_MAIN(ProjectSnapshotTest)
#include "test_project_snapshot.moc"
