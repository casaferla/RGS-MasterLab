#include "project_session_view_model.hpp"
#include "internal/m18_product_project_save_binding.hpp"
#include "internal/m18_product_controller_binding.hpp"
#include "internal/m18_product_upstream_binding.hpp"
#include "internal/m18_product_complete_binding.hpp"
#include "internal/m22_verified_master_preview_parity.hpp"
#include "internal/m22_product_preview_admission.hpp"
#include "internal/m22_product_chain_epoch_capture_candidate.hpp"
#include "internal/m22_product_verified_dispatch.hpp"
#include "internal/m21_product_owner_analysis.hpp"
#include "internal/m21_product_publication_inbox.hpp"
#include "internal/m21_product_qt_readiness_bridge.hpp"
#include "internal/m21_product_analysis_handoff.hpp"
#include "m21_product_status_view_model.hpp"
#include "internal/m21_product_spectrum_evidence.hpp"
#include "internal/m18_private_product_export_action.hpp"
#include "m18_product_export_view_model.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_candidate_gate.hpp"
#include "source_selection_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "audition_source_selector.hpp"
#include "audition_region_view_model.hpp"
#include "dsp_chain_adapter_model.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/project/project_repository.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>

#include "../../unit/audio/wav_test_support.hpp"
#include "../../unit/platform/fake_playback_service.hpp"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "master_output_rate_state.hpp"
#include <rgsml/project/master_output_rate_preference.hpp>
#include <QUrl>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <optional>
#include <filesystem>
#include <vector>
#include <utility>
#include <type_traits>

static_assert(std::is_same_v<
    decltype(&rgsml::app::internal::M18ProductCompleteBinding::bind),
    rgsml::core::Status (*)(
        rgsml::app::internal::M18ProductCompleteBinding::Controllers,
        const rgsml::app::internal::M18ProductCompleteBinding::Fanout&)>);

namespace rgsml::tests {
namespace {

[[nodiscard]] QString wav(QTemporaryDir& dir, const QString& name, int seed,
    std::uint32_t sampleRate = 48'000U,
    std::size_t frameCount = 256U)
{
    std::vector<std::int64_t> samples(frameCount);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = (static_cast<std::int64_t>(i) + seed) % 100;
    const auto data = wav_support::make_wav(1U, 16U, 1U, sampleRate,
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
    dsp::ModuleInstanceId compId;
    std::unique_ptr<app::MasteringChainState> masteringChainState;
    std::unique_ptr<app::MasteringPreviewController> previewController;
    std::unique_ptr<app::GainViewModel> gainViewModel;
    std::unique_ptr<app::EqViewModel> eqViewModel;
    std::unique_ptr<app::CompressorViewModel> compViewModel;
    std::unique_ptr<app::DspChainAdapterModel> adapterModel;
    app::ProjectSessionViewModel project;

    explicit Session(app::ProjectSessionViewModel::UuidFactory uuidFactory = {})
        : observed(new FakePlaybackService()),
          transport(std::unique_ptr<core::IAudioPlaybackService>(observed)),
          source(), selector(&transport), gold(&selector), region(&transport),
          chainId(*core::Uuid::parse("10000000-0000-4000-8000-000000000001").value()),
          gainId(*dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value()),
          eqId(*dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value()),
          compId(*dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000030").value()).value()),
          masteringChainState([this] {
              auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
              auto state = app::MasteringChainState::create_default(*registry.value(), chainId, gainId, eqId, compId);
              return std::make_unique<app::MasteringChainState>(std::move(*state.value()));
          }()),
          previewController(std::make_unique<app::MasteringPreviewController>(masteringChainState.get())),
          gainViewModel(std::make_unique<app::GainViewModel>(masteringChainState.get(), previewController.get())),
          eqViewModel(std::make_unique<app::EqViewModel>(masteringChainState.get(), previewController.get())),
          compViewModel(std::make_unique<app::CompressorViewModel>(masteringChainState.get(), previewController.get())),
          adapterModel(std::make_unique<app::DspChainAdapterModel>(
              gainViewModel.get(), eqViewModel.get(), compViewModel.get(), masteringChainState.get())),
          project(&source, &gold, &region, &transport, masteringChainState.get(),
                  gainViewModel.get(), eqViewModel.get(), adapterModel.get(), previewController.get(),
                  std::move(uuidFactory))
    {
        transport.set_pcm_prepare_handler(
            [this](
                audio::AudioBufferView view,
                std::shared_ptr<const void>,
                std::optional<core::RealizationId>) {
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
            const auto prepared = selector.source_committed(ref);
            if (prepared) {
                (void)selector.switch_to(app::AuditionTarget::PREPARED);
                if (!project.is_committing_project_open()) {
                    adapterModel->resetForNewSource();
                }
            }
            gold.sourceChanged();
        });
    }
};

// M18-only checked integration route for the native ProjectSession
// fixture. All legacy M17 tests above continue using Session's original
// void callback when no physical Product batch has been installed.
void install_m18_checked_source_dispatch(
    Session& session,app::MasterOutputRateState& outputRate)
{
    session.source.set_private_m18_checked_source_committed_handler(
        [&session,&outputRate](
            const core::ResourceReference& source)->core::Status {
            const auto frames=core::FrameCount::create(
                session.source.frame_count());
            const auto sampleRate=core::SampleRate::create(
                session.source.sample_rate_hz());
            if(!frames||!sampleRate)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_CHECKED_SOURCE_FORMAT_NOT_VALID"});
            if(!outputRate.try_source_committed_for_m18(
                   session.source.sample_rate_hz()))
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_CHECKED_SOURCE_RATE_REFUSED"});
            session.region.source_committed(
                *frames.value(),*sampleRate.value());
            const auto prepared=session.selector.source_committed(source);
            if(!prepared)return prepared;
            const auto switchStatus=session.selector.switch_to(
                app::AuditionTarget::PREPARED);
            if(!switchStatus)return switchStatus;
            if(!session.project.is_committing_project_open())
                session.adapterModel->resetForNewSource();
            session.gold.sourceChanged();
            return core::Status::success();
        });
}

}  // namespace

class ProjectSessionTest final : public QObject {
    Q_OBJECT
private slots:
    void source_save_open_existing_fail();
    void m18FreshStartupBindsBeforeFirstSourceSelection();
    void m18CanonicalWavPreferenceSurvivesSaveAsAndProjectOpen();
    void m18GoldReplacementAndClearDeniedBeforeSelectorMutation();
    void m18SingleUpstreamBundleRejectsBusyWorkerAndDuplicateBinding();
    void m18CompleteRealProductBinderRequiresVerifiedM17BeforeUserConsent();
    void m18RealProductVerifiedMasterToPcm24AndFloat32WavAck();
    void m22LiveProductCrossRateQ0M17PreviewFullExportAndRevocation();
    void m18RegionGateCannotSelfUninstallOrCrossThread();
    void m18ProjectMutationDeniedBeforeOpenOrSavePublication();
    void m18ProjectOpenBatchMustNotLeaveSaveAsUnguarded();
    void m18ProductSharedEpochSaveAsHoldsActualFilePublicationLease();
    void m18ActualProjectOpenAndSaveUseOneSharedProductEpoch();
    void m18AuditionGateSetterCannotSelfUninstallOrCrossThread();
    void m18ActualSourcePickerAndAuditionUseOnePhysicalProductBatch();
    void m18ActualProjectOpenCheckedSourceFailureCannotPublishReady();
    void m18SaveBatchMissingOrThrowingFailsClosed();
    void m18SaveOnlyPolicyCannotOpenProjectWithoutEpoch();
    void m18NestedProjectCommandsRefusedDuringSaveBatch();
    void m18NestedProjectCommandsRefusedDuringOpenBatch();
    void m18ProjectOpenBatchKeepsWorkerLockAcrossNestedSourceCommit();
    void master_rate_save_open_and_invalid_atomic();
    void gold_region_roundtrip();
    void missing_source_preflight_preserves_live_session();
    void clear_region_and_replace_source_save_as();
    void invalid_gold_and_hardlink_preflight_preserve_session();
    void region_bounds_and_degraded_opaque_preservation();
    void externally_opened_invalid_processing_rejected_atomically();
    void mastering_chain_normal_save();
    void mastering_chain_repeated_save();
    void mastering_chain_source_replacement_regression();
    void supported_b4_open_and_viewmodel_refresh();
    void open_save_as_identity_revision_preservation();
    void atomic_rejection();
    void legacy_pre_b4_materialization_and_save_as();
    void legacy_two_module_gain_eq_migration();
    void future_opaque_preservation_and_non_degraded_b4();
    void source_reset_vs_project_open();
    void stereo_ms_four_module_project_roundtrip_and_atomic_rejection();
    void product_default_stereo_ms_project_save_open_save_identity();
};


void ProjectSessionTest::
m18CanonicalWavPreferenceSurvivesSaveAsAndProjectOpen()
{
    using project::M18MasterWavPreferenceCodec;
    using project::M18MasterWavEncoding;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto sourcePath=wav(dir,QStringLiteral("m18-preference-source.wav"),41);
    QVERIFY(!sourcePath.isEmpty());
    const auto firstPath=dir.filePath(QStringLiteral("m18-f32.rgsml"));
    const auto secondPath=dir.filePath(QStringLiteral("m18-pcm24.rgsml"));
    Session original;
    original.source.selectSource(QUrl::fromLocalFile(sourcePath));
    QVERIFY(original.source.has_source());
    QCOMPARE(original.project.m18_wav_encoding_index(),0);
    QVERIFY(original.project.chooseM18WavEncoding(1));
    QCOMPARE(original.project.m18_wav_encoding_index(),1);
    original.project.saveProjectAs(QUrl::fromLocalFile(firstPath));
    QVERIFY2(QFileInfo::exists(firstPath),
             original.project.error_message().toUtf8().constData());
    auto saved=load_project(firstPath);
    QVERIFY(saved);
    auto format=M18MasterWavPreferenceCodec::read(
        saved.value()->document().output);
    QVERIFY(format);
    QVERIFY(format.value()->has_value());
    QCOMPARE(format.value()->value().encoding,
             M18MasterWavEncoding::IEEE_FLOAT32);
    QVERIFY(!format.value()->value().pcm24_dither_seed);
    QVERIFY(project::MasterOutputRatePreference::only_known_fields(
        saved.value()->document().output));

    Session reopened;
    reopened.project.openProject(QUrl::fromLocalFile(firstPath));
    QVERIFY2(reopened.source.has_source(),
             reopened.project.error_message().toUtf8().constData());
    QVERIFY(!reopened.project.degraded());
    QCOMPARE(reopened.project.m18_wav_encoding_index(),1);
    // New confirmed PCM24 setting must restore an exact native uint64
    // seed, not an imprecise JSON floating-point or leftover Float32 seed.
    QVERIFY(reopened.project.chooseM18WavEncoding(0));
    QCOMPARE(reopened.project.m18_wav_encoding_index(),0);
    QCOMPARE(reopened.project.m18_pcm24_seed(),
             M18MasterWavPreferenceCodec::default_pcm24_seed());
    reopened.project.saveProjectAs(QUrl::fromLocalFile(secondPath));
    QVERIFY2(QFileInfo::exists(secondPath),
             reopened.project.error_message().toUtf8().constData());
    auto persisted=load_project(secondPath);
    QVERIFY(persisted);
    const auto pcm=M18MasterWavPreferenceCodec::read(
        persisted.value()->document().output);
    QVERIFY(pcm);
    QVERIFY(pcm.value()->has_value());
    QCOMPARE(pcm.value()->value().encoding,
             M18MasterWavEncoding::PCM24_TPDF);
    QCOMPARE(pcm.value()->value().pcm24_dither_seed.value(),
             M18MasterWavPreferenceCodec::default_pcm24_seed());
    QVERIFY(project::MasterOutputRatePreference::only_known_fields(
        persisted.value()->document().output));
}

void ProjectSessionTest::
m18FreshStartupBindsBeforeFirstSourceSelection()
{
    // Real app/main.cpp installs M18 with NO selected SOURCE and rate
    // revision zero. The FIRST user-selected WAV must pass through the
    // checked callback under one physical Product lease and reconcile
    // only after that lease is released. This is distinct from all
    // preproduction tests that bind after an existing SOURCE selection.
    using app::internal::M18ProductCompleteBinding;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto first=wav(dir,QStringLiteral("m18-first-source.wav"),11);
    const auto second=wav(dir,QStringLiteral("m18-second-source.wav"),12);
    QVERIFY(!first.isEmpty()&&!second.isEmpty());
    Session session;
    QVERIFY(!session.source.has_source());
    app::MasterOutputRateState rate;
    QCOMPARE(rate.rate_revision(),UINT64_C(0));
    session.project.set_master_output_rate_state(&rate);
    install_m18_checked_source_dispatch(session,rate);
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto id=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse(
            "ac180000-0000-4000-8000-000000000018").value());
    QVERIFY(id);
    auto terminal=app::TerminalLimiterState::create_default(
        *registry.value(),*id.value());
    QVERIFY(terminal);
    app::TerminalCandidateGate candidates;
    app::TerminalPrivateRenderController renderer{
        candidates,*terminal.value(),
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::optional<core::RealizationId>{return {};},
        [&session]{return session.masteringChainState->chain();}};
    app::TerminalAudioSessionGate device{
        candidates,*session.observed,
        [](audio::AudioBufferView,std::shared_ptr<const void>,
           core::RealizationId)->core::Status {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_FIRST_SOURCE_NO_VERIFIED_DEVICE"});
        }};
    app::TerminalPrivatePublicationCoordinator publisher{
        candidates,device,session.transport,
        [](auto,auto,auto)->core::Result<core::RealizationId> {
            return core::Result<core::RealizationId>::failure(
                core::Error{core::ErrorCode::InvalidState,
                            "M18_FIRST_SOURCE_NO_PUBLISHED_MASTER"});
        }};
    app::TerminalPrivateProductSession m17{
        renderer,publisher,candidates,session.transport,
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::optional<core::RealizationId>{return {};},
        []()->std::optional<core::FrameRange>{return {};},
        []{return false;}};
    const M18ProductExportIdentity initial{
        1U,1U,1U,session.masteringChainState->chain().revision(),
        48000U,rate.rate_revision()};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(1U,initial);
    M18OwnerMutationFanout fanout{owner,1U,initial};
    app::internal::M18ProductUpstreamBinding::Controllers upstream{
        session.project,session.source,rate,
        *session.masteringChainState,session.selector,session.region,
        session.gold,*terminal.value()};
    M18ProductCompleteBinding::Controllers controllers{
        upstream,m17,candidates};
    QVERIFY(M18ProductCompleteBinding::bind(controllers,fanout));
    QVERIFY(M18ProductCompleteBinding::installed(controllers));
    QVERIFY(!M18ProductCompleteBinding::can_offer_verified_master(
        controllers,fanout));
    const auto initialSnapshot=fanout.current_owner_snapshot();
    QVERIFY(initialSnapshot);

    // Mimic the real M17 coordinator's synchronous Selector::changed
    // callback on the first committed SOURCE. It retires its old session
    // inside the already-held M18 physical SOURCE batch, rather than
    // being incorrectly rejected as a second unrelated Product write.
    bool retirementObserved=false;
    bool retirementAccepted=false;
    QObject::connect(&session.selector,
        &app::AuditionSourceSelector::changed,
        &session.source,[&] {
            if(retirementObserved)return;
            retirementObserved=true;
            retirementAccepted=static_cast<bool>(m17.cancel());
        });
    session.source.selectSource(QUrl::fromLocalFile(first));
    QVERIFY(retirementObserved);
    QVERIFY(retirementAccepted);
    QVERIFY(session.source.has_source());
    QCOMPARE(session.source.display_name(),
             QStringLiteral("m18-first-source.wav"));
    QCOMPARE(rate.master_rate_hz(),qint64{48000});
    QVERIFY(rate.rate_revision()>0);
    const auto afterSource=fanout.current_owner_snapshot();
    QVERIFY(afterSource);
    QVERIFY(afterSource->identity.source_generation>
            initialSnapshot->identity.source_generation);
    QVERIFY(afterSource->epoch>initialSnapshot->epoch);
    QVERIFY(M18ProductCompleteBinding::reconcile_after_product_mutation(
        controllers,fanout));
    const auto reconciled=fanout.current_owner_snapshot();
    QVERIFY(reconciled);
    QCOMPARE(reconciled->identity.master_rate_revision,rate.rate_revision());
    QCOMPARE(reconciled->identity.chain_revision,
             session.masteringChainState->chain().revision());
    QVERIFY(!M18ProductCompleteBinding::can_offer_verified_master(
        controllers,fanout));
    QVERIFY(!M18ProductCompleteBinding::authorize_verified_master_for_user(
        controllers,fanout,true));

    // M19/20/21 first SOURCE analysis borrows the REAL prepared owner;
    // it must not fabricate an absent processed or GOLD resource.
    using M21Owners=app::internal::M21ProductOwnerAnalysis;
    const auto firstAnalysisOwner=M21Owners::capture(
        controllers,fanout,session.selector);
    QVERIFY2(firstAnalysisOwner,
        firstAnalysisOwner?"":firstAnalysisOwner.error()->message().c_str());
    QVERIFY(firstAnalysisOwner.value()->prepared);
    QVERIFY(!firstAnalysisOwner.value()->processed);
    QVERIFY(M21Owners::still_current(
        controllers,fanout,session.selector,*firstAnalysisOwner.value()));
    const auto firstAnalysis=M21Owners::measure_current(
        controllers,fanout,session.selector,*firstAnalysisOwner.value(),
        app::m21_private::Column::prepared);
    QVERIFY2(firstAnalysis,
        firstAnalysis?"":firstAnalysis.error()->message().c_str());
    QVERIFY(!M21Owners::measure_current(
        controllers,fanout,session.selector,*firstAnalysisOwner.value(),
        app::m21_private::Column::processed));

    // Set real nondefault MANUAL state first. The next SOURCE must
    // atomically restore Gain defaults under its ONE physical SOURCE
    // lease, not silently skip the nested setter through M18 admission.
    const auto modifiedGain=dsp::GainParameters::create(3.0);
    QVERIFY(modifiedGain);
    QVERIFY(session.masteringChainState->set_gain_parameters(
        *modifiedGain.value()));
    QVERIFY(session.masteringChainState->set_user_bypass(
        session.masteringChainState->gain_instance_id(),true));
    QCOMPARE(session.masteringChainState->gain_parameters().gain_db(),3.0);
    const auto gainBypassed=session.masteringChainState->is_bypassed(
        session.masteringChainState->gain_instance_id());
    QVERIFY(gainBypassed);
    QVERIFY(*gainBypassed.value());

    // Same-rate replacement still changes SOURCE provenance. No stale
    // epoch may be reused just because both source WAVs are 48 kHz.
    session.source.selectSource(QUrl::fromLocalFile(second));
    QVERIFY(session.source.has_source());
    QCOMPARE(session.source.display_name(),
             QStringLiteral("m18-second-source.wav"));
    QCOMPARE(session.masteringChainState->gain_parameters().gain_db(),0.0);
    const auto resetBypassed=session.masteringChainState->is_bypassed(
        session.masteringChainState->gain_instance_id());
    QVERIFY(resetBypassed);
    QVERIFY(!*resetBypassed.value());
    const auto replacement=fanout.current_owner_snapshot();
    QVERIFY(replacement);
    QVERIFY(replacement->epoch>reconciled->epoch);
    QVERIFY(replacement->identity.source_generation>
            reconciled->identity.source_generation);
    // Same-rate SOURCE replacement retires both the old resource
    // identity and all of its M19/20/M21 derived metric snapshots.
    QVERIFY(!M21Owners::still_current(
        controllers,fanout,session.selector,*firstAnalysisOwner.value()));
    QVERIFY(!M21Owners::measure_current(
        controllers,fanout,session.selector,*firstAnalysisOwner.value(),
        app::m21_private::Column::prepared));
    QVERIFY(M18ProductCompleteBinding::reconcile_after_product_mutation(
        controllers,fanout));
    QVERIFY(!M18ProductCompleteBinding::can_offer_verified_master(
        controllers,fanout));

    // A normal Project SaveAs/Open exercises the OTHER physical batch
    // admission. The synchronous M17 retirement callback is still allowed
    // as the selected SOURCE is republished from the saved Project.
    const auto projectPath=dir.filePath(
        QStringLiteral("m18-startup-project.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(projectPath));
    QVERIFY2(QFileInfo::exists(projectPath),
             session.project.error_message().toUtf8().constData());
    const auto beforeOpen=fanout.current_owner_snapshot();
    QVERIFY(beforeOpen);
    retirementObserved=false;
    retirementAccepted=false;
    session.project.openProject(QUrl::fromLocalFile(projectPath));
    QVERIFY2(session.source.has_source(),
             session.project.error_message().toUtf8().constData());
    QVERIFY(retirementObserved);
    QVERIFY(retirementAccepted);
    QVERIFY(!session.project.degraded());
    const auto afterOpen=fanout.current_owner_snapshot();
    QVERIFY(afterOpen);
    QVERIFY(afterOpen->identity.project_generation>
            beforeOpen->identity.project_generation);
    QVERIFY(M18ProductCompleteBinding::reconcile_after_product_mutation(
        controllers,fanout));
    QVERIFY(!M18ProductCompleteBinding::can_offer_verified_master(
        controllers,fanout));

    // Real GOLD has an independent file owner, even if it happens to
    // have the same sample rate as SOURCE. Do not substitute PREPARED
    // PCM, create a Processed identity or assume timeline alignment.
    QVERIFY(!M21Owners::capture_gold(
        controllers,fanout,session.selector,session.gold));
    session.gold.selectGold(QUrl::fromLocalFile(first));
    QVERIFY2(session.gold.has_gold(),
             session.gold.error_message().toUtf8().constData());
    const auto selectedGold=M21Owners::capture_gold(
        controllers,fanout,session.selector,session.gold);
    QVERIFY2(selectedGold,
             selectedGold?"":selectedGold.error()->message().c_str());
    QCOMPARE(selectedGold.value()->sample_rate_hz,48000U);
    QCOMPARE(selectedGold.value()->full_frames,std::int64_t{256});
    QCOMPARE(selectedGold.value()->channels,std::size_t{1});
    QVERIFY(M21Owners::gold_still_current(
        controllers,fanout,session.selector,session.gold,
        *selectedGold.value()));
    // Real GOLD worker uses the exact selected independent WAV file,
    // never the currently selected SOURCE (which is `second`).
    // Decoder, M19/20 and M21 typed import must run on its real PCM.
    const auto goldMeasured=M21Owners::measure_gold_worker(
        *selectedGold.value());
    QVERIFY2(goldMeasured,
             goldMeasured?"":goldMeasured.error()->message().c_str());
    QCOMPARE(goldMeasured.value()->column,app::m21_private::Column::gold);
    QCOMPARE(goldMeasured.value()->scope_frames,std::uint64_t{256});
    QCOMPARE(goldMeasured.value()->sample_rate_hz,48000U);
    QCOMPARE(goldMeasured.value()->generation_token,
             selectedGold.value()->owner.epoch);
    QVERIFY(!goldMeasured.value()->resource_identity.empty());
    QCOMPARE(goldMeasured.value()->metrics[3].quality,
             app::m21_private::Quality::missing); // M16 not qualified
    QVERIFY(goldMeasured.value()->metrics[2].unrounded.has_value());
    app::internal::M21ProductPublicationInbox goldInbox;
    QVERIFY(goldInbox.accept_gold(
        controllers,fanout,session.selector,session.gold,
        *selectedGold.value(),*goldMeasured.value()));
    const auto goldView=goldInbox.view_current(
        controllers,fanout,session.selector,session.gold);
    QVERIFY(goldView.columns[1].has_value());
    QCOMPARE(goldView.columns[1]->lifecycle,
             app::m21_private::Lifecycle::current);
    QVERIFY(!goldView.columns[0]);
    QVERIFY(!goldView.columns[2]);

    // Real three-stage Product handoff: Qt owner captures an independent
    // selected REFERENCE, the worker decodes and measures its actual WAV,
    // and the Qt owner alone decides whether M21 may publish the result.
    using Handoff=app::internal::M21ProductAnalysisHandoff;
    const auto goldJob=Handoff::capture_on_owner(
        controllers,fanout,session.selector,session.gold,
        app::m21_private::Column::gold);
    QVERIFY(goldJob);
    QVERIFY(goldJob.value()->structurally_valid());
    auto mismatchedEpochJob=*goldJob.value();
    ++mismatchedEpochJob.captured_epoch;
    QVERIFY(!mismatchedEpochJob.structurally_valid());
    QVERIFY(!Handoff::measure_on_worker(mismatchedEpochJob));
    auto wrongColumnJob=*goldJob.value();
    wrongColumnJob.column=app::m21_private::Column::prepared;
    QVERIFY(!wrongColumnJob.structurally_valid());
    QVERIFY(!Handoff::measure_on_worker(wrongColumnJob));
    std::optional<core::Result<app::m21_private::Snapshot>> goldFromThread;
    std::thread realReferenceWorker([&] {
        goldFromThread.emplace(Handoff::measure_on_worker(*goldJob.value()));
    });
    realReferenceWorker.join();
    QVERIFY(goldFromThread.has_value());
    QVERIFY(*goldFromThread);
    QCOMPARE(goldFromThread->value()->resource_identity,
             goldMeasured.value()->resource_identity);
    QCOMPARE(goldFromThread->value()->scope_frames,
             goldMeasured.value()->scope_frames);
    app::internal::M21ProductPublicationInbox handoffGoldInbox;
    QVERIFY(Handoff::publish_on_owner(
        controllers,fanout,session.selector,session.gold,
        handoffGoldInbox,*goldJob.value(),*goldFromThread->value()));
    QVERIFY(!Handoff::publish_on_owner(
        controllers,fanout,session.selector,session.gold,
        handoffGoldInbox,*goldJob.value(),*goldFromThread->value()));
    bool workerCannotCapture=false;
    std::thread illegalCapture([&] {
        workerCannotCapture=!Handoff::capture_on_owner(
            controllers,fanout,session.selector,session.gold,
            app::m21_private::Column::gold);
    });
    illegalCapture.join();
    QVERIFY(workerCannotCapture);


    // A truly independent REFERENCE is visible to the private UI readiness
    // bridge only after its own canonical GoldSelection file owner accepted
    // the measured WAV. The same object never invents PREPARED/PROCESSED.
    using GoldQtReadiness=app::internal::M21ProductQtReadinessBridge;
    GoldQtReadiness::Routing goldRouting;
    QVERIFY(goldRouting.select(GoldQtReadiness::AnalysisPanel::tonal_m20,
        app::m21_private::Column::gold));
    QVERIFY(goldRouting.select(GoldQtReadiness::AnalysisPanel::delta_m21,
        app::m21_private::Column::gold));
    const auto independentGoldPanels=GoldQtReadiness::capture(
        controllers,fanout,session.selector,session.gold,goldInbox,goldRouting);
    QVERIFY(independentGoldPanels.current_product_owner);
    QVERIFY(!independentGoldPanels.product_ui_authorized);
    app::M21ProductStatusViewModel goldNativeStatus{
        controllers,fanout,session.selector,session.gold,goldInbox};
    QVERIFY(!goldNativeStatus.production_authorized());
    QVERIFY(goldNativeStatus.selectPanelSource(
        1,QStringLiteral("REFERENCE")));
    QCOMPARE(goldNativeStatus.m20().value("sourceRole").toString(),
             QStringLiteral("REFERENCE"));
    QCOMPARE(goldNativeStatus.m20().value(
        "privateEvidenceStatus").toString(),QStringLiteral("CURRENT"));
    QVERIFY(!goldNativeStatus.m20().value("ready").toBool());
    QVERIFY(goldNativeStatus.m20().value("spectrum").toList().isEmpty());
    QVERIFY(!goldNativeStatus.selectPanelSource(
        1,QStringLiteral("GOLD"))); // internal token never public UI
    QVERIFY(!goldNativeStatus.selectPanelSource(
        7,QStringLiteral("REFERENCE")));
    QCOMPARE(independentGoldPanels.panels[0].value(
        "privateEvidenceStatus").toString(),QStringLiteral("MISSING"));
    QCOMPARE(independentGoldPanels.panels[1].value(
        "privateEvidenceStatus").toString(),QStringLiteral("CURRENT"));
    QCOMPARE(independentGoldPanels.panels[2].value(
        "privateEvidenceStatus").toString(),QStringLiteral("CURRENT"));
    QVERIFY(!independentGoldPanels.panels[1].value("ready").toBool());
    // True selected REFERENCE M20 carries only quality classifications:
    // no invented loudness, zero-valued TP or spectral bins.
    const auto referenceQuality=independentGoldPanels.panels[1]
        .value("privateQualityOnly").toMap();
    QVERIFY(referenceQuality.contains("width"));
    QVERIFY(referenceQuality.contains("P50"));
    QVERIFY(!referenceQuality.contains("lufs"));
    QCOMPARE(referenceQuality.size(),4);
    QVERIFY(independentGoldPanels.panels[0]
        .value("privateQualityOnly").toMap().isEmpty());
    QVERIFY(independentGoldPanels.panels[1].value("stereo").toMap().isEmpty());
    QVERIFY(GoldQtReadiness::still_current(independentGoldPanels,
        controllers,fanout,session.selector,session.gold,
        goldInbox,goldRouting));

    const auto referenceSpectrum=app::internal::M21ProductSpectrumEvidence::
        capture(controllers,fanout,session.selector,session.gold,
                goldInbox,goldRouting);
    QVERIFY(referenceSpectrum.current_owner);
    QCOMPARE(referenceSpectrum.selected,app::m21_private::Column::gold);
    QCOMPARE(referenceSpectrum.actual_spectrum,
             !goldMeasured.value()->average_spectrum.empty());
    QCOMPARE(referenceSpectrum.bins.size(),
             goldMeasured.value()->average_spectrum.size());
    QVERIFY(!referenceSpectrum.product_ui_authorized);
    // Switching the selected Reference must never silently render a
    // guessed spectrum from the unrelated SOURCE or PROCESSED PCM.


    QVERIFY(M21Owners::gold_still_current(
        controllers,fanout,session.selector,session.gold,
        *selectedGold.value()));
    // A huge but syntactically valid producer claim is refused before
    // open/allocate, rather than consuming arbitrarily large RAM or
    // freezing the Qt owner. A later streaming M19 will replace this
    // conservative explicit cap; it is NOT a hidden full-track PASS.
    auto oversize=*selectedGold.value();
    oversize.full_frames=16'777'217LL;
    QVERIFY(!M21Owners::measure_gold_worker(oversize));
    // The read-only worker must reject a physically replaced WAV whose
    // new metadata does not match the authenticated capture. The UI
    // still owns the old selection, but cannot publish a new false GOLD.
    QVERIFY(QFile::remove(first));
    const auto replacedGold=wav(dir,QStringLiteral("m18-first-source.wav"),
                                13,44100U,300U);
    QVERIFY(!replacedGold.isEmpty());
    QVERIFY(!M21Owners::measure_gold_worker(*selectedGold.value()));
    session.gold.clearGold();
    QVERIFY(!session.gold.has_gold());
    const auto retiredGold=goldInbox.view_current(
        controllers,fanout,session.selector,session.gold);
    QCOMPARE(retiredGold.columns[1]->lifecycle,
             app::m21_private::Lifecycle::stale);

    // Clearing a selected REFERENCE invalidates the exact same private
    // Qt panels without affecting Product DSP state or fabricating zeros.
    QVERIFY(!GoldQtReadiness::still_current(independentGoldPanels,
        controllers,fanout,session.selector,session.gold,
        goldInbox,goldRouting));
    goldNativeStatus.refresh();
    QVERIFY(!goldNativeStatus.production_authorized());
    QVERIFY(!goldNativeStatus.m20().value("ready").toBool());
    QCOMPARE(goldNativeStatus.m20().value(
        "privateEvidenceStatus").toString(),QStringLiteral("STALE"));
    QVERIFY(goldNativeStatus.m20().value(
        "privateQualityOnly").toMap().isEmpty());
    QVERIFY(!Handoff::publish_on_owner(
        controllers,fanout,session.selector,session.gold,
        handoffGoldInbox,*goldJob.value(),*goldFromThread->value()));
    QVERIFY(!app::internal::M21ProductSpectrumEvidence::still_current(
        referenceSpectrum,controllers,fanout,session.selector,session.gold,
        goldInbox,goldRouting));
    const auto retiredGoldPanel=GoldQtReadiness::capture(
        controllers,fanout,session.selector,session.gold,goldInbox,goldRouting);
    QVERIFY(!retiredGoldPanel.panels[1].value("ready").toBool());
    QVERIFY(retiredGoldPanel.panels[1]
        .value("privateQualityOnly").toMap().isEmpty());
    QVERIFY(retiredGoldPanel.panels[1].value("stereo").toMap().isEmpty());
    if(retiredGoldPanel.current_product_owner)
        QCOMPARE(retiredGoldPanel.panels[1].value(
            "privateEvidenceStatus").toString(),QStringLiteral("STALE"));

    QVERIFY(!M21Owners::gold_still_current(
        controllers,fanout,session.selector,session.gold,
        *selectedGold.value()));
}

void ProjectSessionTest::
m18RealProductVerifiedMasterToPcm24AndFloat32WavAck()
{
    // M18 EXIT GATE A: one real Project/SOURCE/Qt M17 Product and ONE
    // complete guard authority, followed by two actual transactional
    // Windows WAV deliveries with separate explicit user consent tickets.
    using app::internal::M18ProductCompleteBinding;
    using app::internal::M18PrivateProductExportAction;
    using app::internal::M18PrivateAsyncExportSession;
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr int seed=27;
    constexpr std::size_t n=4096;
    const auto sourcePath=wav(directory,
        QStringLiteral("m18-positive-real-source.wav"),seed,48000U,n);
    QVERIFY(!sourcePath.isEmpty());

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(sourcePath));
    QVERIFY(session.source.has_source());
    QCOMPARE(session.source.frame_count(),qint64{4096});
    QCOMPARE(session.source.sample_rate_hz(),qint64{48000});
    // The synthetic Device remains a REAL Qt PlaybackTransport
    // predecessor. It must hold the exact published MANUAL identity;
    // no terminal Master or WAV result is ever installed on the device.
    session.transport.set_pcm_prepare_handler(
        [&session](audio::AudioBufferView view,
            std::shared_ptr<const void> lease,
            std::optional<core::RealizationId> id)->core::Status {
            if(!lease||!id||view.frame_count().value()<=0)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_REAL_PRODUCT_DEVICE_REQUIRES_MANUAL_OWNER"});
            session.observed->state=core::PlaybackState::STOPPED;
            session.observed->position=view.absolute_start_frame();
            session.observed->duration=view.frame_count();
            session.observed->loop.reset();
            session.observed->audibleRealization={
                core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        });
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    auto rate48=core::SampleRate::create(48000);
    auto frames=core::FrameCount::create(static_cast<std::int64_t>(n));
    auto block=core::FrameCount::create(128);
    QVERIFY(registry&&rate48&&frames&&block);
    auto fmt=audio::AudioFormat::create(
        *rate48.value(),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto samples=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(samples);
    // The MANUAL RenderResult must consume the ACTUAL decoded selected
    // SOURCE bytes, not a numerical approximation recreated by the test.
    auto sourceRef=platform::windows::WindowsResourceReader::
        make_read_reference(sourcePath.toUtf8().toStdString(),
                             "m18-positive-real-source.wav");
    QVERIFY(sourceRef);
    auto sourceHandle=platform::windows::WindowsResourceReader::
        open_read_only(*sourceRef.value());
    QVERIFY(sourceHandle);
    auto sourceDecoder=audio::WavReader::open(
        std::move(*sourceHandle.value()));
    QVERIFY(sourceDecoder);
    QCOMPARE((*sourceDecoder.value())->info().frame_count(),*frames.value());
    QCOMPARE((*sourceDecoder.value())->info().audio_format(),*fmt.value());
    auto decoded=(*sourceDecoder.value())->read_frames(
        core::FrameIndex{0},samples.value()->mutable_view());
    QVERIFY(decoded);
    QCOMPARE(*decoded.value(),*frames.value());
    QVERIFY((*sourceDecoder.value())->close());
    auto emptyManual=dsp::ProcessingChain::create(
        *registry.value(),{dsp::ProcessingStage::MASTER,
                           dsp::ChainSegment::MANUAL});
    QVERIFY(emptyManual);
    auto renderManual=[&](const dsp::ProcessingChain& chain,
        std::vector<dsp::ModuleExecutionBinding> bindings)
        ->core::Result<render::RenderResult> {
        auto request=render::RenderRequest::create(
            samples.value()->view(),
            samples.value()->view().absolute_range(),
            chain,std::move(bindings),*block.value());
        if(!request)return core::Result<render::RenderResult>::failure(
            *request.error());
        return render::render_preview(*request.value(),*registry.value());
    };
    auto prepared=renderManual(*emptyManual.value(),{});
    QVERIFY2(prepared,prepared?"":prepared.error()->message().c_str());
    auto processed=renderManual(
        session.masteringChainState->chain(),
        session.masteringChainState->execution_bindings());
    QVERIFY2(processed,processed?"":processed.error()->message().c_str());
    QVERIFY(session.selector.set_prepared_realization(
        std::move(*prepared.value())));
    QVERIFY(session.selector.set_processed_realization(
        std::move(*processed.value())));
    QVERIFY(session.selector.switch_to(app::AuditionTarget::PROCESSED));
    const auto manualId=session.selector.processed_realization_id();
    QVERIFY(manualId);
    const auto acceptedManual=session.selector.processed_realization_snapshot();
    QVERIFY(acceptedManual);
    QCOMPARE(acceptedManual->view().frame_count(),*frames.value());
    QCOMPARE(session.observed->audibleRealization.realizationId,manualId);

    app::MasterOutputRateState rate;
    rate.source_committed(48000);
    session.project.set_master_output_rate_state(&rate);
    install_m18_checked_source_dispatch(session,rate);
    auto limiterId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse(
            "c3000000-0000-4000-8000-000000000018").value());
    QVERIFY(limiterId);
    auto terminal=app::TerminalLimiterState::create_default(
        *registry.value(),*limiterId.value());
    QVERIFY(terminal);
    QVERIFY(terminal.value()->set_user_bypass(false));
    app::TerminalCandidateGate candidates;
    app::TerminalAudioSessionGate device{
        candidates,*session.observed,
        [](audio::AudioBufferView,std::shared_ptr<const void>,
           core::RealizationId)->core::Status {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_VERIFIED_MASTER_MAY_NOT_INSTALL_DEVICE"});
        }};
    app::TerminalPrivatePublicationCoordinator publisher{
        candidates,device,session.transport,
        [&session](auto p,auto m,auto id) {
            return session.selector.reserve_private_terminal_realization_id(
                std::move(p),std::move(m),id);
        }};
    app::TerminalPrivateRenderController renderer{
        candidates,*terminal.value(),
        [&session]{return session.selector.prepared_realization_snapshot();},
        [&session]{return session.selector.processed_realization_snapshot();},
        [&session]{return session.selector.processed_realization_id();},
        [&session]{return session.masteringChainState->chain();}};
    app::TerminalPrivateProductSession product{
        renderer,publisher,candidates,session.transport,
        [&session]{return session.selector.prepared_realization_snapshot();},
        [&session]{return session.selector.processed_realization_snapshot();},
        [&session]{return session.selector.processed_realization_id();},
        [&session]{return session.region.region();},
        [&session] {
            return session.selector.active_target()
                ==app::AuditionTarget::PROCESSED;
        }};
    const M18ProductExportIdentity identity{
        901U,902U,903U,session.masteringChainState->chain().revision(),
        48000U,rate.rate_revision()};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        17U,identity);
    M18OwnerMutationFanout fanout{owner,17U,identity};
    app::internal::M18ProductUpstreamBinding::Controllers upstream{
        session.project,session.source,rate,
        *session.masteringChainState,session.selector,session.region,
        session.gold,*terminal.value()};
    M18ProductCompleteBinding::Controllers controllers{
        upstream,product,candidates};
    QVERIFY(M18ProductCompleteBinding::preflight(controllers,fanout));
    QVERIFY(M18ProductCompleteBinding::bind(controllers,fanout));
    QVERIFY(M18ProductCompleteBinding::installed(controllers));
    QVERIFY(!owner->capture(identity));
    M18PrivateProductExportAction action{controllers,fanout};
    // The very same QObject adapter intended for the GUI must observe
    // Product readiness, and it must NOT offer Export before M17 has
    // completed its M16 checked terminal render.
    app::M18ProductExportViewModel ui{action,controllers};
    QVERIFY(!ui.can_export());
    QVERIFY(!ui.can_cancel());
    QVERIFY(!ui.can_acknowledge());

    const auto preparedOk=product.request_private(*rate48.value(),true);
    QVERIFY2(preparedOk,preparedOk?"":preparedOk.error()->message().c_str());
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),30000);
    const auto verified=product.capture_m18_verified_stage_for_worker(
        *rate48.value());
    QVERIFY(verified);
    QCOMPARE(verified,candidates.staged());
    // Actual PREPARED and PROCESSED Product analysis, not fixture-provided
    // DTOs. The captured immutable RenderResult owners and accepted
    // RealizationId MUST match the real Qt AuditionSourceSelector.
    using M21Owners=app::internal::M21ProductOwnerAnalysis;
    using Handoff=app::internal::M21ProductAnalysisHandoff;
    auto analysisOwners=M21Owners::capture(
        controllers,fanout,session.selector);
    QVERIFY2(analysisOwners,
        analysisOwners?"":analysisOwners.error()->message().c_str());
    QCOMPARE(analysisOwners.value()->prepared,
             session.selector.prepared_realization_snapshot());
    QCOMPARE(analysisOwners.value()->processed,
             session.selector.processed_realization_snapshot());
    QCOMPARE(analysisOwners.value()->processed_realization_id,
             session.selector.processed_realization_id());
    QVERIFY(M21Owners::still_current(
        controllers,fanout,session.selector,*analysisOwners.value()));
    const auto preparedMetrics=M21Owners::measure_current(
        controllers,fanout,session.selector,*analysisOwners.value(),
        app::m21_private::Column::prepared);
    const auto processedMetrics=M21Owners::measure_current(
        controllers,fanout,session.selector,*analysisOwners.value(),
        app::m21_private::Column::processed);
    QVERIFY2(preparedMetrics,
        preparedMetrics?"":preparedMetrics.error()->message().c_str());
    QVERIFY2(processedMetrics,
        processedMetrics?"":processedMetrics.error()->message().c_str());
    // This worker-side computation uses ONLY retained immutable PCM.
    // Its numeric results agree with the guarded synchronous Product
    // reference; neither operation copies QObject state to a worker.
    const auto workerPrepared=M21Owners::measure_render_worker(
        *analysisOwners.value(),app::m21_private::Column::prepared);
    const auto workerProcessed=M21Owners::measure_render_worker(
        *analysisOwners.value(),app::m21_private::Column::processed);
    QVERIFY(workerPrepared&&workerProcessed);
    QCOMPARE(workerPrepared.value()->resource_identity,
             preparedMetrics.value()->resource_identity);
    QCOMPARE(workerProcessed.value()->resource_identity,
             processedMetrics.value()->resource_identity);
    app::internal::M21ProductPublicationInbox productInbox;
    QVERIFY(productInbox.accept_render(
        controllers,fanout,session.selector,*analysisOwners.value(),
        *workerPrepared.value()));
    QVERIFY(productInbox.accept_render(
        controllers,fanout,session.selector,*analysisOwners.value(),
        *workerProcessed.value()));
    const auto visible=productInbox.view_current(
        controllers,fanout,session.selector,session.gold);
    QCOMPARE(visible.columns[0]->lifecycle,
             app::m21_private::Lifecycle::current);
    QCOMPARE(visible.columns[2]->lifecycle,
             app::m21_private::Lifecycle::current);
    QVERIFY(!visible.columns[1]); // Never invent GOLD.

