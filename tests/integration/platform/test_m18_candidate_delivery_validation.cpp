// M18 candidate readback through REAL canonical WavReader, bounded RAM.
// This test does not authorize M18 publication or certify final Windows FQ.
#include "internal/m18_candidate_delivery_validation.hpp"
#include "wav_test_support.hpp"
#include "m18_streaming_wav.hpp"
#include "internal/transactional_audio_exporter_test_seam.hpp"
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
};
class TestM18Lease final:public platform::windows::M18ExportCommitLease {
public:
    explicit TestM18Lease(std::shared_ptr<TestSessionState> owner)
        : owner_(std::move(owner)) {}
    ~TestM18Lease() override{++owner_->retired;}
    [[nodiscard]] core::Status revalidate_current() const override {
        ++owner_->revalidated;
        return owner_->current?core::Status::success()
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
        QCOMPARE(state->revalidated,std::size_t{1});
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
                    QCOMPARE(permission->revalidated,std::size_t{1});
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

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18CandidateDeliveryValidationTest)
#include "test_m18_candidate_delivery_validation.moc"
