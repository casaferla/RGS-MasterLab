#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/dsp/true_peak_limiter_module.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include "../../oracles/true_peak_limiter/core_test_support.hpp"

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <limits>
#include <vector>

namespace rgsml::tests {
namespace {
using namespace rgsml;
using namespace rgsml::dsp;

[[nodiscard]] ModuleDescriptor desc()
{
    auto d=ModuleDescriptor::create(ModuleDescriptorSpec{
        "rgsml.dsp.true-peak-limiter","m17", {ModuleCategory::DYNAMICS,ModuleCategory::OUTPUT},
        {ProcessingStage::MASTER},{ChainSegment::TERMINAL},false,true,
        PlacementClass::TERMINAL_SLOT,TerminalSlot::FINAL_TRUE_PEAK_LIMITER,
        {},{},{},{},false,true,"1.0.0","rgsml.dsp.true-peak-limiter.parameters/1.0.0"});
    return *d.value();
}
[[nodiscard]] audio::AudioFormat format(int rate)
{
    return *audio::AudioFormat::create(*core::SampleRate::create(rate).value(),
        audio::ChannelLayout::STEREO_LR).value();
}
[[nodiscard]] std::unique_ptr<TruePeakLimiterModule> fresh(int rate,double ceiling)
{
    auto p=TruePeakLimiterParameters::create(ceiling);
    auto module=TruePeakLimiterModule::create(desc(),*p.value());
    if(!module)return {};
    auto spec=DspProcessSpec{format(rate),audio::FrameDomainId::OUTPUT_RATE,
        *core::FrameCount::create(4096).value()};
    if(!module.value()->get()->prepare(spec))return {};
    return std::move(*module.value());
}
[[nodiscard]] core::FrameRange range(int first,int count)
{
    return *core::FrameRange::create(core::FrameIndex{first},
        core::FrameIndex{first+count}).value();
}
[[nodiscard]] std::vector<std::array<double,2>> processBlock(
    TruePeakLimiterModule& mod, int rate,int start,int count, bool begins)
{
    auto in=audio::AudioBuffer::create(format(rate),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{start},*core::FrameCount::create(count).value());
    auto out=audio::AudioBuffer::create(format(rate),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{start},*core::FrameCount::create(count).value());
    if(!in || !out)return {};
    for (std::size_t ch=0;ch<2;++ch) {
        auto x=in.value()->mutable_view().channel(ch);
        if(!x)return {};
        for(int i=0;i<count;++i) {
            const double v=std::sin(double(start+i)*0.18)+((start+i)%17==0?2.0:0.0);
            (*x.value())[static_cast<std::size_t>(i)]=ch==0?v:v*0.4;
        }
    }
    const auto s=mod.process(in.value()->view(),out.value()->mutable_view(),
        DspProcessContext{range(start,count),begins,false});
    if(!s)return {};
    const auto l=out.value()->view().channel(0);
    const auto r=out.value()->view().channel(1);
    if(!l || !r)return {};
    std::vector<std::array<double,2>> raw;
    for(int i=0;i<count;++i)
        raw.push_back({(*l.value())[static_cast<std::size_t>(i)],
            (*r.value())[static_cast<std::size_t>(i)]});
    return raw;
}
[[nodiscard]] std::vector<std::array<double,2>> finalBlock(
    TruePeakLimiterModule& mod, int rate,int start,int count,bool eos)
{
    auto out=audio::AudioBuffer::create(format(rate),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{start},*core::FrameCount::create(count).value());
    if(!out || !mod.finalize(out.value()->mutable_view(),
        DspProcessContext{range(start,count),false,eos}))return {};
    const auto l=out.value()->view().channel(0);
    const auto r=out.value()->view().channel(1);
    if(!l || !r)return {};
    std::vector<std::array<double,2>> raw;
    for(int i=0;i<count;++i)
        raw.push_back({(*l.value())[static_cast<std::size_t>(i)],
            (*r.value())[static_cast<std::size_t>(i)]});
    return raw;
}
struct GainWitnessCollector final {
    std::vector<TruePeakLimiterAppliedGainFrame> frames;
    static void collect(void* context,
        std::span<const TruePeakLimiterAppliedGainFrame> records) noexcept
    {
        auto& dst=*static_cast<GainWitnessCollector*>(context);
        dst.frames.insert(dst.frames.end(),records.begin(),records.end());
    }
};
class TruePeakLimiterCheckpointTest final : public QObject {
    Q_OBJECT
private slots:
    void originAndChunkResumeBitIdentity();
    void checksumFormatBackendAndCeilingRejectAtomically();
    void finalizeBoundaryRejectsCheckpoints();
    void canonicalLengthIsU64LittleEndian();
    void wholeResourceBoundCheckpointRejectsWrongOwnerAndDigest();
};
void TruePeakLimiterCheckpointTest::originAndChunkResumeBitIdentity()
{
    for(const int fs:{44100,48000,96000,192000}) {
        for(const int at:{1,57,228,441}) {
            auto a=fresh(fs,-6.0);
            auto b=fresh(fs,-6.0);
            QVERIFY(a && b);
            GainWitnessCollector witnessA,witnessB;
            witnessA.frames.reserve(1131);
            witnessB.frames.reserve(1131);
            a->set_applied_gain_sink(&GainWitnessCollector::collect,&witnessA);
            b->set_applied_gain_sink(&GainWitnessCollector::collect,&witnessB);
            auto initial=processBlock(*a,fs,0,at,true);
            QCOMPARE(initial.size(),static_cast<std::size_t>(at));
            auto cp=a->runtime_checkpoint();
            QVERIFY(cp);
            QCOMPARE(cp.value()->next_input_frame,std::optional{core::FrameIndex{at}});
            const auto payload=*cp.value();
            QVERIFY(b->restore_runtime_checkpoint(payload));
            const int n=1131;
            std::vector<std::array<double,2>> tailA,tailB;
            for(int start=at;start<n;) {
                const int len=std::min(n-start,31+start%59);
                auto one=processBlock(*a,fs,start,len,false);
                auto two=processBlock(*b,fs,start,len,false);
                QVERIFY(!one.empty() && !two.empty());
                tailA.insert(tailA.end(),one.begin(),one.end());
                tailB.insert(tailB.end(),two.begin(),two.end());
                start+=len;
            }
            const int h=fs/200 + 8;
            for(int emitted=0;emitted<h;) {
                const auto k=std::min(83,h-emitted);
                auto one=finalBlock(*a,fs,n+emitted,k,emitted+k==h);
                auto two=finalBlock(*b,fs,n+emitted,k,emitted+k==h);
                QVERIFY(!one.empty() && !two.empty());
                tailA.insert(tailA.end(),one.begin(),one.end());
                tailB.insert(tailB.end(),two.begin(),two.end());
                emitted+=k;
            }
            QCOMPARE(tailA.size(),tailB.size());
            // A checkpoint tracks input position, not the delivered output.
            // The D-frame delay may leave frame 0 and later real frames
            // pending even when hundreds of input frames were consumed.
            const auto d=fs/200+8;
            const auto emittedBeforeCheckpoint=std::max(0,at-d);
            QCOMPARE(witnessA.frames.size(),std::size_t{1131});
            QCOMPARE(witnessB.frames.size(),
                static_cast<std::size_t>(1131-emittedBeforeCheckpoint));
            for(std::size_t i=0;i<witnessA.frames.size();++i) {
                const auto& row=witnessA.frames[i];
                QCOMPARE(row.programme_frame,static_cast<std::int64_t>(i));
                QCOMPARE(row.raw_output_frame,
                    static_cast<std::int64_t>(i)+fs/200+8);
                QVERIFY(std::isfinite(row.reduction_db));
                QVERIFY(row.reduction_db>=0.0);
                QVERIFY(row.gain>0.0 && row.gain<=1.0);
            }
            for(std::size_t i=0;i<witnessB.frames.size();++i) {
                const auto& resumed=witnessB.frames[i];
                const auto& unbroken=witnessA.frames[
                    i+static_cast<std::size_t>(emittedBeforeCheckpoint)];
                QCOMPARE(resumed.programme_frame,unbroken.programme_frame);
                QCOMPARE(resumed.raw_output_frame,unbroken.raw_output_frame);
                QCOMPARE(std::bit_cast<std::uint64_t>(resumed.gain),
                    std::bit_cast<std::uint64_t>(unbroken.gain));
                QCOMPARE(std::bit_cast<std::uint64_t>(resumed.reduction_db),
                    std::bit_cast<std::uint64_t>(unbroken.reduction_db));
            }
            for(std::size_t i=0;i<tailA.size();++i)
                for(std::size_t ch=0;ch<2;++ch)
                    QCOMPARE(std::bit_cast<std::uint64_t>(tailA[i][ch]),
                        std::bit_cast<std::uint64_t>(tailB[i][ch]));
        }
    }
}
void TruePeakLimiterCheckpointTest::checksumFormatBackendAndCeilingRejectAtomically()
{
    auto original=fresh(44100,-6.0);
    QVERIFY(original);
    auto filled=processBlock(*original,44100,0,290,true);
    QVERIFY(!filled.empty());
    auto checkpoint=original->runtime_checkpoint();
    QVERIFY(checkpoint);
    auto resumed=fresh(44100,-6.0);
    QVERIFY(resumed);
    const auto probe=[&](const DspRuntimeCheckpoint& bad) {
        QVERIFY(!resumed->restore_runtime_checkpoint(bad));
        const auto cpAfter=resumed->runtime_checkpoint();
        QVERIFY(cpAfter);
        const auto pristine=fresh(44100,-6.0);
        QVERIFY(pristine);
        const auto cpPristine=pristine->runtime_checkpoint();
        QVERIFY(cpPristine);
        QCOMPARE(*cpAfter.value(),*cpPristine.value());
    };
    auto corrupt=*checkpoint.value();
    corrupt.payload[42]^=1U;
    probe(corrupt);
    auto wrongRate=*checkpoint.value();
    wrongRate.audio_format=format(48000);
    probe(wrongRate);
    auto wrongBackend=*checkpoint.value();
    wrongBackend.backend_identity="unknown-backend";
    probe(wrongBackend);
    auto wrongSchema=*checkpoint.value();
    wrongSchema.checkpoint_schema_version="invalid";
    probe(wrongSchema);
    auto wrongNext=*checkpoint.value();
    wrongNext.next_input_frame=core::FrameIndex{10};
    probe(wrongNext);
    auto wrongFingerprint=*checkpoint.value();
    wrongFingerprint.sonic_fingerprint="wrong-ceiling";
    probe(wrongFingerprint);
    QVERIFY(resumed->restore_runtime_checkpoint(*checkpoint.value()));
    auto differentCeiling=fresh(44100,-1.0);
    QVERIFY(differentCeiling);
    QVERIFY(!differentCeiling->restore_runtime_checkpoint(*checkpoint.value()));
    // A live, different already-bound stream is not reset using a foreign CP.
    auto bound=fresh(44100,-6.0);
    QVERIFY(bound);
    QVERIFY(!processBlock(*bound,44100,0,17,true).empty());
    QVERIFY(!bound->restore_runtime_checkpoint(*checkpoint.value()));
}
void TruePeakLimiterCheckpointTest::wholeResourceBoundCheckpointRejectsWrongOwnerAndDigest()
{
    constexpr int fs=44100,n=1131,at=57;
    auto full=audio::AudioBuffer::create(format(fs),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*core::FrameCount::create(n).value());
    QVERIFY(full);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=full.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        for(int i=0;i<n;++i) {
            const double v=std::sin(double(i)*0.18)+(i%17==0?2.0:0.0);
            (*lane.value())[static_cast<std::size_t>(i)]=ch==0?v:v*0.4;
        }
    }
    auto original=fresh(fs,-6.0);
    auto resumed=fresh(fs,-6.0);
    QVERIFY(original && resumed);
    QVERIFY(!processBlock(*original,fs,0,at,true).empty());
    const auto cp=original->runtime_checkpoint();
    QVERIFY(cp);
    const auto envelope=m17_verifier::bind_core_checkpoint(
        *cp.value(),"whole-wav-01",*full.value(),9);
    QVERIFY(envelope);
    const auto pristine=resumed->runtime_checkpoint();
    QVERIFY(pristine);
    const auto assertUnchanged=[&]() {
        auto actual=resumed->runtime_checkpoint();
        QVERIFY(actual);
        QCOMPARE(*actual.value(),*pristine.value());
    };
    QVERIFY(!m17_verifier::restore_bound_checkpoint(
        *resumed,*envelope.value(),"other-owner",*full.value(),9));
    assertUnchanged();
    QVERIFY(!m17_verifier::restore_bound_checkpoint(
        *resumed,*envelope.value(),"whole-wav-01",*full.value(),10));
    assertUnchanged();
    auto shorter=audio::AudioBuffer::create(format(fs),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*core::FrameCount::create(n-1).value());
    QVERIFY(shorter);
    QVERIFY(!m17_verifier::restore_bound_checkpoint(
        *resumed,*envelope.value(),"whole-wav-01",*shorter.value(),9));
    assertUnchanged();
    auto lane=full.value()->mutable_view().channel(0);
    QVERIFY(lane);
    const double preserved=(*lane.value())[77];
    (*lane.value())[77]=std::nextafter(
        preserved,std::numeric_limits<double>::infinity());
    QVERIFY(!m17_verifier::restore_bound_checkpoint(
        *resumed,*envelope.value(),"whole-wav-01",*full.value(),9));
    assertUnchanged();
    (*lane.value())[77]=preserved;
    auto corrupted=*envelope.value();
    corrupted.full_pcm_sha256[0]^=1;
    QVERIFY(!m17_verifier::restore_bound_checkpoint(
        *resumed,corrupted,"whole-wav-01",*full.value(),9));
    assertUnchanged();
    QVERIFY(m17_verifier::restore_bound_checkpoint(
        *resumed,*envelope.value(),"whole-wav-01",*full.value(),9));
    const auto direct=processBlock(*original,fs,at,n-at,false);
    const auto restored=processBlock(*resumed,fs,at,n-at,false);
    QCOMPARE(direct.size(),restored.size());
    for(std::size_t i=0;i<direct.size();++i)
        for(std::size_t ch=0;ch<2;++ch)
            QCOMPARE(std::bit_cast<std::uint64_t>(direct[i][ch]),
                std::bit_cast<std::uint64_t>(restored[i][ch]));
}

void TruePeakLimiterCheckpointTest::canonicalLengthIsU64LittleEndian()
{
    auto module=fresh(44100,-1.0);
    QVERIFY(module);
    const auto cp=module->runtime_checkpoint();
    QVERIFY(cp);
    const auto& bytes=cp.value()->payload;
    QVERIFY(bytes.size()>=52U);
    const std::array<std::uint8_t,8> magic{'M','1','7','C','P','1',0,0};
    for(std::size_t i=0;i<magic.size();++i)QCOMPARE(bytes[i],magic[i]);
    QCOMPARE(bytes[8],std::uint8_t{1});
    for(std::size_t i=9;i<12;++i)QCOMPARE(bytes[i],std::uint8_t{0});
    std::uint64_t stored=0;
    for(unsigned i=0;i<8;++i)
        stored|=std::uint64_t{bytes[12U+i]}<<(8*i);
    QCOMPARE(stored,static_cast<std::uint64_t>(bytes.size()));
    // The high dword is part of the frozen length field, not Fs metadata.
    for(std::size_t i=16;i<20;++i)QCOMPARE(bytes[i],std::uint8_t{0});
    auto restored=fresh(44100,-1.0);
    QVERIFY(restored);
    QVERIFY(restored->restore_runtime_checkpoint(*cp.value()));
}
void TruePeakLimiterCheckpointTest::finalizeBoundaryRejectsCheckpoints()
{
    auto empty=fresh(48000,-1.0);
    QVERIFY(empty);
    auto initial=empty->runtime_checkpoint();
    QVERIFY(initial);
    auto clone=fresh(48000,-1.0);
    QVERIFY(clone);
    QVERIFY(clone->restore_runtime_checkpoint(*initial.value()));
    QVERIFY(!processBlock(*empty,48000,0,1,true).empty());
    const int d=248;
    QVERIFY(!finalBlock(*empty,48000,1,30,false).empty());
    QVERIFY(!empty->runtime_checkpoint());
    QVERIFY(!empty->restore_runtime_checkpoint(*initial.value()));
    QVERIFY(!finalBlock(*empty,48000,31,d-30,true).empty());
    QVERIFY(!empty->runtime_checkpoint());
    QVERIFY(!empty->restore_runtime_checkpoint(*initial.value()));
    QVERIFY(finalBlock(*empty,48000,249,1,true).empty());
}
} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakLimiterCheckpointTest)
#include "test_true_peak_limiter_checkpoint.moc"