    // PREPARED and PROCESSED are two independent REAL RenderResult
    // owners. Exercise the same capture/worker/publish protocol as the
    // selected REFERENCE WAV, never changing a QObject on the worker.
    app::internal::M21ProductPublicationInbox handoffRenderInbox;
    for(const auto target:{app::m21_private::Column::prepared,
                           app::m21_private::Column::processed}) {
        const auto job=Handoff::capture_on_owner(
            controllers,fanout,session.selector,session.gold,target);
        QVERIFY(job);
        QVERIFY(job.value()->structurally_valid());
        auto wrongGeneration=*job.value();
        ++wrongGeneration.captured_epoch;
        QVERIFY(!wrongGeneration.structurally_valid());
        QVERIFY(!Handoff::measure_on_worker(wrongGeneration));
        QVERIFY(job.value()->render.has_value());
        QVERIFY(!job.value()->reference.has_value());
        std::optional<core::Result<app::m21_private::Snapshot>> computed;
        std::thread numericalWorker([&] {
            computed.emplace(Handoff::measure_on_worker(*job.value()));
        });
        numericalWorker.join();
        QVERIFY(computed&&*computed);
        const auto& direct=target==app::m21_private::Column::prepared
            ?*workerPrepared.value():*workerProcessed.value();
        QCOMPARE(computed->value()->resource_identity,
                 direct.resource_identity);
        QCOMPARE(computed->value()->generation_token,
                 direct.generation_token);
        QCOMPARE(computed->value()->scope_frames,direct.scope_frames);
        QVERIFY(Handoff::publish_on_owner(
            controllers,fanout,session.selector,session.gold,
            handoffRenderInbox,*job.value(),*computed->value()));
        QVERIFY(!Handoff::publish_on_owner(
            controllers,fanout,session.selector,session.gold,
            handoffRenderInbox,*job.value(),*computed->value()));
    }
    const auto handoffVisible=handoffRenderInbox.view_current(
        controllers,fanout,session.selector,session.gold);
    QCOMPARE(handoffVisible.columns[0]->lifecycle,
             app::m21_private::Lifecycle::current);
    QCOMPARE(handoffVisible.columns[2]->lifecycle,
             app::m21_private::Lifecycle::current);
    QVERIFY(!handoffVisible.columns[1]);


    // Actual M18 Product epoch and M21 publication inbox feed three
    // independent PRIVATE Qt readiness surfaces. Values must remain
    // masked until later real Phase-6 QML authorization; none can use
    // test fixtureMode or pretend the independent REFERENCE is present.
    using QtReadiness=app::internal::M21ProductQtReadinessBridge;
    QtReadiness::Routing analysisRouting;
    QVERIFY(analysisRouting.select(QtReadiness::AnalysisPanel::tonal_m20,
        app::m21_private::Column::processed));
    QVERIFY(analysisRouting.select(QtReadiness::AnalysisPanel::delta_m21,
        app::m21_private::Column::gold));
    const auto privatePanels=QtReadiness::capture(controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting);
    QVERIFY(privatePanels.current_product_owner);
    QVERIFY(!privatePanels.product_ui_authorized);
    QVERIFY(privatePanels.owner_epoch>0);
    // Concrete Qt QObject installed by app/main.cpp observes only
    // authenticated Product-derived statuses, no fake numeric rows.
    app::M21ProductStatusViewModel liveNativeStatus{
        controllers,fanout,session.selector,session.gold,productInbox};
    QVERIFY(!liveNativeStatus.production_authorized());
    QCOMPARE(liveNativeStatus.m19().value(
        "privateEvidenceStatus").toString(),QStringLiteral("CURRENT"));
    QVERIFY(liveNativeStatus.selectPanelSource(
        1,QStringLiteral("PROCESSED")));
    QCOMPARE(liveNativeStatus.m20().value(
        "privateEvidenceStatus").toString(),QStringLiteral("CURRENT"));
    QCOMPARE(liveNativeStatus.m20().value("sourceRole").toString(),
             QStringLiteral("PROCESSED"));
    QVERIFY(liveNativeStatus.m20().value("stereo").toMap().isEmpty());
    QVERIFY(liveNativeStatus.m21().value("metrics").toList().isEmpty());
    QVERIFY(!liveNativeStatus.selectPanelSource(
        -1,QStringLiteral("PREPARED")));
    QVERIFY(!liveNativeStatus.selectPanelSource(
        0,QStringLiteral("INVALID")));
    bool workerUiRejected=false;
    std::thread foreignStatus([&] {
        workerUiRejected=!liveNativeStatus.selectPanelSource(
            0,QStringLiteral("REFERENCE"));
        liveNativeStatus.refresh(); // no-op off owner Qt thread
    });
    foreignStatus.join();
    QVERIFY(workerUiRejected);
    const auto qtProductOwner=fanout.current_owner_snapshot();
    QVERIFY(qtProductOwner);
    QCOMPARE(privatePanels.owner_epoch,qtProductOwner->epoch);
    QVERIFY(privatePanels.owner_identity==qtProductOwner->identity);
    // Even an otherwise valid captured frame is not re-authorized by
    // its epoch alone: all M18 Product identity fields are mandatory.
    auto tamperedQtOwner=privatePanels;
    ++tamperedQtOwner.owner_identity.source_generation;
    QVERIFY(!QtReadiness::still_current(tamperedQtOwner,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));
    tamperedQtOwner=privatePanels;
    ++tamperedQtOwner.owner_identity.project_generation;
    QVERIFY(!QtReadiness::still_current(tamperedQtOwner,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));
    tamperedQtOwner=privatePanels;
    ++tamperedQtOwner.owner_identity.master_rate_revision;
    QVERIFY(!QtReadiness::still_current(tamperedQtOwner,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));

    // Proof also includes the selected real M21 result identity, not just
    // the Product-wide Source/Project epoch. No recomputation is triggered
    // by merely testing a queued Qt frame.
    tamperedQtOwner=privatePanels;
    ++tamperedQtOwner.identities[0].generation;
    QVERIFY(!QtReadiness::still_current(tamperedQtOwner,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));
    tamperedQtOwner=privatePanels;
    tamperedQtOwner.identities[1].resource+=":synthetic";
    QVERIFY(!QtReadiness::still_current(tamperedQtOwner,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));
    tamperedQtOwner=privatePanels;
    ++tamperedQtOwner.identities[0].frames;
    QVERIFY(!QtReadiness::still_current(tamperedQtOwner,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));
    QCOMPARE(privatePanels.panels[0].value("sourceRole").toString(),
             QStringLiteral("PREPARED"));
    QCOMPARE(privatePanels.panels[1].value("sourceRole").toString(),
             QStringLiteral("PROCESSED"));
    QCOMPARE(privatePanels.panels[2].value("sourceRole").toString(),
             QStringLiteral("REFERENCE"));
    QCOMPARE(privatePanels.panels[0].value("privateEvidenceStatus").toString(),
             QStringLiteral("CURRENT"));
    QCOMPARE(privatePanels.panels[1].value("privateEvidenceStatus").toString(),
             QStringLiteral("CURRENT"));
    QCOMPARE(privatePanels.panels[2].value("privateEvidenceStatus").toString(),
             QStringLiteral("MISSING"));
    const auto levelsQuality=privatePanels.panels[0]
        .value("privateQualityOnly").toMap();
    QCOMPARE(levelsQuality.size(),5);
    QVERIFY(levelsQuality.contains("lufs"));
    QVERIFY(levelsQuality.contains("rms"));
    QCOMPARE(levelsQuality.value("truePeak").toString(),
        QStringLiteral("M16 UNQUALIFIED"));
    const auto stereoQuality=privatePanels.panels[1]
        .value("privateQualityOnly").toMap();
    QCOMPARE(stereoQuality.size(),4);
    QVERIFY(stereoQuality.contains("width"));
    QVERIFY(!stereoQuality.contains("lufs"));
    QVERIFY(privatePanels.panels[2]
        .value("privateQualityOnly").toMap().isEmpty());
    for(const auto& map:{levelsQuality,stereoQuality}) {
        for(auto it=map.cbegin();it!=map.cend();++it) {
            QVERIFY(!it.value().toString().isEmpty());
            bool isNumeric=false;
            static_cast<void>(it.value().toString().toDouble(&isNumeric));
            QVERIFY(!isNumeric);
        }
    }
    for(const auto& panel:privatePanels.panels) {
        QVERIFY(!panel.value("ready").toBool());
        QVERIFY(!panel.value("fixtureMode").toBool());
        QVERIFY(!panel.value("productionAuthorized").toBool());
        QVERIFY(panel.value("levels").toMap().isEmpty());
        QVERIFY(panel.value("stereo").toMap().isEmpty());
        QVERIFY(panel.value("metrics").toList().isEmpty());
        QVERIFY(panel.value("spectrum").toList().isEmpty());
        QVERIFY(panel.value("bands").toList().isEmpty());
    }
    QVERIFY(QtReadiness::still_current(privatePanels,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));

