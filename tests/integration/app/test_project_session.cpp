#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "audition_source_selector.hpp"
#include "audition_region_view_model.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/project/project_repository.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>

#include "../../unit/audio/wav_test_support.hpp"
#include "../../unit/platform/fake_playback_service.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <vector>

namespace rgsml::tests {
namespace {

[[nodiscard]] QString wav(QTemporaryDir& dir, const QString& name, int seed)
{
    std::vector<std::int64_t> samples(256U);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = (static_cast<std::int64_t>(i) + seed) % 100;
    const auto data = wav_support::make_wav(1U, 16U, 1U, 48'000U,
        wav_support::pcm_payload(samples, 16U));
    const auto path = dir.filePath(name);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
        file.write(reinterpret_cast<const char*>(data.data()),
            static_cast<qint64>(data.size())) != static_cast<qint64>(data.size())) return {};
    file.close();
    return path;
}

[[nodiscard]] core::Result<project::ProjectSnapshot> load_project(const QString& path)
{
    const auto utf8 = path.toUtf8().toStdString();
    const auto name = QFileInfo{path}.fileName().toUtf8().toStdString();
    auto reference = platform::windows::WindowsResourceReader::make_read_reference(utf8, name);
    if (!reference) return core::Result<project::ProjectSnapshot>::failure(*reference.error());
    auto reader = platform::windows::WindowsResourceReader::open_read_only(*reference.value());
    if (!reader) return core::Result<project::ProjectSnapshot>::failure(*reader.error());
    return project::ProjectRepository::open(std::move(*reader.value()));
}

[[nodiscard]] core::Status write_project(const QString& path,
                                         const project::ProjectSnapshot& snapshot)
{
    const auto utf8 = path.toUtf8().toStdString();
    const auto name = QFileInfo{path}.fileName().toUtf8().toStdString();
    auto reference = platform::windows::WindowsResourceWriter::make_write_reference(utf8, name);
    if (!reference) return core::Status::failure(*reference.error());
    auto writer = platform::windows::WindowsResourceWriter::open_create_new(*reference.value());
    if (!writer) return core::Status::failure(*writer.error());
    return project::ProjectRepository::save_create_new(snapshot,
        std::move(*writer.value()), [utf8, name]() {
            auto ref = platform::windows::WindowsResourceReader::make_read_reference(utf8, name);
            if (!ref) return core::Result<std::unique_ptr<core::IResourceReader>>::failure(*ref.error());
            auto reader = platform::windows::WindowsResourceReader::open_read_only(*ref.value());
            if (!reader) return core::Result<std::unique_ptr<core::IResourceReader>>::failure(*reader.error());
            return core::Result<std::unique_ptr<core::IResourceReader>>::success(
                std::move(*reader.value()));
        });
}

struct Session final {
    FakePlaybackService* observed;
    app::PlaybackTransportViewModel transport;
    app::SourceSelectionViewModel source;
    app::AuditionSourceSelector selector;
    app::GoldSelectionViewModel gold;
    app::AuditionRegionViewModel region;
    app::ProjectSessionViewModel project;

    Session()
        : observed(new FakePlaybackService()),
          transport(std::unique_ptr<core::IAudioPlaybackService>(observed)),
          source(), selector(&transport), gold(&selector), region(&transport),
          project(&source, &gold, &region, &transport)
    {
        transport.set_pcm_prepare_handler([this](audio::AudioBufferView view) {
            observed->state = core::PlaybackState::STOPPED;
            observed->position = view.absolute_start_frame();
            observed->duration = *core::FrameCount::create(
                view.absolute_end_frame().value()).value();
            return core::Status::success();
        });
        source.set_source_committed_handler([this](const core::ResourceReference& ref) {
            auto count = core::FrameCount::create(source.frame_count());
            auto rate = core::SampleRate::create(source.sample_rate_hz());
            if (count && rate) region.source_committed(*count.value(), *rate.value());
            (void)selector.source_committed(ref);
            (void)selector.switch_to(app::AuditionTarget::PREPARED);
            gold.sourceChanged();
        });
    }
};

}  // namespace

