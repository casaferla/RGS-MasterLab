#include "source_selection_view_model.hpp"

#include "../../audio_golden/wav/golden_vectors.hpp"
#include "../../unit/audio/wav_test_support.hpp"
#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace rgsml::tests {
namespace {

using namespace wav_support;

[[nodiscard]] QByteArray to_byte_array(std::span<const std::byte> bytes)
{
    return QByteArray{
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()),
    };
}

template <std::size_t Size>
[[nodiscard]] QByteArray to_byte_array(const std::array<std::uint8_t, Size>& bytes)
{
    return QByteArray{
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()),
    };
}

[[nodiscard]] QString write_file(
    QTemporaryDir& directory,
    const QString& name,
    const QByteArray& bytes)
{
    const auto path = directory.filePath(name);
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(bytes) != bytes.size()) {
        return {};
    }
    file.close();
    return path;
}

}  // namespace

class SourceSelectionTest final : public QObject {
    Q_OBJECT

private slots:
    void initialSuccessCancelFailureRetry();
    void m18SourceMutationDeniedBeforeSelectionAndAcrossThreads();
    void m18SourceGateCannotUninstallOrNestedSelectDuringAdmission();
    void m18SourcePublicationRejectsReentrantQtListeners();
    void m18ThrownAdmissionRecoversWithoutReplacingSource();
    void m18DirectProbedSourceFromForeignThreadCannotBypassOwner();
    void m18DirectSourceAdoptionBlockedDuringOuterPublication();
    void m18PhysicalSourceBatchExcludesAmbiguousDualGateAndDenial();
    void m18PhysicalSourceBatchNeedsCheckedProductDispatcher();
    void m18RejectedCheckedSourceCascadesFailClosedThenRecover();
    void acceptedFormatMappings_data();
    void acceptedFormatMappings();
};