    // This M20 proof is backed by the REAL privately published
    // PROCESSED M19/M20 result; it never synthesizes PSD from LUFS,
    // exports bins to QML or upgrades unavailable Phase-6 status.
    using SpectrumProof=app::internal::M21ProductSpectrumEvidence;
    const auto provenSpectrum=SpectrumProof::capture(
        controllers,fanout,session.selector,session.gold,
        productInbox,analysisRouting);
    QVERIFY(provenSpectrum.current_owner);
    QVERIFY(!provenSpectrum.product_ui_authorized);
    QCOMPARE(provenSpectrum.selected,app::m21_private::Column::processed);
    QCOMPARE(provenSpectrum.actual_spectrum,
             !workerProcessed.value()->average_spectrum.empty());
    QCOMPARE(provenSpectrum.bins.size(),
             workerProcessed.value()->average_spectrum.size());
    for(std::size_t b=0;b<provenSpectrum.bins.size();++b) {
        QCOMPARE(provenSpectrum.bins[b].hz,
                 workerProcessed.value()->average_spectrum[b].hz);
        QCOMPARE(provenSpectrum.bins[b].linear_power,
                 *workerProcessed.value()->average_spectrum[b]
                    .power_density.unrounded);
    }
    QCOMPARE(SpectrumProof::still_current(provenSpectrum,
        controllers,fanout,session.selector,session.gold,
        productInbox,analysisRouting),provenSpectrum.actual_spectrum);
    for(const auto& panel:privatePanels.panels) {
        QVERIFY(panel.value("spectrum").toList().isEmpty());
        QVERIFY(panel.value("bands").toList().isEmpty());
    }


    // A numerical worker cannot inspect Qt-owned routing, owner state or
    // private Product metrics. Its entire return is a static masked map.
    bool rejectedWorker=false;
    std::thread privateQtWorker([&] {
        const auto refused=QtReadiness::capture(
            controllers,fanout,session.selector,session.gold,
            productInbox,analysisRouting);
        rejectedWorker=!refused.current_product_owner
            &&!refused.product_ui_authorized
            &&!refused.panels[0].value("ready").toBool()
            &&refused.panels[0].value("levels").toMap().isEmpty()
            &&refused.panels[0].value("sourceRole").toString()
                ==QStringLiteral("UNAVAILABLE")
            &&!QtReadiness::still_current(privatePanels,controllers,fanout,
                session.selector,session.gold,productInbox,analysisRouting);
    });
    privateQtWorker.join();
    QVERIFY(rejectedWorker);

    QVERIFY(analysisRouting.select(QtReadiness::AnalysisPanel::tonal_m20,
        app::m21_private::Column::prepared));
    QVERIFY(!QtReadiness::still_current(privatePanels,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));
    QVERIFY(!SpectrumProof::still_current(provenSpectrum,
        controllers,fanout,session.selector,session.gold,
        productInbox,analysisRouting));
    const auto reroutedPanels=QtReadiness::capture(controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting);
    QVERIFY(QtReadiness::still_current(reroutedPanels,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));

    QVERIFY(!productInbox.accept_render(
        controllers,fanout,session.selector,*analysisOwners.value(),
        *workerPrepared.value())); // Duplicate epoch rejected.
    for(std::size_t k=0;k<9U;++k) {
        QCOMPARE(workerPrepared.value()->metrics[k].quality,
                 preparedMetrics.value()->metrics[k].quality);
        QCOMPARE(workerPrepared.value()->metrics[k].unrounded,
                 preparedMetrics.value()->metrics[k].unrounded);
        QCOMPARE(workerProcessed.value()->metrics[k].unrounded,
                 processedMetrics.value()->metrics[k].unrounded);
    }
    QCOMPARE(preparedMetrics.value()->scope_frames,
             static_cast<std::uint64_t>(n));
    QCOMPARE(processedMetrics.value()->scope_frames,
             static_cast<std::uint64_t>(n));
    QCOMPARE(preparedMetrics.value()->sample_rate_hz,48000U);
    QCOMPARE(processedMetrics.value()->sample_rate_hz,48000U);
    QCOMPARE(preparedMetrics.value()->column,
             app::m21_private::Column::prepared);
    QCOMPARE(processedMetrics.value()->column,
             app::m21_private::Column::processed);
    QVERIFY(preparedMetrics.value()->resource_identity
            !=processedMetrics.value()->resource_identity);
    // GOLD is a separately selected immutable file and must never be
    // invented by re-tagging PREPARED or PROCESSED PCM.
    QVERIFY(!M21Owners::measure_current(
        controllers,fanout,session.selector,*analysisOwners.value(),
        app::m21_private::Column::gold));
    QVERIFY(ui.can_export());
    QVERIFY(!ui.can_cancel());
    QVERIFY(!ui.can_acknowledge());
    QCOMPARE(ui.status_text(),QStringLiteral("IDLE"));
    QVERIFY(verified->rendered.terminal.checked.final_check);
    QCOMPARE(verified->rendered.manual_chain_revision,
             session.masteringChainState->chain().revision());
    // M22 Product parity: PREVIEW must borrow the EXACT already checked
    // immutable FULL M17 MASTER. Its binary64 selected samples equal the
    // absolute full-programme crop bit-for-bit, without resetting M11–17
    // DSP state or re-running a region-local limiter/M16 measurement.
    const auto selectedWindow=core::FrameRange::create(
        core::FrameIndex{731},core::FrameIndex{2831});
    QVERIFY(selectedWindow);
    const auto fullMaster=verified->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    const auto preview=app::internal::m22_verified_master_preview_crop(
        controllers,fanout,verified,*selectedWindow.value());
    QVERIFY2(preview,preview?"":preview.error()->message().c_str());
    QCOMPARE(preview.value()->frame_count().value(),qint64{2100});
    QCOMPARE(preview.value()->absolute_start_frame().value(),qint64{731});
    QCOMPARE(preview.value()->timebase().frame_domain_id(),
             audio::FrameDomainId::OUTPUT_RATE);
    using FullChainEpoch=app::internal::M22ProductChainEpochCaptureCandidate;
    // Capture a REAL installed Qt Product owner, not the compile-only
    // contract probe. It must bind one actual SOURCE resource identity,
    // one immutable manual/Q0/M17/M16 full chain and current M18 epoch.
    const auto completeChain=FullChainEpoch::capture(
        controllers,fanout,verified);
    QVERIFY2(completeChain,
        completeChain ? "" : completeChain.error()->message().c_str());
    QVERIFY(!completeChain.value()->authorizes_release());
    QVERIFY(FullChainEpoch::still_current(
        controllers,fanout,*completeChain.value()));
    QCOMPARE(completeChain.value()->exact_m17_owner,verified);
    QCOMPARE(completeChain.value()->immutable_chain_witness.manual_source_hz,
             std::int64_t{48000});
    QCOMPARE(completeChain.value()->immutable_chain_witness.final_master_hz,
             std::int64_t{48000});
    QCOMPARE(completeChain.value()->immutable_chain_witness.complete_source_frames,
             std::int64_t{4096});
    QCOMPARE(completeChain.value()->immutable_chain_witness.actual_master_frames,
             std::int64_t{4096});
    QVERIFY(!completeChain.value()->immutable_chain_witness.used_original_q0);
    QVERIFY(completeChain.value()->immutable_chain_witness
                 .frozen_original_q0_sha256.empty());
    using PreviewAdmission=app::internal::M22ProductPreviewAdmission;
    const auto admittedMasterPreview=PreviewAdmission::capture(
        controllers,fanout,verified,*selectedWindow.value());
    QVERIFY2(admittedMasterPreview,
        admittedMasterPreview ? "" :
        admittedMasterPreview.error()->message().c_str());
    QCOMPARE(admittedMasterPreview.value()->crop.absolute_start_frame(),
             preview.value()->absolute_start_frame());
    QCOMPARE(admittedMasterPreview.value()->crop.frame_count(),
             preview.value()->frame_count());
    QVERIFY(PreviewAdmission::still_current(
        controllers,fanout,*admittedMasterPreview.value()));
    // The captured interval aliases the very same checked full M17 PCM,
    // with an immutable full-owner lease. No pseudo-Preview numerical
    // result, worker render or second M16 endpoint is acceptable.
    for(std::size_t ch=0;ch<fullMaster.format().channel_count();++ch) {
        const auto whole=fullMaster.channel(ch);
        const auto crop=admittedMasterPreview.value()->crop.channel(ch);
        QVERIFY(whole&&crop);
        QCOMPARE(crop.value()->data(),whole.value()->data()+731);
        QCOMPARE(crop.value()->size(),std::size_t{2100});
    }
    // Non-owner worker may hold immutable PCM to inspect, but cannot
    // obtain or re-authorize Product Preview through any Qt source.
    bool refusedOffThread=false;
    std::thread privatePreviewWorker([&] {
        const auto forbidden=PreviewAdmission::capture(
            controllers,fanout,verified,*selectedWindow.value());
        const auto forbiddenEpoch=FullChainEpoch::capture(
            controllers,fanout,verified);
        refusedOffThread=!forbidden&&!forbiddenEpoch
            &&!FullChainEpoch::still_current(
                controllers,fanout,*completeChain.value())
            &&!PreviewAdmission::still_current(
                controllers,fanout,*admittedMasterPreview.value());
    });
    privatePreviewWorker.join();
    QVERIFY(refusedOffThread);
    auto wholeChannel=fullMaster.channel(0);
    auto segmentChannel=preview.value()->channel(0);
    QVERIFY(wholeChannel&&segmentChannel);
    for(std::size_t i=0;i<segmentChannel.value()->size();++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>((*segmentChannel.value())[i]),
                 std::bit_cast<std::uint64_t>((*wholeChannel.value())[i+731U]));
    }

    // M22 absolute full-history parity boundary matrix: the preview
    // MUST be an actual zero-copy slice of the original checked M17
    // full programme at the exact OUTPUT_RATE origin, including the
    // very first and last frame. No local DSP state reset, no alternate
    // regional True Peak, and no one-frame off-by-one at EOS.
    constexpr std::array<std::pair<qint64,qint64>,5> kPreviewWindows{{
        {0,static_cast<qint64>(n)}, // entire verified programme
        {0,1},                     // first output sample
        {static_cast<qint64>(n)-1,static_cast<qint64>(n)}, // EOS
        {125,128},                 // short interior segment
        {3719,3857},               // non-aligned final region
    }};
    for(const auto& [begin,end]:kPreviewWindows) {
        const auto requested=core::FrameRange::create(
            core::FrameIndex{begin},core::FrameIndex{end});
        QVERIFY(requested);
        const auto actual=app::internal::m22_verified_master_preview_crop(
            controllers,fanout,verified,*requested.value());
        const auto admitted=PreviewAdmission::capture(
            controllers,fanout,verified,*requested.value());
        QVERIFY(admitted);
        QVERIFY(PreviewAdmission::still_current(
            controllers,fanout,*admitted.value()));
        QCOMPARE(admitted.value()->crop.absolute_start_frame().value(),begin);
        QCOMPARE(admitted.value()->crop.frame_count().value(),end-begin);
        QVERIFY2(actual,actual?"":actual.error()->message().c_str());
        QCOMPARE(actual.value()->absolute_start_frame().value(),begin);
        QCOMPARE(actual.value()->frame_count().value(),end-begin);
        QCOMPARE(actual.value()->timebase().frame_domain_id(),
                 audio::FrameDomainId::OUTPUT_RATE);
        for(std::size_t ch=0;ch<fullMaster.format().channel_count();++ch) {
            const auto allSamples=fullMaster.channel(ch);
            const auto previewSamples=actual.value()->channel(ch);
            QVERIFY(allSamples&&previewSamples);
            // Verify aliasing, not merely numerical equality: no copy,
            // resampling, re-quantization or second render is permitted.
            QCOMPARE(previewSamples.value()->data(),
                     allSamples.value()->data()+begin);
            for(std::size_t i=0;i<previewSamples.value()->size();++i) {
                QCOMPARE(std::bit_cast<std::uint64_t>(
                             (*previewSamples.value())[i]),
                         std::bit_cast<std::uint64_t>(
                             (*allSamples.value())[i+begin]));
            }
        }
    }
    const auto illegalWindow=core::FrameRange::create(
        core::FrameIndex{731},core::FrameIndex{4097});
    QVERIFY(illegalWindow);
    QVERIFY(!app::internal::m22_verified_master_preview_crop(
        controllers,fanout,verified,*illegalWindow.value()));
    QVERIFY(!PreviewAdmission::capture(
        controllers,fanout,verified,*illegalWindow.value()));

