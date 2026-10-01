#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "audition_source_selector.hpp"
#include "audition_region_view_model.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/dsp/module_registry.hpp>
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
    core::Uuid chainId;
    dsp::ModuleInstanceId gainId;
    dsp::ModuleInstanceId eqId;
    std::unique_ptr<app::MasteringChainState> masteringChainState;
    std::unique_ptr<app::MasteringPreviewController> previewController;
    std::unique_ptr<app::GainViewModel> gainViewModel;
    std::unique_ptr<app::EqViewModel> eqViewModel;
    app::ProjectSessionViewModel project;

    Session()
        : observed(new FakePlaybackService()),
          transport(std::unique_ptr<core::IAudioPlaybackService>(observed)),
          source(), selector(&transport), gold(&selector), region(&transport),
          chainId(*core::Uuid::parse("10000000-0000-4000-8000-000000000001").value()),
          gainId(*dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value()),
          eqId(*dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value()),
          masteringChainState([this] {
              auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
              auto state = app::MasteringChainState::create_default(*registry.value(), chainId, gainId, eqId);
              return std::make_unique<app::MasteringChainState>(std::move(*state.value()));
          }()),
          previewController(std::make_unique<app::MasteringPreviewController>(masteringChainState.get())),
          gainViewModel(std::make_unique<app::GainViewModel>(masteringChainState.get(), previewController.get())),
          eqViewModel(std::make_unique<app::EqViewModel>(masteringChainState.get(), previewController.get())),
          project(&source, &gold, &region, &transport, masteringChainState.get(),
                  gainViewModel.get(), eqViewModel.get(), previewController.get())
    {
        transport.set_pcm_prepare_handler([this](audio::AudioBufferView view, std::shared_ptr<const void>) {
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
    void externally_opened_processing_preserved_not_overwritten();
    void mastering_chain_normal_save();
    void mastering_chain_repeated_save();
    void mastering_chain_source_replacement_regression();
    void supported_b4_open_and_viewmodel_refresh();
    void open_save_as_identity_revision_preservation();
    void atomic_rejection();
    void legacy_pre_b4_materialization_and_save_as();
    void future_opaque_preservation_and_non_degraded_b4();
    void source_reset_vs_project_open();
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

void ProjectSessionTest::externally_opened_processing_preserved_not_overwritten()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session author;
    author.source.selectSource(QUrl::fromLocalFile(source));
    const auto first = dir.filePath(QStringLiteral("first.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(first));
    QCOMPARE(author.project.error_message(), QString());

    auto original = load_project(first);
    QVERIFY(original);
    auto doc = original.value()->document();

    // Attach external chain ID & extension metadata
    const auto externalChainId = *core::Uuid::parse("90000000-0000-4000-8000-000000000001").value();
    doc.pipeline.masteringChainId = externalChainId;
    project::Chain extChain;
    extChain.chainId = externalChainId;
    extChain.stage = "MASTER";
    extChain.segment = "MANUAL";
    doc.chains.clear();
    doc.chains.push_back(extChain);

    auto future = project::OpaqueJsonValue::parse("{\"future\":{\"enabled\":true}}");
    QVERIFY(future);
    doc.extensions = *future.value();

    auto externalProject = project::ProjectSnapshot::create(doc);
    QVERIFY(externalProject);
    const auto externalPath = dir.filePath(QStringLiteral("external.rgsml"));
    QVERIFY(write_project(externalPath, *externalProject.value()));

    // Open external project into session with active MasteringChainState
    Session session;
    session.project.openProject(QUrl::fromLocalFile(externalPath));
    QCOMPARE(session.project.error_message(), QString());

    // Save As with unchanged Source must succeed and preserve external processing state
    const auto savedPath = dir.filePath(QStringLiteral("saved_external.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(savedPath));
    QCOMPARE(session.project.error_message(), QString());

    auto reloaded = load_project(savedPath);
    QVERIFY(reloaded);
    const auto& reloadedDoc = reloaded.value()->document();

    // Verify external masteringChainId and chains are exactly preserved, NOT replaced by live B4 chain
    QVERIFY(reloadedDoc.pipeline.masteringChainId.has_value());
    QCOMPARE(*reloadedDoc.pipeline.masteringChainId, externalChainId);
    QCOMPARE(reloadedDoc.chains.size(), std::size_t{1});
    QCOMPARE(reloadedDoc.chains[0].chainId, externalChainId);
    QCOMPARE(reloadedDoc.extensions.canonical_utf8(), future.value()->canonical_utf8());
}

void ProjectSessionTest::mastering_chain_normal_save()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));

    // Configure non-default Gain
    auto gainParams = dsp::GainParameters::create(6.0);
    QVERIFY(gainParams);
    QVERIFY(session.masteringChainState->set_gain_parameters(*gainParams.value()));
    QVERIFY(session.masteringChainState->set_user_bypass(session.gainId, true));

    // Configure genuinely non-default EQ
    auto bandId1 = core::Uuid::parse("10000000-0000-0000-0000-000000000001");
    auto bandId2 = core::Uuid::parse("10000000-0000-0000-0000-000000000002");
    QVERIFY(bandId1 && bandId2);
    auto band1 = dsp::EqBandParameters::create(*bandId1.value(), true, dsp::EqFilterType::BELL,
        dsp::EqRouting::STEREO, dsp::BellPayload{500.0, -3.5, 1.2});
    auto band2 = dsp::EqBandParameters::create(*bandId2.value(), true, dsp::EqFilterType::HIGH_SHELF,
        dsp::EqRouting::STEREO, dsp::ShelfPayload{8000.0, 2.0, 0.707});
    QVERIFY(band1 && band2);
    auto eqParams = dsp::ParametricEqParameters::create({*band1.value(), *band2.value()});
    QVERIFY(eqParams);
    QVERIFY(session.masteringChainState->set_parametric_eq_parameters(*eqParams.value()));

    const auto projectPath = dir.filePath(QStringLiteral("mastering_chain.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(projectPath));
    QCOMPARE(session.project.error_message(), QString());
    QVERIFY(!session.project.degraded());

    auto snapshot = load_project(projectPath);
    QVERIFY(snapshot);
    const auto& doc = snapshot.value()->document();

    // Verify pipeline.masteringChainId matches authoritative chain ID
    QVERIFY(doc.pipeline.masteringChainId.has_value());
    QCOMPARE(*doc.pipeline.masteringChainId, session.chainId);

    // Verify MASTER / MANUAL chain
    QCOMPARE(doc.chains.size(), std::size_t{1});
    const auto& chain = doc.chains.front();
    QCOMPARE(chain.chainId, session.chainId);
    QCOMPARE(chain.stage, std::string("MASTER"));
    QCOMPARE(chain.segment, std::string("MANUAL"));
    QCOMPARE(chain.revision, session.masteringChainState->chain().revision());
    QCOMPARE(chain.modules.size(), std::size_t{2});

    // Verify Module 0: Input Gain
    auto gainInstance = session.masteringChainState->gain_instance();
    QVERIFY(gainInstance);
    const auto& gainAuthority = gainInstance.value()->get();
    const auto& m0 = chain.modules[0];
    QCOMPARE(m0.instanceId, session.gainId.uuid());
    QCOMPARE(m0.typeId, std::string("rgsml.dsp.gain"));
    QCOMPARE(m0.enabled, gainAuthority.enabled());
    QCOMPARE(m0.userBypass, gainAuthority.user_bypass());
    QCOMPARE(m0.controllerSuspended, gainAuthority.controller_suspended());
    QCOMPARE(m0.domainSuspended, gainAuthority.domain_suspended());
    QVERIFY(m0.userBypass);
    QCOMPARE(m0.provenance, std::string("MANUAL"));
    QCOMPARE(m0.owner, std::string("USER"));
    QCOMPARE(m0.linkState, std::string("UNLINKED"));
    QVERIFY(m0.semanticNodeId == gainAuthority.semantic_node_id());
    QVERIFY(m0.algorithmVersion.has_value());
    QCOMPARE(*m0.algorithmVersion, std::string("1.0.0"));
    QVERIFY(m0.parameterSchemaId.has_value());
    QCOMPARE(*m0.parameterSchemaId, std::string("rgsml.dsp.gain.parameters/1.0.0"));

    auto decodedGain = dsp::decode_gain_parameters_json(m0.parameters.canonical_utf8());
    QVERIFY(decodedGain);
    QCOMPARE(*decodedGain.value(), *gainParams.value());

    // Verify Module 1: Parametric EQ
    auto eqInstance = session.masteringChainState->eq_instance();
    QVERIFY(eqInstance);
    const auto& eqAuthority = eqInstance.value()->get();
    const auto& m1 = chain.modules[1];
    QCOMPARE(m1.instanceId, session.eqId.uuid());
    QCOMPARE(m1.typeId, std::string("rgsml.dsp.parametric-eq"));
    QCOMPARE(m1.enabled, eqAuthority.enabled());
    QCOMPARE(m1.userBypass, eqAuthority.user_bypass());
    QCOMPARE(m1.controllerSuspended, eqAuthority.controller_suspended());
    QCOMPARE(m1.domainSuspended, eqAuthority.domain_suspended());
    QVERIFY(!m1.userBypass);
    QCOMPARE(m1.provenance, std::string("MANUAL"));
    QCOMPARE(m1.owner, std::string("USER"));
    QCOMPARE(m1.linkState, std::string("UNLINKED"));
    QVERIFY(m1.semanticNodeId == eqAuthority.semantic_node_id());
    QVERIFY(m1.algorithmVersion.has_value());
    QCOMPARE(*m1.algorithmVersion, std::string("1.0.0"));
    QVERIFY(m1.parameterSchemaId.has_value());
    QCOMPARE(*m1.parameterSchemaId, std::string("rgsml.dsp.parametric-eq.parameters/1.0.0"));

    auto decodedEq = dsp::decode_parametric_eq_parameters_json(m1.parameters.canonical_utf8());
    QVERIFY(decodedEq);
    QCOMPARE(*decodedEq.value(), *eqParams.value());
    QCOMPARE(decodedEq.value()->bands().size(), std::size_t{2});
    QCOMPARE(decodedEq.value()->bands()[0].band_id(), *bandId1.value());
    QCOMPARE(decodedEq.value()->bands()[1].band_id(), *bandId2.value());
}

void ProjectSessionTest::mastering_chain_repeated_save()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));

    const auto firstPath = dir.filePath(QStringLiteral("save1.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(firstPath));
    QCOMPARE(session.project.error_message(), QString());

    // Modify BOTH Gain and EQ
    auto updatedGain = dsp::GainParameters::create(-3.0);
    QVERIFY(updatedGain);
    QVERIFY(session.masteringChainState->set_gain_parameters(*updatedGain.value()));

    auto bandIdNew = core::Uuid::parse("10000000-0000-0000-0000-000000000003");
    QVERIFY(bandIdNew);
    auto bandMod = dsp::EqBandParameters::create(*bandIdNew.value(), true, dsp::EqFilterType::BELL,
        dsp::EqRouting::STEREO, dsp::BellPayload{1000.0, 4.5, 2.0});
    QVERIFY(bandMod);
    auto updatedEq = dsp::ParametricEqParameters::create({*bandMod.value()});
    QVERIFY(updatedEq);
    QVERIFY(session.masteringChainState->set_parametric_eq_parameters(*updatedEq.value()));

    const auto secondPath = dir.filePath(QStringLiteral("save2.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(secondPath));
    QCOMPARE(session.project.error_message(), QString());

    auto snapshot = load_project(secondPath);
    QVERIFY(snapshot);
    const auto& doc = snapshot.value()->document();

    QCOMPARE(doc.chains.size(), std::size_t{1});
    const auto& chain = doc.chains.front();
    QCOMPARE(chain.chainId, session.chainId);
    QCOMPARE(chain.modules[0].instanceId, session.gainId.uuid());
    QCOMPARE(chain.modules[1].instanceId, session.eqId.uuid());

    auto decodedGain = dsp::decode_gain_parameters_json(chain.modules[0].parameters.canonical_utf8());
    QVERIFY(decodedGain);
    QCOMPARE(decodedGain.value()->gain_db(), -3.0);

    auto decodedEq = dsp::decode_parametric_eq_parameters_json(chain.modules[1].parameters.canonical_utf8());
    QVERIFY(decodedEq);
    QCOMPARE(*decodedEq.value(), *updatedEq.value());
}

void ProjectSessionTest::mastering_chain_source_replacement_regression()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto firstSource = wav(dir, QStringLiteral("first.wav"), 0);
    const auto secondSource = wav(dir, QStringLiteral("second.wav"), 1);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(firstSource));

    const auto firstProject = dir.filePath(QStringLiteral("first.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(firstProject));
    QCOMPARE(session.project.error_message(), QString());

    // Replace Source
    session.source.selectSource(QUrl::fromLocalFile(secondSource));

    const auto replacedProject = dir.filePath(QStringLiteral("replaced.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(replacedProject));
    QCOMPARE(session.project.error_message(), QString());
    QVERIFY(!session.project.degraded());

    auto snapshot = load_project(replacedProject);
    QVERIFY(snapshot);
    const auto& doc = snapshot.value()->document();

    QCOMPARE(doc.chains.size(), std::size_t{1});
    QCOMPARE(doc.chains.front().chainId, session.chainId);
    QCOMPARE(doc.chains.front().modules[0].instanceId, session.gainId.uuid());
    QCOMPARE(doc.chains.front().modules[1].instanceId, session.eqId.uuid());
}

void ProjectSessionTest::supported_b4_open_and_viewmodel_refresh()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));

    // Configure non-default Gain & EQ
    auto gainParams = dsp::GainParameters::create(4.5);
    QVERIFY(gainParams);
    QVERIFY(session.masteringChainState->set_gain_parameters(*gainParams.value()));
    QVERIFY(session.masteringChainState->set_user_bypass(session.gainId, true));

    auto bandId1 = core::Uuid::parse("20000000-0000-0000-0000-000000000001");
    auto bandId2 = core::Uuid::parse("20000000-0000-0000-0000-000000000002");
    QVERIFY(bandId1 && bandId2);
    auto band1 = dsp::EqBandParameters::create(*bandId1.value(), true, dsp::EqFilterType::BELL,
        dsp::EqRouting::STEREO, dsp::BellPayload{800.0, -2.5, 1.5});
    auto band2 = dsp::EqBandParameters::create(*bandId2.value(), true, dsp::EqFilterType::HIGH_SHELF,
        dsp::EqRouting::STEREO, dsp::ShelfPayload{10000.0, 3.0, 0.707});
    QVERIFY(band1 && band2);
    auto eqParams = dsp::ParametricEqParameters::create({*band1.value(), *band2.value()});
    QVERIFY(eqParams);
    QVERIFY(session.masteringChainState->set_parametric_eq_parameters(*eqParams.value()));

    const auto path = dir.filePath(QStringLiteral("supported_b4.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(path));
    QCOMPARE(session.project.error_message(), QString());

    Session target;
    target.project.openProject(QUrl::fromLocalFile(path));
    QCOMPARE(target.project.error_message(), QString());
    QVERIFY(!target.project.degraded());

    // Verify exact identity restoration in MasteringChainState
    QCOMPARE(target.masteringChainState->chain_id(), session.chainId);
    QCOMPARE(target.masteringChainState->gain_instance_id(), session.gainId);
    QCOMPARE(target.masteringChainState->eq_instance_id(), session.eqId);
    QCOMPARE(target.masteringChainState->gain_parameters().gain_db(), 4.5);
    QCOMPARE(target.masteringChainState->parametric_eq_parameters(), *eqParams.value());

    // Verify ViewModels reflect adopted state
    QCOMPARE(target.gainViewModel->gain_db(), 4.5);
    QVERIFY(target.gainViewModel->bypass());
    QCOMPARE(target.eqViewModel->band_count(), 2);
    QCOMPARE(target.eqViewModel->selected_band_id(), QString::fromStdString(bandId1.value()->to_string()));
    QVERIFY(!target.eqViewModel->bypass());
}

void ProjectSessionTest::open_save_as_identity_revision_preservation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    const auto path1 = dir.filePath(QStringLiteral("p1.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(path1));
    QCOMPARE(session.project.error_message(), QString());

    auto snap1 = load_project(path1);
    QVERIFY(snap1);
    const auto rev1 = snap1.value()->document().chains.front().revision;

    Session target;
    target.project.openProject(QUrl::fromLocalFile(path1));
    QCOMPARE(target.project.error_message(), QString());

    const auto path2 = dir.filePath(QStringLiteral("p2.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(path2));
    QCOMPARE(target.project.error_message(), QString());

    auto snap2 = load_project(path2);
    QVERIFY(snap2);

    const auto& doc1 = snap1.value()->document();
    const auto& doc2 = snap2.value()->document();

    QCOMPARE(*doc2.pipeline.masteringChainId, *doc1.pipeline.masteringChainId);
    QCOMPARE(doc2.chains.front().chainId, doc1.chains.front().chainId);
    QCOMPARE(doc2.chains.front().revision, rev1);
    QCOMPARE(doc2.chains.front().modules[0].instanceId, doc1.chains.front().modules[0].instanceId);
    QCOMPARE(doc2.chains.front().modules[1].instanceId, doc1.chains.front().modules[1].instanceId);
}

void ProjectSessionTest::atomic_rejection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));

    // Set non-default live state
    auto liveGain = dsp::GainParameters::create(-3.0);
    QVERIFY(liveGain);
    QVERIFY(session.masteringChainState->set_gain_parameters(*liveGain.value()));

    const auto validPath = dir.filePath(QStringLiteral("valid.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(validPath));
    QCOMPARE(session.project.error_message(), QString());

    auto baseSnap = load_project(validPath);
    QVERIFY(baseSnap);

    const auto verify_live_session_unchanged = [&] {
        QCOMPARE(session.masteringChainState->gain_parameters().gain_db(), -3.0);
        QCOMPARE(session.gainViewModel->gain_db(), -3.0);
        QCOMPARE(session.source.source_resource()->reference().locator(),
                 session.source.source_resource()->reference().locator());
    };

    // 1. Invalid Gain JSON
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].parameters = *project::OpaqueJsonValue::parse("{\"invalid\":true}").value();
        auto badSnap = project::ProjectSnapshot::create(doc);
        const auto badPath = dir.filePath(QStringLiteral("bad_gain_json.rgsml"));
        QVERIFY(write_project(badPath, *badSnap.value()));

        session.project.openProject(QUrl::fromLocalFile(badPath));
        QVERIFY(!session.project.error_message().isEmpty());
        verify_live_session_unchanged();
    }

    // 2. Wrong algorithmVersion
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].algorithmVersion = "2.0.0";
        auto badSnap = project::ProjectSnapshot::create(doc);
        const auto badPath = dir.filePath(QStringLiteral("bad_version.rgsml"));
        QVERIFY(write_project(badPath, *badSnap.value()));

        session.project.openProject(QUrl::fromLocalFile(badPath));
        QVERIFY(!session.project.error_message().isEmpty());
        verify_live_session_unchanged();
    }

    // 3. Wrong parameterSchemaId
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[1].parameterSchemaId = "rgsml.dsp.parametric-eq.parameters/2.0.0";
        auto badSnap = project::ProjectSnapshot::create(doc);
        const auto badPath = dir.filePath(QStringLiteral("bad_schema.rgsml"));
        QVERIFY(write_project(badPath, *badSnap.value()));

        session.project.openProject(QUrl::fromLocalFile(badPath));
        QVERIFY(!session.project.error_message().isEmpty());
        verify_live_session_unchanged();
    }

    // 4. Malformed module order (EQ at 0, Gain at 1)
    {
        auto doc = baseSnap.value()->document();
        std::swap(doc.chains[0].modules[0], doc.chains[0].modules[1]);
        auto badSnap = project::ProjectSnapshot::create(doc);
        const auto badPath = dir.filePath(QStringLiteral("bad_order.rgsml"));
        QVERIFY(write_project(badPath, *badSnap.value()));

        session.project.openProject(QUrl::fromLocalFile(badPath));
        QVERIFY(!session.project.error_message().isEmpty());
        verify_live_session_unchanged();
    }

    // 5. Incompatible provenance
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].provenance = "AUTOMATED";
        auto badSnap = project::ProjectSnapshot::create(doc);
        const auto badPath = dir.filePath(QStringLiteral("bad_provenance.rgsml"));
        QVERIFY(write_project(badPath, *badSnap.value()));

        session.project.openProject(QUrl::fromLocalFile(badPath));
        QVERIFY(!session.project.error_message().isEmpty());
        verify_live_session_unchanged();
    }
}

void ProjectSessionTest::legacy_pre_b4_materialization_and_save_as()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));

    // Save project, then strip mastering chain fields to create a valid pre-B4 project
    const auto tempPath = dir.filePath(QStringLiteral("temp.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(tempPath));
    QCOMPARE(session.project.error_message(), QString());

    auto snap = load_project(tempPath);
    QVERIFY(snap);
    auto doc = snap.value()->document();
    doc.pipeline.masteringChainId.reset();
    doc.chains.clear();

    auto legacySnap = project::ProjectSnapshot::create(doc);
    QVERIFY(legacySnap);
    const auto legacyPath = dir.filePath(QStringLiteral("legacy.rgsml"));
    QVERIFY(write_project(legacyPath, *legacySnap.value()));

    // Open legacy pre-B4 project into target session
    Session target;
    const auto previousChainId = target.chainId;
    target.project.openProject(QUrl::fromLocalFile(legacyPath));
    QCOMPARE(target.project.error_message(), QString());

    // Verify fresh IDs generated (not reusing previous project IDs)
    QVERIFY(target.masteringChainState->chain_id() != previousChainId);

    // Verify canonical default state
    QCOMPARE(target.masteringChainState->gain_parameters().gain_db(), 0.0);
    QVERIFY(target.eqViewModel->is_default());

    // Save As produces canonical B4 chain
    const auto savedLegacyPath = dir.filePath(QStringLiteral("saved_legacy.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(savedLegacyPath));
    QCOMPARE(target.project.error_message(), QString());

    auto resavedSnap = load_project(savedLegacyPath);
    QVERIFY(resavedSnap);
    const auto& resavedDoc = resavedSnap.value()->document();
    QVERIFY(resavedDoc.pipeline.masteringChainId.has_value());
    QCOMPARE(*resavedDoc.pipeline.masteringChainId, target.masteringChainState->chain_id());
    QCOMPARE(resavedDoc.chains.size(), std::size_t{1});
    QCOMPARE(resavedDoc.chains[0].modules.size(), std::size_t{2});

    // Repeated Save As retains generated IDs
    const auto savedLegacyPath2 = dir.filePath(QStringLiteral("saved_legacy2.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(savedLegacyPath2));
    QCOMPARE(target.project.error_message(), QString());

    auto resavedSnap2 = load_project(savedLegacyPath2);
    QVERIFY(resavedSnap2);
    QCOMPARE(*resavedSnap2.value()->document().pipeline.masteringChainId, target.masteringChainState->chain_id());
}

void ProjectSessionTest::future_opaque_preservation_and_non_degraded_b4()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    const auto b4Path = dir.filePath(QStringLiteral("normal_b4.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(b4Path));
    QCOMPARE(session.project.error_message(), QString());

    Session targetNormal;
    targetNormal.project.openProject(QUrl::fromLocalFile(b4Path));
    QCOMPARE(targetNormal.project.error_message(), QString());
    QVERIFY(!targetNormal.project.degraded());

    // Add safe future opaque extension
    auto snap = load_project(b4Path);
    QVERIFY(snap);
    auto doc = snap.value()->document();
    doc.extensions = *project::OpaqueJsonValue::parse("{\"opaque_future\":123}").value();

    auto futureSnap = project::ProjectSnapshot::create(doc);
    QVERIFY(futureSnap);
    const auto futurePath = dir.filePath(QStringLiteral("future_b4.rgsml"));
    QVERIFY(write_project(futurePath, *futureSnap.value()));

    Session targetFuture;
    targetFuture.project.openProject(QUrl::fromLocalFile(futurePath));
    QCOMPARE(targetFuture.project.error_message(), QString());
    QVERIFY(targetFuture.project.degraded());

    // Verify B4 mastering state is still adopted
    QCOMPARE(targetFuture.masteringChainState->chain_id(), session.chainId);
}

void ProjectSessionTest::source_reset_vs_project_open()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source1 = wav(dir, QStringLiteral("s1.wav"), 0);
    const auto source2 = wav(dir, QStringLiteral("s2.wav"), 1);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source1));

    // Modify Gain
    QVERIFY(session.gainViewModel->setGainDb(5.0));
    QCOMPARE(session.gainViewModel->gain_db(), 5.0);

    // Save project with Gain = 5.0
    const auto projectPath = dir.filePath(QStringLiteral("project.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(projectPath));
    QCOMPARE(session.project.error_message(), QString());

    // 1. Ordinary Replace Source resets Gain to defaults (0.0 dB)
    session.source.selectSource(QUrl::fromLocalFile(source2));
    QCOMPARE(session.gainViewModel->gain_db(), 0.0);

    // 2. Open Project with persisted state ends with persisted values (5.0 dB), NOT defaults
    Session openedSession;
    openedSession.project.openProject(QUrl::fromLocalFile(projectPath));
    QCOMPARE(openedSession.project.error_message(), QString());
    QCOMPARE(openedSession.gainViewModel->gain_db(), 5.0);
}

}  // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::ProjectSessionTest)
#include "test_project_session.moc"