void SourceSelectionTest::initialSuccessCancelFailureRetry()
{
    app::SourceSelectionViewModel model;
    QCOMPARE(model.source_state(), QStringLiteral("NO_SOURCE"));
    QVERIFY(!model.has_source());
    QVERIFY(model.display_name().isEmpty());
    QVERIFY(model.error_message().isEmpty());
    QVERIFY(!model.read_only());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto validPath = write_file(
        directory,
        QStringLiteral("Valid Source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!validPath.isEmpty());

    QSignalSpy sourceSpy{&model, &app::SourceSelectionViewModel::sourceChanged};
    model.selectSource(QUrl::fromLocalFile(validPath));
    QCOMPARE(sourceSpy.count(), 1);
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_READY"));
    QVERIFY(model.has_source());
    QCOMPARE(model.display_name(), QStringLiteral("Valid Source.wav"));
    QCOMPARE(model.container_label(), QStringLiteral("RIFF/WAVE"));
    QCOMPARE(model.sample_format_label(), QStringLiteral("PCM 16-bit"));
    QCOMPARE(model.sample_rate_hz(), qint64{44100});
    QCOMPARE(model.channel_layout_label(), QStringLiteral("Mono (C)"));
    QCOMPARE(model.channel_count(), 1);
    QCOMPARE(model.frame_count(), qint64{3});
    QCOMPARE(model.duration_label(), QStringLiteral("0:00.000"));
    QVERIFY(model.read_only());

    const auto stateBeforeCancel = model.source_state();
    const auto nameBeforeCancel = model.display_name();
    const auto errorBeforeCancel = model.error_message();
    model.cancelSourceSelection();
    QCOMPARE(sourceSpy.count(), 1);
    QCOMPARE(model.source_state(), stateBeforeCancel);
    QCOMPARE(model.display_name(), nameBeforeCancel);
    QCOMPARE(model.error_message(), errorBeforeCancel);

    model.selectSource(QUrl{QStringLiteral("https://example.invalid/source.wav")});
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_ERROR"));
    QVERIFY(model.has_source());
    QCOMPARE(model.display_name(), nameBeforeCancel);
    QVERIFY(!model.error_message().isEmpty());
    QCOMPARE(sourceSpy.count(), 1);

    const auto malformedPath = write_file(
        directory,
        QStringLiteral("Malformed.wav"),
        QByteArray{"not a wav"});
    model.selectSource(QUrl::fromLocalFile(malformedPath));
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_ERROR"));
    QCOMPARE(model.display_name(), nameBeforeCancel);
    QCOMPARE(sourceSpy.count(), 1);

    const auto retryPath = write_file(
        directory,
        QStringLiteral("Retry RF64.wav"),
        to_byte_array(audio_golden::kRf64F64NegativeZero));
    model.selectSource(QUrl::fromLocalFile(retryPath));
    QCOMPARE(sourceSpy.count(), 2);
    QCOMPARE(model.source_state(), QStringLiteral("SOURCE_READY"));
    QCOMPARE(model.display_name(), QStringLiteral("Retry RF64.wav"));
    QCOMPARE(model.container_label(), QStringLiteral("RF64/WAVE"));
    QCOMPARE(model.sample_format_label(), QStringLiteral("IEEE float 64-bit"));
    QVERIFY(model.error_message().isEmpty());
}

void SourceSelectionTest::m18SourceMutationDeniedBeforeSelectionAndAcrossThreads()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto firstPath=write_file(directory,
        QStringLiteral("m18-current.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    const auto secondPath=write_file(directory,
        QStringLiteral("m18-replacement.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!firstPath.isEmpty()&&!secondPath.isEmpty());
    app::SourceSelectionViewModel model;
    model.selectSource(QUrl::fromLocalFile(firstPath));
    QVERIFY(model.has_source());
    QCOMPARE(model.display_name(),QStringLiteral("m18-current.wav"));
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    std::size_t rejected=0;
    model.set_private_m18_source_mutation_admission(
        [&](const char* op)->core::Status {
            if(op!=nullptr)++rejected;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_SOURCE_WORKER_COMMIT_BUSY"});
        });
    model.selectSource(QUrl::fromLocalFile(secondPath));
    QCOMPARE(rejected,std::size_t{1});
    QCOMPARE(model.display_name(),QStringLiteral("m18-current.wav"));
    QVERIFY(!model.error_message().isEmpty());
    QCOMPARE(changed.count(),0);
    const auto priorError=model.error_message();
    std::thread foreign([&] {
        model.selectSource(QUrl::fromLocalFile(secondPath));
    });
    foreign.join();
    QCOMPARE(rejected,std::size_t{1});
    QCOMPARE(changed.count(),0);
    QCOMPARE(model.error_message(),priorError);
    QCOMPARE(model.display_name(),QStringLiteral("m18-current.wav"));
    model.set_private_m18_source_mutation_admission({});
    model.selectSource(QUrl::fromLocalFile(secondPath));
    QCOMPARE(model.display_name(),QStringLiteral("m18-replacement.wav"));
    QCOMPARE(changed.count(),1);
    QVERIFY(model.error_message().isEmpty());
}

void SourceSelectionTest::
m18SourceGateCannotUninstallOrNestedSelectDuringAdmission()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto first=write_file(directory,QStringLiteral("m18-outer-source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    const auto second=write_file(directory,QStringLiteral("m18-nested-source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!first.isEmpty()&&!second.isEmpty());
    app::SourceSelectionViewModel model;
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    QSignalSpy errors{&model,&app::SourceSelectionViewModel::errorChanged};
    std::size_t admissions=0;
    model.set_private_m18_source_mutation_admission(
        [&](const char* op)->core::Status {
            if(!op ||std::string_view{op}!="source_adopt_probed")
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,"M18_SOURCE_OPERATION_UNKNOWN"});
            ++admissions;
            model.set_private_m18_source_mutation_admission({});
            // The callback cannot reenter a file picker and replace the
            // still-not-published outer SOURCE, or erase its own gate.
            model.selectSource(QUrl::fromLocalFile(second));
            return core::Status::success();
        });
    model.selectSource(QUrl::fromLocalFile(first));
    QCOMPARE(admissions,std::size_t{1});
    QCOMPARE(model.display_name(),QStringLiteral("m18-outer-source.wav"));
    QCOMPARE(changed.count(),1);
    QCOMPARE(errors.count(),0);
    // The attempt to tear down the policy was refused; the second
    // independent SOURCE choice still requires Product admission.
    model.selectSource(QUrl::fromLocalFile(second));
    QCOMPARE(admissions,std::size_t{2});
    QCOMPARE(model.display_name(),QStringLiteral("m18-nested-source.wav"));
    QCOMPARE(changed.count(),2);
    QCOMPARE(errors.count(),0);
    model.set_private_m18_source_mutation_admission({});
}

void SourceSelectionTest::
m18SourcePublicationRejectsReentrantQtListeners()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto first=write_file(dir,QStringLiteral("m18-qt-primary.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    const auto second=write_file(dir,QStringLiteral("m18-qt-reentrant.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!first.isEmpty()&&!second.isEmpty());
    app::SourceSelectionViewModel model;
    std::size_t admissions=0,committedHandlerCalls=0,observerCalls=0;
    model.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++admissions;
            return core::Status::success();
        });
    model.set_source_committed_handler([&](const core::ResourceReference&) {
        ++committedHandlerCalls;
        model.selectSource(QUrl::fromLocalFile(second));
        // The current SOURCE mutation guard must still be installed.
        model.set_private_m18_source_mutation_admission({});
        // Cannot replace/destroy a std::function while its own closure
        // is executing; the same handler must run on the next SOURCE.
        model.set_source_committed_handler({});
    });
    QObject::connect(&model,&app::SourceSelectionViewModel::sourceChanged,
        &model,[&] {
            ++observerCalls;
            model.selectSource(QUrl::fromLocalFile(second));
        });
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    QSignalSpy errors{&model,&app::SourceSelectionViewModel::errorChanged};
    model.selectSource(QUrl::fromLocalFile(first));
    QCOMPARE(admissions,std::size_t{1});
    QCOMPARE(committedHandlerCalls,std::size_t{1});
    QCOMPARE(observerCalls,std::size_t{1});
    QCOMPARE(changed.count(),1);
    QCOMPARE(errors.count(),0);
    QCOMPARE(model.display_name(),QStringLiteral("m18-qt-primary.wav"));
    // After the entire callback/Qt publication scope exits, a second
    // independent owner-thread action is allowed by that same authority.
    model.selectSource(QUrl::fromLocalFile(second));
    QCOMPARE(admissions,std::size_t{2});
    QCOMPARE(committedHandlerCalls,std::size_t{2});
    QCOMPARE(observerCalls,std::size_t{2});
    QCOMPARE(changed.count(),2);
    QCOMPARE(errors.count(),0);
    QCOMPARE(model.display_name(),QStringLiteral("m18-qt-reentrant.wav"));
    model.set_private_m18_source_mutation_admission({});
}

