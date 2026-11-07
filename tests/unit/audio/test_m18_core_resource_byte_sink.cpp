// M18 preproduction only: exact ResourceWriter byte-port adapter.
// No OS file, no publication, no implied durability or Windows FQ.
#include "internal/m18_core_resource_byte_sink.hpp"
#include <rgsml/core/error.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace rgsml::tests {
namespace {
[[nodiscard]] core::ResourceReference make_memory_reference()
{
    auto r=core::ResourceReference::create(
        "test.memory","m18-core-writer",false,true,"M18 sink test");
    Q_ASSERT(r.value()!=nullptr);
    return *r.value();
}
class MemoryWriter final : public core::IResourceWriter {
public:
    MemoryWriter():reference_(make_memory_reference()){}
    [[nodiscard]] const core::ResourceReference& reference() const noexcept override
    { return reference_; }
    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override
    { return core::ResourceCapabilities::create(true,true,true,true); }
    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override
    { return core::Result<std::uint64_t>::success(bytes.size()); }
    [[nodiscard]] core::Result<std::size_t> write(
        std::span<const std::byte> source) override
    {
        ++calls;
        if (throw_on_write)throw std::runtime_error("writer failure");
        if (error_on_call!=0U &&calls>=error_on_call)
            return core::Result<std::size_t>::failure(
                core::Error{core::ErrorCode::IoFailure,"synthetic write error"});
        if (zero_after_call!=0U &&calls>=zero_after_call)
            return core::Result<std::size_t>::success(0U);
        if (invalid_transfer)
            return core::Result<std::size_t>::success(source.size()+1U);
        const auto n=std::min(max_transfer,source.size());
        bytes.insert(bytes.end(),source.begin(),source.begin()+n);
        return core::Result<std::size_t>::success(n);
    }
    [[nodiscard]] core::Status seek_bytes(std::uint64_t) override
    { return core::Status::success(); }
    [[nodiscard]] core::Status resize_bytes(std::uint64_t) override
    { return core::Status::success(); }
    [[nodiscard]] core::Status flush() override
    { ++flush_calls;return core::Status::success(); }
    [[nodiscard]] core::Status close() override
    { ++close_calls;return core::Status::success(); }

    core::ResourceReference reference_;
    std::vector<std::byte> bytes;
    std::size_t max_transfer{std::numeric_limits<std::size_t>::max()};
    std::size_t calls{0};
    std::size_t error_on_call{0};
    std::size_t zero_after_call{0};
    std::size_t flush_calls{0};
    std::size_t close_calls{0};
    bool invalid_transfer{false};
    bool throw_on_write{false};
};
} // namespace

class M18CoreResourceByteSinkTest final : public QObject {
    Q_OBJECT
private slots:
    void acceptShortWritesAndNoHiddenCommit();
    void failClosedOnZeroErrorOverreportAndException();
    void streamingPcm24UsesTheSameCanonicalBytePort();
    void streamingPcm24MonoOddPayloadPadAndFailure();
    void streamingFloat32KeepsDitherDisabled();
};

void M18CoreResourceByteSinkTest::acceptShortWritesAndNoHiddenCommit()
{
    using audio::internal::M18CoreResourceByteSink;
    using audio::internal::M18CoreSinkFailure;
    MemoryWriter w;
    w.max_transfer=2U;
    M18CoreResourceByteSink sink(w);
    const std::array<std::uint8_t,7> data{{0U,1U,2U,3U,4U,5U,255U}};
    QVERIFY(sink.write_all(data));
    QCOMPARE(sink.failure(),M18CoreSinkFailure::NONE);
    QCOMPARE(sink.bytes_accepted(),std::uint64_t{7});
    QCOMPARE(w.calls,std::size_t{4});
    QCOMPARE(w.bytes.size(),data.size());
    for (std::size_t i=0;i<data.size();++i)
        QCOMPARE(std::to_integer<std::uint8_t>(w.bytes[i]),data[i]);
    QVERIFY(sink.write_all(std::span<const std::uint8_t>{}));
    QCOMPARE(w.flush_calls,std::size_t{0});
    QCOMPARE(w.close_calls,std::size_t{0});
}

