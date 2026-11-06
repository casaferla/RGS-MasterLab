// M18 candidate readback through REAL canonical WavReader, bounded RAM.
// This test does not authorize M18 publication or certify final Windows FQ.
#include "internal/m18_candidate_delivery_validation.hpp"
#include "wav_test_support.hpp"
#include "m18_streaming_wav.hpp"
#include "internal/transactional_audio_exporter_test_seam.hpp"
#include "internal/m18_epoch_commit_barrier.hpp"
#include "internal/m18_product_epoch_ticket.hpp"
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
#include <thread>
#include <memory>
#include <optional>
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
    void realWindowsSixRatesBothLayoutsBothEncodingsOddEven();
    void realWindowsFailureInjectionCleansUpBothFormats();
    void crossThreadEpochBarrierPreventsCommitRaceWithoutQtBlocking();
    void exclusiveProductProjectMutationBlocksWorkerCommitUntilComplete();
    void productMutationBatchPinsIdentityAndForbidsPartialCapture();
    void productEpochTicketConsentAndMasterIdentityAreWorkerSafe();
    void realWorkerTicketExportsOnlyWithCurrentConsentAndSource();
    void realWindowsFinalAuthorityRevocationRollsBackBothFormats();
    void realWorkerRevokedBeforeCommitAdmissionCleansCandidate();
};

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
        78U,79U,changed,true);
    QVERIFY(!deniedBatch);
    QVERIFY((*oldCommit.value())->revalidate_current());
    oldCommit.value()->reset();
    QVERIFY(owner.capture(before));

    // An authorized Project batch advances the monotonic identity once,
    // while holding exclusive protection across ALL downstream mutations.
    auto batch=owner.try_begin_product_mutation(78U,79U,changed,true);
    QVERIFY(batch);
    QCOMPARE((*batch.value())->epoch(),std::uint64_t{79U});
    QVERIFY(!previousFactory());
    QVERIFY(!owner.capture(before));
    QVERIFY(!owner.capture(changed)); // the new project is still incomplete
    QVERIFY(!owner.try_transition(79U,80U,changed,true));
    QVERIFY(!owner.try_begin_product_mutation(79U,80U,changed,true));
    batch.value()->reset();
    // The old ticket stays revoked even if the old M17 PCM was retained.
    QVERIFY(!previousFactory());
    auto now=owner.capture(changed);
    QVERIFY(now);
    auto newLease=now.value()->commit_lease_factory()();
    QVERIFY(newLease);
    QVERIFY((*newLease.value())->revalidate_current());
    QVERIFY(!owner.try_begin_product_mutation(79U,80U,changed,false));
    newLease.value()->reset();
    QVERIFY(owner.try_transition(79U,80U,changed,false));
    QVERIFY(!now.value()->commit_lease_factory()());
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
    M18ProductEpochTicketAuthority authority{200U,id};
    QVERIFY(!authority.capture(id)); // No implicit user consent.
    QVERIFY(!authority.try_transition(200U,200U,id,true));
    QVERIFY(authority.try_transition(200U,201U,id,true));
    auto captured=authority.capture(id);
    QVERIFY(captured);
    const auto factory=captured.value()->commit_lease_factory();

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