void SourceSelectionTest::
m18ThrownAdmissionRecoversWithoutReplacingSource()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto first=write_file(dir,QStringLiteral("m18-before-throw.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    const auto second=write_file(dir,QStringLiteral("m18-after-throw.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!first.isEmpty()&&!second.isEmpty());
    app::SourceSelectionViewModel model;
    model.selectSource(QUrl::fromLocalFile(first));
    QVERIFY(model.has_source());
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    model.set_private_m18_source_mutation_admission(
        [](const char*)->core::Status {
            throw std::runtime_error("M18_PRODUCT_SOURCE_GATE_FAULT");
        });
    model.selectSource(QUrl::fromLocalFile(second));
    QCOMPARE(model.display_name(),QStringLiteral("m18-before-throw.wav"));
    QCOMPARE(changed.count(),0);
    QVERIFY(!model.error_message().isEmpty());

    std::size_t subsequentAdmission=0;
    model.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++subsequentAdmission;
            return core::Status::success();
        });
    model.selectSource(QUrl::fromLocalFile(second));
    QCOMPARE(subsequentAdmission,std::size_t{1});
    QCOMPARE(changed.count(),1);
    QCOMPARE(model.display_name(),QStringLiteral("m18-after-throw.wav"));
    QVERIFY(model.error_message().isEmpty());
    model.set_private_m18_source_mutation_admission({});
}

