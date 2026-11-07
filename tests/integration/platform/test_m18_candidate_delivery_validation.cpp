// M18 candidate readback through REAL canonical WavReader, bounded RAM.
// This test does not authorize M18 publication or certify final Windows FQ.
#include "internal/m18_candidate_delivery_validation.hpp"
#include "wav_test_support.hpp"
#include "m18_streaming_wav.hpp"
#include "internal/transactional_audio_exporter_test_seam.hpp"
#include "internal/m18_epoch_commit_barrier.hpp"
#include "internal/m18_product_epoch_ticket.hpp"
#include "internal/m18_owner_mutation_fanout.hpp"
#include "internal/m18_async_export_worker.hpp"
#include <rgsml/platform/windows/transactional_audio_exporter.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <QtTest/QTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <future>
#include <limits>
#include <thread>
#include <memory>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <vector>

namespace rgsml::tests {
namespace {
class ByteCollector final : public audio::internal::M18ByteSink {
public:
    [[nodiscard]] bool write_all(
        std::span<const std::uint8_t> data) noexcept override
    {
        try{
            bytes.insert(bytes.end(),data.begin(),data.end());
            return true;
        }catch(...){return false;}
    }
    std::vector<std::uint8_t> bytes;
};

[[nodiscard]] std::shared_ptr<audio::AudioBuffer> master(
    audio::ChannelLayout layout,std::int64_t frames,
    std::int64_t fs=48000)
{
    const auto hz=core::SampleRate::create(fs);
    if(!hz)return {};
    const auto format=audio::AudioFormat::create(*hz.value(),layout);
    const auto count=core::FrameCount::create(frames);
    if(!format||!count)return {};
    auto buffer=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*count.value());
    if(!buffer)return {};
    auto result=std::make_shared<audio::AudioBuffer>(
        std::move(*buffer.value()));
    auto view=result->mutable_view();
    for(std::size_t ch=0;ch<view.format().channel_count();++ch){
        auto plane=view.channel(ch);
        Q_ASSERT(plane);
        for(std::size_t i=0;i<plane.value()->size();++i){
            (*plane.value())[i]=(i%2U==0U?0.4:-0.4)
                *(ch==0U?1.0:0.75);
        }
    }
    return result;
}

[[nodiscard]] wav_support::Bytes render(
    audio::AudioBufferView original,
    audio::WavSampleFormat encoding,
    std::uint64_t seed)
{
    using namespace audio::internal;
    ByteCollector sink;
    const auto channels=static_cast<std::uint32_t>(
        original.format().channel_count());
    const auto frames=static_cast<std::uint64_t>(
        original.frame_count().value());
    const M18WavSpec spec{
        encoding==audio::WavSampleFormat::PCM_S24
            ?M18WavEncoding::PCM24:M18WavEncoding::IEEE_FLOAT32,
        static_cast<std::uint32_t>(
            original.format().sample_rate().value()),
        channels,frames,seed};
    M18StreamingWavWriter writer(sink,spec);
    if(writer.start()!=M18StreamStatus::OK)return {};
    std::vector<double> interleaved(
        static_cast<std::size_t>(frames)*channels);
    for(std::size_t ch=0;ch<channels;++ch){
        const auto p=original.channel(ch);
        Q_ASSERT(p);
        for(std::size_t i=0;i<frames;++i)
            interleaved[i*channels+ch]=(*p.value())[i];
    }
    if(writer.append(interleaved)!=M18StreamStatus::OK)return {};
    const auto finished=writer.finish();
    if(finished.status!=M18StreamStatus::OK ||!finished.receipt)return {};
    wav_support::Bytes bytes;
    bytes.reserve(sink.bytes.size());
    for(const auto b:sink.bytes)bytes.push_back(static_cast<std::byte>(b));
    return bytes;
}

[[nodiscard]] core::Result<std::unique_ptr<audio::WavReader>>
open(wav_support::Bytes bytes,
     const std::shared_ptr<wav_support::ReaderControl>& control)
{
    return audio::WavReader::open(
        wav_support::memory_reader(std::move(bytes),control));
}

struct TestSessionState final {
    bool current{true};
    std::size_t acquired{0};
    std::size_t revalidated{0};
    std::size_t retired{0};
    std::size_t rejectOnRevalidationCall{0};
};
class TestM18Lease final:public platform::windows::M18ExportCommitLease {
public:
    explicit TestM18Lease(std::shared_ptr<TestSessionState> owner)
        : owner_(std::move(owner)) {}
    ~TestM18Lease() override{++owner_->retired;}
    [[nodiscard]] core::Status revalidate_current() const override {
        ++owner_->revalidated;
        return (owner_->current
                &&(owner_->rejectOnRevalidationCall==0
                   ||owner_->revalidated!=owner_->rejectOnRevalidationCall))
            ?core::Status::success()
            :core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_TEST_EPOCH_REJECTED"});
    }
private:
    std::shared_ptr<TestSessionState> owner_;
};
[[nodiscard]] platform::windows::M18AcquireCommitLease
make_test_commit_authority(std::shared_ptr<TestSessionState> state)
{
    return [state=std::move(state)]()
        ->core::Result<std::unique_ptr<platform::windows::M18ExportCommitLease>> {
        ++state->acquired;
        std::unique_ptr<platform::windows::M18ExportCommitLease> result=
            std::make_unique<TestM18Lease>(state);
        return core::Result<std::unique_ptr<
            platform::windows::M18ExportCommitLease>>::success(std::move(result));
    };
}
[[nodiscard]] std::string m18_path(const QString& value)
{return value.toUtf8().toStdString();}
[[nodiscard]] core::Result<platform::windows::M18TransactionalAudioExportRequest>
make_real_transaction_request(
    const QString& src,const QString& dst,
    std::shared_ptr<const audio::AudioBuffer> owner,
    audio::WavSampleFormat format,
    std::optional<std::uint64_t> seed,double ceiling,
    std::shared_ptr<TestSessionState> session)
{
    using namespace platform::windows;
    auto sourceRef=WindowsResourceReader::make_read_reference(
        m18_path(src),m18_path(QFileInfo{src}.fileName()));
    if(!sourceRef)return core::Result<M18TransactionalAudioExportRequest>::failure(
        *sourceRef.error());
    auto destRef=WindowsResourceWriter::make_write_reference(
        m18_path(dst),m18_path(QFileInfo{dst}.fileName()));
    if(!destRef)return core::Result<M18TransactionalAudioExportRequest>::failure(
        *destRef.error());
    return M18TransactionalAudioExportRequest::create(
        *sourceRef.value(),std::move(owner),*destRef.value(),
        format,seed,ceiling,ExportCollisionPolicy::FAIL_IF_EXISTS,
        make_test_commit_authority(std::move(session)));
}
[[nodiscard]] bool m18_no_temporary_candidates(const QString& dir)
{
    return QDir{dir}.entryList(
        {QStringLiteral("*.rgsml-export-*.tmp")},
        QDir::Files|QDir::Hidden).isEmpty();
}
} // namespace

class M18CandidateDeliveryValidationTest final : public QObject{
    Q_OBJECT
private slots:
    void exactPcm24ReadbackAndCeilingEvidence();
    void float32AndWrongSeedFailClosed();
    void tamperedPayloadAndWrongMasterRejected();
    void realWindowsM18TransactionalReadback();
    void realWindowsM18RollbackAndEpochGate();
    void realWindowsM18CooperativeCancellationAndAsyncWorker();
    void realWindowsAsyncWorkerRejectsEpochRevocationBeforeCommit();
    void realWindowsSixRatesBothLayoutsBothEncodingsOddEven();
    void realWindowsFailureInjectionCleansUpBothFormats();
    void crossThreadEpochBarrierPreventsCommitRaceWithoutQtBlocking();
    void exclusiveProductProjectMutationBlocksWorkerCommitUntilComplete();
    void productMutationBatchPinsIdentityAndForbidsPartialCapture();
    void productDestroyedDuringProjectMutationCannotReviveWorkerExport();
    void productEpochTicketConsentAndMasterIdentityAreWorkerSafe();
    void sameProductMutationFanoutRevokesWorkerAcrossControllerCallbacks();
    void retiredProductMutationFanoutRejectsCachedControllerCallbacks();
    void productMutationFanoutRejectsOverflowAndInvalidOperations();
    void actualProductSaveBatchBlocksWorkerAndOtherEditorCallbacks();
    void productOpenBatchUsesOneExclusiveEpochForSourceRateAndGold();
    void realWorkerTicketExportsOnlyWithCurrentConsentAndSource();
    void realWindowsExportDeniedAfterAbortedProductProjectBatch();
    void realWindowsFinalAuthorityRevocationRollsBackBothFormats();
    void realWorkerRevokedBeforeCommitAdmissionCleansCandidate();
};