    QCOMPARE(verified->rendered.terminal.checked.terminal
                 .compensated_pcm.view().frame_count(),*frames.value());
    QVERIFY(!owner->capture(fanout.current_owner_snapshot()->identity));
    QCOMPARE(session.observed->audibleRealization.realizationId,manualId);
    // The accepted M17 stage must not be enough to republish onto the
    // original SOURCE path, even with a matching physical file identity.
    const auto originalSourceRef=session.source.source_resource()->reference();
    QVERIFY(!action.begin(originalSourceRef,
        audio::WavSampleFormat::IEEE_F32,std::nullopt,
        terminal.value()->parameters().ceiling_db_tp(),true));
    QVERIFY(!action.has_unacknowledged_export());
    QVERIFY(owner->can_install_full_product_guards(
        fanout.current_owner_snapshot()->epoch,
        fanout.current_owner_snapshot()->identity));
    // The Product dialog cannot create a new "export only" dBTP Ceiling
    // unrelated to the actually accepted M17 limiter parameters.
    const auto wrongCeilingPath=directory.filePath(
        QStringLiteral("m18-unverified-ceiling.wav"));
    auto wrongCeilingRef=platform::windows::WindowsResourceWriter::
        make_write_reference(wrongCeilingPath.toUtf8().toStdString(),
                             "m18-unverified-ceiling.wav");
    QVERIFY(wrongCeilingRef);
    QVERIFY(!action.begin(*wrongCeilingRef.value(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,-2.0,true));
    QVERIFY(!QFileInfo::exists(wrongCeilingPath));
    QVERIFY(!action.has_unacknowledged_export());

    const auto initialProductEpoch=fanout.current_owner_snapshot();
    QVERIFY(initialProductEpoch);
    std::uint64_t expectedConsentEpoch=initialProductEpoch->epoch;
    for(const auto encoding:{
            audio::WavSampleFormat::PCM_S24,
            audio::WavSampleFormat::IEEE_F32}) {
        const auto filename=QStringLiteral("m18-product-authorized-%1.wav")
            .arg(static_cast<int>(encoding));
        const auto output=directory.filePath(filename);
        auto destination=platform::windows::WindowsResourceWriter::
            make_write_reference(output.toUtf8().toStdString(),
                                 filename.toUtf8().toStdString());
        QVERIFY(destination);
        const auto seedForPcm=encoding==audio::WavSampleFormat::PCM_S24
            ?std::optional<std::uint64_t>{UINT64_C(1234567)}
            :std::nullopt;
        QVERIFY(!action.has_unacknowledged_export());
        // Invalid PCM24 seed or a dithered Float32 dialog choice cannot
        // consume consent, create partial output or start the M18 worker.
        const auto malformedSeed=encoding==audio::WavSampleFormat::PCM_S24
            ?std::optional<std::uint64_t>{}
            :std::optional<std::uint64_t>{UINT64_C(17)};
        QVERIFY(!action.begin(
            *destination.value(),encoding,malformedSeed,
            terminal.value()->parameters().ceiling_db_tp(),true));
        QVERIFY(!QFileInfo::exists(output));
        QVERIFY(!action.has_unacknowledged_export());
        // The same Product, verified stage and valid output format are
        // insufficient without a NEW explicit user confirmation.
        QVERIFY(!action.begin(
            *destination.value(),encoding,seedForPcm,
            terminal.value()->parameters().ceiling_db_tp(),false));
        QVERIFY(!QFileInfo::exists(output));
        QVERIFY(!owner->capture(fanout.current_owner_snapshot()->identity));
        const bool accepted=ui.beginExport(QUrl::fromLocalFile(output),
            encoding==audio::WavSampleFormat::PCM_S24?0:1);
        QVERIFY2(accepted,ui.error_text().toUtf8().constData());
        QVERIFY(action.has_unacknowledged_export());
        // M22 Preview cannot retain a prior authorisation while an
        // M18 export ticket is live. A queued GUI preview must drop
        // the old verified owner even though its PCM remains in memory.
        QVERIFY(!PreviewAdmission::still_current(
            controllers,fanout,*admittedMasterPreview.value()));
        QVERIFY(!FullChainEpoch::still_current(
            controllers,fanout,*completeChain.value()));
        QVERIFY(!FullChainEpoch::capture(
            controllers,fanout,verified));
        QVERIFY(!PreviewAdmission::capture(
            controllers,fanout,verified,*selectedWindow.value()));
        QVERIFY(!ui.can_export()); // one unacknowledged Product job
        // The first worker might finish before the next Qt instruction.
        // Do not make a timing-dependent false "still encoding" claim.
        // Irrespective of timing, an unacknowledged result must prohibit
        // any second Product worker or replacement of the output path.
        QVERIFY(!action.begin(
            *destination.value(),encoding,seedForPcm,
            terminal.value()->parameters().ceiling_db_tp(),true));
        QTRY_VERIFY_WITH_TIMEOUT(
            action.snapshot().phase==
                M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED,
            30000);
        const auto report=action.snapshot();
        QCOMPARE(report.encoded_frames,report.total_frames);
        QCOMPARE(report.total_frames,static_cast<std::uint64_t>(n));
        QVERIFY(!report.failure_code);
        QVERIFY(report.verified_file_size_bytes);
        QVERIFY(*report.verified_file_size_bytes>0);
        QCOMPARE(report.verified_encoded_sha256.size(),std::size_t{64});
        QVERIFY(QFileInfo::exists(output));
        QCOMPARE(static_cast<std::uint64_t>(QFileInfo{output}.size()),
                 *report.verified_file_size_bytes);
        // Verify the actual user-visible WAV with a second independent
        // decoder path; an encoded checksum alone is not PCM format proof.
        auto outputRef=platform::windows::WindowsResourceReader::
            make_read_reference(output.toUtf8().toStdString(),
                                filename.toUtf8().toStdString());
        QVERIFY(outputRef);
        auto readerResource=platform::windows::WindowsResourceReader::
            open_read_only(*outputRef.value());
        QVERIFY(readerResource);
        auto inspected=audio::WavReader::open(
            std::move(*readerResource.value()));
        QVERIFY2(inspected,inspected?"":inspected.error()->message().c_str());
        QCOMPARE((*inspected.value())->info().encoded_sample_format(),encoding);
        QCOMPARE((*inspected.value())->info().frame_count(),*frames.value());
        QCOMPARE((*inspected.value())->info().audio_format().sample_rate(),
                 *rate48.value());
        QCOMPARE((*inspected.value())->info().audio_format().channel_layout(),
                 audio::ChannelLayout::MONO_C);
        // M22 true Product end-to-end path:
        // SOURCE decoded -> genuine MANUAL -> checked full M17 MASTER
        // -> REAL user-consented M18 IEEE_F32 WAV -> independently
        // decoded samples. Quantization must be exactly one f64->f32
        // cast; no second compressor/limiter or region-only rerender.
        if(encoding==audio::WavSampleFormat::IEEE_F32
            ||encoding==audio::WavSampleFormat::PCM_S24) {
            auto decodedExport=audio::AudioBuffer::create(
                *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                core::FrameIndex{0},*frames.value());
            QVERIFY(decodedExport);
            const auto readBack=(*inspected.value())->read_frames(
                core::FrameIndex{0},decodedExport.value()->mutable_view());
            QVERIFY(readBack);
            QCOMPARE(*readBack.value(),*frames.value());
            const auto exported=decodedExport.value()->view();
            const auto master=verified->rendered.terminal.checked.terminal
                .compensated_pcm.view();
            QCOMPARE(master.frame_count(),exported.frame_count());
            QCOMPARE(master.format().sample_rate(),
                     exported.format().sample_rate());
            for(std::size_t channel=0;
                channel<master.format().channel_count();++channel) {
                const auto finalM17=master.channel(channel);
                const auto finalWav=exported.channel(channel);
                QVERIFY(finalM17&&finalWav);
                QCOMPARE(finalM17.value()->size(),finalWav.value()->size());
                for(std::size_t sample=0;
                    sample<finalWav.value()->size();++sample) {
                    const auto checkedMaster=(*finalM17.value())[sample];
                    const auto actualWav=(*finalWav.value())[sample];
                    QVERIFY(std::isfinite(checkedMaster));
                    QVERIFY(std::isfinite(actualWav));
                    if(encoding==audio::WavSampleFormat::IEEE_F32) {
                        const auto expected=static_cast<double>(
                            static_cast<float>(checkedMaster));
                        QCOMPARE(std::bit_cast<std::uint64_t>(actualWav),
                                 std::bit_cast<std::uint64_t>(expected));
                    }else {
                        // Genuine M18 Philox/TPDF PCM24 quantization may
                        // vary individual bits but cannot create a second
                        // sound stage. Eight PCM24 LSB is a deliberately
                        // loose dither+quantizer upper bound, NOT a
                        // substitute for the frozen statistical QTP.
                        constexpr double kMaxDitherQuantizerError=
                            8.0/8388608.0;
                        QVERIFY(std::abs(actualWav-checkedMaster)
                                <=kMaxDitherQuantizerError);
                    }
                }
            }
        }
        QVERIFY((*inspected.value())->close());
        QVERIFY(action.can_acknowledge());
        QVERIFY(ui.can_acknowledge());
        QVERIFY(!action.can_cancel());
        QCOMPARE(ui.status_text(),QStringLiteral("VERIFIED DELIVERED"));
        QCOMPARE(ui.verified_sha256().size(),qsizetype{64});
        QCOMPARE(ui.verified_bytes(),
                 static_cast<qulonglong>(*report.verified_file_size_bytes));
        QVERIFY(ui.acknowledgeExport());
        QVERIFY(!ui.can_acknowledge());
        QVERIFY(ui.can_export());
        // ACK is a NEW Product epoch; the old Preview receipt must
        // stay retired, even though the exact checked full Master
        // remains eligible for a fresh explicitly requested Preview.
        QVERIFY(!PreviewAdmission::still_current(
            controllers,fanout,*admittedMasterPreview.value()));
        const auto newPreviewEpoch=PreviewAdmission::capture(
            controllers,fanout,verified,*selectedWindow.value());
        QVERIFY(newPreviewEpoch);
        QVERIFY(!FullChainEpoch::still_current(
            controllers,fanout,*completeChain.value()));
        const auto refreshedFullChain=FullChainEpoch::capture(
            controllers,fanout,verified);
        QVERIFY(refreshedFullChain);
        QVERIFY(FullChainEpoch::still_current(
            controllers,fanout,*refreshedFullChain.value()));
        QVERIFY(PreviewAdmission::still_current(
            controllers,fanout,*newPreviewEpoch.value()));
        expectedConsentEpoch+=2; // one user consent, one terminal ACK
        const auto afterAck=fanout.current_owner_snapshot();
        QVERIFY(afterAck);
        QCOMPARE(afterAck->epoch,expectedConsentEpoch);
        QVERIFY(afterAck->identity==initialProductEpoch->identity);
        QVERIFY(!action.has_unacknowledged_export());
        QCOMPARE(action.snapshot().phase,
                 M18PrivateAsyncExportSession::Phase::IDLE);
        QVERIFY(!owner->capture(fanout.current_owner_snapshot()->identity));
        QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
        QCOMPARE(product.capture_m18_verified_stage_for_worker(
                     *rate48.value()),verified);
        QCOMPARE(session.observed->audibleRealization.realizationId,manualId);
        QCOMPARE(session.source.frame_count(),qint64{4096});
        QCOMPARE(session.source.display_name(),
                 QStringLiteral("m18-positive-real-source.wav"));
    }
    // M22 FULL is NOT a duplicate DSP renderer: it must take the exact
    // same verified M17 Master from the canonical M18 Product context
    // and dispatch through the existing user-consented M18 WAV action.
    using M22Dispatch=app::internal::M22ProductVerifiedDispatch;
    const auto dispatchOutput=directory.filePath(
        QStringLiteral("m22-current-verified-full-dispatch.wav"));
    auto dispatchResource=platform::windows::WindowsResourceWriter::
        make_write_reference(dispatchOutput.toUtf8().toStdString(),
            "m22-current-verified-full-dispatch.wav");
    QVERIFY(dispatchResource);
    const auto currentDispatchedPreview=M22Dispatch::begin_preview(
        controllers,fanout,verified,*selectedWindow.value());
    QVERIFY(currentDispatchedPreview);
    QVERIFY(!M22Dispatch::begin_full(
        controllers,fanout,action,{},*dispatchResource.value(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,
        terminal.value()->parameters().ceiling_db_tp(),true));
    QVERIFY(!M22Dispatch::begin_full(
        controllers,fanout,action,verified,*dispatchResource.value(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,
        terminal.value()->parameters().ceiling_db_tp(),false));
    QVERIFY(!QFileInfo::exists(dispatchOutput));
    const auto dispatched=M22Dispatch::begin_full(
        controllers,fanout,action,verified,*dispatchResource.value(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,
        terminal.value()->parameters().ceiling_db_tp(),true);
    QVERIFY2(dispatched,dispatched?"":dispatched.error()->message().c_str());
    QVERIFY(action.has_unacknowledged_export());
    QVERIFY(!M22Dispatch::begin_preview(
        controllers,fanout,verified,*selectedWindow.value()));
    QTRY_VERIFY_WITH_TIMEOUT(
        action.snapshot().phase==
            M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED,
        30000);
    QCOMPARE(action.snapshot().total_frames,
             static_cast<std::uint64_t>(n));
    QCOMPARE(action.snapshot().encoded_frames,
             static_cast<std::uint64_t>(n));
    QVERIFY(QFileInfo::exists(dispatchOutput));
    QCOMPARE(action.snapshot().verified_encoded_sha256.size(),
             std::size_t{64});
    QVERIFY(action.acknowledge());
    expectedConsentEpoch+=2;
    const auto afterDispatch=fanout.current_owner_snapshot();
    QVERIFY(afterDispatch);
    QCOMPARE(afterDispatch->epoch,expectedConsentEpoch);
    QVERIFY(afterDispatch->identity==initialProductEpoch->identity);
    QVERIFY(!PreviewAdmission::still_current(
        controllers,fanout,*currentDispatchedPreview.value()));
    QVERIFY(ui.can_export());

    // A fresh confirmed Export into an already-published WAV must fail
    // without overwriting the first verified output or losing the M17
    // Master. A failed job still requires explicit ACK to release consent.
    const auto collisionPath=directory.filePath(
        QStringLiteral("m18-product-authorized-%1.wav")
            .arg(static_cast<int>(audio::WavSampleFormat::IEEE_F32)));
    const auto originalBytes=QFileInfo{collisionPath}.size();
    QVERIFY(originalBytes>0);
    auto collisionRef=platform::windows::WindowsResourceWriter::
        make_write_reference(collisionPath.toUtf8().toStdString(),
                             "m18-f32-verified-existing.wav");
    QVERIFY(collisionRef);
    QVERIFY(ui.beginExport(QUrl::fromLocalFile(collisionPath),1));
    QVERIFY(!ui.can_export());
    QTRY_VERIFY_WITH_TIMEOUT(
        action.snapshot().phase==M18PrivateAsyncExportSession::Phase::FAILED,
        30000);
    QVERIFY(action.snapshot().failure_code);
    QVERIFY(!action.snapshot().verified_file_size_bytes);
    QVERIFY(action.snapshot().verified_encoded_sha256.empty());
    QCOMPARE(QFileInfo{collisionPath}.size(),originalBytes);
    QVERIFY(!action.can_cancel());
    QVERIFY(action.can_acknowledge());
    QVERIFY(ui.can_acknowledge());
    QCOMPARE(ui.status_text(),QStringLiteral("FAILED"));
    QVERIFY(!ui.error_text().isEmpty());
    QVERIFY(ui.acknowledgeExport());
    QVERIFY(ui.can_export());
    expectedConsentEpoch+=2;
    const auto afterCollision=fanout.current_owner_snapshot();
    QVERIFY(afterCollision);
    QCOMPARE(afterCollision->epoch,expectedConsentEpoch);
    QVERIFY(afterCollision->identity==initialProductEpoch->identity);
    QVERIFY(!action.has_unacknowledged_export());
    QCOMPARE(product.capture_m18_verified_stage_for_worker(
                 *rate48.value()),verified);
    QCOMPARE(session.observed->audibleRealization.realizationId,manualId);

    // A previously verified and ACKed M17 Master is NOT permission to
    // export after a direct MANUAL DSP edit. A parameter-only Gain change
    // deliberately leaves the chain's structural revision unchanged:
    // the exact verified M17 Product generation must still be retired.
    QVERIFY(ui.can_export());
    const auto revisionBeforeParam=
        session.masteringChainState->chain().revision();
    const auto differentGain=dsp::GainParameters::create(2.25);
    QVERIFY(differentGain);
    QVERIFY(session.masteringChainState->set_gain_parameters(
        *differentGain.value()));
    QCOMPARE(session.masteringChainState->chain().revision(),
             revisionBeforeParam);
    QVERIFY(!fanout.m17_verified_epoch_current());
    QVERIFY(!ui.can_export());
    // M21 must reject an earlier real-PCM result after this very same
    // accepted DSP change, even when chain().revision() did not move.
    QVERIFY(!M21Owners::still_current(
        controllers,fanout,session.selector,*analysisOwners.value()));
    QVERIFY(!M21Owners::measure_current(
        controllers,fanout,session.selector,*analysisOwners.value(),
        app::m21_private::Column::prepared));
    // A completed worker may still hold its old valid-looking numbers;
    // the owning Qt context MUST refuse publication of this capture.
    QVERIFY(workerPrepared.value()->metrics[2].unrounded);
    const auto stalePresentation=productInbox.view_current(
        controllers,fanout,session.selector,session.gold);
    QCOMPARE(stalePresentation.columns[0]->lifecycle,
             app::m21_private::Lifecycle::stale);
    QCOMPARE(stalePresentation.columns[2]->lifecycle,
             app::m21_private::Lifecycle::stale);

    // Same checked Product readout must be revoked after parameter-only
    // DSP revision even though the structural chain revision is unchanged.
    QVERIFY(!QtReadiness::still_current(reroutedPanels,controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting));
    // Old M20 proof remains stale even if the original double samples
    // are retained in private memory.
    QVERIFY(!SpectrumProof::still_current(provenSpectrum,
        controllers,fanout,session.selector,session.gold,
        productInbox,analysisRouting));
    const auto stalePrivatePanels=QtReadiness::capture(controllers,fanout,
        session.selector,session.gold,productInbox,analysisRouting);
    QVERIFY(!stalePrivatePanels.product_ui_authorized);
    liveNativeStatus.refresh();
    QVERIFY(!liveNativeStatus.production_authorized());
    QVERIFY(!liveNativeStatus.m19().value("ready").toBool());
    QVERIFY(liveNativeStatus.m19().value(
        "privateQualityOnly").toMap().isEmpty());
    QVERIFY(!stalePrivatePanels.panels[0].value("ready").toBool());
    QVERIFY(stalePrivatePanels.panels[0]
        .value("privateQualityOnly").toMap().isEmpty());
    QVERIFY(stalePrivatePanels.panels[0].value("levels").toMap().isEmpty());
    if(stalePrivatePanels.current_product_owner)
        QCOMPARE(stalePrivatePanels.panels[0].value(
            "privateEvidenceStatus").toString(),QStringLiteral("STALE"));

    QVERIFY(!productInbox.accept_render(
        controllers,fanout,session.selector,*analysisOwners.value(),
        *workerPrepared.value())); // Late worker refused.
    QVERIFY(!M21Owners::still_current(
        controllers,fanout,session.selector,*analysisOwners.value()));
    // The stale M17 shared_ptr is still in memory, but M22 Preview may
    // not expose it after even a parameter-only accepted Gain edit.
    QVERIFY(!app::internal::m22_verified_master_preview_crop(
        controllers,fanout,verified,*selectedWindow.value()));
    QVERIFY(!PreviewAdmission::still_current(
        controllers,fanout,*admittedMasterPreview.value()));
    QVERIFY(!FullChainEpoch::still_current(
        controllers,fanout,*completeChain.value()));
    QVERIFY(!FullChainEpoch::capture(
        controllers,fanout,verified));
    QVERIFY(!PreviewAdmission::capture(
        controllers,fanout,verified,*selectedWindow.value()));
    const auto parameterOnlyStalePath=directory.filePath(
        QStringLiteral("m18-stale-parameter-only-master.wav"));
    QVERIFY(!ui.beginExport(
        QUrl::fromLocalFile(parameterOnlyStalePath),1));
    QVERIFY(!QFileInfo::exists(parameterOnlyStalePath));
    QVERIFY(!action.has_unacknowledged_export());
    const auto gain=session.masteringChainState->gain_instance_id();
    const auto oldBypass=session.masteringChainState->is_bypassed(gain);
    QVERIFY(oldBypass);
    QVERIFY(session.masteringChainState->set_user_bypass(
        gain,!(*oldBypass.value())));
    QVERIFY(!ui.can_export());
    const auto stalePath=directory.filePath(
        QStringLiteral("m18-stale-manual-revision.wav"));
    QVERIFY(!ui.beginExport(QUrl::fromLocalFile(stalePath),1));
    QVERIFY(!action.has_unacknowledged_export());
    QVERIFY(!QFileInfo::exists(stalePath));
    QVERIFY(M18ProductCompleteBinding::reconcile_after_product_mutation(
        controllers,fanout));
    QVERIFY(!ui.can_export());
    QVERIFY(!ui.beginExport(QUrl::fromLocalFile(stalePath),0));
    QVERIFY(!action.has_unacknowledged_export());
    QVERIFY(!QFileInfo::exists(stalePath));
}

void ProjectSessionTest::
m22LiveProductCrossRateQ0M17PreviewFullExportAndRevocation()
{
    // A physical SOURCE WAV and real installed Qt M18 Product, rather
    // than the compile-only M22 Product capture or a synthetic owner.
    // This test requires the frozen original Q0 bank installed in the
    // test tree. It NEVER releases private FIR coefficient bytes.
    using app::internal::M18ProductCompleteBinding;
    using app::internal::M18PrivateProductExportAction;
    using app::internal::M22ProductChainEpochCaptureCandidate;
    using app::internal::M22ProductPreviewAdmission;
    using app::internal::M22ProductVerifiedDispatch;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18OwnerMutationFanout;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    constexpr std::size_t kSourceFrames=4096;
    constexpr std::int64_t kOutFrames=
        (static_cast<std::int64_t>(kSourceFrames)*44100+48000-1)/48000;
    const auto sourcePath=wav(directory,
        QStringLiteral("m22-real-source-48000.wav"),39,48000U,kSourceFrames);
    QVERIFY(!sourcePath.isEmpty());
    QFile inputBefore{sourcePath};
    QVERIFY(inputBefore.open(QIODevice::ReadOnly));
    const auto originalBytes=inputBefore.readAll();
    inputBefore.close();
    QVERIFY(!originalBytes.isEmpty());

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(sourcePath));
    QVERIFY(session.source.has_source());
    QCOMPARE(session.source.sample_rate_hz(),qint64{48000});
    QCOMPARE(session.source.frame_count(),qint64{kSourceFrames});
    session.transport.set_pcm_prepare_handler(
        [&session](audio::AudioBufferView view,
            std::shared_ptr<const void> lease,
            std::optional<core::RealizationId> id)->core::Status {
            if(!lease||!id)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M22_TEST_REQUIRES_ACTUAL_MANUAL_OWNER"});
            session.observed->state=core::PlaybackState::STOPPED;
            session.observed->position=view.absolute_start_frame();
            session.observed->duration=view.frame_count();
            session.observed->loop.reset();
            session.observed->audibleRealization={
                core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        });

    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    auto rate48=core::SampleRate::create(48000);
    auto rate44=core::SampleRate::create(44100);
    auto sourceFrames=core::FrameCount::create(
        static_cast<std::int64_t>(kSourceFrames));
    auto block=core::FrameCount::create(128);
    QVERIFY(registry&&rate48&&rate44&&sourceFrames&&block);
    auto ref=platform::windows::WindowsResourceReader::make_read_reference(
        sourcePath.toUtf8().toStdString(),"m22-real-source-48000.wav");
    QVERIFY(ref);
    auto file=platform::windows::WindowsResourceReader::open_read_only(
        *ref.value());
    QVERIFY(file);
    auto decoder=audio::WavReader::open(std::move(*file.value()));
    QVERIFY(decoder);
    auto format=decoder.value()->get()->info().audio_format();
    auto sourcePcm=audio::AudioBuffer::create(
        format,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*sourceFrames.value());
    QVERIFY(sourcePcm);
    auto read=decoder.value()->get()->read_frames(
        core::FrameIndex{0},sourcePcm.value()->mutable_view());
    QVERIFY(read);
    QCOMPARE(*read.value(),*sourceFrames.value());
    QVERIFY(decoder.value()->get()->close());

    auto neutral=dsp::ProcessingChain::create(
        *registry.value(),{dsp::ProcessingStage::MASTER,
                           dsp::ChainSegment::MANUAL});
    QVERIFY(neutral);
    const auto renderManual=[&](const dsp::ProcessingChain& chain,
        std::vector<dsp::ModuleExecutionBinding> bindings)
        ->core::Result<render::RenderResult> {
        auto request=render::RenderRequest::create(
            sourcePcm.value()->view(),
            sourcePcm.value()->view().absolute_range(),
            chain,std::move(bindings),*block.value());
        if(!request)return core::Result<render::RenderResult>::failure(
            *request.error());
        return render::render_preview(*request.value(),*registry.value());
    };
    auto prepared=renderManual(*neutral.value(),{});
    auto processed=renderManual(
        session.masteringChainState->chain(),
        session.masteringChainState->execution_bindings());
    QVERIFY(prepared&&processed);
    QVERIFY(session.selector.set_prepared_realization(
        std::move(*prepared.value())));
    QVERIFY(session.selector.set_processed_realization(
        std::move(*processed.value())));
    QVERIFY(session.selector.switch_to(app::AuditionTarget::PROCESSED));
    QVERIFY(session.selector.processed_realization_snapshot());

    app::MasterOutputRateState outputRate;
    outputRate.source_committed(48000);
    outputRate.set_original_q0_family_ready(true);
    QVERIFY(outputRate.choose_master_rate(44100));
    QCOMPARE(outputRate.master_rate_hz(),qint64{44100});
    session.project.set_master_output_rate_state(&outputRate);
    install_m18_checked_source_dispatch(session,outputRate);

    const auto limiterId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse(
            "c4000000-0000-4000-8000-000000000018").value());
    QVERIFY(limiterId);
    auto terminal=app::TerminalLimiterState::create_default(
        *registry.value(),*limiterId.value());
    QVERIFY(terminal);
    QVERIFY(terminal.value()->set_user_bypass(false));
    app::TerminalCandidateGate candidates;
    app::TerminalAudioSessionGate device{
        candidates,*session.observed,
        [](audio::AudioBufferView,std::shared_ptr<const void>,
           core::RealizationId)->core::Status {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M22_CROSS_RATE_MASTER_MAY_NOT_INSTALL_DEVICE"});
        }};
    app::TerminalPrivatePublicationCoordinator publisher{
        candidates,device,session.transport,
        [&session](auto p,auto m,auto id){
            return session.selector.reserve_private_terminal_realization_id(
                std::move(p),std::move(m),id);
        }};
    app::TerminalPrivateRenderController renderer{
        candidates,*terminal.value(),
        [&session]{return session.selector.prepared_realization_snapshot();},
        [&session]{return session.selector.processed_realization_snapshot();},
        [&session]{return session.selector.processed_realization_id();},
        [&session]{return session.masteringChainState->chain();}};
    const auto originalQ0Root=std::filesystem::path{RGSML_M22_SOURCE_DIR}
        /"resources"/"fir";
    renderer.set_original_q0_fir_root(originalQ0Root);
    QVERIFY2(renderer.full_original_q0_family_ready(),
        "Missing genuine complete SHA-authenticated original Q0 FIR bank");
    app::TerminalPrivateProductSession product{
        renderer,publisher,candidates,session.transport,
        [&session]{return session.selector.prepared_realization_snapshot();},
        [&session]{return session.selector.processed_realization_snapshot();},
        [&session]{return session.selector.processed_realization_id();},
        [&session]{return session.region.region();},
        [&session]{return session.selector.active_target()
                         ==app::AuditionTarget::PROCESSED;}};
    const M18ProductExportIdentity identity{
        991U,992U,993U,session.masteringChainState->chain().revision(),
        44100U,outputRate.rate_revision()};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        71U,identity);
    M18OwnerMutationFanout fanout{owner,71U,identity};
    app::internal::M18ProductUpstreamBinding::Controllers upstream{
        session.project,session.source,outputRate,
        *session.masteringChainState,session.selector,session.region,
        session.gold,*terminal.value()};
    M18ProductCompleteBinding::Controllers bound{
        upstream,product,candidates};
    QVERIFY(M18ProductCompleteBinding::preflight(bound,fanout));
    QVERIFY(M18ProductCompleteBinding::bind(bound,fanout));
    QVERIFY(!M18ProductCompleteBinding::can_offer_verified_master(
        bound,fanout));

    // Actual app M17 worker executes source-rate manual -> original Q0
    // 48000 to 44100 -> active M17 -> M16 global check. No test factory
    // may fabricate a verified M17 stage or a Product permission.
    const auto begin=product.request_private(*rate44.value(),true);
    QVERIFY2(begin,begin?"":begin.error()->message().c_str());
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),30000);
    const auto verified=product.capture_m18_verified_stage_for_worker(
        *rate44.value());
    QVERIFY(verified);
    QVERIFY(M18ProductCompleteBinding::can_offer_verified_master(
        bound,fanout));
    QVERIFY(!verified->rendered.terminal.checked.terminal.bypassed);
    QVERIFY(verified->rendered.terminal.checked.final_check.has_value());
    QCOMPARE(verified->rendered.output_rate.value(),std::int64_t{44100});
    QCOMPARE(verified->rendered.programme_frames.value(),
             std::int64_t{kSourceFrames});
    const auto full=verified->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    QCOMPARE(full.frame_count().value(),kOutFrames);
    QCOMPARE(full.format().sample_rate().value(),std::int64_t{44100});
    QCOMPARE(full.timebase().frame_domain_id(),
             audio::FrameDomainId::OUTPUT_RATE);

    const auto provenance=M22ProductChainEpochCaptureCandidate::capture(
        bound,fanout,verified);
    QVERIFY2(provenance,provenance?"":provenance.error()->message().c_str());
    QVERIFY(!provenance.value()->authorizes_release());
    QVERIFY(provenance.value()->immutable_chain_witness.used_original_q0);
    QCOMPARE(provenance.value()->immutable_chain_witness.manual_source_hz,
             std::int64_t{48000});
    QCOMPARE(provenance.value()->immutable_chain_witness.final_master_hz,
             std::int64_t{44100});
    QCOMPARE(provenance.value()->immutable_chain_witness.complete_source_frames,
             std::int64_t{kSourceFrames});
    QCOMPARE(provenance.value()->immutable_chain_witness.actual_master_frames,
             kOutFrames);
    QVERIFY(!provenance.value()->immutable_chain_witness
                 .frozen_original_q0_sha256.empty());
    QVERIFY(M22ProductChainEpochCaptureCandidate::still_current(
        bound,fanout,*provenance.value()));
    const auto window=core::FrameRange::create(
        core::FrameIndex{97},core::FrameIndex{913});
    QVERIFY(window);
    const auto preview=M22ProductVerifiedDispatch::begin_preview(
        bound,fanout,verified,*window.value());
    QVERIFY2(preview,preview?"":preview.error()->message().c_str());
    QVERIFY(M22ProductPreviewAdmission::still_current(
        bound,fanout,*preview.value()));
    QCOMPARE(preview.value()->crop.frame_count().value(),std::int64_t{816});
    QCOMPARE(preview.value()->crop.absolute_start_frame().value(),
             std::int64_t{97});
    for(std::size_t channel=0;channel<full.format().channel_count();++channel) {
        const auto whole=full.channel(channel);
        const auto part=preview.value()->crop.channel(channel);
        QVERIFY(whole&&part);
        QCOMPARE(part.value()->data(),whole.value()->data()+97);
        for(std::size_t i=0;i<part.value()->size();++i)
            QCOMPARE(std::bit_cast<std::uint64_t>((*part.value())[i]),
                     std::bit_cast<std::uint64_t>((*whole.value())[i+97]));
    }
    bool rejectedWorker=false;
    std::thread background([&]{
        rejectedWorker=
            !M22ProductChainEpochCaptureCandidate::capture(
                bound,fanout,verified)
            &&!M22ProductPreviewAdmission::capture(
                bound,fanout,verified,*window.value())
            &&!M22ProductChainEpochCaptureCandidate::still_current(
                bound,fanout,*provenance.value());
    });
    background.join();
    QVERIFY(rejectedWorker);

    M18PrivateProductExportAction exporter{bound,fanout};
    const auto filePath=directory.filePath(
        QStringLiteral("m22-real-output-44100-f32.wav"));
    auto destination=platform::windows::WindowsResourceWriter::
        make_write_reference(filePath.toUtf8().toStdString(),
            "m22-real-output-44100-f32.wav");
    QVERIFY(destination);
    QVERIFY(M22ProductVerifiedDispatch::begin_full(
        bound,fanout,exporter,verified,*destination.value(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,
        terminal.value()->parameters().ceiling_db_tp(),true));
    QVERIFY(!M22ProductChainEpochCaptureCandidate::still_current(
        bound,fanout,*provenance.value()));
    QVERIFY(!M22ProductPreviewAdmission::still_current(
        bound,fanout,*preview.value()));
    QTRY_VERIFY_WITH_TIMEOUT(exporter.snapshot().phase==
        app::internal::M18PrivateAsyncExportSession::Phase::VERIFIED_DELIVERED,
        30000);
    QCOMPARE(exporter.snapshot().encoded_frames,
             static_cast<std::uint64_t>(kOutFrames));
    QCOMPARE(exporter.snapshot().total_frames,
             static_cast<std::uint64_t>(kOutFrames));
    QVERIFY(QFileInfo::exists(filePath));

    // Independent on-disk readback: rate/format/frame count must come from
    // the WAV container, never the requested UI rate or a guessed ratio.
    auto outRef=platform::windows::WindowsResourceReader::make_read_reference(
        filePath.toUtf8().toStdString(),"m22-real-output-44100-f32.wav");
    QVERIFY(outRef);
    auto outHandle=platform::windows::WindowsResourceReader::open_read_only(
        *outRef.value());
    QVERIFY(outHandle);
    auto reader=audio::WavReader::open(std::move(*outHandle.value()));
    QVERIFY(reader);
    QCOMPARE(reader.value()->get()->info().frame_count().value(),kOutFrames);
    QCOMPARE(reader.value()->get()->info().audio_format()
                 .sample_rate().value(),std::int64_t{44100});
    QCOMPARE(reader.value()->get()->info().encoded_sample_format(),
             audio::WavSampleFormat::IEEE_F32);
    QVERIFY(reader.value()->get()->close());
    QFile inputAfter{sourcePath};
    QVERIFY(inputAfter.open(QIODevice::ReadOnly));
    QCOMPARE(inputAfter.readAll(),originalBytes);
    inputAfter.close();

    QVERIFY(exporter.acknowledge());
    QVERIFY(!M22ProductPreviewAdmission::still_current(
        bound,fanout,*preview.value()));
    QVERIFY(!M22ProductChainEpochCaptureCandidate::still_current(
        bound,fanout,*provenance.value()));
    const auto recaptured=M22ProductChainEpochCaptureCandidate::capture(
        bound,fanout,verified);
    QVERIFY(recaptured);
    QVERIFY(M22ProductChainEpochCaptureCandidate::still_current(
        bound,fanout,*recaptured.value()));
    // Changing MASTER output rate is a REAL Product mutation, and MUST
    // retire previously accepted cross-rate M17 even when SOURCE persists.
    QVERIFY(outputRate.choose_master_rate(48000));
    QVERIFY(M18ProductCompleteBinding::reconcile_after_product_mutation(
        bound,fanout));
    QVERIFY(!M22ProductChainEpochCaptureCandidate::still_current(
        bound,fanout,*recaptured.value()));
    QVERIFY(!M22ProductVerifiedDispatch::begin_preview(
        bound,fanout,verified,*window.value()));
    QVERIFY(!M18ProductCompleteBinding::can_offer_verified_master(
        bound,fanout));
}

void ProjectSessionTest::
m18CompleteRealProductBinderRequiresVerifiedM17BeforeUserConsent()
{
    using app::internal::M18ProductCompleteBinding;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=wav(dir,QStringLiteral("m18-complete-source.wav"),37);
    QVERIFY(!src.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(src));
    QVERIFY(session.source.has_source());
    app::MasterOutputRateState rate;
    rate.source_committed(session.source.sample_rate_hz());
    session.project.set_master_output_rate_state(&rate);
    install_m18_checked_source_dispatch(session,rate);
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto terminalId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse(
            "be000000-0000-4000-8000-000000000018").value());
    QVERIFY(terminalId);
    auto terminal=app::TerminalLimiterState::create_default(
        *registry.value(),*terminalId.value());
    QVERIFY(terminal);

    // These are the REAL M17 production classes from the existing
    // mastering-chain test target: neither fake authority nor standalone
    // stub may be substituted for a verified terminal Product owner.
    app::TerminalCandidateGate candidates;
    app::TerminalPrivateRenderController renderer{
        candidates,*terminal.value(),
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::optional<core::RealizationId>{return {};},
        [&] {return session.masteringChainState->chain();}};
    app::TerminalAudioSessionGate device{
        candidates,*session.observed,
        [](audio::AudioBufferView,std::shared_ptr<const void>,
           core::RealizationId)->core::Status {
            return core::Status::success();
        }};
    app::TerminalPrivatePublicationCoordinator publisher{
        candidates,device,session.transport,
        [](auto,auto,auto)->core::Result<core::RealizationId> {
            return core::Result<core::RealizationId>::failure(
                core::Error{core::ErrorCode::InvalidState,
                            "M18_TEST_NO_PRIVATE_REALIZATION_TO_PUBLISH"});
        }};
    app::TerminalPrivateProductSession m17{
        renderer,publisher,candidates,session.transport,
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::shared_ptr<const render::RenderResult>{return {};},
        []()->std::optional<core::RealizationId>{return {};},
        []()->std::optional<core::FrameRange>{return {};},
        []{return false;}};