void SourceSelectionTest::
m18DirectProbedSourceFromForeignThreadCannotBypassOwner()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto first=write_file(dir,QStringLiteral("m18-owner-source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    const auto second=write_file(dir,QStringLiteral("m18-worker-source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!first.isEmpty()&&!second.isEmpty());
    app::SourceSelectionViewModel model;
    model.selectSource(QUrl::fromLocalFile(first));
    QVERIFY(model.has_source());
    std::size_t admissions=0;
    model.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++admissions;
            return core::Status::success();
        });
    const auto utf8=second.toUtf8().toStdString();
    const auto name=QFileInfo{second}.fileName().toUtf8().toStdString();
    const auto ref=platform::windows::WindowsResourceReader::make_read_reference(
        utf8,name);
    QVERIFY(ref);
    auto reader=platform::windows::WindowsResourceReader::open_read_only(
        *ref.value());
    QVERIFY(reader);
    auto source=audio::SourceResource::probe(std::move(*reader.value()));
    QVERIFY(source);
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    bool deniedFromWorker=false;
    std::thread worker([&] {
        auto result=model.adopt_probed_source(std::move(*source.value()));
        deniedFromWorker=!result &&result.error()->code()
            ==core::ErrorCode::InvalidState;
    });
    worker.join();
    QVERIFY(deniedFromWorker);
    QCOMPARE(admissions,std::size_t{0});
    QCOMPARE(changed.count(),0);
    QCOMPARE(model.display_name(),QStringLiteral("m18-owner-source.wav"));
    model.selectSource(QUrl::fromLocalFile(second));
    QCOMPARE(admissions,std::size_t{1});
    QCOMPARE(changed.count(),1);
    QCOMPARE(model.display_name(),QStringLiteral("m18-worker-source.wav"));
    model.set_private_m18_source_mutation_admission({});
}

void SourceSelectionTest::
m18DirectSourceAdoptionBlockedDuringOuterPublication()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto first=write_file(dir,QStringLiteral("m18-primary-direct.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    const auto second=write_file(dir,QStringLiteral("m18-reentrant-direct.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!first.isEmpty()&&!second.isEmpty());
    const auto path=second.toUtf8().toStdString();
    const auto name=QFileInfo{second}.fileName().toUtf8().toStdString();
    auto reference=platform::windows::WindowsResourceReader::make_read_reference(
        path,name);
    QVERIFY(reference);
    auto reader=platform::windows::WindowsResourceReader::open_read_only(
        *reference.value());
    QVERIFY(reader);
    auto replacement=audio::SourceResource::probe(
        std::move(*reader.value()));
    QVERIFY(replacement);
    app::SourceSelectionViewModel model;
    std::size_t admits=0,handlers=0,nestedRejected=0;
    model.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++admits;
            return core::Status::success();
        });
    model.set_source_committed_handler([&](const core::ResourceReference&) {
        ++handlers;
        if(handlers==1) {
            auto nested=model.adopt_probed_source(
                std::move(*replacement.value()));
            if(!nested &&nested.error()->code()
               ==core::ErrorCode::InvalidState)++nestedRejected;
        }
    });
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    model.selectSource(QUrl::fromLocalFile(first));
    QCOMPARE(admits,std::size_t{1});
    QCOMPARE(handlers,std::size_t{1});
    QCOMPARE(nestedRejected,std::size_t{1});
    QCOMPARE(changed.count(),1);
    QCOMPARE(model.display_name(),QStringLiteral("m18-primary-direct.wav"));
    model.selectSource(QUrl::fromLocalFile(second));
    QCOMPARE(admits,std::size_t{2});
    QCOMPARE(handlers,std::size_t{2});
    QCOMPARE(changed.count(),2);
    QCOMPARE(model.display_name(),QStringLiteral("m18-reentrant-direct.wav"));
    model.set_private_m18_source_mutation_admission({});
}