void M18CoreResourceByteSinkTest::failClosedOnZeroErrorOverreportAndException()
{
    using audio::internal::M18CoreResourceByteSink;
    using audio::internal::M18CoreSinkFailure;
    const std::array<std::uint8_t,3> data{{1U,2U,3U}};
    {
        MemoryWriter w;
        w.max_transfer=1U;
        w.zero_after_call=2U;
        M18CoreResourceByteSink sink(w);
        QVERIFY(!sink.write_all(data));
        QCOMPARE(sink.failure(),M18CoreSinkFailure::ZERO_PROGRESS);
        QCOMPARE(sink.bytes_accepted(),std::uint64_t{1});
        QVERIFY(!sink.write_all(data));
        QCOMPARE(w.bytes.size(),std::size_t{1});
    }
    {
        MemoryWriter w;
        w.error_on_call=1U;
        M18CoreResourceByteSink sink(w);
        QVERIFY(!sink.write_all(data));
        QCOMPARE(sink.failure(),M18CoreSinkFailure::WRITER_ERROR);
        QCOMPARE(sink.bytes_accepted(),std::uint64_t{0});
    }
    {
        MemoryWriter w;
        w.invalid_transfer=true;
        M18CoreResourceByteSink sink(w);
        QVERIFY(!sink.write_all(data));
        QCOMPARE(sink.failure(),M18CoreSinkFailure::INVALID_TRANSFER);
        QVERIFY(w.bytes.empty());
    }
    {
        MemoryWriter w;
        w.throw_on_write=true;
        M18CoreResourceByteSink sink(w);
        QVERIFY(!sink.write_all(data));
        QCOMPARE(sink.failure(),M18CoreSinkFailure::EXCEPTION);
        QVERIFY(w.bytes.empty());
    }
}

void M18CoreResourceByteSinkTest::streamingPcm24UsesTheSameCanonicalBytePort()
{
    using namespace audio::internal;
    MemoryWriter w;
    w.max_transfer=3U;
    M18CoreResourceByteSink sink(w);
    const M18WavSpec spec{M18WavEncoding::PCM24,48000U,2U,4U,42U};
    M18StreamingWavWriter wav(sink,spec);
    QCOMPARE(wav.start(),M18StreamStatus::OK);
    const std::array<double,8> samples{{
        0.0,0.0,0.25,-0.25,0.5,-0.5,0.75,-0.75}};
    QCOMPARE(wav.append(samples),M18StreamStatus::OK);
    const auto completed=wav.finish();
    QCOMPARE(completed.status,M18StreamStatus::OK);
    QVERIFY(completed.receipt.has_value());
    QCOMPARE(completed.receipt->frames,std::uint64_t{4});
    QCOMPARE(w.bytes.size(),
             static_cast<std::size_t>(completed.receipt->plan.file_bytes));
    QCOMPARE(sink.bytes_accepted(),
             static_cast<std::uint64_t>(w.bytes.size()));
    QCOMPARE(std::to_integer<char>(w.bytes[0]),'R');
    QCOMPARE(std::to_integer<char>(w.bytes[1]),'I');
    // This is ONLY uncommitted bytes: no flush/close/rename/readback.
    QCOMPARE(w.flush_calls,std::size_t{0});
    QCOMPARE(w.close_calls,std::size_t{0});
}