    const M18ProductExportIdentity initial{
        201U,202U,203U,session.masteringChainState->chain().revision(),
        48000U,rate.rate_revision()};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        30U,initial);
    M18OwnerMutationFanout fanout{owner,30U,initial};
    app::internal::M18ProductUpstreamBinding::Controllers upstream{
        session.project,session.source,rate,*session.masteringChainState,
        session.selector,session.region,session.gold,*terminal.value()};
    M18ProductCompleteBinding::Controllers entire{
        upstream,m17,candidates};
    QVERIFY(M18ProductCompleteBinding::preflight(entire,fanout));
    bool workerRejected=false;
    std::thread foreign([&] {
        workerRejected=
            !M18ProductCompleteBinding::bind(entire,fanout);
    });
    foreign.join();
    QVERIFY(workerRejected);
    QVERIFY(M18ProductCompleteBinding::preflight(entire,fanout));
    QVERIFY(M18ProductCompleteBinding::bind(entire,fanout));
    QVERIFY(M18ProductCompleteBinding::installed(entire));
    QVERIFY(!M18ProductCompleteBinding::preflight(entire,fanout));
    QVERIFY(!M18ProductCompleteBinding::bind(entire,fanout));

    // Even a genuine Qt SOURCE and a fully wired Product do NOT
    // authorize WAV export in the absence of a real verified M17 Master.
    QVERIFY(!M18ProductCompleteBinding::authorize_verified_master_for_user(
        entire,fanout,false));
    QVERIFY(!M18ProductCompleteBinding::authorize_verified_master_for_user(
        entire,fanout,true));
    // Exercise the actual private Product Export action, not merely its
    // ticket issuer. An unverified M17 stage must fail BEFORE worker
    // launch and may not publish a WAV file or leave user consent.
    const auto deniedWav=dir.filePath(
        QStringLiteral("m18-no-verified-stage-export.wav"));
    const auto exportDestination=
        platform::windows::WindowsResourceWriter::
            make_write_reference(
                deniedWav.toUtf8().toStdString(),
                "m18-no-verified-stage-export.wav");
    QVERIFY(exportDestination);
    app::internal::M18PrivateProductExportAction exportAction{
        entire,fanout};
    QVERIFY(!exportAction.begin(
        *exportDestination.value(),audio::WavSampleFormat::IEEE_F32,
        std::nullopt,-1.0,true));
    QVERIFY(!exportAction.has_unacknowledged_export());
    QVERIFY(!exportAction.can_cancel());
    QVERIFY(!exportAction.can_acknowledge());
    QVERIFY(!QFileInfo::exists(deniedWav));
    QCOMPARE(exportAction.snapshot().phase,
             app::internal::M18PrivateAsyncExportSession::Phase::IDLE);
    // Invalid output requests are pure preflight: no ticket or worker
    // may be minted before a valid format and a real user decision.
    QVERIFY(!exportAction.begin(
        *exportDestination.value(),audio::WavSampleFormat::PCM_S24,
        std::nullopt,-1.0,true));
    QVERIFY(!exportAction.begin(
        *exportDestination.value(),audio::WavSampleFormat::IEEE_F32,
        std::optional<std::uint64_t>{42U},-1.0,true));
    QVERIFY(!exportAction.begin(
        *exportDestination.value(),audio::WavSampleFormat::IEEE_F32,
        std::nullopt,-1.0,false));
    // A dialog cannot invent a different M17 ceiling and then report
    // that its new WAV passed the wrong True Peak threshold.
    const auto mismatchedCeiling=exportAction.begin(
        *exportDestination.value(),audio::WavSampleFormat::IEEE_F32,
        std::nullopt,-2.0,true);
    QVERIFY(!mismatchedCeiling);
    QVERIFY(mismatchedCeiling.error()->message()
        =="M18_PRODUCT_EXPORT_CEILING_NOT_CURRENT_M17");
    QVERIFY(!exportAction.has_unacknowledged_export());
    QVERIFY(!QFileInfo::exists(deniedWav));
    // A selected SOURCE cannot simultaneously be the new output WAV.
    // Reject the direct identity alias before any consent transition.
    QVERIFY(session.source.source_resource());
    QVERIFY(!exportAction.begin(
        session.source.source_resource()->reference(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,-1.0,true));
    QVERIFY(!exportAction.has_unacknowledged_export());
    exportAction.cancel();
    QVERIFY(!exportAction.acknowledge());
    bool foreignStartDenied=false;
    bool foreignStateUnreadable=false;
    std::thread rogueExport([&] {
        foreignStartDenied=!exportAction.begin(
            *exportDestination.value(),audio::WavSampleFormat::IEEE_F32,
            std::nullopt,-1.0,true);
        exportAction.cancel();
        foreignStateUnreadable=!exportAction.can_cancel()
            &&!exportAction.can_acknowledge()
            &&!exportAction.has_unacknowledged_export()
            &&exportAction.snapshot().phase==
                app::internal::M18PrivateAsyncExportSession::Phase::FAILED;
    });
    rogueExport.join();
    QVERIFY(foreignStartDenied);
    QVERIFY(foreignStateUnreadable);
    QCOMPARE(exportAction.snapshot().phase,
             app::internal::M18PrivateAsyncExportSession::Phase::IDLE);
    QVERIFY(!owner->capture(initial));
    QVERIFY(owner->can_install_full_product_guards(30U,initial));
    // A legitimate Region edit consumes a single shared Product
    // generation but still cannot invent a freshly verified M17 stage.
    const auto region=core::FrameRange::create(
        core::FrameIndex{5},core::FrameIndex{23});
    QVERIFY(region);
    QVERIFY(session.region.set_region(*region.value()));
    QCOMPARE(fanout.current_owner_snapshot()->epoch,UINT64_C(31));
    QVERIFY(!owner->capture(initial));
    QVERIFY(!M18ProductCompleteBinding::authorize_verified_master_for_user(
        entire,fanout,true));

    // An accepted manual edit invalidates M17 but changes the real
    // M11–15 revision. Reconcile that accepted model revision before
    // any future verified Master is eligible for a fresh consent.
    const auto gainId=session.masteringChainState->gain_instance_id();
    const auto bypass=session.masteringChainState->is_bypassed(gainId);
    QVERIFY(bypass);
    QVERIFY(session.masteringChainState->set_user_bypass(
        gainId,!(*bypass.value())));
    const auto afterEdit=fanout.current_owner_snapshot();
    QVERIFY(afterEdit);
    QCOMPARE(afterEdit->epoch,UINT64_C(32));
    QVERIFY(afterEdit->identity.chain_revision!=
            session.masteringChainState->chain().revision());

    bool workerReconcileDenied=false;
    std::thread worker([&] {
        workerReconcileDenied=
            !M18ProductCompleteBinding::reconcile_after_product_mutation(
                entire,fanout);
    });
    worker.join();
    QVERIFY(workerReconcileDenied);
    QVERIFY(M18ProductCompleteBinding::reconcile_after_product_mutation(
        entire,fanout));
    const auto reconciled=fanout.current_owner_snapshot();
    QVERIFY(reconciled);
    QCOMPARE(reconciled->epoch,UINT64_C(33));
    QCOMPARE(reconciled->identity.source_generation,UINT64_C(201));
    QCOMPARE(reconciled->identity.project_generation,UINT64_C(202));
    QCOMPARE(reconciled->identity.master_generation,UINT64_C(205));
    QCOMPARE(reconciled->identity.chain_revision,
             session.masteringChainState->chain().revision());
    QCOMPARE(reconciled->identity.master_rate_revision,rate.rate_revision());
    // Reconciliation never grants an export ticket or silently revives
    // the now-obsolete M17 Master.
    QVERIFY(!owner->capture(reconciled->identity));
    QVERIFY(!M18ProductCompleteBinding::authorize_verified_master_for_user(
        entire,fanout,true));
    QVERIFY(M18ProductCompleteBinding::installed(entire));
}

void ProjectSessionTest::
m18SingleUpstreamBundleRejectsBusyWorkerAndDuplicateBinding()
{
    using app::internal::M18ProductUpstreamBinding;
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=wav(dir,QStringLiteral("m18-bound-src.wav"),17);
    const auto other=wav(dir,QStringLiteral("m18-bound-next-src.wav"),23);
    QVERIFY(!src.isEmpty()&&!other.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(src));
    QVERIFY(session.source.has_source());
    const auto original=session.source.display_name();
    app::MasterOutputRateState rate;
    rate.source_committed(session.source.sample_rate_hz());
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto id=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse(
            "ac000000-0000-4000-8000-000000000018").value());
    QVERIFY(id);
    auto terminal=app::TerminalLimiterState::create_default(
        *registry.value(),*id.value());
    QVERIFY(terminal);
    const M18ProductExportIdentity identity{
        51U,61U,71U,session.masteringChainState->chain().revision(),
        48000U,rate.rate_revision()};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        90U,identity);
    QVERIFY(owner->try_transition(90U,91U,identity,true));
    auto ticket=owner->capture(identity);
    QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,91U,identity};
    M18ProductUpstreamBinding::Controllers controllers{
        session.project,session.source,rate,*session.masteringChainState,
        session.selector,session.region,session.gold,*terminal.value()};
    QVERIFY(M18ProductUpstreamBinding::unbound(controllers));
    // A source picker's old void callback must not be accepted as an
    // epoch-safe commit: require the checked Product dispatcher first.
    QVERIFY(!M18ProductUpstreamBinding::bind(controllers,fanout));
    QVERIFY(M18ProductUpstreamBinding::unbound(controllers));
    install_m18_checked_source_dispatch(session,rate);
    QVERIFY(session.source.m18_checked_source_committed_installed());
    bool workerBindingDenied=false;
    std::thread forbiddenWorker([&] {
        workerBindingDenied=
            !M18ProductUpstreamBinding::bind(controllers,fanout);
    });
    forbiddenWorker.join();
    QVERIFY(workerBindingDenied);
    QVERIFY(M18ProductUpstreamBinding::unbound(controllers));
    // An independently installed GOLD ViewModel admission would double
    // charge the shared epoch: refuse the bundle until GOLD delegates
    // solely to the Audition selector.
    session.gold.set_private_m18_gold_mutation_admission(
        fanout.status_admission());
    QVERIFY(!M18ProductUpstreamBinding::bind(controllers,fanout));
    QVERIFY(!session.project.m18_open_batch_installed());
    QVERIFY(!session.source.m18_source_batch_installed());
    session.gold.set_private_m18_gold_mutation_admission({});
    QVERIFY(M18ProductUpstreamBinding::unbound(controllers));
    // Mixing an older one-shot Project policy with the new Project-open
    // physical batch would allow partial, separately admitted writes.
    session.project.set_private_m18_project_mutation_admission(
        fanout.status_admission());
    QVERIFY(!M18ProductUpstreamBinding::bind(controllers,fanout));
    QVERIFY(!session.source.m18_source_batch_installed());
    session.project.set_private_m18_project_mutation_admission({});
    QVERIFY(M18ProductUpstreamBinding::unbound(controllers));
    QVERIFY(M18ProductUpstreamBinding::bind(controllers,fanout));
    QVERIFY(M18ProductUpstreamBinding::installed(controllers));
    QVERIFY(!M18ProductUpstreamBinding::bind(controllers,fanout));
    auto beforeCommit=fanout.current_owner_snapshot();
    QVERIFY(beforeCommit);
    QCOMPARE(beforeCommit->epoch,UINT64_C(91));
    QCOMPARE(beforeCommit->identity.source_generation,UINT64_C(51));
    QCOMPARE(beforeCommit->identity.project_generation,UINT64_C(61));
    QCOMPARE(beforeCommit->identity.master_generation,UINT64_C(71));

    auto lease=ticket.value()->commit_lease_factory()();
    QVERIFY(lease);
    session.source.selectSource(QUrl::fromLocalFile(other));
    QCOMPARE(session.source.display_name(),original);
    QCOMPARE(session.source.sample_rate_hz(),qint64{48000});
    const auto blockedProject=dir.filePath(
        QStringLiteral("m18-blocked-project.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(blockedProject));
    QVERIFY(!QFileInfo::exists(blockedProject));
    QVERIFY(!terminal.value()->set_ceiling_db_tp(-2.0));
    const auto originalChainRevision=
        session.masteringChainState->chain().revision();
    const auto gainId=session.masteringChainState->gain_instance_id();
    const auto gainState=session.masteringChainState->is_bypassed(gainId);
    QVERIFY(gainState);
    QVERIFY(!session.masteringChainState->set_user_bypass(
        gainId,!(*gainState.value())));
    QCOMPARE(session.masteringChainState->chain().revision(),
             originalChainRevision);
    const auto oldRateRevision=rate.rate_revision();
    // A same-Fs SOURCE reselection is a fresh Product identity too.
    // Neither the selected rate nor its revision may change under BUSY.
    QVERIFY(!rate.try_source_committed_for_m18(48000));
    QCOMPARE(rate.rate_revision(),oldRateRevision);
    const auto candidate=core::FrameRange::create(
        core::FrameIndex{7},core::FrameIndex{19});
    QVERIFY(candidate);
    QVERIFY(!session.region.set_region(*candidate.value()));
    QVERIFY(!session.region.region());
    QVERIFY((*lease.value())->revalidate_current());

    // After release, one genuine region change consumes exactly the
    // shared generation and revokes the old asynchronous export consent.
    lease.value()->reset();
    QVERIFY(session.region.set_region(*candidate.value()));
    QVERIFY(session.region.region());
    auto afterRegion=fanout.current_owner_snapshot();
    QVERIFY(afterRegion);
    QCOMPARE(afterRegion->epoch,UINT64_C(92));
    QCOMPARE(afterRegion->identity.source_generation,UINT64_C(51));
    QCOMPARE(afterRegion->identity.project_generation,UINT64_C(61));
    QCOMPARE(afterRegion->identity.master_generation,UINT64_C(72));
    QVERIFY(!owner->capture(identity));
    QVERIFY(!ticket.value()->commit_lease_factory()());
    QVERIFY(M18ProductUpstreamBinding::installed(controllers));
    // After worker release, the real .rgsml SaveAs must own ONE physical
    // writer batch through serialization, file commit and readback.
    const auto deliveredProject=dir.filePath(
        QStringLiteral("m18-bound-delivered-project.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(deliveredProject));
    QVERIFY(QFileInfo::exists(deliveredProject));
    QVERIFY(session.project.error_message().isEmpty());
    auto afterSave=fanout.current_owner_snapshot();
    QVERIFY(afterSave);
    QCOMPARE(afterSave->epoch,UINT64_C(93));
    QCOMPARE(afterSave->identity.source_generation,UINT64_C(51));
    QCOMPARE(afterSave->identity.project_generation,UINT64_C(62));
    QCOMPARE(afterSave->identity.master_generation,UINT64_C(73));
    // Full Project OPEN must re-use one exclusive epoch for its nested
    // SOURCE, rate, audition and chain restoration callbacks. Neither
    // child may consume an independently authorized export generation.
    session.project.openProject(QUrl::fromLocalFile(deliveredProject));
    QVERIFY(session.project.error_message().isEmpty());
    QVERIFY(session.source.has_source());
    QCOMPARE(session.source.display_name(),original);
    auto afterOpen=fanout.current_owner_snapshot();
    QVERIFY(afterOpen);
    QCOMPARE(afterOpen->epoch,UINT64_C(94));
    QCOMPARE(afterOpen->identity.source_generation,UINT64_C(52));
    QCOMPARE(afterOpen->identity.project_generation,UINT64_C(63));
    QCOMPARE(afterOpen->identity.master_generation,UINT64_C(74));
    QVERIFY(M18ProductUpstreamBinding::installed(controllers));
}

void ProjectSessionTest::
m18ActualProjectOpenCheckedSourceFailureCannotPublishReady()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto source=wav(
        dir,QStringLiteral("m18-project-downstream-fault.wav"),21,48000U);
    QVERIFY(!source.isEmpty());
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(source));
    QVERIFY(author.source.has_source());
    const auto projectFile=dir.filePath(
        QStringLiteral("m18-project-downstream-fault.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(projectFile));
    QVERIFY(QFileInfo::exists(projectFile));

    Session target;
    app::MasterOutputRateState rate;
    target.project.set_master_output_rate_state(&rate);
    const M18ProductExportIdentity id{711U,712U,713U,0U,48000U,7U};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        401U,id);
    QVERIFY(owner->try_transition(401U,402U,id,true));
    auto ticket=owner->capture(id);QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,402U,id};
    app::internal::M18ProductControllerBinding::bind_source_chain_rate(
        target.source,*target.masteringChainState,rate,fanout);
    app::internal::M18ProductControllerBinding::bind_audition_region_gold(
        target.selector,target.region,target.gold,fanout);
    app::internal::M18ProductProjectSaveBinding::bind_project_open_and_save(
        target.project,fanout);
    target.source.set_private_m18_checked_source_committed_handler(
        [](const core::ResourceReference&)->core::Status {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_VERIFIED_SOURCE_PREPARED_HANDOFF_REFUSED"});
        });
    target.project.openProject(QUrl::fromLocalFile(projectFile));
    QVERIFY(!target.project.error_message().isEmpty());
    QVERIFY(!target.source.has_source());
    QCOMPARE(target.source.sample_rate_hz(),qint64{0});
    QCOMPARE(rate.rate_revision(),std::uint64_t{0});
    QVERIFY(!ticket.value()->commit_lease_factory()());
    QVERIFY(!owner->capture(id));

    // The physical failed batch released its writer lease. A fresh
    // explicitly checked SOURCE dispatch may successfully open the
    // same Project without reviving old verified-M17 consent.
    install_m18_checked_source_dispatch(target,rate);
    target.project.openProject(QUrl::fromLocalFile(projectFile));
    QCOMPARE(target.project.error_message(),QString());
    QVERIFY(target.source.has_source());
    QVERIFY(target.selector.prepared_available());
    QCOMPARE(target.source.sample_rate_hz(),qint64{48000});
    QCOMPARE(rate.source_rate_hz(),qint64{48000});
    QVERIFY(!ticket.value()->commit_lease_factory()());
}

void ProjectSessionTest::
m18AuditionGateSetterCannotSelfUninstallOrCrossThread()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto path=wav(dir,QStringLiteral("m18-audition-owner.wav"),13);
    QVERIFY(!path.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(path));
    QVERIFY(session.source.has_source());
    QVERIFY(session.selector.prepared_available());
    // The initial SOURCE callback already selected PREPARED. Repeating
    // that selection is correctly a no-op, so prepare a distinct GOLD
    // target to exercise a REAL Product-authorized owner mutation.
    const auto goldPath=wav(dir,QStringLiteral("m18-audition-gold.wav"),15);
    QVERIFY(!goldPath.isEmpty());
    session.gold.selectGold(QUrl::fromLocalFile(goldPath));
    QVERIFY(session.gold.has_gold());
    QVERIFY(session.selector.gold_available());
    QCOMPARE(session.selector.active_target(),
             std::optional{app::AuditionTarget::PREPARED});
    std::size_t denied=0;
    session.selector.set_private_m18_mutation_admission(
        [&](const char*)->core::Status {
            ++denied;
            // The running callback must not destroy its own closure.
            session.selector.set_private_m18_mutation_admission({});
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_AUDITION_WORKER_COMMIT_BUSY"});
        });
    std::thread foreignGateRemoval([&] {
        session.selector.set_private_m18_mutation_admission({});
    });
    foreignGateRemoval.join();
    const auto first=session.selector.switch_to(app::AuditionTarget::GOLD);
    QVERIFY(!first);
    QCOMPARE(denied,std::size_t{1});
    const auto second=session.selector.switch_to(app::AuditionTarget::GOLD);
    QVERIFY(!second);
    QCOMPARE(denied,std::size_t{2});
    session.selector.set_private_m18_mutation_admission({});
    const auto admitted=session.selector.switch_to(app::AuditionTarget::GOLD);
    QVERIFY(admitted);
}

void ProjectSessionTest::
m18ActualSourcePickerAndAuditionUseOnePhysicalProductBatch()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto sourcePath=wav(
        dir,QStringLiteral("m18-physical-batch-source.wav"),15,48000U);
    QVERIFY(!sourcePath.isEmpty());
    Session target;
    app::MasterOutputRateState targetRate;
    const M18ProductExportIdentity id{411U,412U,413U,0U,48000U,3U};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        2000U,id);
    QVERIFY(owner->try_transition(2000U,2001U,id,true));
    auto ticket=owner->capture(id);
    QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,2001U,id};
    install_m18_checked_source_dispatch(target,targetRate);
    app::internal::M18ProductControllerBinding::bind_source_chain_rate(
        target.source,*target.masteringChainState,targetRate,fanout);
    app::internal::M18ProductControllerBinding::bind_audition_region_gold(
        target.selector,target.region,target.gold,fanout);
    auto commit=ticket.value()->commit_lease_factory()();
    QVERIFY(commit);

    // The real picker must fail BEFORE updating SOURCE or entering the
    // synchronous AuditionSourceSelector's source_committed cascade.
    target.source.selectSource(QUrl::fromLocalFile(sourcePath));
    QVERIFY(!target.source.has_source());
    QVERIFY(!target.selector.prepared_available());
    QVERIFY((*commit.value())->revalidate_current());
    commit.value()->reset();

    // Exactly one Product SOURCE lease includes Prepared construction,
    // playback stop/handoff, selector Qt listeners and GOLD retirement.
    target.source.selectSource(QUrl::fromLocalFile(sourcePath));
    QVERIFY(target.source.has_source());
    QVERIFY(target.selector.prepared_available());
    QCOMPARE(target.source.sample_rate_hz(),qint64{48000});
    QVERIFY(!ticket.value()->commit_lease_factory()());
    QVERIFY(!owner->capture(id));
    // Subsequent manual DSP edits are a different Product action.
    const auto gain=dsp::GainParameters::create(2.0);
    QVERIFY(gain);
    QVERIFY(target.masteringChainState->set_gain_parameters(*gain.value()));
}

void ProjectSessionTest::
m18ActualProjectOpenAndSaveUseOneSharedProductEpoch()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=wav(dir,QStringLiteral("m18-actual-source.wav"),13,48000U);
    const auto goldFile=wav(dir,QStringLiteral("m18-actual-gold.wav"),21,48000U);
    QVERIFY(!src.isEmpty()&&!goldFile.isEmpty());
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(src));
    QVERIFY(author.source.has_source());
    author.gold.selectGold(QUrl::fromLocalFile(goldFile));
    QVERIFY(author.gold.has_gold());
    app::MasterOutputRateState authorRate;
    authorRate.source_committed(48000);
    QVERIFY(authorRate.choose_master_rate(192000));
    author.project.set_master_output_rate_state(&authorRate);
    const auto path=dir.filePath(QStringLiteral("m18-actual-open.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(path));
    QVERIFY(QFileInfo::exists(path));
    QCOMPARE(author.project.error_message(),QString());

    Session target;
    app::MasterOutputRateState targetRate;
    target.project.set_master_output_rate_state(&targetRate);
    const M18ProductExportIdentity id{191U,192U,193U,0U,48000U,11U};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        500U,id);
    QVERIFY(owner->try_transition(500U,501U,id,true));
    auto ticket=owner->capture(id);
    QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,501U,id};
    install_m18_checked_source_dispatch(target,targetRate);
    app::internal::M18ProductControllerBinding::bind_source_chain_rate(
        target.source,*target.masteringChainState,targetRate,fanout);
    app::internal::M18ProductControllerBinding::bind_audition_region_gold(
        target.selector,target.region,target.gold,fanout);
    app::internal::M18ProductProjectSaveBinding::bind_project_open_and_save(
        target.project,fanout);

    // Real worker lease prevents even the first transport/SOURCE change.
    auto committing=ticket.value()->commit_lease_factory()();
    QVERIFY(committing);
    target.project.openProject(QUrl::fromLocalFile(path));
    QVERIFY(!target.project.error_message().isEmpty());
    QVERIFY(!target.source.has_source());
    QVERIFY(!target.gold.has_gold());
    QCOMPARE(targetRate.rate_revision(),quint64{0});
    QVERIFY((*committing.value())->revalidate_current());
    committing.value()->reset();

    // Exactly one Project batch owns all nested real controller writes.
    target.project.openProject(QUrl::fromLocalFile(path));
    QCOMPARE(target.project.error_message(),QString());
    QVERIFY(target.source.has_source());
    QCOMPARE(target.source.sample_rate_hz(),qint64{48000});
    QVERIFY(target.gold.has_gold());
    QCOMPARE(targetRate.source_rate_hz(),qint64{48000});
    QCOMPARE(targetRate.master_rate_hz(),qint64{192000});
    QCOMPARE(targetRate.rate_revision(),quint64{2});
    QVERIFY(!ticket.value()->commit_lease_factory()());
    QVERIFY(!owner->capture(id));

    const auto saved=dir.filePath(QStringLiteral("m18-actual-save.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(saved));
    QVERIFY(QFileInfo::exists(saved));
    QCOMPARE(target.project.error_message(),QString());
    const auto loaded=load_project(saved);
    QVERIFY(loaded);
}

void ProjectSessionTest::
m18ProductSharedEpochSaveAsHoldsActualFilePublicationLease()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto source=wav(
        dir,QStringLiteral("m18-shared-save-source.wav"),36);
    QVERIFY(!source.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    QVERIFY(session.source.has_source());
    const auto original=dir.filePath(
        QStringLiteral("m18-shared-before.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(original));
    QVERIFY(QFileInfo::exists(original));
    QCOMPARE(session.project.error_message(),QString());
    const auto target=dir.filePath(
        QStringLiteral("m18-shared-after.rgsml"));
    const M18ProductExportIdentity id{41U,42U,43U,0U,48000U,5U};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        600U,id);
    QVERIFY(owner->try_transition(600U,601U,id,true));
    auto ticket=owner->capture(id);QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,601U,id};
    app::internal::M18ProductProjectSaveBinding::bind_project_save_only(
        session.project,fanout);
    auto worker=ticket.value()->commit_lease_factory()();
    QVERIFY(worker);
    session.project.saveProjectAs(QUrl::fromLocalFile(target));
    QVERIFY(!QFileInfo::exists(target));
    QVERIFY(!session.project.error_message().isEmpty());
    QVERIFY((*worker.value())->revalidate_current());
    worker.value()->reset();
    session.project.saveProjectAs(QUrl::fromLocalFile(target));
    QVERIFY(QFileInfo::exists(target));
    QCOMPARE(session.project.error_message(),QString());
    QVERIFY(!ticket.value()->commit_lease_factory()());
    QVERIFY(fanout.status_admission()("source_edit_after_save"));
    // Open Project remains fail-closed until its full, distinct nested
    // SOURCE/MASTER/GOLD exclusive transaction is also bound.
    session.project.openProject(QUrl::fromLocalFile(original));
    QVERIFY(!session.project.error_message().isEmpty());
    session.project.set_private_m18_project_save_batch_begin({});
}

void ProjectSessionTest::
m18RegionGateCannotSelfUninstallOrCrossThread()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto source=wav(dir,QStringLiteral("m18-region-owner.wav"),0);
    QVERIFY(!source.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    QVERIFY(session.source.has_source());
    const auto first=core::FrameRange::create(
        core::FrameIndex{7},core::FrameIndex{19});
    QVERIFY(first);
    const auto nested=core::FrameRange::create(
        core::FrameIndex{8},core::FrameIndex{18});
    QVERIFY(nested);
    std::size_t denied=0,nestedDenied=0;
    session.region.set_private_m18_region_mutation_admission(
        [&](const char*)->core::Status {
            ++denied;
            session.region.set_private_m18_region_mutation_admission({});
            const auto inner=session.region.set_region(*nested.value());
            if(!inner)++nestedDenied;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_REGION_WORKER_BUSY"});
        });
    std::thread foreign([&] {
        session.region.set_private_m18_region_mutation_admission({});
    });
    foreign.join();
    QVERIFY(!session.region.set_region(*first.value()));
    QVERIFY(!session.region.set_region(*first.value()));
    QCOMPARE(denied,std::size_t{2});
    QCOMPARE(nestedDenied,std::size_t{2});
    QVERIFY(!session.region.region());
    session.region.set_private_m18_region_mutation_admission({});
    QVERIFY(session.region.set_region(*first.value()));
    QVERIFY(session.region.region());
    QCOMPARE(session.region.region()->begin().value(),std::int64_t{7});
}

void ProjectSessionTest::
m18GoldReplacementAndClearDeniedBeforeSelectorMutation()
{
    // Optional M18 gate must refuse both GOLD paths before modifying the
    // actual AuditionSourceSelector owner. The existing M17 Product flow
    // remains unchanged when this gate is absent.
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto source=wav(dir,QStringLiteral("m18-gold-source.wav"),7);
    const auto goldA=wav(dir,QStringLiteral("m18-gold-a.wav"),8);
    const auto goldB=wav(dir,QStringLiteral("m18-gold-b.wav"),9);
    QVERIFY(!source.isEmpty()&&!goldA.isEmpty()&&!goldB.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    QVERIFY(session.source.has_source());
    // With a real M18 Product guard installed, clicking Clear on an
    // already-empty GOLD cannot revoke the current verified MASTER.
    std::size_t emptyClearAdmissions=0;
    session.gold.set_private_m18_gold_mutation_admission(
        [&](const char*)->core::Status {
            ++emptyClearAdmissions;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_EMPTY_GOLD_IS_NOT_AN_EPOCH_MUTATION"});
        });
    session.gold.clearGold();
    QCOMPARE(emptyClearAdmissions,std::size_t{0});
    QVERIFY(!session.gold.has_gold());
    QVERIFY(session.gold.error_message().isEmpty());
    session.gold.set_private_m18_gold_mutation_admission({});
    // Selecting the SOURCE itself as GOLD (or its alias) must fail
    // before the selector consumes a Product/MASTER export epoch.
    std::size_t forbiddenAliasAdmissions=0;
    session.selector.set_private_m18_mutation_admission(
        [&](const char*)->core::Status {
            ++forbiddenAliasAdmissions;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_GOLD_ALIAS_MUST_NOT_BURN_EPOCH"});
        });
    session.gold.selectGold(QUrl::fromLocalFile(source));
    QCOMPARE(forbiddenAliasAdmissions,std::size_t{0});
    QVERIFY(!session.gold.has_gold());
    QVERIFY(!session.gold.error_message().isEmpty());
    session.selector.set_private_m18_mutation_admission({});
    session.gold.selectGold(QUrl::fromLocalFile(goldA));
    QVERIFY(session.gold.has_gold());
    QVERIFY(session.gold.gold_resource());
    const std::string original{session.gold.gold_resource()->reference().locator()};
    std::size_t denied=0;
    session.gold.set_private_m18_gold_mutation_admission(
        [&](const char* operation)->core::Status {
            ++denied;
            // A synchronous callback cannot destroy its own Product gate.
            session.gold.set_private_m18_gold_mutation_admission({});
            if(operation==nullptr)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,"M18_GOLD_OPERATION_MISSING"});
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_GOLD_COMMIT_BUSY"});
        });
    std::thread foreignGateRemoval([&session] {
        session.gold.set_private_m18_gold_mutation_admission({});
    });
    foreignGateRemoval.join();
    session.gold.selectGold(QUrl::fromLocalFile(goldB));
    QVERIFY(session.gold.has_gold());
    QVERIFY(session.gold.gold_resource());
    QVERIFY(std::string{session.gold.gold_resource()->reference().locator()}==original);
    QCOMPARE(denied,std::size_t{1});
    QVERIFY(!session.gold.error_message().isEmpty());
    session.gold.clearGold();
    QVERIFY(session.gold.has_gold());
    QVERIFY(std::string{session.gold.gold_resource()->reference().locator()}==original);
    QCOMPARE(denied,std::size_t{2});
    // The foreign worker is NEVER permitted to mutate or even publish a
    // Qt-owned GOLD error; the Product owner remains the only writer.
    std::thread foreignClear([&session]{session.gold.clearGold();});
    foreignClear.join();
    QCOMPARE(denied,std::size_t{2});
    QVERIFY(session.gold.has_gold());
    QVERIFY(std::string{session.gold.gold_resource()->reference().display_name()}
            ==std::string{"m18-gold-a.wav"});
    session.gold.set_private_m18_gold_mutation_admission({});
    session.gold.selectGold(QUrl::fromLocalFile(goldB));
    QVERIFY(session.gold.has_gold());
    QVERIFY(session.gold.gold_resource());
    QVERIFY(std::string{session.gold.gold_resource()->reference().locator()}!=original);
    session.gold.clearGold();
    QVERIFY(!session.gold.has_gold());
}