void SourceSelectionTest::
m18PhysicalSourceBatchExcludesAmbiguousDualGateAndDenial()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto file=write_file(dir,QStringLiteral("m18-batch-source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!file.isEmpty());
    app::SourceSelectionViewModel model;
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    std::size_t statusCalls=0,batchCalls=0;
    model.set_private_m18_source_mutation_admission(
        [&](const char*)->core::Status {
            ++statusCalls;
            return core::Status::success();
        });
    model.set_private_m18_source_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++batchCalls;
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<const int>(1));
        });
    model.selectSource(QUrl::fromLocalFile(file));
    QVERIFY(!model.has_source());
    QCOMPARE(statusCalls,std::size_t{0});
    QCOMPARE(batchCalls,std::size_t{0});
    QCOMPARE(changed.count(),0);
    QVERIFY(!model.error_message().isEmpty());
    // Even an allowed status callback cannot be stacked with an
    // independent batch authorizer: choose ONE Product epoch authority.
    model.set_private_m18_source_mutation_admission({});
    std::size_t downstream=0;
    model.set_private_m18_checked_source_committed_handler(
        [&](const core::ResourceReference&)->core::Status {
            ++downstream;
            return core::Status::success();
        });
    model.set_private_m18_source_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++batchCalls;
            return core::Result<std::shared_ptr<const void>>::failure(
                core::Error{core::ErrorCode::InvalidState,
                            "M18_WORKER_COMMIT_LEASE_HELD"});
        });
    model.selectSource(QUrl::fromLocalFile(file));
    QVERIFY(!model.has_source());
    QCOMPARE(changed.count(),0);
    QCOMPARE(batchCalls,std::size_t{1});
    model.set_private_m18_source_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++batchCalls;
            // The shared Product lease has not yet been returned. Its
            // admission callback cannot uninstall itself or reenter
            // the SOURCE picker before acquiring physical exclusivity.
            model.selectSource(QUrl::fromLocalFile(file));
            model.set_private_m18_source_batch_begin({});
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<const int>(3));
        });
    model.selectSource(QUrl::fromLocalFile(file));
    QVERIFY(model.has_source());
    QCOMPARE(batchCalls,std::size_t{2});
    QCOMPARE(statusCalls,std::size_t{0});
    QCOMPARE(downstream,std::size_t{1});
    QCOMPARE(changed.count(),1);
    QCOMPARE(model.display_name(),QStringLiteral("m18-batch-source.wav"));
    QVERIFY(model.error_message().isEmpty());
    model.set_private_m18_source_batch_begin({});
}