void M18CoreResourceByteSinkTest::streamingFloat32KeepsDitherDisabled()
{
    using namespace audio::internal;
    MemoryWriter w;
    w.max_transfer=2U;
    M18CoreResourceByteSink sink(w);
    // Even an arbitrary seed in private spec MUST be irrelevant to F32.
    const M18WavSpec spec{
        M18WavEncoding::IEEE_FLOAT32,96000U,1U,3U,UINT64_MAX};
    M18StreamingWavWriter wav(sink,spec);
    QCOMPARE(wav.start(),M18StreamStatus::OK);
    const std::array<double,3> samples{{0.0,-0.0,0.5}};
    QCOMPARE(wav.append(samples),M18StreamStatus::OK);
    const auto result=wav.finish();
    QCOMPARE(result.status,M18StreamStatus::OK);
    QVERIFY(result.receipt.has_value());
    QVERIFY(!result.receipt->identity.dither_seed.has_value());
    QCOMPARE(w.bytes.size(),
        static_cast<std::size_t>(result.receipt->plan.file_bytes));
    const auto payload=static_cast<std::size_t>(
        result.receipt->plan.header_bytes);
    QVERIFY(w.bytes.size()>=payload+12U);
    // IEEE little-endian: +0.0 then signed -0.0 then +0.5f.
    for(std::size_t i=0;i<4U;++i)
        QCOMPARE(std::to_integer<std::uint8_t>(w.bytes[payload+i]),
                 std::uint8_t{0});
    for(std::size_t i=0;i<3U;++i)
        QCOMPARE(std::to_integer<std::uint8_t>(w.bytes[payload+4U+i]),
                 std::uint8_t{0});
    QCOMPARE(std::to_integer<std::uint8_t>(w.bytes[payload+7U]),
             std::uint8_t{0x80});
    QCOMPARE(std::to_integer<std::uint8_t>(w.bytes[payload+11U]),
             std::uint8_t{0x3f});
    QCOMPARE(w.close_calls,std::size_t{0});
    QCOMPARE(w.flush_calls,std::size_t{0});
    MemoryWriter failed;
    failed.error_on_call=1U;
    M18CoreResourceByteSink badSink(failed);
    M18StreamingWavWriter rejected(badSink,spec);
    QCOMPARE(rejected.start(),M18StreamStatus::SINK_FAILED);
    QCOMPARE(badSink.failure(),M18CoreSinkFailure::WRITER_ERROR);
    QVERIFY(!rejected.finish().receipt.has_value());
}


void M18CoreResourceByteSinkTest::streamingPcm24MonoOddPayloadPadAndFailure()
{
    using namespace audio::internal;
    const M18WavSpec spec{M18WavEncoding::PCM24,48000U,1U,3U,42U};
    const std::array<double,3> samples{{0.1,-0.2,0.3}};
    const auto plan=m18_plan_wav(spec);
    QCOMPARE(plan.status,M18WavStatus::OK);
    QCOMPARE(plan.plan.data_bytes,std::uint64_t{9});
    QCOMPARE(plan.plan.file_bytes,std::uint64_t{54}); // 44 + 9 + RIFF pad
    MemoryWriter w;
    w.max_transfer=1U; // adversarial short progress for pad too
    M18CoreResourceByteSink sink(w);
    M18StreamingWavWriter wav(sink,spec);
    QCOMPARE(wav.start(),M18StreamStatus::OK);
    QCOMPARE(wav.append(samples),M18StreamStatus::OK);
    const auto receipt=wav.finish();
    QCOMPARE(receipt.status,M18StreamStatus::OK);
    QVERIFY(receipt.receipt.has_value());
    QCOMPARE(w.bytes.size(),std::size_t{54});
    QCOMPARE(std::to_integer<std::uint8_t>(w.bytes.back()),std::uint8_t{0});
    const auto independent=m18_make_bounded_wav_candidate(spec,samples);
    QCOMPARE(independent.status,M18WavStatus::OK);
    QCOMPARE(independent.bytes.size(),w.bytes.size());
    for(std::size_t i=0;i<w.bytes.size();++i)
        QCOMPARE(std::to_integer<std::uint8_t>(w.bytes[i]),
                 independent.bytes[i]);
    QCOMPARE(sink.bytes_accepted(),std::uint64_t{54});
    QCOMPARE(w.flush_calls,std::size_t{0});
    QCOMPARE(w.close_calls,std::size_t{0});
    // Failure exactly on the THIRD physical write (the padding byte).
    MemoryWriter blocked;
    blocked.error_on_call=3U;
    M18CoreResourceByteSink failedSink(blocked);
    M18StreamingWavWriter failed(failedSink,spec);
    QCOMPARE(failed.start(),M18StreamStatus::OK);
    QCOMPARE(failed.append(samples),M18StreamStatus::OK);
    const auto bad=failed.finish();
    QCOMPARE(bad.status,M18StreamStatus::SINK_FAILED);
    QVERIFY(!bad.receipt.has_value());
    QCOMPARE(blocked.bytes.size(),std::size_t{53});
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M18CoreResourceByteSinkTest)
#include "test_m18_core_resource_byte_sink.moc"