class ProjectSessionTest final : public QObject {
    Q_OBJECT
private slots:
    void source_save_open_existing_fail();
    void gold_region_roundtrip();
    void missing_source_preflight_preserves_live_session();
    void clear_region_and_replace_source_save_as();
    void invalid_gold_and_hardlink_preflight_preserve_session();
    void region_bounds_and_degraded_opaque_preservation();
};

void ProjectSessionTest::source_save_open_existing_fail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);
    QVERIFY(!source.isEmpty());
    Session session;
    QVERIFY(!session.project.can_save_project());
    session.source.selectSource(QUrl::fromLocalFile(source));
    QVERIFY(session.project.can_save_project());
    const auto path = dir.filePath(QStringLiteral("fresh.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(path));
    QCOMPARE(session.project.error_message(), QString());
    QVERIFY(QFile::exists(path));
    auto firstSnapshot = load_project(path);
    QVERIFY(firstSnapshot);
    const auto before = QFile(path).size();
    session.project.saveProjectAs(QUrl::fromLocalFile(path));
    QVERIFY(!session.project.error_message().isEmpty());
    QCOMPARE(QFile(path).size(), before);
    Session restored;
    restored.project.openProject(QUrl::fromLocalFile(path));
    QCOMPARE(restored.project.error_message(), QString());
    QCOMPARE(restored.source.source_resource()->reference().locator(),
             session.source.source_resource()->reference().locator());
    QCOMPARE(restored.project.project_display_name(), QStringLiteral("fresh"));
    const auto anotherPath = dir.filePath(QStringLiteral("another.rgsml"));
    restored.project.saveProjectAs(QUrl::fromLocalFile(anotherPath));
    QCOMPARE(restored.project.error_message(), QString());
    auto secondSnapshot = load_project(anotherPath);
    QVERIFY(secondSnapshot);
    QCOMPARE(secondSnapshot.value()->document().projectId.to_string(),
             firstSnapshot.value()->document().projectId.to_string());
    QCOMPARE(secondSnapshot.value()->document().sourceResourceId.to_string(),
             firstSnapshot.value()->document().sourceResourceId.to_string());
}

void ProjectSessionTest::gold_region_roundtrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);
    const auto gold = wav(dir, QStringLiteral("gold.wav"), 1);
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    session.gold.selectGold(QUrl::fromLocalFile(gold));
    QVERIFY(session.gold.has_gold());
    auto range = core::FrameRange::create(core::FrameIndex{7}, core::FrameIndex{19});
    QVERIFY(range);
    QVERIFY(session.region.set_region(*range.value()));
    const auto path = dir.filePath(QStringLiteral("gold.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(path));
    QCOMPARE(session.project.error_message(), QString());
    Session restored;
    restored.project.openProject(QUrl::fromLocalFile(path));
    QCOMPARE(restored.project.error_message(), QString());
    QVERIFY(restored.gold.has_gold());
    QCOMPARE(restored.region.region()->begin().value(), std::int64_t{7});
    QCOMPARE(restored.region.region()->end().value(), std::int64_t{19});
}

void ProjectSessionTest::missing_source_preflight_preserves_live_session()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    const auto path = dir.filePath(QStringLiteral("missing.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(path));
    QCOMPARE(session.project.error_message(), QString());
    const auto previous = session.source.source_resource()->reference().locator();
    QVERIFY(QFile::remove(source));
    session.project.openProject(QUrl::fromLocalFile(path));
    QVERIFY(!session.project.error_message().isEmpty());
    QCOMPARE(session.source.source_resource()->reference().locator(), previous);
}

void ProjectSessionTest::clear_region_and_replace_source_save_as()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto firstSource = wav(dir, QStringLiteral("first.wav"), 0);
    const auto secondSource = wav(dir, QStringLiteral("second.wav"), 1);
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(firstSource));
    const auto range = core::FrameRange::create(core::FrameIndex{7},
                                               core::FrameIndex{19});
    QVERIFY(range);
    QVERIFY(session.region.set_region(*range.value()));
    const auto firstProject = dir.filePath(QStringLiteral("first.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(firstProject));
    QCOMPARE(session.project.error_message(), QString());

    QVERIFY(session.region.clear_region());
    const auto clearedProject = dir.filePath(QStringLiteral("cleared.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(clearedProject));
    QCOMPARE(session.project.error_message(), QString());
    Session cleared;
    cleared.project.openProject(QUrl::fromLocalFile(clearedProject));
    QCOMPARE(cleared.project.error_message(), QString());
    QVERIFY(!cleared.region.region());

    session.source.selectSource(QUrl::fromLocalFile(secondSource));
    const auto replacedProject = dir.filePath(QStringLiteral("replaced.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(replacedProject));
    QCOMPARE(session.project.error_message(), QString());
    Session replaced;
    replaced.project.openProject(QUrl::fromLocalFile(replacedProject));
    QCOMPARE(replaced.project.error_message(), QString());
    QCOMPARE(replaced.source.source_resource()->reference().locator(),
             session.source.source_resource()->reference().locator());
    QVERIFY(!replaced.region.region());
}

void ProjectSessionTest::invalid_gold_and_hardlink_preflight_preserve_session()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);
    const auto gold = wav(dir, QStringLiteral("gold.wav"), 1);
    const auto live = wav(dir, QStringLiteral("live.wav"), 2);
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(source));
    author.gold.selectGold(QUrl::fromLocalFile(gold));
    QVERIFY(author.gold.has_gold());
    const auto path = dir.filePath(QStringLiteral("gold.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(path));
    QCOMPARE(author.project.error_message(), QString());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(live));
    const auto prior = session.source.source_resource()->reference().locator();
    QVERIFY(QFile::remove(gold));
    session.project.openProject(QUrl::fromLocalFile(path));
    QVERIFY(!session.project.error_message().isEmpty());
    QCOMPARE(session.source.source_resource()->reference().locator(), prior);
    std::error_code ec;
    std::filesystem::create_hard_link(std::filesystem::path(source.toStdWString()),
                                      std::filesystem::path(gold.toStdWString()), ec);
    QVERIFY2(!ec, ec.message().c_str());
    session.project.openProject(QUrl::fromLocalFile(path));
    QVERIFY(!session.project.error_message().isEmpty());
    QCOMPARE(session.source.source_resource()->reference().locator(), prior);
}

void ProjectSessionTest::region_bounds_and_degraded_opaque_preservation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);
    const auto live = wav(dir, QStringLiteral("live.wav"), 1);
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(source));
    const auto first = dir.filePath(QStringLiteral("first.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(first));
    QCOMPARE(author.project.error_message(), QString());
    auto original = load_project(first);
    QVERIFY(original);
    auto doc = original.value()->document();
    auto regionId = core::Uuid::parse("a4444444-4444-4444-8444-444444444444");
    QVERIFY(regionId);
    doc.timeSelections.auditionRegions.push_back({*regionId.value(),
        doc.sourceResourceId, "SOURCE", 1, 1000});
    auto invalidRegion = project::ProjectSnapshot::create(doc);
    QVERIFY(invalidRegion);
    const auto badPath = dir.filePath(QStringLiteral("out-of-range.rgsml"));
    QVERIFY(write_project(badPath, *invalidRegion.value()));
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(live));
    const auto prior = session.source.source_resource()->reference().locator();
    session.project.openProject(QUrl::fromLocalFile(badPath));
    QVERIFY(!session.project.error_message().isEmpty());
    QCOMPARE(session.source.source_resource()->reference().locator(), prior);

    doc = original.value()->document();
    auto future = project::OpaqueJsonValue::parse("{\"future\":{\"enabled\":true}}");
    QVERIFY(future);
    doc.extensions = *future.value();
    auto degraded = project::ProjectSnapshot::create(doc);
    QVERIFY(degraded);
    const auto futurePath = dir.filePath(QStringLiteral("future.rgsml"));
    QVERIFY(write_project(futurePath, *degraded.value()));
    session.project.openProject(QUrl::fromLocalFile(futurePath));
    QCOMPARE(session.project.error_message(), QString());
    QVERIFY(session.project.degraded());
    QVERIFY(session.project.status_text().contains(QStringLiteral("degraded")));
    const auto saved = dir.filePath(QStringLiteral("preserved.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(saved));
    QCOMPARE(session.project.error_message(), QString());
    auto reopened = load_project(saved);
    QVERIFY(reopened);
    QCOMPARE(reopened.value()->document().extensions.canonical_utf8(),
             future.value()->canonical_utf8());
}

}  // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::ProjectSessionTest)
#include "test_project_session.moc"