void ProjectSessionTest::m18ProjectOpenBatchMustNotLeaveSaveAsUnguarded()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=wav(dir,QStringLiteral("batch-save-source.wav"),27,48000U);
    QVERIFY(!src.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(src));
    QVERIFY(session.source.has_source());
    const auto initial=dir.filePath(QStringLiteral("batch-save-initial.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(initial));
    QVERIFY(QFileInfo::exists(initial));
    // Installing only an Open Project batch cannot grant an unrelated
    // SaveAs permission: the new output must not be created.
    session.project.set_private_m18_project_open_batch_begin(
        []()->core::Result<std::shared_ptr<const void>> {
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(1));
        });
    const auto proposed=dir.filePath(QStringLiteral("batch-save-denied.rgsml"));
    // No Project API may be entered from a foreign worker once a Product
    // M18 gate is installed, including premature error QObjects/signals.
    QSignalSpy foreignSignals{&session.project,&app::ProjectSessionViewModel::changed};
    std::thread foreign([&session,&proposed,&initial] {
        session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
        session.project.openProject(QUrl::fromLocalFile(initial));
    });
    foreign.join();
    QCOMPARE(foreignSignals.count(),0);
    QVERIFY(!QFileInfo::exists(proposed));
    session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
    QVERIFY(!session.project.error_message().isEmpty());
    QVERIFY(!QFileInfo::exists(proposed));

    std::size_t saveCalls=0;
    session.project.set_private_m18_project_save_admission(
        [&](const char* op)->core::Status {
            if(op==nullptr)return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_PROJECT_SAVE_MISSING_OP"});
            ++saveCalls;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_PROJECT_SAVE_WORKER_BUSY"});
        });
    // A status-only callback cannot protect an Open-batch Project SaveAs
    // over a long synchronous write. Refuse without invoking the callback.
    session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
    QCOMPARE(saveCalls,std::size_t{0});
    QVERIFY(!QFileInfo::exists(proposed));
    // The legacy one-shot admission is still a supported isolated policy
    // when no Project-wide M18 batch is enabled (no background Product).
    session.project.set_private_m18_project_open_batch_begin({});
    session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
    QCOMPARE(saveCalls,std::size_t{1});
    QVERIFY(!QFileInfo::exists(proposed));

    // A throw from an installed save gate fails closed before any file
    // allocation; it cannot be mistaken for an empty successful policy.
    session.project.set_private_m18_project_save_admission(
        [&](const char*)->core::Status {
            ++saveCalls;
            throw std::runtime_error("M18_PROJECT_SAVE_ADMISSION_THROWN");
        });
    session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
    QCOMPARE(saveCalls,std::size_t{2});
    QVERIFY(!QFileInfo::exists(proposed));

    // Installing two competing SaveAs admission modes must be rejected:
    // the Project cannot choose the more permissive callback.
    session.project.set_private_m18_project_save_admission(
        [&](const char*)->core::Status {
            ++saveCalls;return core::Status::success();
        });
    session.project.set_private_m18_project_mutation_admission(
        [](const char*)->core::Status {return core::Status::success();});
    session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
    QCOMPARE(saveCalls,std::size_t{2});
    QVERIFY(!QFileInfo::exists(proposed));

    // A genuine M18 Product must furnish one real exclusive RAII batch
    // for both Open and SaveAs; a mere status callback is insufficient.
    session.project.set_private_m18_project_mutation_admission({});
    session.project.set_private_m18_project_save_admission({});
    session.project.set_private_m18_project_open_batch_begin(
        []()->core::Result<std::shared_ptr<const void>> {
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(1));
        });
    bool batchHeld=false;
    std::size_t batchesStarted=0,batchesReleased=0;
    struct SaveScope final {
        SaveScope(bool* a,std::size_t* r):active(a),released(r){}
        SaveScope(const SaveScope&)=delete;
        SaveScope& operator=(const SaveScope&)=delete;
        bool* active;std::size_t* released;
        ~SaveScope(){*active=false;++*released;}
    };
    session.project.set_private_m18_project_save_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            if(batchHeld)
                return core::Result<std::shared_ptr<const void>>::failure(
                    core::Error{core::ErrorCode::InvalidState,
                                "M18_PROJECT_SAVE_ALREADY_BUSY"});
            batchHeld=true;++batchesStarted;
            std::shared_ptr<const void> lease=
                std::make_shared<SaveScope>(
                    &batchHeld,&batchesReleased);
            return core::Result<std::shared_ptr<const void>>::success(
                std::move(lease));
        });
    bool observedFinalPublicationInsideLease=false;
    QObject::connect(&session.project,&app::ProjectSessionViewModel::changed,
        &session.project,[&] {
            if(session.project.status_text()==
               QStringLiteral("Project saved and verified."))
                observedFinalPublicationInsideLease=batchHeld;
        });
    session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
    QVERIFY(observedFinalPublicationInsideLease);
    QCOMPARE(batchesStarted,std::size_t{1});
    QCOMPARE(batchesReleased,std::size_t{1});
    QVERIFY(!batchHeld);
    QVERIFY(QFileInfo::exists(proposed));
    QCOMPARE(session.project.error_message(),QString());
    // A collision after admission is a failed transaction, not a
    // permanently held Single-Writer epoch or permission to overwrite.
    QFile published{proposed};
    QVERIFY(published.open(QIODevice::ReadOnly));
    const auto originalBytes=published.readAll();
    published.close();
    session.project.saveProjectAs(QUrl::fromLocalFile(proposed));
    QCOMPARE(batchesStarted,std::size_t{2});
    QCOMPARE(batchesReleased,std::size_t{2});
    QVERIFY(!batchHeld);
    QVERIFY(!session.project.error_message().isEmpty());
    QVERIFY(published.open(QIODevice::ReadOnly));
    QCOMPARE(published.readAll(),originalBytes);
    published.close();
    session.project.set_private_m18_project_save_batch_begin({});
    session.project.set_private_m18_project_open_batch_begin({});
}

void ProjectSessionTest::m18SaveOnlyPolicyCannotOpenProjectWithoutEpoch()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto oldSource=wav(dir,QStringLiteral("old-open-source.wav"),19);
    const auto newSource=wav(dir,QStringLiteral("new-open-source.wav"),31);
    QVERIFY(!oldSource.isEmpty()&&!newSource.isEmpty());
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(newSource));
    QVERIFY(author.source.has_source());
    const auto projectPath=dir.filePath(QStringLiteral("new-project.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(projectPath));
    QCOMPARE(author.project.error_message(),QString());

    Session reopened;
    reopened.source.selectSource(QUrl::fromLocalFile(oldSource));
    QVERIFY(reopened.source.has_source());
    const std::string originalLocator{
        reopened.source.source_resource()->reference().locator()};
    std::size_t saveBatches=0,openBatches=0;
    reopened.project.set_private_m18_project_save_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++saveBatches;
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(1));
        });
    reopened.project.openProject(QUrl::fromLocalFile(projectPath));
    QVERIFY(!reopened.project.error_message().isEmpty());
    QCOMPARE(openBatches,std::size_t{0});
    QCOMPARE(saveBatches,std::size_t{0});
    QVERIFY(reopened.source.has_source());
    QVERIFY(std::string{reopened.source.source_resource()->reference().locator()}
        ==originalLocator);

    // A separately installed SaveAs status callback still cannot protect
    // destructive Open Project without its own exclusive admission.
    reopened.project.set_private_m18_project_save_batch_begin({});
    reopened.project.set_private_m18_project_save_admission(
        [](const char*)->core::Status {return core::Status::success();});
    reopened.project.openProject(QUrl::fromLocalFile(projectPath));
    QVERIFY(!reopened.project.error_message().isEmpty());
    QVERIFY(std::string{reopened.source.source_resource()->reference().locator()}
        ==originalLocator);

    reopened.project.set_private_m18_project_open_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++openBatches;
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(2));
        });
    reopened.project.openProject(QUrl::fromLocalFile(projectPath));
    QCOMPARE(reopened.project.error_message(),QString());
    QCOMPARE(openBatches,std::size_t{1});
    QVERIFY(reopened.source.has_source());
    QVERIFY(std::string{reopened.source.source_resource()->reference().locator()}
        !=originalLocator);
    reopened.project.set_private_m18_project_open_batch_begin({});
    reopened.project.set_private_m18_project_save_admission({});
}

void ProjectSessionTest::m18NestedProjectCommandsRefusedDuringOpenBatch()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto oldSource=wav(dir,QStringLiteral("nested-old.wav"),14);
    const auto newSource=wav(dir,QStringLiteral("nested-new.wav"),17);
    QVERIFY(!oldSource.isEmpty()&&!newSource.isEmpty());
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(newSource));
    const auto toOpen=dir.filePath(QStringLiteral("nested-project.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(toOpen));
    QVERIFY(QFileInfo::exists(toOpen));

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(oldSource));
    QVERIFY(session.source.has_source());
    const auto deniedSave=dir.filePath(QStringLiteral("nested-open-save.rgsml"));
    std::size_t openBatches=0,saveBatches=0;
    session.project.set_private_m18_project_save_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++saveBatches;
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(1));
        });
    session.project.set_private_m18_project_open_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++openBatches;
            // Bounded fault injection: absent single-writer protection
            // would recurse only once, then the assertions fail.
            if(openBatches==1) {
                // A callback must never destroy its own std::function
                // while the Product's Project-open lease is being issued.
                session.project.set_private_m18_project_open_batch_begin({});
                session.project.set_private_m18_project_save_batch_begin({});
                session.project.openProject(QUrl::fromLocalFile(toOpen));
                session.project.saveProjectAs(QUrl::fromLocalFile(deniedSave));
            }
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(2));
        });
    std::thread foreignOpenDisarm([&] {
        session.project.set_private_m18_project_open_batch_begin({});
        session.project.set_private_m18_project_save_batch_begin({});
    });
    foreignOpenDisarm.join();
    session.project.openProject(QUrl::fromLocalFile(toOpen));
    QCOMPARE(openBatches,std::size_t{1});
    QCOMPARE(saveBatches,std::size_t{0});
    QVERIFY(!QFileInfo::exists(deniedSave));
    QCOMPARE(session.project.error_message(),QString());
    QVERIFY(session.source.has_source());
    QVERIFY(std::string{session.source.source_resource()->reference().display_name()}
        ==std::string{"nested-new.wav"});

    // The same initial Project may be opened again after the scope is
    // released: outer epoch ownership must not become permanently BUSY.
    session.project.openProject(QUrl::fromLocalFile(toOpen));
    QCOMPARE(openBatches,std::size_t{2});
    QCOMPARE(saveBatches,std::size_t{0});
    QCOMPARE(session.project.error_message(),QString());
}

void ProjectSessionTest::m18NestedProjectCommandsRefusedDuringSaveBatch()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto source=wav(dir,QStringLiteral("nested-save-source.wav"),21);
    QVERIFY(!source.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    QVERIFY(session.source.has_source());
    const auto first=dir.filePath(QStringLiteral("nested-first.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(first));
    QVERIFY(QFileInfo::exists(first));

    const auto outer=dir.filePath(QStringLiteral("nested-outer.rgsml"));
    const auto inner=dir.filePath(QStringLiteral("nested-must-not-create.rgsml"));
    const auto second=dir.filePath(QStringLiteral("nested-after-return.rgsml"));
    std::size_t saveBatches=0,openBatches=0;
    session.project.set_private_m18_project_open_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++openBatches;
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(1));
        });
    session.project.set_private_m18_project_save_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++saveBatches;
            // A nested Product callback may not uninstall the SaveAs
            // lease provider or escape the active Project epoch.
            session.project.set_private_m18_project_save_batch_begin({});
            session.project.set_private_m18_project_open_batch_begin({});
            // Simulate a synchronous Qt callback trying to start another
            // Project mutation while the first SaveAs lease is being issued.
            session.project.saveProjectAs(QUrl::fromLocalFile(inner));
            session.project.openProject(QUrl::fromLocalFile(first));
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(2));
        });
    std::thread foreignSaveDisarm([&] {
        session.project.set_private_m18_project_save_batch_begin({});
        session.project.set_private_m18_project_open_batch_begin({});
    });
    foreignSaveDisarm.join();
    session.project.saveProjectAs(QUrl::fromLocalFile(outer));
    QCOMPARE(saveBatches,std::size_t{1});
    QCOMPARE(openBatches,std::size_t{0});
    QVERIFY(QFileInfo::exists(outer));
    QVERIFY(!QFileInfo::exists(inner));
    QCOMPARE(session.project.error_message(),QString());

    // The owner sentinel is RAII: no stale BUSY remains after returning.
    session.project.saveProjectAs(QUrl::fromLocalFile(second));
    QCOMPARE(saveBatches,std::size_t{2});
    QCOMPARE(openBatches,std::size_t{0});
    QVERIFY(QFileInfo::exists(second));
    QVERIFY(!QFileInfo::exists(inner));
}

void ProjectSessionTest::m18SaveBatchMissingOrThrowingFailsClosed()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto source=wav(dir,QStringLiteral("m18-save-fail-source.wav"),11);
    QVERIFY(!source.isEmpty());
    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    QVERIFY(session.source.has_source());
    const auto initial=dir.filePath(QStringLiteral("m18-save-initial.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(initial));
    QCOMPARE(session.project.error_message(),QString());
    const auto priorName=session.project.project_display_name();

    session.project.set_private_m18_project_open_batch_begin(
        []()->core::Result<std::shared_ptr<const void>> {
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(1));
        });
    const auto destination=dir.filePath(QStringLiteral("m18-save-attempt.rgsml"));
    std::size_t calls=0;
    session.project.set_private_m18_project_save_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++calls;
            return core::Result<std::shared_ptr<const void>>::success(
                std::shared_ptr<const void>{});
        });
    session.project.saveProjectAs(QUrl::fromLocalFile(destination));
    QCOMPARE(calls,std::size_t{1});
    QVERIFY(!QFileInfo::exists(destination));
    QCOMPARE(session.project.project_display_name(),priorName);

    session.project.set_private_m18_project_save_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++calls;
            throw std::runtime_error("M18_SAVE_BATCH_FAULT");
        });
    session.project.saveProjectAs(QUrl::fromLocalFile(destination));
    QCOMPARE(calls,std::size_t{2});
    QVERIFY(!QFileInfo::exists(destination));
    QCOMPARE(session.project.project_display_name(),priorName);

    // Two independently granting callbacks are not one coherent epoch.
    session.project.set_private_m18_project_save_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++calls;
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<int>(2));
        });
    session.project.set_private_m18_project_save_admission(
        [](const char*)->core::Status {return core::Status::success();});
    session.project.saveProjectAs(QUrl::fromLocalFile(destination));
    QCOMPARE(calls,std::size_t{2});
    QVERIFY(!QFileInfo::exists(destination));
    QCOMPARE(session.project.project_display_name(),priorName);

    session.project.set_private_m18_project_save_admission({});
    session.project.saveProjectAs(QUrl::fromLocalFile(destination));
    QCOMPARE(calls,std::size_t{3});
    QCOMPARE(session.project.error_message(),QString());
    QVERIFY(QFileInfo::exists(destination));
    session.project.set_private_m18_project_save_batch_begin({});
    session.project.set_private_m18_project_open_batch_begin({});
}

void ProjectSessionTest::
m18ProjectMutationDeniedBeforeOpenOrSavePublication()
{
    // Opt-in PREPRODUCTION Product epoch BUSY: denied Project open/save
    // must not replace a SOURCE, rate, chain, or publish a new project.
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=wav(dir,QStringLiteral("current.wav"),4,48000U);
    QVERIFY(!src.isEmpty());
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(src));
    QVERIFY(author.source.has_source());
    const auto initial=dir.filePath(QStringLiteral("good.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(initial));
    QCOMPARE(author.project.error_message(),QString());
    QVERIFY(QFileInfo::exists(initial));
    std::size_t blockedWrites=0;
    author.project.set_private_m18_project_mutation_admission(
        [&](const char*)->core::Status {
            ++blockedWrites;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_PROJECT_COMMIT_BUSY"});
        });
    const auto deniedOutput=dir.filePath(QStringLiteral("blocked.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(deniedOutput));
    QVERIFY(!author.project.error_message().isEmpty());
    QVERIFY(!QFileInfo::exists(deniedOutput));
    QCOMPARE(blockedWrites,std::size_t{1});
    author.project.set_private_m18_project_mutation_admission({});

    Session reopened;
    std::size_t blockedOpens=0;
    std::size_t attemptedSourceAdoptions=0;
    // Shared busy policy: project must stop BEFORE entering any Source
    // adoption callback. No Qt event-loop pumping or half-open session.
    reopened.source.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++attemptedSourceAdoptions;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_SOURCE_COMMIT_BUSY"});
        });
    reopened.project.set_private_m18_project_mutation_admission(
        [&](const char*)->core::Status {
            ++blockedOpens;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_PROJECT_COMMIT_BUSY"});
        });
    reopened.project.openProject(QUrl::fromLocalFile(initial));
    QCOMPARE(blockedOpens,std::size_t{1});
    QCOMPARE(attemptedSourceAdoptions,std::size_t{0});
    QVERIFY(!reopened.project.error_message().isEmpty());
    QVERIFY(!reopened.source.has_source());
    reopened.project.set_private_m18_project_mutation_admission({});
    reopened.source.set_private_m18_source_mutation_admission({});
    reopened.project.openProject(QUrl::fromLocalFile(initial));
    QCOMPARE(reopened.project.error_message(),QString());
    QVERIFY(reopened.source.has_source());
    QCOMPARE(reopened.source.sample_rate_hz(),qint64{48000});
}

void ProjectSessionTest::
m18ProjectOpenBatchKeepsWorkerLockAcrossNestedSourceCommit()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=wav(dir,QStringLiteral("batch-source.wav"),4,48000U);
    const auto goldPath=wav(dir,QStringLiteral("batch-gold.wav"),7,48000U);
    QVERIFY(!src.isEmpty()&&!goldPath.isEmpty());
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(src));
    QVERIFY(author.source.has_source());
    author.gold.selectGold(QUrl::fromLocalFile(goldPath));
    QVERIFY(author.gold.has_gold());
    app::MasterOutputRateState selectedMaster;
    selectedMaster.source_committed(author.source.sample_rate_hz());
    QVERIFY(selectedMaster.choose_master_rate(192000));
    author.project.set_master_output_rate_state(&selectedMaster);
    const auto projectPath=dir.filePath(QStringLiteral("batch.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(projectPath));
    QVERIFY(QFileInfo::exists(projectPath));

    // One exclusive Product mutation batch surrounds every synchronous
    // Qt controller update in Project open, including Source adoption.
    struct Scope final {
        Scope(bool* active,std::size_t* released)
            :active_(active),released_(released) {}
        ~Scope(){*active_=false;++*released_;}
        bool* active_;
        std::size_t* released_;
    };
    Session reopened;
    app::MasterOutputRateState reopenedMaster;
    reopened.project.set_master_output_rate_state(&reopenedMaster);
    bool mutationActive=false;
    std::size_t batchesStarted=0,batchesReleased=0,sourceAdoptions=0;
    std::size_t goldAdoptions=0,rateAdoptions=0;
    reopened.project.set_private_m18_project_open_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++batchesStarted;
            mutationActive=true;
            std::shared_ptr<const void> token=
                std::make_shared<Scope>(&mutationActive,&batchesReleased);
            return core::Result<std::shared_ptr<const void>>::success(
                std::move(token));
        });
    reopened.source.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++sourceAdoptions;
            if(mutationActive)return core::Status::success();
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_SOURCE_MUTATION_WITHOUT_PROJECT_BATCH"});
        });
    reopened.gold.set_private_m18_gold_mutation_admission(
        [&](const char*)->core::Status {
            ++goldAdoptions;
            return mutationActive?core::Status::success()
                :core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_GOLD_MUTATION_OUTSIDE_PROJECT_BATCH"});
        });
    reopenedMaster.set_private_m18_rate_mutation_admission([&] {
        ++rateAdoptions;
        return mutationActive;
    });
    reopened.project.openProject(QUrl::fromLocalFile(projectPath));
    QCOMPARE(reopened.project.error_message(),QString());
    QVERIFY(reopened.source.has_source());
    QVERIFY(reopened.gold.has_gold());
    QCOMPARE(reopenedMaster.master_rate_hz(),qint64{192000});
    QCOMPARE(goldAdoptions,std::size_t{1});
    // M18 now explicitly admits BOTH nested SOURCE-rate adoption and
    // persisted MASTER target selection through the same held Project
    // lease. Previously the void SOURCE-rate reset bypassed the epoch
    // and this test counted only the final target edit.
    QCOMPARE(rateAdoptions,std::size_t{2});
    QCOMPARE(batchesStarted,std::size_t{1});
    QCOMPARE(sourceAdoptions,std::size_t{1});
    QCOMPARE(batchesReleased,std::size_t{1});
    QVERIFY(!mutationActive); // Product batch closed at function exit.

    // A denied batch must fail BEFORE transport stop and Source adoption.
    Session denied;
    std::size_t deniedSourceAdoptions=0;
    denied.project.set_private_m18_project_open_batch_begin(
        []()->core::Result<std::shared_ptr<const void>> {
            return core::Result<std::shared_ptr<const void>>::failure(
                core::Error{core::ErrorCode::InvalidState,
                            "M18_PRODUCT_PROJECT_COMMIT_ALREADY_BUSY"});
        });
    denied.source.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++deniedSourceAdoptions;
            return core::Status::success();
        });
    denied.project.openProject(QUrl::fromLocalFile(projectPath));
    QVERIFY(!denied.project.error_message().isEmpty());
    QVERIFY(!denied.source.has_source());
    QCOMPARE(deniedSourceAdoptions,std::size_t{0});

    // An inner Source admission can fail *after* the outer Product batch
    // was acquired. Its RAII ownership must still be released on this
    // early return, so a subsequent legal Project open cannot inherit a
    // permanently busy Product epoch.
    Session failedInner;
    bool innerBatchActive=false;
    std::size_t innerBatches=0,innerReleases=0,innerAttempts=0;
    failedInner.project.set_private_m18_project_open_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            if(innerBatchActive)
                return core::Result<std::shared_ptr<const void>>::failure(
                    core::Error{core::ErrorCode::InvalidState,
                                "M18_BATCH_ALREADY_ACTIVE_AFTER_ERROR"});
            ++innerBatches;
            innerBatchActive=true;
            std::shared_ptr<const void> owner=
                std::make_shared<Scope>(&innerBatchActive,&innerReleases);
            return core::Result<std::shared_ptr<const void>>::success(
                std::move(owner));
        });
    failedInner.source.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++innerAttempts;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_NESTED_SOURCE_ADMISSION_REJECTED"});
        });
    failedInner.project.openProject(QUrl::fromLocalFile(projectPath));
    QVERIFY(!failedInner.project.error_message().isEmpty());
    QVERIFY(!failedInner.source.has_source());
    QCOMPARE(innerBatches,std::size_t{1});
    QCOMPARE(innerReleases,std::size_t{1});
    QCOMPARE(innerAttempts,std::size_t{1});
    QVERIFY(!innerBatchActive);
    failedInner.source.set_private_m18_source_mutation_admission({});
    failedInner.project.openProject(QUrl::fromLocalFile(projectPath));
    QVERIFY(failedInner.source.has_source());
    QCOMPARE(innerBatches,std::size_t{2});
    QCOMPARE(innerReleases,std::size_t{2});
    QVERIFY(!innerBatchActive);
}