void M18CandidateDeliveryValidationTest::
sameProductMutationFanoutRevokesWorkerAcrossControllerCallbacks()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    // No application/QML activation: this is the exact same future
    // Product-mutation callback shared by multiple controller domains.
    const M18ProductExportIdentity initial{
        71U,72U,73U,0U,48000U,11U};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        900U,initial);
    QVERIFY(owner->try_transition(900U,901U,initial,true));
    auto ticket=owner->capture(initial);
    QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,901U,initial};
    auto sourceGate=fanout.status_admission();
    auto chainGate=fanout.status_admission();
    auto rateGate=fanout.rate_admission();

    // A genuinely held worker lease excludes all owner mutations.
    auto commit=ticket.value()->commit_lease_factory()();
    QVERIFY(commit);
    QVERIFY(!sourceGate("source_selection"));
    QVERIFY(!chainGate("compressor_parameters"));
    QVERIFY(!rateGate());
    QVERIFY((*commit.value())->revalidate_current());
    commit.value()->reset();

    // An admitted SOURCE edit retires the old verified Master ticket.
    QVERIFY(sourceGate("source_selection"));
    QVERIFY(!ticket.value()->commit_lease_factory()());
    // Chain and rate edits share the SAME epoch sequence, not their own
    // per-widget status callbacks or independently grantable tickets.
    QVERIFY(chainGate("compressor_parameters"));
    QVERIFY(rateGate());
    QVERIFY(!owner->capture(initial)); // explicit consent was revoked

    bool foreignDenied=false;
    std::thread thread([&]{
        const auto status=chainGate("off_thread_chain");
        foreignDenied=!status &&status.error()->code()
            ==core::ErrorCode::InvalidState;
    });
    thread.join();
    QVERIFY(foreignDenied);
    QVERIFY(!sourceGate(nullptr));
    QVERIFY(!sourceGate(""));
}

void M18CandidateDeliveryValidationTest::
retiredProductMutationFanoutRejectsCachedControllerCallbacks()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    const M18ProductExportIdentity id{81U,82U,83U,0U,96000U,19U};
    auto authority=std::make_shared<M18ProductEpochTicketAuthority>(
        40U,id);
    M18OwnerMutationFanout::StatusAdmission retainedSource;
    M18OwnerMutationFanout::RateAdmission retainedRate;
    {
        M18OwnerMutationFanout ProductCallbacks{authority,40U,id};
        retainedSource=ProductCallbacks.status_admission();
        retainedRate=ProductCallbacks.rate_admission();
        QVERIFY(retainedSource("source_committed"));
        QVERIFY(retainedRate());
    }
    // A surviving Qt model must not be able to resurrect its deleted
    // Product callback owner after that Product binding was retired.
    const auto result=retainedSource("stale_after_owner_destroyed");
    QVERIFY(!result);
    QCOMPARE(result.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(!retainedRate());
    // Product authority lifetime itself is separate from this adapter.
    // The future app must destroy/revoke both together; neither this
    // test nor the adapter grants a fresh export ticket.
}

void M18CandidateDeliveryValidationTest::
productMutationFanoutRejectsOverflowAndInvalidOperations()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    constexpr auto max=std::numeric_limits<std::uint64_t>::max();
    const M18ProductExportIdentity maxGeneration{
        14U,15U,max,0U,48000U,0U};
    auto authority=std::make_shared<M18ProductEpochTicketAuthority>(
        49U,maxGeneration);
    M18OwnerMutationFanout g{authority,49U,maxGeneration};
    QVERIFY(!g.status_admission()("gain_change"));
    QVERIFY(!g.rate_admission()());
    // The gate must also deny absent operation tags before consuming a
    // generation, leaving the accepted Product identity unmodified.
    const M18ProductExportIdentity valid{
        14U,15U,16U,0U,48000U,0U};
    auto current=std::make_shared<M18ProductEpochTicketAuthority>(72U,valid);
    M18OwnerMutationFanout normal{current,72U,valid};
    const auto gate=normal.status_admission();
    QVERIFY(!gate(nullptr));
    QVERIFY(!gate(""));
    QVERIFY(gate("source_committed"));
    const auto stale=normal.status_admission();
    QVERIFY(stale("source_committed_same_fs"));
    auto exhausted=std::make_shared<M18ProductEpochTicketAuthority>(
        max,valid);
    M18OwnerMutationFanout last{exhausted,max,valid};
    QVERIFY(!last.status_admission()("source_committed"));
}

void M18CandidateDeliveryValidationTest::
actualProductSaveBatchBlocksWorkerAndOtherEditorCallbacks()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    const M18ProductExportIdentity id{24U,25U,26U,0U,48000U,7U};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        400U,id);
    QVERIFY(owner->try_transition(400U,401U,id,true));
    auto ticket=owner->capture(id);QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,401U,id};
    const auto save=fanout.project_save_batch();
    const auto edit=fanout.status_admission();
    auto committing=ticket.value()->commit_lease_factory()();
    QVERIFY(committing);
    // A worker in the final commit/readback phase must deny a physical
    // Project SaveAs batch before any snapshot/file is touched.
    QVERIFY(!save());
    QVERIFY((*committing.value())->revalidate_current());
    committing.value()->reset();

    auto saving=save();QVERIFY(saving);
    QVERIFY(saving.value()->get()!=nullptr);
    QVERIFY(!edit("source_selection"));
    QVERIFY(!edit("terminal_ceiling"));
    QVERIFY(!save()); // no nested Project transaction
    QVERIFY(!owner->capture(id));
    saving.value()->reset();
    // Once the exclusive Project scope is released, new owner-thread
    // mutations may advance again; old worker consent stays revoked.
    QVERIFY(edit("source_selection"));
    QVERIFY(!ticket.value()->commit_lease_factory()());
    bool foreignSaveDenied=false;
    std::thread foreign([&] {
        const auto result=save();
        foreignSaveDenied=!result &&result.error()->code()
            ==core::ErrorCode::InvalidState;
    });
    foreign.join();
    QVERIFY(foreignSaveDenied);
}

void M18CandidateDeliveryValidationTest::
productOpenBatchUsesOneExclusiveEpochForSourceRateAndGold()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    using platform::windows::internal::M18OwnerMutationFanout;
    const M18ProductExportIdentity id{61U,62U,63U,0U,48000U,12U};
    auto owner=std::make_shared<M18ProductEpochTicketAuthority>(
        700U,id);
    QVERIFY(owner->try_transition(700U,701U,id,true));
    auto ticket=owner->capture(id);
    QVERIFY(ticket);
    M18OwnerMutationFanout fanout{owner,701U,id};
    const auto open=fanout.project_open_batch();
    auto source=fanout.status_admission();
    auto gold=fanout.status_admission();
    auto rate=fanout.rate_admission();
    auto worker=ticket.value()->commit_lease_factory()();
    QVERIFY(worker);
    // An external worker may already be completing a file rename. The
    // Project must refuse BEFORE stopping transport or touching SOURCE.
    QVERIFY(!open());
    QVERIFY((*worker.value())->revalidate_current());
    worker.value()->reset();

    auto batch=open();QVERIFY(batch);
    QVERIFY(batch.value()->get()!=nullptr);
    QVERIFY(!open()); // nested Project open is never allowed
    QVERIFY(!fanout.project_save_batch()());
    QVERIFY(source("source_adopt_probed"));
    QVERIFY(source("source_committed"));
    QVERIFY(rate());
    QVERIFY(gold("set_gold"));
    QVERIFY(gold("region_set"));
    QVERIFY(gold("switch_target"));
    // Unrelated edits cannot slip through the Project-only allowance
    // while the exclusive authorizer has been granted.
    QVERIFY(!source("gain_set"));
    QVERIFY(!source("ceiling_update"));
    QVERIFY(!source(nullptr));
    QVERIFY(!ticket.value()->commit_lease_factory()());

    // Releasing the opaque Project operation unblocks the normal
    // per-controller gate, but never revives old verified-M17 consent.
    batch.value()->reset();
    QVERIFY(source("gain_set"));
    QVERIFY(!ticket.value()->commit_lease_factory()());
}