void SourceSelectionTest::
m18PhysicalSourceBatchNeedsCheckedProductDispatcher()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto path=write_file(
        dir,QStringLiteral("m18-requires-checked-source.wav"),
        to_byte_array(audio_golden::kRiffPcm16Mono));
    QVERIFY(!path.isEmpty());
    app::SourceSelectionViewModel model;
    std::size_t batches=0,checked=0;
    model.set_private_m18_source_batch_begin(
        [&]()->core::Result<std::shared_ptr<const void>> {
            ++batches;
            return core::Result<std::shared_ptr<const void>>::success(
                std::make_shared<const int>(1));
        });
    QSignalSpy changed{&model,&app::SourceSelectionViewModel::sourceChanged};
    model.selectSource(QUrl::fromLocalFile(path));
    QVERIFY(!model.has_source());
    QCOMPARE(batches,std::size_t{0});
    QCOMPARE(changed.count(),0);
    QVERIFY(!model.error_message().isEmpty());
    // A legacy void handler is never sufficient proof of completed M18
    // cascaded Product writes, even if a physical batch was installed.
    model.set_private_m18_checked_source_committed_handler(
        [&](const core::ResourceReference&)->core::Status {
            ++checked;
            return core::Status::success();
        });
    model.selectSource(QUrl::fromLocalFile(path));
    QCOMPARE(batches,std::size_t{1});
    QCOMPARE(checked,std::size_t{1});
    QVERIFY(model.has_source());
    QCOMPARE(model.display_name(),
             QStringLiteral("m18-requires-checked-source.wav"));
    QVERIFY(model.error_message().isEmpty());
    QCOMPARE(changed.count(),1);
    model.set_private_m18_source_batch_begin({});
    model.set_private_m18_checked_source_committed_handler({});
}

void SourceSelectionTest::acceptedFormatMappings_data()
{
    QTest::addColumn<QByteArray>("wav");
    QTest::addColumn<QString>("formatLabel");
    QTest::addColumn<QString>("layoutLabel");
    QTest::addColumn<int>("channels");

    const auto pcm16 = make_wav(
        1, 16, 1, 48000,
        pcm_payload(std::array<std::int64_t, 1>{0}, 16));
    const auto pcm24 = make_wav(
        1, 24, 2, 48000,
        pcm_payload(std::array<std::int64_t, 2>{0, 1}, 24));
    const auto pcm32 = make_wav(
        1, 32, 1, 48000,
        pcm_payload(std::array<std::int64_t, 1>{0}, 32));
    const auto f32 = make_wav(
        3, 32, 2, 48000,
        f32_payload(std::array<std::uint32_t, 2>{0, 0x3f000000U}));
    const auto f64 = make_wav(
        3, 64, 1, 48000,
        f64_payload(std::array<std::uint64_t, 1>{0}));

    QTest::newRow("pcm16") << to_byte_array(pcm16) << QStringLiteral("PCM 16-bit")
                            << QStringLiteral("Mono (C)") << 1;
    QTest::newRow("pcm24") << to_byte_array(pcm24) << QStringLiteral("PCM 24-bit")
                            << QStringLiteral("Stereo (L/R)") << 2;
    QTest::newRow("pcm32") << to_byte_array(pcm32) << QStringLiteral("PCM 32-bit")
                            << QStringLiteral("Mono (C)") << 1;
    QTest::newRow("f32") << to_byte_array(f32) << QStringLiteral("IEEE float 32-bit")
                          << QStringLiteral("Stereo (L/R)") << 2;
    QTest::newRow("f64") << to_byte_array(f64) << QStringLiteral("IEEE float 64-bit")
                          << QStringLiteral("Mono (C)") << 1;
}

void SourceSelectionTest::acceptedFormatMappings()
{
    QFETCH(QByteArray, wav);
    QFETCH(QString, formatLabel);
    QFETCH(QString, layoutLabel);
    QFETCH(int, channels);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = write_file(directory, QStringLiteral("mapping.wav"), wav);
    QVERIFY(!path.isEmpty());
    app::SourceSelectionViewModel model;
    model.selectSource(QUrl::fromLocalFile(path));
    QVERIFY(model.has_source());
    QCOMPARE(model.sample_format_label(), formatLabel);
    QCOMPARE(model.channel_layout_label(), layoutLabel);
    QCOMPARE(model.channel_count(), channels);
    QCOMPARE(model.sample_rate_hz(), qint64{48000});
    QCOMPARE(model.frame_count(), qint64{1});
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::SourceSelectionTest)

#include "test_source_selection.moc"