void ProjectSessionTest::master_rate_save_open_and_invalid_atomic()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto sourcePath=wav(dir,QStringLiteral("rate-source.wav"),0,44'100U);
    QVERIFY(!sourcePath.isEmpty());
    Session author;
    author.source.selectSource(QUrl::fromLocalFile(sourcePath));
    app::MasterOutputRateState selected;
    // This case specifically verifies SOURCE 44.1 -> MASTER 192, not
    // the default 48 kHz WAV fixture used by the other Project tests.
    QCOMPARE(author.source.sample_rate_hz(),qint64{44100});
    selected.source_committed(author.source.sample_rate_hz());
    author.project.set_master_output_rate_state(&selected);
    QCOMPARE(selected.master_rate_hz(),qint64{44100});
    QVERIFY(selected.choose_master_rate(192000));
    const auto path=dir.filePath(QStringLiteral("master192.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(path));
    QCOMPARE(author.project.error_message(),QString());

    auto saved=load_project(path);
    QVERIFY(saved);
    auto decoded=project::MasterOutputRatePreference::read(
        saved.value()->document().output);
    QVERIFY(decoded);
    QVERIFY(decoded.value()->has_value());
    QCOMPARE(decoded.value()->value(),std::int64_t{192000});

    Session reopened;
    app::MasterOutputRateState restored;
    reopened.project.set_master_output_rate_state(&restored);
    reopened.project.openProject(QUrl::fromLocalFile(path));
    QCOMPARE(reopened.project.error_message(),QString());
    QVERIFY(!reopened.project.degraded()); // recognized known output schema
    QCOMPARE(restored.source_rate_hz(),qint64{44100});
    QCOMPARE(restored.master_rate_hz(),qint64{192000});

    // M18 preproduction: Project open must fail closed if an outer worker
    // commit denies its nested MASTER mutation. Never Q_ASSERT/crash in
    // debug or continue saving metadata with the wrong 44.1k target.
    Session deniedRate;
    app::MasterOutputRateState deniedMaster;
    deniedRate.project.set_master_output_rate_state(&deniedMaster);
    std::size_t deniedRateEdits=0;
    deniedMaster.set_private_m18_rate_mutation_admission([&] {
        ++deniedRateEdits;
        return false;
    });
    deniedRate.project.openProject(QUrl::fromLocalFile(path));
    QVERIFY(!deniedRate.project.error_message().isEmpty());
    QCOMPARE(deniedRateEdits,std::size_t{1});
    // The first denied M18 gate now applies to SOURCE-rate restoration
    // itself, before any MASTER rate is published. The former expectation
    // (44.1k/44.1k) relied on an unsafe legacy void-rate bypass.
    QCOMPARE(deniedMaster.source_rate_hz(),qint64{0});
    QCOMPARE(deniedMaster.master_rate_hz(),qint64{0});
    QCOMPARE(deniedMaster.rate_revision(),std::uint64_t{0});
    // SOURCE adoption may already have occurred under this deliberately
    // incomplete per-controller test setup; the real Product requires a
    // Project-wide exclusive mutation batch and revoked export consent.
    QVERIFY(deniedRate.source.has_source());
    deniedMaster.set_private_m18_rate_mutation_admission({});
    deniedRate.project.openProject(QUrl::fromLocalFile(path));
    QCOMPARE(deniedRate.project.error_message(),QString());
    QCOMPARE(deniedMaster.master_rate_hz(),qint64{192000});

    const auto second=dir.filePath(QStringLiteral("master192-again.rgsml"));
    reopened.project.saveProjectAs(QUrl::fromLocalFile(second));
    QCOMPARE(reopened.project.error_message(),QString());
    auto again=load_project(second);
    QVERIFY(again);
    auto recovered=project::MasterOutputRatePreference::read(
        again.value()->document().output);
    QVERIFY(recovered);
    QCOMPARE(recovered.value()->value(),std::int64_t{192000});

    // Known but malformed output: fail before replacing live SOURCE or
    // even touching the rate authority revision.
    auto malformedDoc=saved.value()->document();
    auto badValue=project::OpaqueJsonValue::parse(
        R"({"targetMasterOutputRateHz":"192000"})");
    QVERIFY(badValue);
    malformedDoc.output=*badValue.value();
    auto malformed=project::ProjectSnapshot::create(malformedDoc);
    QVERIFY(malformed);
    const auto invalidPath=dir.filePath(QStringLiteral("invalid-rate.rgsml"));
    QVERIFY(write_project(invalidPath,*malformed.value()));
    const auto oldSource=reopened.source.source_resource()->reference().locator();
    const auto oldRevision=restored.rate_revision();
    reopened.project.openProject(QUrl::fromLocalFile(invalidPath));
    QVERIFY(!reopened.project.error_message().isEmpty());
    QCOMPARE(restored.rate_revision(),oldRevision);
    QCOMPARE(restored.master_rate_hz(),qint64{192000});
    QCOMPARE(reopened.source.source_resource()->reference().locator(),oldSource);
}

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

void ProjectSessionTest::externally_opened_invalid_processing_rejected_atomically()
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

    // B4.5 contract: an active mastering chain with incompatible topology is
    // rejected, not preserved as inactive/degraded processing.
    const auto externalChainId = *core::Uuid::parse(
        "90000000-0000-4000-8000-000000000001").value();
    doc.pipeline.masteringChainId = externalChainId;
    project::Chain extChain;
    extChain.chainId = externalChainId;
    extChain.stage = "MASTER";
    extChain.segment = "MANUAL";
    doc.chains.clear();
    doc.chains.push_back(extChain); // invalid supported-B4 topology: zero modules

    auto future = project::OpaqueJsonValue::parse(
        "{\"future\":{\"enabled\":true}}");
    QVERIFY(future);
    doc.extensions = *future.value();

    auto externalProject = project::ProjectSnapshot::create(doc);
    QVERIFY(externalProject);
    const auto externalPath = dir.filePath(QStringLiteral("external.rgsml"));
    QVERIFY(write_project(externalPath, *externalProject.value()));

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(live));
    auto liveGain = dsp::GainParameters::create(-2.0);
    QVERIFY(liveGain);
    QVERIFY(session.masteringChainState->set_gain_parameters(*liveGain.value()));

    const auto priorSource =
        session.source.source_resource()->reference().locator();
    const auto priorChainId = session.masteringChainState->chain_id();

    session.project.openProject(QUrl::fromLocalFile(externalPath));

    QVERIFY(!session.project.error_message().isEmpty());
    QCOMPARE(session.source.source_resource()->reference().locator(), priorSource);
    QCOMPARE(session.masteringChainState->chain_id(), priorChainId);
    QCOMPARE(session.masteringChainState->gain_parameters().gain_db(), -2.0);
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
    QCOMPARE(chain.modules.size(), std::size_t{3});

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

    // Verify Module 2: Compressor
    auto compInstance = session.masteringChainState->compressor_instance();
    QVERIFY(compInstance);
    const auto& compAuthority = compInstance.value()->get();
    const auto& m2 = chain.modules[2];
    QCOMPARE(m2.instanceId, session.compId.uuid());
    QCOMPARE(m2.typeId, std::string("rgsml.dsp.compressor"));
    QCOMPARE(m2.enabled, compAuthority.enabled());
    QCOMPARE(m2.userBypass, compAuthority.user_bypass());
    QCOMPARE(m2.controllerSuspended, compAuthority.controller_suspended());
    QCOMPARE(m2.domainSuspended, compAuthority.domain_suspended());
    QVERIFY(m2.userBypass); // Compressor initially user-bypassed
    QCOMPARE(m2.provenance, std::string("MANUAL"));
    QCOMPARE(m2.owner, std::string("USER"));
    QCOMPARE(m2.linkState, std::string("UNLINKED"));
    QVERIFY(m2.semanticNodeId == compAuthority.semantic_node_id());
    QVERIFY(m2.algorithmVersion.has_value());
    QCOMPARE(*m2.algorithmVersion, std::string("1.0.0"));
    QVERIFY(m2.parameterSchemaId.has_value());
    QCOMPARE(*m2.parameterSchemaId, std::string("rgsml.dsp.compressor.parameters/1.0.0"));

    auto decodedComp = dsp::decode_compressor_parameters_json(m2.parameters.canonical_utf8());
    QVERIFY(decodedComp);
    QVERIFY(*decodedComp.value() == *dsp::CompressorParameters::create_default().value());
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
    const auto gold = wav(dir, QStringLiteral("gold.wav"), 1);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    session.gold.selectGold(QUrl::fromLocalFile(gold));
    QVERIFY(session.gold.has_gold());
    auto region = core::FrameRange::create(core::FrameIndex{7}, core::FrameIndex{19});
    QVERIFY(region);
    QVERIFY(session.region.set_region(*region.value()));

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

    const auto savedPath = dir.filePath(QStringLiteral("supported_b4_saved.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(savedPath));
    QCOMPARE(session.project.error_message(), QString());

    auto saved = load_project(savedPath);
    QVERIFY(saved);
    auto doc = saved.value()->document();
    QVERIFY(doc.pipeline.masteringChainId.has_value());
    QCOMPARE(doc.chains.size(), std::size_t{1});
    QCOMPARE(doc.chains[0].modules.size(), std::size_t{3});

    constexpr std::uint64_t persistedRevision = 42U;
    doc.chains[0].revision = persistedRevision;
    doc.chains[0].modules[0].enabled = false;
    doc.chains[0].modules[0].controllerSuspended = true;
    doc.chains[0].modules[1].domainSuspended = true;

    auto explicitSnapshot = project::ProjectSnapshot::create(doc);
    QVERIFY(explicitSnapshot);
    const auto path = dir.filePath(QStringLiteral("supported_b4_explicit.rgsml"));
    QVERIFY(write_project(path, *explicitSnapshot.value()));

    Session target;
    target.project.openProject(QUrl::fromLocalFile(path));
    QCOMPARE(target.project.error_message(), QString());
    QVERIFY(!target.project.degraded());

    QCOMPARE(target.source.source_resource()->reference().locator(),
             session.source.source_resource()->reference().locator());
    QVERIFY(target.gold.has_gold());
    QVERIFY(target.region.region().has_value());
    QCOMPARE(target.region.region()->begin().value(), std::int64_t{7});
    QCOMPARE(target.region.region()->end().value(), std::int64_t{19});

    QCOMPARE(target.masteringChainState->chain_id(), doc.chains[0].chainId);
    QCOMPARE(target.masteringChainState->chain().revision(), persistedRevision);
    QCOMPARE(target.masteringChainState->module_count(), std::size_t{3});
    QCOMPARE(target.masteringChainState->gain_instance_id().uuid(),
             doc.chains[0].modules[0].instanceId);
    QCOMPARE(target.masteringChainState->eq_instance_id().uuid(),
             doc.chains[0].modules[1].instanceId);
    QCOMPARE(target.masteringChainState->compressor_instance_id().uuid(),
             doc.chains[0].modules[2].instanceId);

    const auto instances = target.masteringChainState->instances();
    QCOMPARE(instances[0].module_type_id(), std::string_view{"rgsml.dsp.gain"});
    QCOMPARE(instances[1].module_type_id(), std::string_view{"rgsml.dsp.parametric-eq"});
    QCOMPARE(instances[2].module_type_id(), std::string_view{"rgsml.dsp.compressor"});
    QCOMPARE(instances[0].enabled(), doc.chains[0].modules[0].enabled);
    QCOMPARE(instances[0].user_bypass(), doc.chains[0].modules[0].userBypass);
    QCOMPARE(instances[0].controller_suspended(), doc.chains[0].modules[0].controllerSuspended);
    QCOMPARE(instances[0].domain_suspended(), doc.chains[0].modules[0].domainSuspended);
    QCOMPARE(instances[1].enabled(), doc.chains[0].modules[1].enabled);
    QCOMPARE(instances[1].user_bypass(), doc.chains[0].modules[1].userBypass);
    QCOMPARE(instances[1].controller_suspended(), doc.chains[0].modules[1].controllerSuspended);
    QCOMPARE(instances[1].domain_suspended(), doc.chains[0].modules[1].domainSuspended);
    QCOMPARE(instances[2].enabled(), doc.chains[0].modules[2].enabled);
    QCOMPARE(instances[2].user_bypass(), doc.chains[0].modules[2].userBypass);
    QCOMPARE(instances[2].controller_suspended(), doc.chains[0].modules[2].controllerSuspended);
    QCOMPARE(instances[2].domain_suspended(), doc.chains[0].modules[2].domainSuspended);
    QCOMPARE(instances[0].provenance(), dsp::ModuleProvenance::MANUAL);
    QCOMPARE(instances[0].owner(), dsp::ModuleOwner::USER);
    QCOMPARE(instances[0].link_state(), dsp::ModuleLinkState::UNLINKED);
    QVERIFY(!instances[0].semantic_node_id().has_value());
    QCOMPARE(instances[1].provenance(), dsp::ModuleProvenance::MANUAL);
    QCOMPARE(instances[1].owner(), dsp::ModuleOwner::USER);
    QCOMPARE(instances[1].link_state(), dsp::ModuleLinkState::UNLINKED);
    QVERIFY(!instances[1].semantic_node_id().has_value());
    QCOMPARE(instances[2].provenance(), dsp::ModuleProvenance::MANUAL);
    QCOMPARE(instances[2].owner(), dsp::ModuleOwner::USER);
    QCOMPARE(instances[2].link_state(), dsp::ModuleLinkState::UNLINKED);
    QVERIFY(!instances[2].semantic_node_id().has_value());

    auto gainDescriptor = target.masteringChainState->find_descriptor("rgsml.dsp.gain");
    auto eqDescriptor = target.masteringChainState->find_descriptor("rgsml.dsp.parametric-eq");
    auto compDescriptor = target.masteringChainState->find_descriptor("rgsml.dsp.compressor");
    QVERIFY(gainDescriptor && eqDescriptor && compDescriptor);
    QVERIFY(gainDescriptor.value()->get().algorithm_version().has_value());
    QVERIFY(gainDescriptor.value()->get().parameter_schema_id().has_value());
    QVERIFY(eqDescriptor.value()->get().algorithm_version().has_value());
    QVERIFY(eqDescriptor.value()->get().parameter_schema_id().has_value());
    QVERIFY(compDescriptor.value()->get().algorithm_version().has_value());
    QVERIFY(compDescriptor.value()->get().parameter_schema_id().has_value());
    QCOMPARE(*gainDescriptor.value()->get().algorithm_version(), std::string("1.0.0"));
    QCOMPARE(*gainDescriptor.value()->get().parameter_schema_id(),
             std::string("rgsml.dsp.gain.parameters/1.0.0"));
    QCOMPARE(*eqDescriptor.value()->get().algorithm_version(), std::string("1.0.0"));
    QCOMPARE(*eqDescriptor.value()->get().parameter_schema_id(),
             std::string("rgsml.dsp.parametric-eq.parameters/1.0.0"));
    QCOMPARE(*compDescriptor.value()->get().algorithm_version(), std::string("1.0.0"));
    QCOMPARE(*compDescriptor.value()->get().parameter_schema_id(),
             std::string("rgsml.dsp.compressor.parameters/1.0.0"));

    QCOMPARE(target.masteringChainState->gain_parameters(), *gainParams.value());
    QCOMPARE(target.masteringChainState->parametric_eq_parameters(), *eqParams.value());
    auto persistedComp = dsp::decode_compressor_parameters_json(
        doc.chains[0].modules[2].parameters.canonical_utf8());
    QVERIFY(persistedComp);
    QVERIFY(target.masteringChainState->compressor_parameters() == *persistedComp.value());

    QCOMPARE(target.gainViewModel->gain_db(), 4.5);
    QVERIFY(target.gainViewModel->bypass());
    QCOMPARE(target.eqViewModel->band_count(), 2);
    QCOMPARE(target.eqViewModel->selected_band_id(),
             QString::fromStdString(bandId1.value()->to_string()));
    QVERIFY(!target.eqViewModel->bypass());
    QCOMPARE(target.masteringChainState->parametric_eq_parameters(), *eqParams.value());
}

void ProjectSessionTest::open_save_as_identity_revision_preservation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source));
    auto gain = dsp::GainParameters::create(-4.25);
    QVERIFY(gain);
    QVERIFY(session.masteringChainState->set_gain_parameters(*gain.value()));
    auto bandId = core::Uuid::parse("21000000-0000-0000-0000-000000000001");
    QVERIFY(bandId);
    auto band = dsp::EqBandParameters::create(*bandId.value(), true, dsp::EqFilterType::BELL,
        dsp::EqRouting::MID, dsp::BellPayload{1250.0, 2.25, 1.1});
    QVERIFY(band);
    auto eq = dsp::ParametricEqParameters::create({*band.value()});
    QVERIFY(eq);
    QVERIFY(session.masteringChainState->set_parametric_eq_parameters(*eq.value()));

    const auto authoredPath = dir.filePath(QStringLiteral("authored.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(authoredPath));
    QCOMPARE(session.project.error_message(), QString());

    auto authored = load_project(authoredPath);
    QVERIFY(authored);
    auto doc = authored.value()->document();
    constexpr std::uint64_t persistedRevision = 77U;
    doc.chains[0].revision = persistedRevision;
    doc.chains[0].modules[0].enabled = false;
    doc.chains[0].modules[0].controllerSuspended = true;
    doc.chains[0].modules[1].domainSuspended = true;

    auto explicitSnapshot = project::ProjectSnapshot::create(doc);
    QVERIFY(explicitSnapshot);
    const auto inputPath = dir.filePath(QStringLiteral("input.rgsml"));
    QVERIFY(write_project(inputPath, *explicitSnapshot.value()));

    Session target;
    target.project.openProject(QUrl::fromLocalFile(inputPath));
    QCOMPARE(target.project.error_message(), QString());

    const auto outputPath = dir.filePath(QStringLiteral("output.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(outputPath));
    QCOMPARE(target.project.error_message(), QString());

    auto output = load_project(outputPath);
    QVERIFY(output);
    const auto& doc2 = output.value()->document();

    QVERIFY(doc2.pipeline.masteringChainId == doc.pipeline.masteringChainId);
    QCOMPARE(doc2.chains.size(), doc.chains.size());
    const auto& c1 = doc.chains.front();
    const auto& c2 = doc2.chains.front();
    QCOMPARE(c2.chainId, c1.chainId);
    QCOMPARE(c2.stage, c1.stage);
    QCOMPARE(c2.segment, c1.segment);
    QCOMPARE(c2.revision, persistedRevision);
    QCOMPARE(c2.modules.size(), c1.modules.size());

    for (std::size_t i = 0; i < c1.modules.size(); ++i) {
        const auto& m1 = c1.modules[i];
        const auto& m2 = c2.modules[i];
        QCOMPARE(m2.instanceId, m1.instanceId);
        QCOMPARE(m2.typeId, m1.typeId);
        QCOMPARE(m2.enabled, m1.enabled);
        QCOMPARE(m2.userBypass, m1.userBypass);
        QCOMPARE(m2.controllerSuspended, m1.controllerSuspended);
        QCOMPARE(m2.domainSuspended, m1.domainSuspended);
        QCOMPARE(m2.provenance, m1.provenance);
        QCOMPARE(m2.owner, m1.owner);
        QCOMPARE(m2.linkState, m1.linkState);
        QVERIFY(m2.semanticNodeId == m1.semanticNodeId);
        QVERIFY(m2.algorithmVersion == m1.algorithmVersion);
        QVERIFY(m2.parameterSchemaId == m1.parameterSchemaId);
        QCOMPARE(m2.parameters.canonical_utf8(), m1.parameters.canonical_utf8());
    }

    auto g1 = dsp::decode_gain_parameters_json(c1.modules[0].parameters.canonical_utf8());
    auto g2 = dsp::decode_gain_parameters_json(c2.modules[0].parameters.canonical_utf8());
    auto e1 = dsp::decode_parametric_eq_parameters_json(c1.modules[1].parameters.canonical_utf8());
    auto e2 = dsp::decode_parametric_eq_parameters_json(c2.modules[1].parameters.canonical_utf8());
    QVERIFY(g1 && g2 && e1 && e2);
    QCOMPARE(*g2.value(), *g1.value());
    QCOMPARE(*e2.value(), *e1.value());
}

void ProjectSessionTest::atomic_rejection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto candidateSource = wav(dir, QStringLiteral("candidate.wav"), 0);
    const auto liveSource = wav(dir, QStringLiteral("live.wav"), 1);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(candidateSource));
    const auto validPath = dir.filePath(QStringLiteral("valid.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(validPath));
    QCOMPARE(session.project.error_message(), QString());

    auto baseSnap = load_project(validPath);
    QVERIFY(baseSnap);

    session.source.selectSource(QUrl::fromLocalFile(liveSource));
    auto liveGain = dsp::GainParameters::create(-3.0);
    QVERIFY(liveGain);
    QVERIFY(session.masteringChainState->set_gain_parameters(*liveGain.value()));
    auto liveBandId = core::Uuid::parse("22000000-0000-0000-0000-000000000001");
    QVERIFY(liveBandId);
    auto liveBand = dsp::EqBandParameters::create(*liveBandId.value(), true,
        dsp::EqFilterType::BELL, dsp::EqRouting::STEREO,
        dsp::BellPayload{900.0, -1.5, 1.3});
    QVERIFY(liveBand);
    auto liveEq = dsp::ParametricEqParameters::create({*liveBand.value()});
    QVERIFY(liveEq);
    QVERIFY(session.masteringChainState->set_parametric_eq_parameters(*liveEq.value()));

    const auto priorSource = session.source.source_resource()->reference().locator();
    const auto priorChainId = session.masteringChainState->chain_id();

    const auto verify_live_session_unchanged = [&] {
        QCOMPARE(session.source.source_resource()->reference().locator(), priorSource);
        QCOMPARE(session.masteringChainState->chain_id(), priorChainId);
        QCOMPARE(session.masteringChainState->gain_parameters(), *liveGain.value());
        QCOMPARE(session.masteringChainState->parametric_eq_parameters(), *liveEq.value());
        QCOMPARE(session.gainViewModel->gain_db(), -3.0);
        QVERIFY(!session.gold.has_gold());
        QVERIFY(!session.region.region().has_value());
    };

    const auto reject_document = [&](project::ProjectDocument doc, const QString& name) {
        auto badSnap = project::ProjectSnapshot::create(std::move(doc));
        QVERIFY(badSnap);
        const auto badPath = dir.filePath(name);
        QVERIFY(write_project(badPath, *badSnap.value()));
        session.project.openProject(QUrl::fromLocalFile(badPath));
        QVERIFY(!session.project.error_message().isEmpty());
        verify_live_session_unchanged();
    };

    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].parameters =
            *project::OpaqueJsonValue::parse("{\"invalid\":true}").value();
        reject_document(std::move(doc), QStringLiteral("bad_gain_json.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[1].parameters =
            *project::OpaqueJsonValue::parse("{\"invalid\":true}").value();
        reject_document(std::move(doc), QStringLiteral("bad_eq_json.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].algorithmVersion = "2.0.0";
        reject_document(std::move(doc), QStringLiteral("bad_version.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[1].parameterSchemaId =
            "rgsml.dsp.parametric-eq.parameters/2.0.0";
        reject_document(std::move(doc), QStringLiteral("bad_schema.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        std::swap(doc.chains[0].modules[0], doc.chains[0].modules[1]);
        reject_document(std::move(doc), QStringLiteral("bad_order.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[1].typeId = "rgsml.dsp.future-eq";
        reject_document(std::move(doc), QStringLiteral("unknown_active_module.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].provenance = "AUTOMATED";
        reject_document(std::move(doc), QStringLiteral("bad_provenance.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].owner = "SYSTEM";
        reject_document(std::move(doc), QStringLiteral("bad_owner.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        doc.chains[0].modules[0].linkState = "LINKED";
        reject_document(std::move(doc), QStringLiteral("bad_link.rgsml"));
    }
    {
        auto doc = baseSnap.value()->document();
        auto semantic = core::Uuid::parse("23000000-0000-0000-0000-000000000001");
        QVERIFY(semantic);
        doc.chains[0].modules[0].semanticNodeId = *semantic.value();
        reject_document(std::move(doc), QStringLiteral("bad_semantic.rgsml"));
    }

    // ProjectSnapshot enforces referential integrity before a malformed package
    // can be produced through the supported writer. This is the authoritative
    // pre-open rejection for a missing referenced mastering chain.
    {
        auto doc = baseSnap.value()->document();
        auto missing = core::Uuid::parse("24000000-0000-0000-0000-000000000001");
        QVERIFY(missing);
        doc.pipeline.masteringChainId = *missing.value();
        auto impossible = project::ProjectSnapshot::create(std::move(doc));
        QVERIFY(!impossible);
        verify_live_session_unchanged();
    }
}

void ProjectSessionTest::legacy_pre_b4_materialization_and_save_as()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    Session author;
    author.source.selectSource(QUrl::fromLocalFile(source));
    const auto tempPath = dir.filePath(QStringLiteral("temp.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(tempPath));
    QCOMPARE(author.project.error_message(), QString());

    auto snap = load_project(tempPath);
    QVERIFY(snap);
    auto doc = snap.value()->document();
    doc.pipeline.masteringChainId.reset();
    doc.chains.clear();

    auto legacySnap = project::ProjectSnapshot::create(doc);
    QVERIFY(legacySnap);
    const auto legacyPath = dir.filePath(QStringLiteral("legacy.rgsml"));
    QVERIFY(write_project(legacyPath, *legacySnap.value()));

    const std::array<core::Uuid, 4> generated{
        *core::Uuid::parse("25000000-0000-4000-8000-000000000001").value(),
        *core::Uuid::parse("25000000-0000-4000-8000-000000000002").value(),
        *core::Uuid::parse("25000000-0000-4000-8000-000000000003").value(),
        *core::Uuid::parse("25000000-0000-4000-8000-000000000004").value()
    };
    auto next = std::make_shared<std::size_t>(0);
    Session target{[generated, next] {
        Q_ASSERT(*next < generated.size());
        return generated[(*next)++];
    }};

    const auto previousChainId = target.masteringChainState->chain_id();
    const auto previousGainId = target.masteringChainState->gain_instance_id();
    const auto previousEqId = target.masteringChainState->eq_instance_id();

    // Exercise the real ProjectSessionViewModel::openProject coordination path:
    // preserve the conceptual EQ selection while the persisted project materializes
    // a mastering chain with different ModuleInstanceId values.
    target.adapterModel->selectModuleByInstanceId(
        QString::fromStdString(previousEqId.to_string()));
    const auto previousSelectedInstanceId = target.adapterModel->selected_instance_id();
    QCOMPARE(target.adapterModel->selected_index(), 1);

    target.project.openProject(QUrl::fromLocalFile(legacyPath));
    QCOMPARE(target.project.error_message(), QString());

    QCOMPARE(target.masteringChainState->chain_id(), generated[0]);
    QCOMPARE(target.masteringChainState->gain_instance_id().uuid(), generated[1]);
    QCOMPARE(target.masteringChainState->eq_instance_id().uuid(), generated[2]);
    QCOMPARE(target.masteringChainState->compressor_instance_id().uuid(), generated[3]);
    QVERIFY(target.masteringChainState->chain_id() != previousChainId);
    QVERIFY(target.masteringChainState->gain_instance_id() != previousGainId);
    QVERIFY(target.masteringChainState->eq_instance_id() != previousEqId);

    const auto restoredEqInstanceId =
        QString::fromStdString(target.masteringChainState->eq_instance_id().to_string());
    QVERIFY(target.adapterModel->selected_instance_id() != previousSelectedInstanceId);
    QCOMPARE(target.adapterModel->selected_instance_id(), restoredEqInstanceId);
    QVERIFY(target.adapterModel->active_module() != nullptr);
    QCOMPARE(target.adapterModel->active_module()->instance_id(), restoredEqInstanceId);
    QCOMPARE(target.adapterModel->selected_index(), 1);
    QCOMPARE(target.adapterModel->active_module()->type_id(),
             QStringLiteral("rgsml.dsp.parametric-eq"));

    QCOMPARE(target.masteringChainState->gain_parameters().gain_db(), 0.0);
    QVERIFY(!target.gainViewModel->bypass());
    QVERIFY(target.eqViewModel->is_default());
    QVERIFY(!target.eqViewModel->bypass());

    const auto savedLegacyPath = dir.filePath(QStringLiteral("saved_legacy.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(savedLegacyPath));
    QCOMPARE(target.project.error_message(), QString());

    auto resavedSnap = load_project(savedLegacyPath);
    QVERIFY(resavedSnap);
    const auto& resavedDoc = resavedSnap.value()->document();
    QVERIFY(resavedDoc.pipeline.masteringChainId.has_value());
    QCOMPARE(*resavedDoc.pipeline.masteringChainId, generated[0]);
    QCOMPARE(resavedDoc.chains.size(), std::size_t{1});
    QCOMPARE(resavedDoc.chains[0].chainId, generated[0]);
    QCOMPARE(resavedDoc.chains[0].stage, std::string("MASTER"));
    QCOMPARE(resavedDoc.chains[0].segment, std::string("MANUAL"));
    QCOMPARE(resavedDoc.chains[0].modules.size(), std::size_t{3});
    QCOMPARE(resavedDoc.chains[0].modules[0].instanceId, generated[1]);
    QCOMPARE(resavedDoc.chains[0].modules[0].typeId, std::string("rgsml.dsp.gain"));
    QCOMPARE(resavedDoc.chains[0].modules[1].instanceId, generated[2]);
    QCOMPARE(resavedDoc.chains[0].modules[1].typeId,
             std::string("rgsml.dsp.parametric-eq"));
    QCOMPARE(resavedDoc.chains[0].modules[2].instanceId, generated[3]);
    QCOMPARE(resavedDoc.chains[0].modules[2].typeId,
             std::string("rgsml.dsp.compressor"));

    const auto savedLegacyPath2 = dir.filePath(QStringLiteral("saved_legacy2.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(savedLegacyPath2));
    QCOMPARE(target.project.error_message(), QString());

    auto resavedSnap2 = load_project(savedLegacyPath2);
    QVERIFY(resavedSnap2);
    const auto& second = resavedSnap2.value()->document();
    QCOMPARE(*second.pipeline.masteringChainId, generated[0]);
    QCOMPARE(second.chains[0].chainId, generated[0]);
    QCOMPARE(second.chains[0].modules[0].instanceId, generated[1]);
    QCOMPARE(second.chains[0].modules[1].instanceId, generated[2]);
    QCOMPARE(second.chains[0].modules[2].instanceId, generated[3]);
}

void ProjectSessionTest::legacy_two_module_gain_eq_migration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source = wav(dir, QStringLiteral("source.wav"), 0);

    // Build a 2-module Gain + EQ project document manually
    const auto chainId = *core::Uuid::parse("30000000-0000-4000-8000-000000000001").value();
    const auto gainId = *core::Uuid::parse("30000000-0000-4000-8000-000000000010").value();
    const auto eqId = *core::Uuid::parse("30000000-0000-4000-8000-000000000020").value();
    const auto sourceId = *core::Uuid::parse("30000000-0000-4000-8000-000000000090").value();
    const auto projectId = *core::Uuid::parse("30000000-0000-4000-8000-000000000000").value();

    project::ProjectDocument doc;
    doc.projectId = projectId;
    doc.displayName = "legacy_two_module";
    doc.sourceResourceId = sourceId;

    const auto sourceUtf8 = source.toUtf8().toStdString();
    const auto sourceName = QFileInfo{source}.fileName().toUtf8().toStdString();
    const auto providerId = std::string(platform::windows::WindowsResourceReader::provider_id());
    doc.resources.push_back(project::Resource{sourceId, "AUDIO", {
        project::LocatorHint{providerId, sourceUtf8, sourceName}
    }, std::nullopt});

    doc.pipeline.masteringChainId = chainId;
    project::Chain chain;
    chain.chainId = chainId;
    chain.stage = "MASTER";
    chain.segment = "MANUAL";
    chain.revision = 1U;

    auto gainParams = dsp::GainParameters::create(-3.5);
    QVERIFY(gainParams);
    auto gainJson = dsp::encode_gain_parameters_json(*gainParams.value());
    QVERIFY(gainJson);

    project::Module m0;
    m0.instanceId = gainId;
    m0.typeId = "rgsml.dsp.gain";
    m0.enabled = true;
    m0.userBypass = false;
    m0.provenance = "MANUAL";
    m0.owner = "USER";
    m0.linkState = "UNLINKED";
    m0.algorithmVersion = "1.0.0";
    m0.parameterSchemaId = "rgsml.dsp.gain.parameters/1.0.0";
    m0.parameters = *project::OpaqueJsonValue::parse(*gainJson.value()).value();
    chain.modules.push_back(m0);

    auto bandId = *core::Uuid::parse("30000000-0000-0000-0000-000000000001").value();
    auto band = dsp::EqBandParameters::create(bandId, true, dsp::EqFilterType::BELL, dsp::EqRouting::STEREO, dsp::BellPayload{1000.0, 2.0, 1.0});
    QVERIFY(band);
    auto eqParams = dsp::ParametricEqParameters::create({*band.value()});
    QVERIFY(eqParams);
    auto eqJson = dsp::encode_parametric_eq_parameters_json(*eqParams.value());
    QVERIFY(eqJson);

    project::Module m1;
    m1.instanceId = eqId;
    m1.typeId = "rgsml.dsp.parametric-eq";
    m1.enabled = true;
    m1.userBypass = true; // EQ bypassed in legacy doc
    m1.provenance = "MANUAL";
    m1.owner = "USER";
    m1.linkState = "UNLINKED";
    m1.algorithmVersion = "1.0.0";
    m1.parameterSchemaId = "rgsml.dsp.parametric-eq.parameters/1.0.0";
    m1.parameters = *project::OpaqueJsonValue::parse(*eqJson.value()).value();
    chain.modules.push_back(m1);

    doc.chains.push_back(chain);

    auto legacySnap = project::ProjectSnapshot::create(doc);
    QVERIFY(legacySnap);
    const auto legacyPath = dir.filePath(QStringLiteral("legacy_2mod.rgsml"));
    QVERIFY(write_project(legacyPath, *legacySnap.value()));

    const auto compUuid = *core::Uuid::parse("30000000-0000-4000-8000-000000000030").value();
    Session target{[compUuid] { return compUuid; }};

    target.project.openProject(QUrl::fromLocalFile(legacyPath));
    QCOMPARE(target.project.error_message(), QString());

    // Verify Gain & EQ instance IDs and parameters preserved
    QCOMPARE(target.masteringChainState->gain_instance_id().uuid(), gainId);
    QCOMPARE(target.masteringChainState->eq_instance_id().uuid(), eqId);
    QCOMPARE(target.masteringChainState->gain_parameters().gain_db(), -3.5);
    QCOMPARE(target.masteringChainState->parametric_eq_parameters(), *eqParams.value());

    auto gainBypass = target.masteringChainState->is_bypassed(target.masteringChainState->gain_instance_id());
    QVERIFY(gainBypass);
    QVERIFY(!*gainBypass.value());

    auto eqBypass = target.masteringChainState->is_bypassed(target.masteringChainState->eq_instance_id());
    QVERIFY(eqBypass);
    QVERIFY(*eqBypass.value());

    // Verify Compressor instance created with default parameters & user bypass = true
    QCOMPARE(target.masteringChainState->compressor_instance_id().uuid(), compUuid);
    QVERIFY(target.masteringChainState->compressor_parameters() == *dsp::CompressorParameters::create_default().value());

    auto compBypass = target.masteringChainState->is_bypassed(target.masteringChainState->compressor_instance_id());
    QVERIFY(compBypass);
    QVERIFY(*compBypass.value()); // Must be user-bypassed!

    // Save As and verify 3-module roundtrip
    const auto resavedPath = dir.filePath(QStringLiteral("migrated_3mod.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(resavedPath));
    QCOMPARE(target.project.error_message(), QString());

    auto resavedSnap = load_project(resavedPath);
    QVERIFY(resavedSnap);
    const auto& resavedDoc = resavedSnap.value()->document();
    QCOMPARE(resavedDoc.chains[0].modules.size(), std::size_t{3});
    QCOMPARE(resavedDoc.chains[0].modules[0].instanceId, gainId);
    QCOMPARE(resavedDoc.chains[0].modules[1].instanceId, eqId);
    QCOMPARE(resavedDoc.chains[0].modules[2].instanceId, compUuid);
    QVERIFY(resavedDoc.chains[0].modules[2].userBypass);
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
    QCOMPARE(targetFuture.masteringChainState->chain_id(), session.chainId);

    const auto preservedPath = dir.filePath(QStringLiteral("future_preserved.rgsml"));
    targetFuture.project.saveProjectAs(QUrl::fromLocalFile(preservedPath));
    QCOMPARE(targetFuture.project.error_message(), QString());
    auto preserved = load_project(preservedPath);
    QVERIFY(preserved);
    QCOMPARE(preserved.value()->document().extensions.canonical_utf8(),
             doc.extensions.canonical_utf8());
    QVERIFY(preserved.value()->document().pipeline.masteringChainId.has_value());
    QCOMPARE(*preserved.value()->document().pipeline.masteringChainId, session.chainId);
}

void ProjectSessionTest::source_reset_vs_project_open()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto source1 = wav(dir, QStringLiteral("s1.wav"), 0);
    const auto source2 = wav(dir, QStringLiteral("s2.wav"), 1);

    Session session;
    session.source.selectSource(QUrl::fromLocalFile(source1));

    QVERIFY(session.gainViewModel->setGainDb(5.0));
    QVERIFY(session.gainViewModel->can_undo());
    auto bandId = core::Uuid::parse("26000000-0000-0000-0000-000000000001");
    QVERIFY(bandId);
    auto band = dsp::EqBandParameters::create(*bandId.value(), true,
        dsp::EqFilterType::BELL, dsp::EqRouting::SIDE,
        dsp::BellPayload{1400.0, -2.0, 1.4});
    QVERIFY(band);
    auto persistedEq = dsp::ParametricEqParameters::create({*band.value()});
    QVERIFY(persistedEq);
    QVERIFY(session.masteringChainState->set_parametric_eq_parameters(*persistedEq.value()));
    session.eqViewModel->refreshFromAuthority();

    const auto projectPath = dir.filePath(QStringLiteral("project.rgsml"));
    session.project.saveProjectAs(QUrl::fromLocalFile(projectPath));
    QCOMPARE(session.project.error_message(), QString());

    session.source.selectSource(QUrl::fromLocalFile(source2));
    QCOMPARE(session.gainViewModel->gain_db(), 0.0);
    QVERIFY(!session.gainViewModel->can_undo());
    QVERIFY(!session.gainViewModel->can_redo());
    QVERIFY(session.eqViewModel->is_default());
    QVERIFY(!session.gainViewModel->bypass());
    QVERIFY(!session.eqViewModel->bypass());

    Session openedSession;
    openedSession.project.openProject(QUrl::fromLocalFile(projectPath));
    QCOMPARE(openedSession.project.error_message(), QString());
    QCOMPARE(openedSession.gainViewModel->gain_db(), 5.0);
    QVERIFY(!openedSession.gainViewModel->can_undo());
    QVERIFY(!openedSession.gainViewModel->can_redo());
    QCOMPARE(openedSession.masteringChainState->parametric_eq_parameters(),
             *persistedEq.value());
    QCOMPARE(openedSession.eqViewModel->band_count(), 1);
    QCOMPARE(openedSession.eqViewModel->selected_band_id(),
             QString::fromStdString(bandId.value()->to_string()));
}


void ProjectSessionTest::product_default_stereo_ms_project_save_open_save_identity()
{
    // Product-startup M15 is a real fourth persisted module, but the legacy
    // 2/3-node project factories remain unchanged and never auto-insert it.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto wavPath = wav(dir, QStringLiteral("product_default_source.wav"), 9);
    QVERIFY(!wavPath.isEmpty());

    Session author;
    author.source.selectSource(QUrl::fromLocalFile(wavPath));
    QVERIFY(author.source.source_resource());
    const auto stereoUuid = core::Uuid::parse(
        "17000000-0000-4000-8000-000000000015");
    QVERIFY(stereoUuid);
    const auto stereoId = dsp::ModuleInstanceId::from_uuid(*stereoUuid.value());
    QVERIFY(stereoId);
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto product = app::MasteringChainState::create_product_default(
        *registry.value(), author.chainId, author.gainId, author.eqId,
        author.compId, *stereoId.value());
    QVERIFY(product);
    *author.masteringChainState = std::move(*product.value());
    author.adapterModel->refreshFromAuthority();
    QCOMPARE(author.masteringChainState->module_count(), std::size_t{4});
    const auto defaults = dsp::StereoMsParameters::create_default();
    QVERIFY(defaults);
    QCOMPARE(author.masteringChainState->stereo_ms_parameters(),
             std::optional{*defaults.value()});
    auto bypass = author.masteringChainState->is_bypassed(*stereoId.value());
    QVERIFY(bypass);
    QVERIFY(*bypass.value());

    const auto firstPath = dir.filePath(QStringLiteral("product_default_four.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(firstPath));
    QCOMPARE(author.project.error_message(), QString{});
    auto first = load_project(firstPath);
    QVERIFY(first);
    const auto& authored = first.value()->document().chains;
    QCOMPARE(authored.size(), std::size_t{1});
    QCOMPARE(authored[0].modules.size(), std::size_t{4});
    QCOMPARE(authored[0].modules[0].typeId, std::string("rgsml.dsp.gain"));
    QCOMPARE(authored[0].modules[1].typeId, std::string("rgsml.dsp.parametric-eq"));
    QCOMPARE(authored[0].modules[2].typeId, std::string("rgsml.dsp.compressor"));
    QCOMPARE(authored[0].modules[3].typeId, std::string("rgsml.dsp.stereo-ms"));
    QCOMPARE(authored[0].modules[3].instanceId, *stereoUuid.value());
    QCOMPARE(authored[0].modules[3].userBypass, true);

    Session reopened;
    reopened.project.openProject(QUrl::fromLocalFile(firstPath));
    QCOMPARE(reopened.project.error_message(), QString{});
    QVERIFY(!reopened.project.degraded());
    QCOMPARE(reopened.masteringChainState->module_count(), std::size_t{4});
    QCOMPARE(reopened.masteringChainState->chain_id(), author.chainId);
    QCOMPARE(reopened.masteringChainState->stereo_ms_instance_id(),
             std::optional{*stereoId.value()});
    QCOMPARE(reopened.masteringChainState->stereo_ms_parameters(),
             std::optional{*defaults.value()});
    bypass = reopened.masteringChainState->is_bypassed(*stereoId.value());
    QVERIFY(bypass);
    QVERIFY(*bypass.value());

    const auto secondPath = dir.filePath(QStringLiteral("product_default_four_again.rgsml"));
    reopened.project.saveProjectAs(QUrl::fromLocalFile(secondPath));
    QCOMPARE(reopened.project.error_message(), QString{});
    auto second = load_project(secondPath);
    QVERIFY(second);
    const auto& roundtripped = second.value()->document().chains;
    QCOMPARE(roundtripped.size(), std::size_t{1});
    QCOMPARE(roundtripped[0].chainId, authored[0].chainId);
    QCOMPARE(roundtripped[0].revision, authored[0].revision);
    QCOMPARE(roundtripped[0].modules.size(), std::size_t{4});
    for (std::size_t i = 0; i < 4U; ++i) {
        QCOMPARE(roundtripped[0].modules[i].typeId, authored[0].modules[i].typeId);
        QCOMPARE(roundtripped[0].modules[i].instanceId, authored[0].modules[i].instanceId);
        QCOMPARE(roundtripped[0].modules[i].userBypass, authored[0].modules[i].userBypass);
        QCOMPARE(roundtripped[0].modules[i].parameters.canonical_utf8(),
                 authored[0].modules[i].parameters.canonical_utf8());
    }
}

void ProjectSessionTest::stereo_ms_four_module_project_roundtrip_and_atomic_rejection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto wavPath = wav(dir, QStringLiteral("m15_source.wav"), 8);
    QVERIFY(!wavPath.isEmpty());

    Session author;
    author.source.selectSource(QUrl::fromLocalFile(wavPath));
    QVERIFY(author.source.source_resource());
    const auto msUuid = core::Uuid::parse(
        "15000000-0000-4000-8000-000000000015");
    QVERIFY(msUuid);
    auto msId = dsp::ModuleInstanceId::from_uuid(*msUuid.value());
    QVERIFY(msId);
    auto params = dsp::StereoMsParameters::create(
        -1.5, 2.5, false, dsp::MonoBassMode::LR24, 125.0, 35.0);
    QVERIFY(params);
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto optIn = app::MasteringChainState::create_with_stereo_ms(
        *registry.value(), author.chainId, author.gainId, author.eqId,
        author.compId, *msId.value(), *params.value(), true);
    QVERIFY(optIn);
    *author.masteringChainState = std::move(*optIn.value());
    // Preserve non-default chain revision and independent bypass state.
    QVERIFY(author.masteringChainState->set_user_bypass(author.compId, false));
    const auto revision = author.masteringChainState->chain().revision();

    const auto firstPath = dir.filePath(QStringLiteral("four_modules.rgsml"));
    author.project.saveProjectAs(QUrl::fromLocalFile(firstPath));
    QCOMPARE(author.project.error_message(), QString{});
    auto first = load_project(firstPath);
    QVERIFY(first);
    const auto& original = first.value()->document();
    QCOMPARE(original.chains.size(), std::size_t{1});
    QCOMPARE(original.chains[0].modules.size(), std::size_t{4});
    const auto& fourth = original.chains[0].modules[3];
    QCOMPARE(fourth.typeId, std::string("rgsml.dsp.stereo-ms"));
    QCOMPARE(fourth.instanceId, *msUuid.value());
    QCOMPARE(fourth.userBypass, true);
    QCOMPARE(original.chains[0].revision, revision);
    QCOMPARE(fourth.parameterSchemaId,
             std::optional<std::string>{"rgsml.dsp.stereo-ms.parameters/1.0.0"});
    QCOMPARE(fourth.algorithmVersion, std::optional<std::string>{"1.0.0"});
    auto decoded = dsp::decode_stereo_ms_parameters_json(
        fourth.parameters.canonical_utf8());
    QVERIFY(decoded);
    QCOMPARE(*decoded.value(), *params.value());

    Session target;
    target.project.openProject(QUrl::fromLocalFile(firstPath));
    QCOMPARE(target.project.error_message(), QString{});
    QVERIFY(!target.project.degraded());
    QCOMPARE(target.masteringChainState->module_count(), std::size_t{4});
    QCOMPARE(target.masteringChainState->chain_id(), author.chainId);
    QCOMPARE(target.masteringChainState->chain().revision(), revision);
    QCOMPARE(target.masteringChainState->stereo_ms_instance_id(),
             std::optional{*msId.value()});
    QCOMPARE(target.masteringChainState->stereo_ms_parameters(),
             std::optional{*params.value()});
    QCOMPARE(target.masteringChainState->instances()[3].instance_id(),
             *msId.value());
    auto bypass = target.masteringChainState->is_bypassed(*msId.value());
    QVERIFY(bypass);
    QVERIFY(*bypass.value());
    auto bindings = target.masteringChainState->execution_bindings();
    QCOMPARE(bindings.size(), std::size_t{4});
    QCOMPARE(std::get<dsp::StereoMsParameters>(bindings[3].parameters),
             *params.value());

    // Save-as from the reopened session must retain exactly the canonical
    // descriptor, typed parameters, stable IDs, revision and bypass.
    const auto secondPath = dir.filePath(QStringLiteral("four_modules_again.rgsml"));
    target.project.saveProjectAs(QUrl::fromLocalFile(secondPath));
    QCOMPARE(target.project.error_message(), QString{});
    auto second = load_project(secondPath);
    QVERIFY(second);
    const auto& again = second.value()->document().chains[0];
    QCOMPARE(again.revision, revision);
    QCOMPARE(again.modules.size(), std::size_t{4});
    for (std::size_t i = 0; i < 4U; ++i) {
        QCOMPARE(again.modules[i].instanceId,
                 original.chains[0].modules[i].instanceId);
        QCOMPARE(again.modules[i].typeId,
                 original.chains[0].modules[i].typeId);
        QCOMPARE(again.modules[i].userBypass,
                 original.chains[0].modules[i].userBypass);
        QCOMPARE(again.modules[i].parameters.canonical_utf8(),
                 original.chains[0].modules[i].parameters.canonical_utf8());
    }

    // Reject malformed four-node input BEFORE stopping playback or
    // mutating the currently open project/chain/source.
    const auto currentSource = target.source.source_resource()->reference().locator();
    const auto verifyStillOpen = [&]() {
        QCOMPARE(target.source.source_resource()->reference().locator(), currentSource);
        QCOMPARE(target.masteringChainState->chain_id(), author.chainId);
        QCOMPARE(target.masteringChainState->chain().revision(), revision);
        QCOMPARE(target.masteringChainState->stereo_ms_instance_id(),
                 std::optional{*msId.value()});
        QCOMPARE(target.masteringChainState->stereo_ms_parameters(),
                 std::optional{*params.value()});
    };
    const auto reject = [&](project::ProjectDocument bad, const QString& name) {
        auto candidate = project::ProjectSnapshot::create(std::move(bad));
        QVERIFY(candidate);
        const auto path = dir.filePath(name);
        QVERIFY(write_project(path, *candidate.value()));
        target.project.openProject(QUrl::fromLocalFile(path));
        QVERIFY2(!target.project.error_message().isEmpty(),
                 "Corrupt Stereo/M-S project must fail closed before adoption");
        verifyStillOpen();
    };
    {
        auto bad = original;
        bad.chains[0].modules[3].parameters =
            *project::OpaqueJsonValue::parse("{\"invalid\":true}").value();
        reject(std::move(bad), QStringLiteral("bad_ms_payload.rgsml"));
    }
    {
        auto bad = original;
        bad.chains[0].modules[3].parameterSchemaId =
            "rgsml.dsp.stereo-ms.parameters/2.0.0";
        reject(std::move(bad), QStringLiteral("bad_ms_schema.rgsml"));
    }
    {
        auto bad = original;
        bad.chains[0].modules[3].typeId = "rgsml.dsp.unknown-stereo";
        reject(std::move(bad), QStringLiteral("bad_ms_type.rgsml"));
    }
    {
        auto bad = original;
        std::swap(bad.chains[0].modules[2], bad.chains[0].modules[3]);
        reject(std::move(bad), QStringLiteral("bad_ms_order.rgsml"));
    }
    {
        auto bad = original;
        bad.chains[0].modules[3].provenance = "AUTOMATED";
        reject(std::move(bad), QStringLiteral("bad_ms_provenance.rgsml"));
    }
}

}  // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::ProjectSessionTest)
#include "test_project_session.moc"