void M18CandidateDeliveryValidationTest::
realWindowsAsyncWorkerRejectsEpochRevocationBeforeCommit()
{
    // The file worker is independent of the QObject Product. Its exact
    // immutable ticket must fail after a genuinely accepted Source/Project
    // mutation, even if candidate encoding has already completed.
    using namespace platform::windows;
    using internal::M18AsyncExportWorker;
    using internal::M18ProductEpochTicketAuthority;
    using internal::M18ProductExportIdentity;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto source=dir.filePath(QStringLiteral("actual-input.bin"));
    const auto output=dir.filePath(QStringLiteral("obsolete-master.wav"));
    QFile file{source};
    QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(file.write("source-is-stable",16),qint64{16});
    file.close();
    const auto src=WindowsResourceReader::make_read_reference(
        m18_path(source),m18_path(QFileInfo{source}.fileName()));
    const auto dst=WindowsResourceWriter::make_write_reference(
        m18_path(output),m18_path(QFileInfo{output}.fileName()));
    const auto pcm=master(audio::ChannelLayout::MONO_C,8193,48000);
    QVERIFY(src&&dst&&pcm);
    const M18ProductExportIdentity identity{901U,902U,903U,0U,48000U};
    M18ProductEpochTicketAuthority owner{500U,identity};
    QVERIFY(owner.try_transition(500U,501U,identity,true));
    auto ticket=owner.capture(identity);
    QVERIFY(ticket);
    const auto factory=ticket.value()->commit_lease_factory();

    std::promise<void> reachesCommit;
    auto awaited=reachesCommit.get_future();
    auto announce=std::make_shared<std::promise<void>>(
        std::move(reachesCommit));
    std::promise<void> allowCommit;
    auto release=allowCommit.get_future().share();
    M18AcquireCommitLease delayed=[factory,announce,release]() {
        announce->set_value();
        release.wait();
        return factory();
    };
    auto request=M18TransactionalAudioExportRequest::create(
        *src.value(),pcm,*dst.value(),audio::WavSampleFormat::PCM_S24,
        std::optional<std::uint64_t>{UINT64_C(0x5566778899AABBCC)},
        0.0,ExportCollisionPolicy::FAIL_IF_EXISTS,std::move(delayed));
    QVERIFY(request);
    auto task=M18AsyncExportWorker::launch(std::move(*request.value()));
    QVERIFY(task);
    const auto entered=awaited.wait_for(std::chrono::seconds{20});
    if(entered==std::future_status::ready) {
        QCOMPARE((*task.value())->encoded_frames(),std::uint64_t{8193});
        QVERIFY(!(*task.value())->finished());
        QVERIFY(!(*task.value())->result_if_finished());
    }
    // Release even on failed timing assertions, so no orphan worker
    // remains blocked waiting on a test-owned promise.
    auto nextIdentity=identity;
    ++nextIdentity.source_generation;
    const auto revoked=owner.try_transition(
        501U,502U,nextIdentity,false);
    allowCommit.set_value();
    QVERIFY(entered==std::future_status::ready);
    QVERIFY(revoked);
    QTRY_VERIFY_WITH_TIMEOUT((*task.value())->finished(),30000);
    const auto result=(*task.value())->result_if_finished();
    QVERIFY(result);
    QVERIFY(!*result);
    QCOMPARE(result->error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(!QFileInfo::exists(output));
    QVERIFY(m18_no_temporary_candidates(dir.path()));
    QFile verify{source};
    QVERIFY(verify.open(QIODevice::ReadOnly));
    QCOMPARE(verify.readAll(),QByteArray("source-is-stable",16));
}

void M18CandidateDeliveryValidationTest::
realWindowsM18CooperativeCancellationAndAsyncWorker()
{
    // One REAL Windows transactional path, two encoding families, no UI
    // callback or unverified candidate file exposed to the Product.
    using namespace platform::windows;
    using internal::M18AsyncExportWorker;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto inputPath=dir.filePath(QStringLiteral("source-safe.bin"));
    QFile input{inputPath};
    QVERIFY(input.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(input.write("source-stable",13),qint64{13});
    input.close();

    for(const auto encoding:{
            audio::WavSampleFormat::PCM_S24,
            audio::WavSampleFormat::IEEE_F32}) {
        const auto data=master(audio::ChannelLayout::MONO_C,8193,48000);
        QVERIFY(data);
        const auto seed=encoding==audio::WavSampleFormat::PCM_S24
            ?std::optional<std::uint64_t>{UINT64_C(0x5A55A55A01234567)}
            :std::nullopt;
        for(int scenario=0;scenario<4;++scenario) {
            const auto out=dir.filePath(
                QStringLiteral("cancel-%1-%2.wav")
                .arg(static_cast<int>(encoding)).arg(scenario));
            auto auth=std::make_shared<TestSessionState>();
            auto req=make_real_transaction_request(
                inputPath,out,data,encoding,seed,0.0,auth);
            QVERIFY(req);
            std::size_t calls=0;
            // Scenario 0: cancel before candidate creation.
            // Scenario 1: cancel during streaming after first 4096 frames.
            // Scenario 2: cancel AFTER publish/readback while lease is held.
            // Scenario 3: throwing cancellation callback denies closed.
            const std::function<bool()> cancellation=[&,scenario] {
                ++calls;
                if(scenario==0)return true;
                if(scenario==1)return calls>=3;
                if(scenario==2)return QFileInfo::exists(out);
                throw std::runtime_error("denied callback");
            };
            const auto result=TransactionalAudioExporter::export_new_file(
                *req.value(),cancellation);
            QVERIFY(!result);
            QCOMPARE(result.error()->code(),core::ErrorCode::InvalidState);
            QVERIFY(!QFileInfo::exists(out));
            QVERIFY(m18_no_temporary_candidates(dir.path()));
            QCOMPARE(auth->acquired,scenario==2?std::size_t{1}
                                                :std::size_t{0});
            QCOMPARE(auth->retired,auth->acquired);
        }

        // A throwing progress observer must also fail closed and clean
        // its partially written WAV; telemetry is NEVER an authority.
        const auto progressOut=dir.filePath(
            QStringLiteral("throwing-progress-%1.wav")
                .arg(static_cast<int>(encoding)));
        auto progressSession=std::make_shared<TestSessionState>();
        auto progressReq=make_real_transaction_request(
            inputPath,progressOut,data,encoding,seed,0.0,progressSession);
        QVERIFY(progressReq);
        const auto badProgress=TransactionalAudioExporter::export_new_file(
            *progressReq.value(),{},
            [](std::uint64_t,std::uint64_t) {
                throw std::runtime_error("cancelled telemetry");
            });
        QVERIFY(!badProgress);
        QCOMPARE(badProgress.error()->code(),core::ErrorCode::InvalidState);
        QVERIFY(!QFileInfo::exists(progressOut));
        QVERIFY(m18_no_temporary_candidates(dir.path()));

        // Actual detached nonblocking worker, with only an immutable
        // AudioBuffer, value references, and Qt-free commit lease factory.
        const auto out=dir.filePath(QStringLiteral("async-%1.wav")
            .arg(static_cast<int>(encoding)));
        auto auth=std::make_shared<TestSessionState>();
        auto req=make_real_transaction_request(
            inputPath,out,data,encoding,seed,0.0,auth);
        QVERIFY(req);
        auto task=M18AsyncExportWorker::launch(std::move(*req.value()));
        QVERIFY(task);
        QCOMPARE((*task.value())->total_frames(),std::uint64_t{8193});
        QTRY_VERIFY_WITH_TIMEOUT((*task.value())->finished(),30000);
        QCOMPARE((*task.value())->encoded_frames(),std::uint64_t{8193});
        const auto finished=(*task.value())->result_if_finished();
        QVERIFY(finished);
        QVERIFY(*finished);
        QCOMPARE(finished->value()->sample_format(),encoding);
        QCOMPARE(finished->value()->frame_count().value(),std::int64_t{8193});
        QCOMPARE(finished->value()->encoded_file_sha256().size(),
                 std::size_t{64});
        QVERIFY(QFileInfo::exists(out));
        QVERIFY(m18_no_temporary_candidates(dir.path()));
        QCOMPARE(auth->acquired,std::size_t{1});
        QCOMPARE(auth->retired,std::size_t{1});

        // A second worker targeting an already delivered name must never
        // overwrite/reencode the existing WAV or mint a commit lease.
        QFile published{out};
        QVERIFY(published.open(QIODevice::ReadOnly));
        const auto alreadyPublished=published.readAll();
        published.close();
        auto secondAuth=std::make_shared<TestSessionState>();
        auto duplicate=make_real_transaction_request(
            inputPath,out,data,encoding,seed,0.0,secondAuth);
        QVERIFY(duplicate);
        auto duplicateTask=M18AsyncExportWorker::launch(
            std::move(*duplicate.value()));
        QVERIFY(duplicateTask);
        QTRY_VERIFY_WITH_TIMEOUT((*duplicateTask.value())->finished(),30000);
        const auto second=(*duplicateTask.value())->result_if_finished();
        QVERIFY(second);
        QVERIFY(!*second);
        QCOMPARE(secondAuth->acquired,std::size_t{0});
        QVERIFY(m18_no_temporary_candidates(dir.path()));
        QVERIFY(published.open(QIODevice::ReadOnly));
        QCOMPARE(published.readAll(),alreadyPublished);
        published.close();

        // Idempotent cancellation after success never lies about result.
        (*task.value())->cancel();
        QVERIFY((*task.value())->finished());
        QVERIFY((*task.value())->result_if_finished());
    }
    // Deterministic race: cancel the nonblocking worker WHILE the
    // epoch factory is deliberately waiting immediately before commit.
    // Once admitted, the worker must observe cancellation and remove
    // the candidate; no destination file or partial evidence may escape.
    const auto blockedOutput=dir.filePath(
        QStringLiteral("cancel-while-commit-lease-waits.wav"));
    const auto sourceReference=WindowsResourceReader::make_read_reference(
        m18_path(inputPath),m18_path(QFileInfo{inputPath}.fileName()));
    const auto destinationReference=WindowsResourceWriter::make_write_reference(
        m18_path(blockedOutput),
        m18_path(QFileInfo{blockedOutput}.fileName()));
    QVERIFY(sourceReference&&destinationReference);
    std::promise<void> atFactory;
    auto arrived=atFactory.get_future();
    auto announce=std::make_shared<std::promise<void>>(
        std::move(atFactory));
    std::promise<void> releaseFactory;
    auto mayContinue=releaseFactory.get_future().share();
    auto lockState=std::make_shared<TestSessionState>();
    M18AcquireCommitLease pausedFactory=[
        auth=make_test_commit_authority(lockState),
        announce,mayContinue]() mutable
        ->core::Result<std::unique_ptr<M18ExportCommitLease>> {
        announce->set_value();
        mayContinue.wait();
        return auth();
    };
    // Copyable pause callback shares the one-shot producer and never
    // captures QObject or a mutable Product ViewModel.
    auto sharedPaused=std::make_shared<M18AcquireCommitLease>(
        std::move(pausedFactory));
    auto blockedRequest=M18TransactionalAudioExportRequest::create(
        *sourceReference.value(),master(audio::ChannelLayout::MONO_C,8193),
        *destinationReference.value(),audio::WavSampleFormat::PCM_S24,
        std::optional<std::uint64_t>{UINT64_C(7263)},0.0,
        ExportCollisionPolicy::FAIL_IF_EXISTS,
        [sharedPaused]() {return (*sharedPaused)();});
    QVERIFY(blockedRequest);
    auto stopped=M18AsyncExportWorker::launch(
        std::move(*blockedRequest.value()));
    QVERIFY(stopped);
    const auto entered=arrived.wait_for(std::chrono::seconds{20});
    if(entered==std::future_status::ready) {
        // Encoding 100% is NOT a successful delivery: the worker is still
        // waiting for the Product commit lease and final WAV readback.
        QCOMPARE((*stopped.value())->encoded_frames(),std::uint64_t{8193});
        QVERIFY(!(*stopped.value())->finished());
        QVERIFY(!(*stopped.value())->result_if_finished());
    }
    // Release FIRST to ensure no detached job remains blocked if any
    // subsequent assertion fails.
    (*stopped.value())->cancel();
    releaseFactory.set_value();
    QVERIFY(entered==std::future_status::ready);
    QTRY_VERIFY_WITH_TIMEOUT((*stopped.value())->finished(),30000);
    const auto canceled=(*stopped.value())->result_if_finished();
    QVERIFY(canceled);
    QVERIFY(!*canceled);
    QVERIFY(!QFileInfo::exists(blockedOutput));
    QVERIFY(m18_no_temporary_candidates(dir.path()));
    QFile unchanged{inputPath};
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(),QByteArray("source-stable",13));
}

void M18CandidateDeliveryValidationTest::
exactPcm24ReadbackAndCeilingEvidence()
{
    using platform::windows::internal::validate_m18_candidate_decoded_delivery;
    const auto original=master(audio::ChannelLayout::STEREO_LR,4105);
    QVERIFY(original);
    const auto bytes=render(original->view(),audio::WavSampleFormat::PCM_S24,
        UINT64_C(0xFEDCBA9876543210));
    auto control=std::make_shared<wav_support::ReaderControl>();
    auto wav=open(bytes,control);
    QVERIFY(wav);
    const auto evidence=validate_m18_candidate_decoded_delivery(
        *wav.value()->get(),original->view(),
        audio::WavSampleFormat::PCM_S24,
        UINT64_C(0xFEDCBA9876543210),0.0);
    QVERIFY(evidence);
    QCOMPARE(evidence.value()->exact_frames,std::uint64_t{4105});
    QCOMPARE(evidence.value()->encoded_format,audio::WavSampleFormat::PCM_S24);
    QCOMPARE(evidence.value()->canonical_decoded_sha256.size(),std::size_t{64});
    QCOMPARE(evidence.value()->decoded_true_peak.programme_frames,
        std::uint64_t{4105});
    QCOMPARE(evidence.value()->decoded_true_peak.qualification_status,
        analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
    QVERIFY(evidence.value()->meets_ceiling);
    QVERIFY(control->readCalls>1U);
    auto again=open(bytes,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(again);
    const auto nonconforming=validate_m18_candidate_decoded_delivery(
        *again.value()->get(),original->view(),
        audio::WavSampleFormat::PCM_S24,
        UINT64_C(0xFEDCBA9876543210),-12.0);
    QVERIFY(nonconforming);
    QVERIFY(!nonconforming.value()->meets_ceiling);
    QVERIFY(nonconforming.value()->decoded_true_peak.global_db_tp.has_value());
}

void M18CandidateDeliveryValidationTest::float32AndWrongSeedFailClosed()
{
    using platform::windows::internal::validate_m18_candidate_decoded_delivery;
    const auto original=master(audio::ChannelLayout::MONO_C,85);
    QVERIFY(original);
    auto bytes=render(original->view(),audio::WavSampleFormat::IEEE_F32,0U);
    auto control=std::make_shared<wav_support::ReaderControl>();
    auto wav=open(bytes,control);
    QVERIFY(wav);
    const auto valid=validate_m18_candidate_decoded_delivery(
        *wav.value()->get(),original->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,0.0);
    QVERIFY(valid);
    QVERIFY(valid.value()->meets_ceiling);
    auto other=open(bytes,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(other);
    QVERIFY(!validate_m18_candidate_decoded_delivery(
        *other.value()->get(),original->view(),
        audio::WavSampleFormat::IEEE_F32,17U,0.0));
    QVERIFY(!validate_m18_candidate_decoded_delivery(
        *other.value()->get(),original->view(),
        audio::WavSampleFormat::IEEE_F32,std::nullopt,-13.0));
}

void M18CandidateDeliveryValidationTest::
tamperedPayloadAndWrongMasterRejected()
{
    using platform::windows::internal::validate_m18_candidate_decoded_delivery;
    const auto original=master(audio::ChannelLayout::STEREO_LR,4105);
    QVERIFY(original);
    constexpr auto seed=UINT64_C(0x123456789ABCDEF0);
    auto bytes=render(original->view(),audio::WavSampleFormat::PCM_S24,seed);
    const auto header=audio::internal::m18_plan_wav({
        audio::internal::M18WavEncoding::PCM24,48000U,2U,4105U,seed});
    QCOMPARE(header.status,audio::internal::M18WavStatus::OK);
    const auto tamper=static_cast<std::size_t>(header.plan.header_bytes)
        +(4101U*2U+1U)*3U;
    QVERIFY(tamper<bytes.size());
    bytes[tamper]^=std::byte{0x01};
    auto wav=open(bytes,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(wav);
    const auto checked=validate_m18_candidate_decoded_delivery(
        *wav.value()->get(),original->view(),
        audio::WavSampleFormat::PCM_S24,seed,0.0);
    QVERIFY(!checked);
    QCOMPARE(checked.error()->code(),core::ErrorCode::IoFailure);
    const auto clean=render(original->view(),
        audio::WavSampleFormat::PCM_S24,seed);
    auto again=open(clean,std::make_shared<wav_support::ReaderControl>());
    QVERIFY(again);
    const auto wrong=master(audio::ChannelLayout::MONO_C,4105);
    QVERIFY(wrong);
    const auto rejected=validate_m18_candidate_decoded_delivery(
        *again.value()->get(),wrong->view(),
        audio::WavSampleFormat::PCM_S24,seed,0.0);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(),
        core::ErrorCode::MalformedAudioContainer);
}

void M18CandidateDeliveryValidationTest::
realWindowsM18TransactionalReadback()
{
    using namespace platform::windows;
    for(const auto encoding:{audio::WavSampleFormat::PCM_S24,
                             audio::WavSampleFormat::IEEE_F32}){
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto src=dir.filePath(QStringLiteral("source.bin"));
        QFile input{src};
        QVERIFY(input.open(QIODevice::WriteOnly|QIODevice::NewOnly));
        QCOMPARE(input.write("owner",5),qint64{5});
        input.close();
        const auto dst=dir.filePath(QStringLiteral("master.wav"));
        const auto pcm=master(audio::ChannelLayout::STEREO_LR,4105);
        QVERIFY(pcm);
        auto state=std::make_shared<TestSessionState>();
        const auto seed=encoding==audio::WavSampleFormat::PCM_S24
            ?std::optional<std::uint64_t>{UINT64_C(0xFEDCBA9876543210)}
            :std::nullopt;
        const auto request=make_real_transaction_request(
            src,dst,pcm,encoding,seed,-1.0,state);
        QVERIFY(request);
        const auto saved=TransactionalAudioExporter::export_new_file(
            *request.value());
        QVERIFY(saved);
        QCOMPARE(saved.value()->sample_format(),encoding);
        QCOMPARE(saved.value()->frame_count().value(),std::int64_t{4105});
        QCOMPARE(saved.value()->encoded_file_sha256().size(),std::size_t{64});
        QCOMPARE(saved.value()->decoded_audio_sha256().size(),std::size_t{64});
        QCOMPARE(static_cast<std::uint64_t>(QFileInfo{dst}.size()),
            saved.value()->exact_file_size_bytes());
        QFile unchanged{src};QVERIFY(unchanged.open(QIODevice::ReadOnly));
        QCOMPARE(unchanged.readAll(),QByteArray("owner",5));
        unchanged.close();
        QCOMPARE(state->acquired,std::size_t{1});
        QCOMPARE(state->revalidated,std::size_t{2});
        QCOMPARE(state->retired,std::size_t{1});
        QVERIFY(m18_no_temporary_candidates(dir.path()));
        const auto rr=WindowsResourceReader::make_read_reference(
            m18_path(dst),m18_path(QFileInfo{dst}.fileName()));
        QVERIFY(rr);
        auto bytes=WindowsResourceReader::open_read_only(*rr.value());
        QVERIFY(bytes);
        auto opened=audio::WavReader::open(std::move(*bytes.value()));
        QVERIFY(opened);
        QCOMPARE(opened.value()->get()->info().encoded_sample_format(),encoding);
        QCOMPARE(opened.value()->get()->info().frame_count().value(),
            std::int64_t{4105});
        const auto blocked=TransactionalAudioExporter::export_new_file(
            *request.value());
        QVERIFY(!blocked);
        QCOMPARE(blocked.error()->code(),core::ErrorCode::InvalidState);
        QCOMPARE(state->acquired,std::size_t{1});
        QVERIFY(m18_no_temporary_candidates(dir.path()));
    }
}

void M18CandidateDeliveryValidationTest::
realWindowsM18RollbackAndEpochGate()
{
    using namespace platform::windows;
    using internal::ExportFailurePoint;
    using internal::set_export_failure_point_for_test;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=dir.filePath(QStringLiteral("source.bin"));
    QFile input{src};QVERIFY(input.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(input.write("source",6),qint64{6});
    input.close();
    const auto pcm=master(audio::ChannelLayout::MONO_C,4105);
    QVERIFY(pcm);
    auto state=std::make_shared<TestSessionState>();
    const auto rejectedPath=dir.filePath(QStringLiteral("nonconforming.wav"));
    const auto over=make_real_transaction_request(
        src,rejectedPath,pcm,audio::WavSampleFormat::PCM_S24,
        42U,-12.0,state);
    QVERIFY(over);
    const auto failed=TransactionalAudioExporter::export_new_file(
        *over.value());
    QVERIFY(!failed);
    // If a lower-level container/codec error wins before the ceiling
    // check, report the EXACT underlying reason instead of a blind FAIL.
    const auto reason=failed.error()->message();
    QCOMPARE(failed.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY2(reason.find("nonconforming")!=std::string::npos,
             reason.c_str());
    QVERIFY(!QFileInfo::exists(rejectedPath));
    QCOMPARE(state->acquired,std::size_t{0});
    QVERIFY(m18_no_temporary_candidates(dir.path()));

    const auto expiredPath=dir.filePath(QStringLiteral("expired.wav"));
    const auto expired=make_real_transaction_request(
        src,expiredPath,pcm,audio::WavSampleFormat::IEEE_F32,
        std::nullopt,-1.0,state);
    QVERIFY(expired);
    state->current=false;
    const auto denied=TransactionalAudioExporter::export_new_file(
        *expired.value());
    QVERIFY(!denied);
    QVERIFY(!QFileInfo::exists(expiredPath));
    QCOMPARE(state->acquired,std::size_t{1});
    QCOMPARE(state->revalidated,std::size_t{1});
    QCOMPARE(state->retired,std::size_t{1});
    QVERIFY(m18_no_temporary_candidates(dir.path()));

    state->current=true;
    const auto commitPath=dir.filePath(QStringLiteral("commit-failure.wav"));
    const auto commit=make_real_transaction_request(
        src,commitPath,pcm,audio::WavSampleFormat::IEEE_F32,
        std::nullopt,-1.0,state);
    QVERIFY(commit);
    set_export_failure_point_for_test(ExportFailurePoint::COMMIT);
    const auto sabotaged=TransactionalAudioExporter::export_new_file(
        *commit.value());
    set_export_failure_point_for_test(ExportFailurePoint::NONE);
    QVERIFY(!sabotaged);
    QVERIFY(!QFileInfo::exists(commitPath));
    QVERIFY(m18_no_temporary_candidates(dir.path()));

    // Neither missing seed nor missing commit lease can create a file.
    const auto r=WindowsResourceReader::make_read_reference(
        m18_path(src),m18_path(QFileInfo{src}.fileName()));
    const auto w=WindowsResourceWriter::make_write_reference(
        m18_path(commitPath),m18_path(QFileInfo{commitPath}.fileName()));
    QVERIFY(r &&w);
    QVERIFY(!M18TransactionalAudioExportRequest::create(
        *r.value(),pcm,*w.value(),audio::WavSampleFormat::PCM_S24,
        std::nullopt,-1.0,ExportCollisionPolicy::FAIL_IF_EXISTS,
        make_test_commit_authority(state)));
    QVERIFY(!M18TransactionalAudioExportRequest::create(
        *r.value(),pcm,*w.value(),audio::WavSampleFormat::IEEE_F32,
        std::nullopt,-1.0,ExportCollisionPolicy::FAIL_IF_EXISTS,{}));
    QVERIFY(!QFileInfo::exists(commitPath));
}


void M18CandidateDeliveryValidationTest::
realWindowsSixRatesBothLayoutsBothEncodingsOddEven()
{
    // Complete delivery filesystem matrix: ALL supported Master output
    // rates, mono/stereo, PCM24/Float32 and both payload parities.
    // Use the original Windows create-new transaction, not memory mocks.
    using namespace platform::windows;
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto sourcePath=dir.filePath(QStringLiteral("source.bin"));
    QFile source{sourcePath};
    QVERIFY(source.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(source.write("M18",3),qint64{3});
    source.close();
    int exercised=0;
    for(const auto fs:rates) {
        for(const auto layout:{audio::ChannelLayout::MONO_C,
                               audio::ChannelLayout::STEREO_LR}) {
            for(const auto encoding:{audio::WavSampleFormat::PCM_S24,
                                     audio::WavSampleFormat::IEEE_F32}) {
                for(const std::int64_t frames:{4104,4105}) {
                    auto sourceOwner=master(layout,frames,fs);
                    QVERIFY(sourceOwner);
                    const auto seed=encoding==audio::WavSampleFormat::PCM_S24
                        ?std::optional<std::uint64_t>{
                            UINT64_C(0xF0E1D2C3B4A59687)}
                        :std::nullopt;
                    const auto destination=dir.filePath(
                        QStringLiteral("m18-%1-%2-%3-%4.wav")
                            .arg(fs).arg(static_cast<int>(layout))
                            .arg(static_cast<int>(encoding)).arg(frames));
                    const auto permission=std::make_shared<TestSessionState>();
                    auto requested=make_real_transaction_request(
                        sourcePath,destination,sourceOwner,encoding,seed,
                        0.0,permission);
                    QVERIFY(requested);
                    const auto exported=TransactionalAudioExporter::export_new_file(
                        *requested.value());
                    QVERIFY2(exported,exported
                        ?"":exported.error()->message().c_str());
                    QCOMPARE(exported.value()->sample_format(),encoding);
                    QCOMPARE(exported.value()->frame_count().value(),frames);
                    QCOMPARE(exported.value()->audio_format(),
                             sourceOwner->view().format());
                    QCOMPARE(exported.value()->encoded_file_sha256().size(),
                             std::size_t{64});
                    QCOMPARE(exported.value()->decoded_audio_sha256().size(),
                             std::size_t{64});
                    QCOMPARE(static_cast<std::uint64_t>(
                             QFileInfo{destination}.size()),
                             exported.value()->exact_file_size_bytes());
                    auto rr=WindowsResourceReader::make_read_reference(
                        m18_path(destination),
                        m18_path(QFileInfo{destination}.fileName()));
                    QVERIFY(rr);
                    auto bytes=WindowsResourceReader::open_read_only(*rr.value());
                    QVERIFY(bytes);
                    auto decoded=audio::WavReader::open(
                        std::move(*bytes.value()));
                    QVERIFY(decoded);
                    const auto& info=decoded.value()->get()->info();
                    QCOMPARE(info.encoded_sample_format(),encoding);
                    QCOMPARE(info.frame_count().value(),frames);
                    QCOMPARE(info.audio_format(),sourceOwner->view().format());
                    const std::uint64_t sampleBytes=
                        encoding==audio::WavSampleFormat::PCM_S24?3U:4U;
                    const auto logicalBytes=
                        static_cast<std::uint64_t>(frames)
                        *sourceOwner->view().format().channel_count()
                        *sampleBytes;
                    QCOMPARE(info.data_size_bytes(),logicalBytes);
                    QCOMPARE(permission->acquired,std::size_t{1});
                    QCOMPARE(permission->revalidated,std::size_t{2});
                    QCOMPARE(permission->retired,std::size_t{1});
                    QVERIFY(m18_no_temporary_candidates(dir.path()));
                    ++exercised;
                }
            }
        }
    }
    QCOMPARE(exercised,48);
}


void M18CandidateDeliveryValidationTest::
realWindowsFailureInjectionCleansUpBothFormats()
{
    // M18 must preserve legacy Windows TransactionalAudioExporter failure
    // atomicity BEFORE and AFTER the rename. No QML/product activation.
    using namespace platform::windows;
    using internal::ExportFailurePoint;
    using internal::set_export_failure_point_for_test;
    constexpr std::array<ExportFailurePoint,7> injected{
        ExportFailurePoint::CANDIDATE_CREATE,
        ExportFailurePoint::WRITER_IO,
        ExportFailurePoint::VALIDATION,
        ExportFailurePoint::ENCODED_CHECKSUM,
        ExportFailurePoint::DECODED_CHECKSUM,
        ExportFailurePoint::COMMIT,
        ExportFailurePoint::DESTINATION_READBACK};
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto sourcePath=dir.filePath(QStringLiteral("original-source.bin"));
    QFile input{sourcePath};
    QVERIFY(input.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    QCOMPARE(input.write("source-unchanged",16),qint64{16});
    input.close();
    int cases=0;
    for(const auto encoding:{audio::WavSampleFormat::PCM_S24,
                             audio::WavSampleFormat::IEEE_F32}) {
        const auto pcm=master(audio::ChannelLayout::MONO_C,4105);
        QVERIFY(pcm);
        const auto seed=encoding==audio::WavSampleFormat::PCM_S24
            ?std::optional<std::uint64_t>{
                UINT64_C(0xE5F0ABCDEF012345)}:std::nullopt;
        for(const auto fail:injected) {
            auto state=std::make_shared<TestSessionState>();
            const auto output=dir.filePath(
                QStringLiteral("denied-%1-%2.wav")
                    .arg(static_cast<int>(encoding))
                    .arg(static_cast<int>(fail)));
            auto request=make_real_transaction_request(
                sourcePath,output,pcm,encoding,seed,0.0,state);
            QVERIFY(request);
            set_export_failure_point_for_test(fail);
            const auto rejected=TransactionalAudioExporter::export_new_file(
                *request.value());
            // Always reset failure injection BEFORE any assertions.
            set_export_failure_point_for_test(ExportFailurePoint::NONE);
            QVERIFY2(!rejected,
                rejected?"unexpected accepted M18 fault":rejected.error()->message().c_str());
            QVERIFY(!QFileInfo::exists(output));
            QVERIFY(m18_no_temporary_candidates(dir.path()));
            const auto shouldHaveLease=
                fail==ExportFailurePoint::COMMIT
                ||fail==ExportFailurePoint::DESTINATION_READBACK;
            const auto expected=shouldHaveLease?std::size_t{1}:std::size_t{0};
            QCOMPARE(state->acquired,expected);
            QCOMPARE(state->revalidated,expected);
            QCOMPARE(state->retired,expected);
            QFile unchanged{sourcePath};
            QVERIFY(unchanged.open(QIODevice::ReadOnly));
            QCOMPARE(unchanged.readAll(),QByteArray("source-unchanged",16));
            unchanged.close();
            ++cases;
        }
    }
    QCOMPARE(cases,14);
}


void M18CandidateDeliveryValidationTest::
exclusiveProductProjectMutationBlocksWorkerCommitUntilComplete()
{
    using platform::windows::internal::M18EpochCommitBarrier;
    M18EpochCommitBarrier authority{51U};
    auto batched=authority.try_begin_mutation(51U,52U);
    QVERIFY(batched);
    QCOMPARE((*batched.value())->epoch(),std::uint64_t{52U});
    QCOMPARE(authority.epoch_snapshot(),std::uint64_t{52U});

    // The newly advanced epoch does not authorize a worker until the
    // entire SOURCE/Project/chain/MASTER transaction has finished.
    std::promise<bool> deniedPromise;
    auto deniedFuture=deniedPromise.get_future();
    std::thread worker([&] {
        const auto denied=authority.try_acquire(52U);
        deniedPromise.set_value(!denied
            &&denied.error()->code()==core::ErrorCode::InvalidState);
    });
    const bool workerDenied=deniedFuture.get();
    worker.join();
    QVERIFY(workerDenied);
    QVERIFY(!authority.try_acquire(51U)); // stale pre-project ticket
    QVERIFY(!authority.try_begin_mutation(52U,53U));
    QVERIFY(!authority.try_advance_epoch(52U,53U));
    QVERIFY(!authority.try_retire(52U));

    // No spin, sleep, event-loop block, or QObject access. Releasing the
    // Product-owned batch makes ONLY the new epoch eligible.
    batched.value()->reset();
    QVERIFY(!authority.try_acquire(51U));
    auto newer=authority.try_acquire(52U);
    QVERIFY(newer);
    QVERIFY((*newer.value())->revalidate_current());
    QVERIFY(!authority.try_begin_mutation(52U,53U));
    newer.value()->reset();
    auto next=authority.try_begin_mutation(52U,53U);
    QVERIFY(next);
    next.value()->reset();
    QVERIFY(authority.try_retire(53U));
    QVERIFY(!authority.try_acquire(53U));
}

void M18CandidateDeliveryValidationTest::
crossThreadEpochBarrierPreventsCommitRaceWithoutQtBlocking()
{
    // A real worker holds the commit permit while the Qt owner attempts
    // to advance/retire. Both fail immediately instead of blocking Qt.
    // This standalone primitive is NOT wired into Product mutations yet.
    using platform::windows::internal::M18EpochCommitBarrier;
    auto barrier=std::make_shared<M18EpochCommitBarrier>(77U);
    QVERIFY(!barrier->try_acquire(76U));
    QCOMPARE(barrier->epoch_snapshot(),std::uint64_t{77});
    std::promise<bool> admitted;
    auto admittedFuture=admitted.get_future();
    std::promise<void> release;
    auto releaseFuture=release.get_future();
    bool workerLeaseStillCurrent=false;
    std::thread worker([&,future=std::move(releaseFuture)]() mutable {
        auto held=barrier->try_acquire(77U);
        const bool ok=static_cast<bool>(held);
        admitted.set_value(ok);
        if(!held)return;
        future.wait();
        workerLeaseStillCurrent=static_cast<bool>(
            (*held.value())->revalidate_current());
        held.value()->reset();
    });
    const bool workerAdmitted=admittedFuture.get();
    const auto blockedSecond=barrier->try_acquire(77U);
    const auto blockedChange=barrier->try_advance_epoch(77U,78U);
    const auto blockedRetire=barrier->try_retire(77U);
    // Unblock/join unconditionally BEFORE any QVERIFY/QCOMPARE return.
    release.set_value();
    worker.join();
    QVERIFY(workerAdmitted);
    QVERIFY(workerLeaseStillCurrent);
    QVERIFY(!blockedSecond);
    QVERIFY(!blockedChange);
    QVERIFY(!blockedRetire);
    QCOMPARE(blockedSecond.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(blockedChange.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(blockedRetire.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(barrier->epoch_snapshot(),std::uint64_t{77});
    // A Source change may now advance. Stale tickets must fail closed.
    QVERIFY(barrier->try_advance_epoch(77U,78U));
    QCOMPARE(barrier->epoch_snapshot(),std::uint64_t{78});
    QVERIFY(!barrier->try_acquire(77U));
    QVERIFY(!barrier->try_advance_epoch(77U,79U));
    auto current=barrier->try_acquire(78U);
    QVERIFY(current);
    QVERIFY((*current.value())->revalidate_current());
    QVERIFY(!barrier->try_retire(78U));
    current.value()->reset();
    QVERIFY(barrier->try_retire(78U));
    QVERIFY(!barrier->try_acquire(78U));
    QVERIFY(!barrier->try_advance_epoch(78U,79U));
}


void M18CandidateDeliveryValidationTest::
productMutationBatchPinsIdentityAndForbidsPartialCapture()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    const M18ProductExportIdentity before{11U,12U,13U,0U,192000U};
    M18ProductEpochTicketAuthority owner{77U,before};
    QVERIFY(owner.try_transition(77U,78U,before,true));
    auto oldTicket=owner.capture(before);
    QVERIFY(oldTicket);
    const auto previousFactory=oldTicket.value()->commit_lease_factory();
    const auto changed=[&] {
        auto next=before;
        next.project_generation=99U;
        next.master_generation=101U;
        return next;
    }();
    // An active worker commit has priority: the owner must refuse the
    // proposed Project replacement without changing any Product identity.
    auto oldCommit=previousFactory();
    QVERIFY(oldCommit);
    auto deniedBatch=owner.try_begin_product_mutation(
        78U,79U,changed,false);
    QVERIFY(!deniedBatch);
    QVERIFY((*oldCommit.value())->revalidate_current());
    oldCommit.value()->reset();
    QVERIFY(owner.capture(before));
    // No Product batch may directly carry export consent, even if its
    // caller claims that the newly loaded Project already has permission.
    QVERIFY(!owner.try_begin_product_mutation(
        78U,79U,changed,true));
    QVERIFY(owner.capture(before));

    // An authorized Project batch advances the monotonic identity once,
    // while holding exclusive protection across ALL downstream mutations.
    auto batch=owner.try_begin_product_mutation(78U,79U,changed,false);
    QVERIFY(batch);
    QCOMPARE((*batch.value())->epoch(),std::uint64_t{79U});
    QVERIFY(!previousFactory());
    QVERIFY(!owner.capture(before));
    QVERIFY(!owner.capture(changed)); // the new project is still incomplete
    QVERIFY(!owner.try_transition(79U,80U,changed,true));
    QVERIFY(!owner.try_begin_product_mutation(79U,80U,changed,false));
    batch.value()->reset();
    // An aborted Project open cannot restore export consent on RAII
    // release. Even a valid-looking new identity requires fresh Product
    // verification plus an explicit separate consent transition.
    QVERIFY(!previousFactory());
    QVERIFY(!owner.capture(changed));
    QVERIFY(owner.try_transition(79U,80U,changed,true));
    auto now=owner.capture(changed);
    QVERIFY(now);
    auto newLease=now.value()->commit_lease_factory()();
    QVERIFY(newLease);
    QVERIFY((*newLease.value())->revalidate_current());
    QVERIFY(!owner.try_begin_product_mutation(80U,81U,changed,false));
    newLease.value()->reset();
    QVERIFY(owner.try_transition(80U,81U,changed,false));
    QVERIFY(!now.value()->commit_lease_factory()());
}

void M18CandidateDeliveryValidationTest::
productDestroyedDuringProjectMutationCannotReviveWorkerExport()
{
    // The Qt Product owner may be destroyed while a Project-open batch
    // token still lives in an early-return cleanup path. A retained worker
    // factory must fail both before and after that token is released.
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    const M18ProductExportIdentity initial{31U,32U,33U,0U,44100U};
    auto owner=std::make_unique<M18ProductEpochTicketAuthority>(
        201U,initial);
    QVERIFY(owner->try_transition(201U,202U,initial,true));
    auto prior=owner->capture(initial);
    QVERIFY(prior);
    const auto factory=prior.value()->commit_lease_factory();
    QVERIFY(factory()); // prove Product consent was actually live
    auto replacement=initial;
    replacement.project_generation=92U;
    // Recheck that the transient lease from above was released before
    // the new exclusive Project mutation begins.
    auto batch=owner->try_begin_product_mutation(
        202U,203U,replacement,false);
    QVERIFY(batch);
    QVERIFY(!factory());
    auto held=std::move(*batch.value());
    owner.reset(); // Product retirement is nonblocking and fail-closed.
    QVERIFY(!factory());
    held.reset(); // Still never grants old Product/epoch credentials.
    QVERIFY(!factory());
}

void M18CandidateDeliveryValidationTest::
productEpochTicketConsentAndMasterIdentityAreWorkerSafe()
{
    // PREPRODUCTION admission contract: no QObjects, no Product hook, no
    // filesystem publication. Simulate Product-owner consent and mutations.
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    // The initial native ProcessingChain revision is zero by design;
    // only the explicitly minted Source/Project/MASTER generations and
    // Product epoch require nonzero values.
    const M18ProductExportIdentity id{11U,12U,13U,0U,48000U};
    QVERIFY(id.valid());
    // Exactly the M17 frozen Final SRC family may be present in a
    // Product-issued M18 ticket; no DEVICE-only/unsupported rate.
    for(const auto rate:{
            44100U,48000U,88200U,96000U,176400U,192000U}) {
        auto validRate=id;
        validRate.master_rate_hz=rate;
        QVERIFY(validRate.valid());
    }
    for(const auto rate:{0U,8000U,32000U,44101U,384000U}) {
        auto invalidRate=id;
        invalidRate.master_rate_hz=rate;
        QVERIFY(!invalidRate.valid());
    }
    auto missingSource=id;missingSource.source_generation=0;
    auto missingProject=id;missingProject.project_generation=0;
    auto missingMaster=id;missingMaster.master_generation=0;
    QVERIFY(!missingSource.valid());
    QVERIFY(!missingProject.valid());
    QVERIFY(!missingMaster.valid());
    // Monotonic 64-bit epoch IDs must never wrap and accidentally accept
    // an ancient ticket as the current Product after a UINT64_MAX edge.
    M18ProductEpochTicketAuthority nearMax{
        std::numeric_limits<std::uint64_t>::max()-1U,id};
    const auto lastEpoch=std::numeric_limits<std::uint64_t>::max();
    QVERIFY(nearMax.try_transition(lastEpoch-1U,lastEpoch,id,true));
    auto finalTicket=nearMax.capture(id);
    QVERIFY(finalTicket);
    QVERIFY(!nearMax.try_transition(lastEpoch,0U,id,false));
    QVERIFY(!nearMax.try_transition(lastEpoch,lastEpoch,id,false));
    QVERIFY(!nearMax.try_begin_product_mutation(
        lastEpoch,0U,id,false));
    QVERIFY(nearMax.capture(id));
    M18ProductEpochTicketAuthority authority{200U,id};
    QVERIFY(!authority.capture(id)); // No implicit user consent.
    QVERIFY(!authority.try_transition(200U,200U,id,true));
    QVERIFY(authority.try_transition(200U,201U,id,true));
    auto captured=authority.capture(id);
    QVERIFY(captured);
    // Matching Fs and DSP revision do not compensate for a different
    // Source/Project/Master generation: every identity component is exact.
    auto forgedProject=id;++forgedProject.project_generation;
    auto forgedMaster=id;++forgedMaster.master_generation;
    auto forgedSource=id;++forgedSource.source_generation;
    QVERIFY(!authority.capture(forgedProject));
    QVERIFY(!authority.capture(forgedMaster));
    QVERIFY(!authority.capture(forgedSource));
    auto invalidNext=id;invalidNext.source_generation=0;
    QVERIFY(!authority.try_transition(201U,202U,invalidNext,true));
    QVERIFY(authority.capture(id)); // rejected proposal is non-mutating
    const auto factory=captured.value()->commit_lease_factory();

    // The worker is allowed to acquire ONLY an immutable factory lease.
    // Minting consent, advancing generations or capturing a new ticket
    // from an alien thread must be rejected even if the epoch is current.
    bool deniedAllOwnerOperations=false;
    std::thread foreignOwner([&] {
        const auto forbiddenCapture=authority.capture(id);
        const auto forbiddenTransition=authority.try_transition(
            201U,202U,id,false);
        const auto forbiddenBatch=authority.try_begin_product_mutation(
            201U,202U,id,false);
        deniedAllOwnerOperations=!forbiddenCapture
            &&!forbiddenTransition &&!forbiddenBatch;
    });
    foreignOwner.join();
    QVERIFY(deniedAllOwnerOperations);
    QVERIFY(authority.capture(id)); // no epoch change or lost consent

    std::promise<bool> admission;
    auto ready=admission.get_future();
    std::promise<void> release;
    auto go=release.get_future();
    bool leaseRemainsCurrent=false;
    std::thread worker([factory,&admission,&leaseRemainsCurrent,
                        done=std::move(go)]() mutable {
        auto held=factory();
        admission.set_value(static_cast<bool>(held));
        if(!held)return;
        done.wait();
        leaseRemainsCurrent=static_cast<bool>(
            (*held.value())->revalidate_current());
        held.value()->reset();
    });
    const bool admitted=ready.get();
    auto switched=id;
    switched.source_generation=33U;
    // Real Product MUST refuse/defer any such mutation while held.
    const auto blockedSource=authority.try_transition(
        201U,202U,switched,false);
    const auto blockedConsent=authority.try_transition(
        201U,202U,id,false);
    const auto blockedSecond=factory();
    release.set_value();
    worker.join(); // Always join BEFORE assertions can return early.
    QVERIFY(admitted);
    QVERIFY(leaseRemainsCurrent);
    QVERIFY(!blockedSource);
    QVERIFY(!blockedConsent);
    QVERIFY(!blockedSecond);
    QCOMPARE(blockedSource.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(blockedConsent.error()->code(),core::ErrorCode::InvalidState);

    // After the commit is fully released, a Source revision revokes the
    // old ticket, even if the old Master buffer was retained elsewhere.
    QVERIFY(authority.try_transition(201U,202U,switched,false));
    QVERIFY(!factory());
    QVERIFY(!authority.capture(switched));
    QVERIFY(!authority.capture(id));
    auto wrongRate=switched;
    wrongRate.master_rate_hz=44100U;
    QVERIFY(!authority.capture(wrongRate));
    // A fresh, explicit consent and Product identity may mint a new ticket.
    QVERIFY(authority.try_transition(202U,203U,wrongRate,true));
    auto newest=authority.capture(wrongRate);
    QVERIFY(newest);
    QVERIFY(!authority.capture(switched));
    auto newLease=newest.value()->commit_lease_factory()();
    QVERIFY(newLease);
    QVERIFY((*newLease.value())->revalidate_current());
    newLease.value()->reset();
    QVERIFY(authority.try_transition(203U,204U,wrongRate,false));
    QVERIFY(!newest.value()->commit_lease_factory()());

    // Destroying the Product authority is NOT an implicit worker grant.
    // A retained ticket and even an already-held permit must fail closed.
    auto dying=std::make_unique<M18ProductEpochTicketAuthority>(300U,id);
    QVERIFY(dying->try_transition(300U,301U,id,true));
    auto orphan=dying->capture(id);
    QVERIFY(orphan);
    const auto orphanFactory=orphan.value()->commit_lease_factory();
    auto orphanLease=orphanFactory();
    QVERIFY(orphanLease);
    dying.reset();
    QVERIFY(!(*orphanLease.value())->revalidate_current());
    orphanLease.value()->reset();
    QVERIFY(!orphanFactory());
}


void M18CandidateDeliveryValidationTest::
realWindowsExportDeniedAfterAbortedProductProjectBatch()
{
    // End-to-end negative case: an exclusive Product Project-open batch
    // may have already replaced SOURCE/MASTER before it fails. Releasing
    // that batch must NOT reauthorize an old ticket or automatically
    // authorize the new partially restored Project to write a WAV file.
    using namespace platform::windows;
    using internal::M18ProductEpochTicketAuthority;
    using internal::M18ProductExportIdentity;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=dir.filePath(QStringLiteral("original-source.bin"));
    QFile input{src};
    QVERIFY(input.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(input.write("unchanged",9),qint64{9});
    input.close();
    const auto srcRef=WindowsResourceReader::make_read_reference(
        m18_path(src),m18_path(QFileInfo{src}.fileName()));
    QVERIFY(srcRef);
    const auto pcm=master(audio::ChannelLayout::MONO_C,4105,48000);
    QVERIFY(pcm);
    const M18ProductExportIdentity oldId{61U,62U,63U,0U,48000U};
    M18ProductEpochTicketAuthority owner{101U,oldId};
    QVERIFY(owner.try_transition(101U,102U,oldId,true));
    auto oldTicket=owner.capture(oldId);
    QVERIFY(oldTicket);
    const auto priorFactory=oldTicket.value()->commit_lease_factory();

    auto newProjectId=oldId;
    newProjectId.source_generation=71U;
    newProjectId.project_generation=72U;
    newProjectId.master_generation=73U;
    auto projectBatch=owner.try_begin_product_mutation(
        102U,103U,newProjectId,false);
    QVERIFY(projectBatch);
    QVERIFY(!priorFactory());
    QVERIFY(!owner.capture(newProjectId));
    projectBatch.value()->reset();  // Simulated failed/aborted open.
    QVERIFY(!priorFactory());
    QVERIFY(!owner.capture(newProjectId));

    for(const auto format:{
            audio::WavSampleFormat::PCM_S24,
            audio::WavSampleFormat::IEEE_F32}) {
        // Both output encodings must hit the SAME fail-closed Product
        // lease revocation, even when the WAV samples were encoded.
        const auto dest=dir.filePath(QStringLiteral("aborted-project-%1.wav")
            .arg(static_cast<int>(format)));
        const auto dstRef=WindowsResourceWriter::make_write_reference(
            m18_path(dest),m18_path(QFileInfo{dest}.fileName()));
        QVERIFY(dstRef);
        const auto seed=format==audio::WavSampleFormat::PCM_S24
            ?std::optional<std::uint64_t>{UINT64_C(0xBADC0FFEE0DDF00D)}
            :std::nullopt;
        auto deniedRequest=M18TransactionalAudioExportRequest::create(
            *srcRef.value(),pcm,*dstRef.value(),
            format,seed,0.0,
            ExportCollisionPolicy::FAIL_IF_EXISTS,priorFactory);
        QVERIFY(deniedRequest);
        const auto rejected=TransactionalAudioExporter::export_new_file(
            *deniedRequest.value());
        QVERIFY(!rejected);
        QCOMPARE(rejected.error()->code(),core::ErrorCode::InvalidState);
        QVERIFY(!QFileInfo::exists(dest));
        QVERIFY(m18_no_temporary_candidates(dir.path()));
    }
    QFile check{src};
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(),QByteArray("unchanged",9));
    // Only a later checked M17 MASTER plus explicit new consent MAY
    // advance the Product epoch and mint an altogether new ticket.
    QVERIFY(owner.try_transition(103U,104U,newProjectId,true));
    QVERIFY(owner.capture(newProjectId));
}

void M18CandidateDeliveryValidationTest::
realWorkerTicketExportsOnlyWithCurrentConsentAndSource()
{
    // Real Windows create-new/readback, but synthetic Product-issued
    // identity: Product mutators are NOT wired into this prototype.
    using namespace platform::windows;
    using internal::M18ProductEpochTicketAuthority;
    using internal::M18ProductExportIdentity;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto original=dir.filePath(QStringLiteral("worker-source.bin"));
    QFile source{original};
    QVERIFY(source.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(source.write("source-unchanged",16),qint64{16});
    source.close();
    const auto pcm=master(audio::ChannelLayout::MONO_C,4105,44100);
    QVERIFY(pcm);
    const M18ProductExportIdentity id{100U,101U,102U,103U,44100U};
    M18ProductEpochTicketAuthority owner{900U,id};
    QVERIFY(owner.try_transition(900U,901U,id,true));
    auto permittedTicket=owner.capture(id);
    QVERIFY(permittedTicket);
    auto sourceRef=WindowsResourceReader::make_read_reference(
        m18_path(original),m18_path(QFileInfo{original}.fileName()));
    QVERIFY(sourceRef);
    const auto validPath=dir.filePath(QStringLiteral("worker-real.wav"));
    const auto destRef=WindowsResourceWriter::make_write_reference(
        m18_path(validPath),m18_path(QFileInfo{validPath}.fileName()));
    QVERIFY(destRef);
    const auto seed=std::optional<std::uint64_t>{
        UINT64_C(0xEABCBAD981234567)};
    auto request=M18TransactionalAudioExportRequest::create(
        *sourceRef.value(),pcm,*destRef.value(),
        audio::WavSampleFormat::PCM_S24,seed,0.0,
        ExportCollisionPolicy::FAIL_IF_EXISTS,
        permittedTicket.value()->commit_lease_factory());
    QVERIFY(request);
    // Transfer a COPY of the immutable request (no QObject) to the worker.
    // Product's owning thread never blocks on the WAV algorithm.
    bool workerPassed=false;
    std::thread worker([&]{
        const auto delivered=TransactionalAudioExporter::export_new_file(
            *request.value());
        workerPassed=static_cast<bool>(delivered)
            &&delivered.value()->frame_count().value()==4105
            &&delivered.value()->encoded_file_sha256().size()==64U
            &&delivered.value()->decoded_audio_sha256().size()==64U;
    });
    worker.join();
    QVERIFY(workerPassed);
    QVERIFY(QFileInfo::exists(validPath));
    QVERIFY(m18_no_temporary_candidates(dir.path()));

    // A stale ticket cannot publish a second file, even with retained PCM.
    auto switched=id;
    switched.project_generation=201U;
    QVERIFY(owner.try_transition(901U,902U,switched,false));
    const auto deniedPath=dir.filePath(QStringLiteral("revoked.wav"));
    const auto deniedDest=WindowsResourceWriter::make_write_reference(
        m18_path(deniedPath),m18_path(QFileInfo{deniedPath}.fileName()));
    QVERIFY(deniedDest);
    auto expired=M18TransactionalAudioExportRequest::create(
        *sourceRef.value(),pcm,*deniedDest.value(),
        audio::WavSampleFormat::PCM_S24,seed,0.0,
        ExportCollisionPolicy::FAIL_IF_EXISTS,
        permittedTicket.value()->commit_lease_factory());
    QVERIFY(expired);
    bool rejectedAsExpected=false;
    std::thread revoked([&] {
        const auto result=TransactionalAudioExporter::export_new_file(
            *expired.value());
        rejectedAsExpected=!result &&result.error()->code()
            ==core::ErrorCode::InvalidState;
    });
    revoked.join();
    QVERIFY(rejectedAsExpected);
    QVERIFY(!QFileInfo::exists(deniedPath));
    QVERIFY(m18_no_temporary_candidates(dir.path()));
    QFile readOriginal{original};QVERIFY(readOriginal.open(QIODevice::ReadOnly));
    QCOMPARE(readOriginal.readAll(),QByteArray("source-unchanged",16));
    readOriginal.close();

    // Physical delivery was already complete; revocation does not
    // retroactively remove the user's previously verified WAV file.
    QVERIFY(QFileInfo::exists(validPath));
}


void M18CandidateDeliveryValidationTest::
realWindowsFinalAuthorityRevocationRollsBackBothFormats()
{
    // The authority may be retired after the first pre-rename check but
    // during final readback. M18 MUST delete the freshly committed file
    // instead of returning VERIFIED on a revoked session.
    using namespace platform::windows;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=dir.filePath(QStringLiteral("source.bin"));
    QFile original{src};
    QVERIFY(original.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(original.write("unchanged",9),qint64{9});
    original.close();
    for(const auto format:{audio::WavSampleFormat::PCM_S24,
                           audio::WavSampleFormat::IEEE_F32}) {
        const auto pcm=master(audio::ChannelLayout::MONO_C,4105);
        QVERIFY(pcm);
        auto state=std::make_shared<TestSessionState>();
        state->rejectOnRevalidationCall=2;
        const auto dest=dir.filePath(QStringLiteral("late-revoked-%1.wav")
            .arg(static_cast<int>(format)));
        const auto seed=format==audio::WavSampleFormat::PCM_S24
            ?std::optional<std::uint64_t>{UINT64_C(0xFFFFFFFF00000001)}
            :std::nullopt;
        const auto request=make_real_transaction_request(
            src,dest,pcm,format,seed,0.0,state);
        QVERIFY(request);
        const auto rejected=TransactionalAudioExporter::export_new_file(
            *request.value());
        QVERIFY(!rejected);
        QCOMPARE(rejected.error()->code(),core::ErrorCode::InvalidState);
        QCOMPARE(state->acquired,std::size_t{1});
        QCOMPARE(state->revalidated,std::size_t{2});
        QCOMPARE(state->retired,std::size_t{1});
        QVERIFY(!QFileInfo::exists(dest));
        QVERIFY(m18_no_temporary_candidates(dir.path()));
        QFile check{src};QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(),QByteArray("unchanged",9));
    }
}


void M18CandidateDeliveryValidationTest::
realWorkerRevokedBeforeCommitAdmissionCleansCandidate()
{
    // Deterministic interleaving: the real Windows exporter has prepared
    // an actual WAV and reaches the Product lease factory; revoke on the
    // owner thread BEFORE allowing the worker to acquire that lease.
    using namespace platform::windows;
    using internal::M18ProductEpochTicketAuthority;
    using internal::M18ProductExportIdentity;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto src=dir.filePath(QStringLiteral("source.bin"));
    QFile input{src};
    QVERIFY(input.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(input.write("original",8),qint64{8});
    input.close();
    const auto destination=dir.filePath(QStringLiteral("must-not-commit.wav"));
    const auto srcRef=WindowsResourceReader::make_read_reference(
        m18_path(src),m18_path(QFileInfo{src}.fileName()));
    const auto dstRef=WindowsResourceWriter::make_write_reference(
        m18_path(destination),m18_path(QFileInfo{destination}.fileName()));
    QVERIFY(srcRef &&dstRef);
    const M18ProductExportIdentity id{10U,11U,12U,13U,48000U};
    M18ProductEpochTicketAuthority owner{66U,id};
    QVERIFY(owner.try_transition(66U,67U,id,true));
    auto ticket=owner.capture(id);QVERIFY(ticket);
    const auto realFactory=ticket.value()->commit_lease_factory();
    std::promise<void> reachedAdmission;
    auto entered=reachedAdmission.get_future();
    std::promise<void> mayProceed;
    auto released=mayProceed.get_future().share();
    M18AcquireCommitLease pauseBeforeCommit=
        [realFactory,released,&reachedAdmission]()
          ->core::Result<std::unique_ptr<M18ExportCommitLease>> {
            reachedAdmission.set_value();
            released.wait();
            return realFactory();
        };
    const auto pcm=master(audio::ChannelLayout::MONO_C,4105);
    QVERIFY(pcm);
    auto request=M18TransactionalAudioExportRequest::create(
        *srcRef.value(),pcm,*dstRef.value(),
        audio::WavSampleFormat::PCM_S24,UINT64_C(0xC0FFEE0102030405),
        0.0,ExportCollisionPolicy::FAIL_IF_EXISTS,
        std::move(pauseBeforeCommit));
    QVERIFY(request);
    bool correctlyDenied=false;
    std::thread worker([&] {
        const auto exported=TransactionalAudioExporter::export_new_file(
            *request.value());
        correctlyDenied=!exported &&exported.error()->code()
            ==core::ErrorCode::InvalidState;
    });
    // Wait until candidate processing has actually reached its commit
    // boundary: no timing assumptions or sleeps.
    const bool admissionReached=
        entered.wait_for(std::chrono::seconds(30))==std::future_status::ready;
    const auto revoked=admissionReached
        ?owner.try_transition(67U,68U,id,false)
        :core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_TEST_WORKER_NEVER_REACHED_COMMIT_ADMISSION"});
    mayProceed.set_value();
    worker.join(); // ALWAYS join before assertions, including failure.
    QVERIFY(admissionReached);
    QVERIFY(revoked);
    QVERIFY(correctlyDenied);
    QVERIFY(!QFileInfo::exists(destination));
    QVERIFY(m18_no_temporary_candidates(dir.path()));
    QFile source{src};QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.readAll(),QByteArray("original",8));
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18CandidateDeliveryValidationTest)
#include "test_m18_candidate_delivery_validation.moc"
