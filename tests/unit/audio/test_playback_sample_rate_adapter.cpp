#include "wav_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>
#include <rgsml/audio/master_monitoring_rate_plan.hpp>
#include <rgsml/audio/internal/phase11_strict_scalar_reference.hpp>
#include <rgsml/audio/internal/phase11_rgsmsr1_candidate_asset.hpp>
#include <rgsml/audio/internal/phase11_pinned_scalar_candidate_reference.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/audio/monitoring_frame_map.hpp>

#include <QTest>
#include <QDebug>
#include <QElapsedTimer>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace wav_support;

[[nodiscard]] core::SampleRate rate(std::int64_t value)
{
    auto result = core::SampleRate::create(value);
    Q_ASSERT(result);
    return *result.value();
}

[[nodiscard]] std::unique_ptr<audio::WavReader> open_f64_stereo(
    std::uint32_t sampleRate,
    std::span<const std::uint64_t> interleaved,
    const std::shared_ptr<ReaderControl>& control =
        std::make_shared<ReaderControl>())
{
    auto bytes = make_wav(
        3U, 64U, 2U, sampleRate, f64_payload(interleaved));
    auto reader = audio::WavReader::open(memory_reader(std::move(bytes), control));
    Q_ASSERT(reader);
    return std::move(*reader.value());
}

[[nodiscard]] audio::PlaybackSampleRateAdapter adapter(
    std::int64_t inputRate,
    std::int64_t outputRate,
    std::int64_t inputFrames)
{
    auto result = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(inputRate),
            rate(outputRate),
            audio::ChannelLayout::STEREO_LR,
            frame_count(inputFrames),
        });
    Q_ASSERT(result);
    return std::move(*result.value());
}

[[nodiscard]] audio::AudioBuffer output_buffer(
    const audio::PlaybackSampleRateAdapter& converter,
    std::int64_t start,
    std::int64_t frames)
{
    auto format = audio::AudioFormat::create(
        converter.output_rate(), converter.channel_layout());
    Q_ASSERT(format);
    auto buffer = audio::AudioBuffer::create(
        *format.value(),
        audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{start},
        frame_count(frames));
    Q_ASSERT(buffer);
    return std::move(*buffer.value());
}

[[nodiscard]] std::vector<std::uint64_t> render_partitioned(
    audio::PlaybackSampleRateAdapter& converter,
    audio::WavReader& reader,
    std::span<const std::int64_t> partition,
    std::size_t channel)
{
    std::vector<std::uint64_t> result;
    std::int64_t start = 0;
    for (const auto frames : partition) {
        auto block = output_buffer(converter, start, frames);
        auto rendered = converter.read_frames(
            reader, core::FrameIndex{start}, block.mutable_view());
        Q_ASSERT(rendered);
        auto bits = channel_bits(block, channel);
        result.insert(result.end(), bits.begin(), bits.end());
        start += frames;
    }
    return result;
}

}  // namespace

class PlaybackSampleRateAdapterTest final : public QObject {
    Q_OBJECT

private slots:
    void policyIdentityAndFrameMap();
    void phase11StrictScalarReferenceSixBySixAgainstConceptualGrid();
    void phase11CandidateBinaryParserRejectsMalformedAndRunsStrictReference();
    void phase11PinnedScalarReferenceSixBySixSyntheticGeometry();
    void offlineCandidateMatchesFrozenPlaybackKernelBitwise();
    void experimentalFullResourceSixBySixRateMatrix();
    void finalOutputExperimentalSpectralScreenSixBySix();
    void fullResourceSrcCancellationAbortsBeforePublication();
    void finalMasterMonitoringCannotRetargetMasterRate();
    void isolatedOfflineMonitoringPcmSixBySixMatrix();
    void monitoringFrameMapSixRateEndpointsAndLoops();
    void monitoringMasterSnapshotFailClosedAcrossSixRatePairs();
    void monitoringBorrowsMasterWithoutSourceAlias();
    void monitoring44100SixDeviceRatesRemainBlockInvariant();
    void monitoringFullSixBySixSignalBlockInvariance();
    void monitoringExperimentalPassbandAndAliasScreen();
    void monitoringPassbandGridSixBySixScreen();
    void monitoringExtendedStopbandScreen();
    void monitoringStereoImpulseSeparationSixBySix();
    void monitoringOneSecondStereoDurationAndOwnershipSmoke();
    void monitoringNonFiniteInputFailsClosedForSixRateFamilies();
    void monitoringOversizedOfflineBlockRejectsWithoutMutatingMaster();
    void monitoringVirtualLongTrackResourceMatrix();
    void monitoringResourcePreflightFailsOnOverflow();
    void monitoringPrepareAdviceSixMasterRatesAndHeadroom();
    void monitoringPrepareAdviceDurationRoundingAndUnknownState();
    void monitoringPrepareAdviceBudgetBoundariesAndFreshObservations();
    void monitoringImminentAllocationUsesFreshIncrementalBudget();
    void monitoringExplicitPreallocationDecisionContract();
    void assetsAreFrozenAndSymmetric();
    void constantAndChunkInvariance();
    void reverseDirectionAndAbsoluteSeekInvariance();
    void oneFrameBoundaryAndStereoIndependence();
    void decodeWindowIsTrackDurationIndependent();
    void conservativeInputDependenciesMatchIndependentTapOracle();
    void conservativeInputDependenciesRejectInvalidRanges();
    void noFutureDependencyPrefixAgainstIndependentOracle();
};


void PlaybackSampleRateAdapterTest::
phase11CandidateBinaryParserRejectsMalformedAndRunsStrictReference()
{
    // Real RGSMSR1 wire geometry, but SYNTHETIC binary64 taps. This is
    // qualification infrastructure, not a real 14-asset release oracle.
    constexpr std::uint32_t L=2,M=1,T=512;
    constexpr std::size_t n=L*T+1,header=52;
    std::vector<std::uint8_t> bytes(header+8*n,0);
    constexpr std::array<std::uint8_t,8> magic{
        'R','G','S','M','S','R','1',0};
    std::copy(magic.begin(),magic.end(),bytes.begin());
    const auto put16=[&](std::size_t at,std::uint16_t value) {
        for(std::size_t i=0;i<2;++i)
            bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));
    };
    const auto put32=[&](std::size_t at,std::uint32_t value) {
        for(std::size_t i=0;i<4;++i)
            bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));
    };
    const auto put64=[&](std::size_t at,std::uint64_t value) {
        for(std::size_t i=0;i<8;++i)
            bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));
    };
    put16(8,1);put16(10,1);
    put32(12,L);put32(16,M);put32(20,T);
    put64(28,n);put64(36,n);put64(44,8*n);
    // Bit-symmetric signed zero is preserved. A plausible but false
    // SHA is NOT accepted as proof; this parser deliberately has no
    // checksum/release entry point.
    put64(header,std::bit_cast<std::uint64_t>(-0.0));
    put64(header+8*(n-1),std::bit_cast<std::uint64_t>(-0.0));
    put64(header+8*(n/2),std::bit_cast<std::uint64_t>(2.0));
    put64(header+8*(n/2-1),std::bit_cast<std::uint64_t>(0.5));
    put64(header+8*(n/2+1),std::bit_cast<std::uint64_t>(0.5));
    const auto parse=[&](const std::vector<std::uint8_t>& input) {
        return audio::internal::parse_phase11_rgsmsr1_candidate_bits(
            input,L,M,T);
    };
    auto valid=parse(bytes);
    QVERIFY2(valid,valid?"":valid.error()->message().c_str());
    QCOMPARE(valid.value()->size(),n);
    QCOMPARE((*valid.value())[0],std::bit_cast<std::uint64_t>(-0.0));
    QCOMPARE((*valid.value())[n/2],std::bit_cast<std::uint64_t>(2.0));
    // Test-only digest is computed here to exercise the external-hash
    // enforcement API, NOT to claim independently frozen release authority.
    const auto pinned=core::sha256_hex(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()});
    const auto readPinned=[&](const std::vector<std::uint8_t>& input,
                              std::string_view digest) {
        return audio::internal::parse_phase11_rgsmsr1_pinned_candidate_bits(
            input,L,M,T,digest);
    };
    auto locked=readPinned(bytes,pinned);
    QVERIFY(locked);
    QCOMPARE(locked.value()->size(),valid.value()->size());
    for(std::size_t i=0;i<locked.value()->size();++i)
        QCOMPARE((*locked.value())[i],(*valid.value())[i]);
    auto wrongSha=pinned;
    wrongSha[0]=wrongSha[0]=='0'?'1':'0';
    QVERIFY(!readPinned(bytes,wrongSha));
    QVERIFY(!readPinned(bytes,std::string_view{}));
    auto uppercaseSha=pinned;
    uppercaseSha[0]='G';
    QVERIFY(!readPinned(bytes,uppercaseSha));
    // Preserve valid symmetry/finite geometry but alter one central tap.
    // Unpinned shape parsing accepts it; pinned data integrity MUST refuse.
    auto changed=bytes;
    const auto different=std::bit_cast<std::uint64_t>(2.25);
    for(std::size_t k=0;k<8;++k)
        changed[header+8*(n/2)+k]=static_cast<std::uint8_t>(
            different>>(8*k));
    QVERIFY(parse(changed));
    QVERIFY(!readPinned(changed,pinned));

    const std::array<double,3> source{-0.5,0.25,0.75};
    std::array<double,6> output{};
    auto rendered=audio::internal::render_phase11_strict_scalar_reference(
        source,output,0,L,M,*valid.value());
    QVERIFY2(rendered,rendered?"":rendered.error()->message().c_str());
    QCOMPARE(output[0],-1.0);
    QCOMPARE(output[1],-0.125);
    auto tamper=[&](std::size_t offset,std::uint8_t mask) {
        auto wrong=bytes;
        wrong[offset]^=mask;
        QVERIFY(!parse(wrong));
    };
    tamper(0,1);             // magic
    tamper(8,1);             // version
    tamper(10,1);            // encoding
    tamper(12,1);            // wrong rational L
    tamper(16,1);            // wrong rational M
    tamper(20,1);            // frozen T
    tamper(24,1);            // reserved
    tamper(28,1);            // N
    tamper(36,1);            // coefficient count
    tamper(44,1);            // payload bytes
    tamper(header+8*(n/2-1),1); // broken exact mirror
    auto nonfinite=bytes;
    put64(header+8*(n/2),std::bit_cast<std::uint64_t>(
        std::numeric_limits<double>::infinity()));
    QVERIFY(!parse(bytes));
    bytes=std::move(nonfinite);
    QVERIFY(!audio::internal::parse_phase11_rgsmsr1_candidate_bits(
        bytes,L,M,510)); // wrong T
    QVERIFY(!audio::internal::parse_phase11_rgsmsr1_candidate_bits(
        bytes,1,1,T)); // identity must not load FIR
    bytes.push_back(0);
    QVERIFY(!parse(bytes)); // no appended/truncated candidate
}

void PlaybackSampleRateAdapterTest::
phase11PinnedScalarReferenceSixBySixSyntheticGeometry()
{
    // All 36 ordered rate paths with a fake center-delta FIR in exact
    // RGSMSR1 bytes; this DOES NOT qualify real MPFR Kaiser coefficients,
    // continuous DTFT, or any product Final SRC admission.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    const std::array<double,3> source{-0.5,0.25,0.75};
    int identities=0,conversions=0;
    for(const auto sourceRate:rates)for(const auto masterRate:rates) {
        const auto common=std::gcd(sourceRate,masterRate);
        const auto L=masterRate/common;
        const auto M=sourceRate/common;
        std::uint32_t frozenT=512;
        const auto length=(static_cast<std::int64_t>(source.size())*L+M-1)/M;
        std::vector<double> whole(static_cast<std::size_t>(length),-17.0);
        std::vector<std::uint8_t> asset;
        std::string pin;
        if(sourceRate!=masterRate) {
            const auto frozen=audio::internal::phase11_frozen_shape(
                static_cast<std::uint32_t>(L),static_cast<std::uint32_t>(M));
            QVERIFY(frozen);
            frozenT=frozen->T;
            constexpr std::size_t header=52;
            const auto count=static_cast<std::size_t>(L)*frozenT+1;
            QCOMPARE(count,static_cast<std::size_t>(frozen->N));
            asset.assign(header+8*count,0);
            const std::array<std::uint8_t,8> magic{
                'R','G','S','M','S','R','1',0};
            std::copy(magic.begin(),magic.end(),asset.begin());
            const auto put=[&](std::size_t at,std::uint64_t value,
                               std::size_t countBytes) {
                for(std::size_t k=0;k<countBytes;++k)
                    asset[at+k]=static_cast<std::uint8_t>(
                        value>>(8*k));
            };
            put(8,1,2);put(10,1,2);
            put(12,static_cast<std::uint64_t>(L),4);
            put(16,static_cast<std::uint64_t>(M),4);
            put(20,frozenT,4);
            put(28,count,8);put(36,count,8);put(44,count*8,8);
            put(header+8*(count/2),
                std::bit_cast<std::uint64_t>(static_cast<double>(L)),8);
            pin=core::sha256_hex(std::span<const std::byte>{
                reinterpret_cast<const std::byte*>(asset.data()),
                asset.size()});
            ++conversions;
        } else {
            ++identities;
        }
        const auto reference=[&](std::span<double> dest,std::int64_t start,
                                 std::string_view sha) {
            return audio::internal::render_phase11_pinned_scalar_reference(
                source,dest,start,sourceRate,masterRate,frozenT,
                asset,sha);
        };
        const auto full=reference(whole,0,pin);
        QVERIFY2(full,full?"":full.error()->message().c_str());
        for(std::int64_t m=0;m<length;++m) {
            const auto numerator=m*M;
            double expected=0.0;
            if(numerator%L==0) {
                const auto src=numerator/L;
                expected=source[static_cast<std::size_t>(
                    src<static_cast<std::int64_t>(source.size())
                    ?src:2*static_cast<std::int64_t>(source.size())-1-src)]
                    *static_cast<double>(L);
            }
            QCOMPARE(whole[static_cast<std::size_t>(m)],expected);
        }
        std::vector<double> partitioned(whole.size(),-17.0);
        const auto split=length/2;
        QVERIFY(reference(
            std::span<double>{partitioned}.first(static_cast<std::size_t>(split)),
            0,pin));
        QVERIFY(reference(
            std::span<double>{partitioned}.subspan(static_cast<std::size_t>(split)),
            split,pin));
        QCOMPARE(partitioned.size(),whole.size());
        for(std::size_t i=0;i<whole.size();++i)
            QCOMPARE(std::bit_cast<std::uint64_t>(partitioned[i]),
                     std::bit_cast<std::uint64_t>(whole[i]));
        if(sourceRate!=masterRate) {
            // The exact format matches Q0 geometry. A forged caller-supplied
            // T cannot relax the preflight even if the SHA stays identical.
            QVERIFY(!audio::internal::render_phase11_pinned_scalar_reference(
                source,whole,0,sourceRate,masterRate,frozenT+2,
                asset,pin));
            auto wrong=pin;
            wrong[0]=wrong[0]=='0'?'1':'0';
            QVERIFY(!reference(whole,0,wrong));
            QVERIFY(!reference(whole,0,{}));
        } else {
            QCOMPARE(std::bit_cast<std::uint64_t>(whole[0]),
                     std::bit_cast<std::uint64_t>(source[0]));
            QVERIFY(!reference(whole,0,"fake-kernel-hash"));
        }
    }
    QCOMPARE(identities,6);
    QCOMPARE(conversions,30);
    std::array<double,1> refused{0.0};
    QVERIFY(!audio::internal::render_phase11_pinned_scalar_reference(
        source,refused,0,32000,96000,512,{},{}));
}

void PlaybackSampleRateAdapterTest::phase11StrictScalarReferenceSixBySixAgainstConceptualGrid()
{
    // 36 tested ordered pairs, including 30 REAL non-identity ratio GEOMETRIES.
    // Synthetic FIR taps only; NOT frozen 14 coefficient release or product SRC.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    const std::array<double,5> source{-0.0,0.25,-0.5,0.75,0.125};
    std::size_t checked=0,identityCount=0,filteredCount=0;
    for(const auto fin:rates) {
        for(const auto fout:rates) {
            const auto common=std::gcd(fin,fout);
            const auto L=fout/common;
            const auto M=fin/common;
            const auto total=(static_cast<std::int64_t>(source.size())*L+M-1)/M;
            std::vector<std::uint64_t> bits;
            if(fin!=fout) {
                const auto delay=std::max(L,M);
                bits.resize(static_cast<std::size_t>(2*delay+1),0);
                // Symmetric dyadic fixtures: exact long-double conceptual
                // zero-stuffed reference cannot disagree due to rounding.
                for(const auto& [offset,value]:
                    {std::pair{std::int64_t{0},0.5},
                     std::pair{std::int64_t{1},-0.125},
                     std::pair{L,0.125}}) {
                    const auto u=std::bit_cast<std::uint64_t>(value);
                    bits[static_cast<std::size_t>(delay-offset)]=u;
                    bits[static_cast<std::size_t>(delay+offset)]=u;
                }
                ++filteredCount;
            } else {
                ++identityCount;
            }
            std::vector<double> whole(static_cast<std::size_t>(total));
            auto done=audio::internal::render_phase11_strict_scalar_reference(
                source,std::span<double>{whole},0,L,M,bits);
            QVERIFY2(done,done?"":done.error()->message().c_str());
            if(fin==fout) {
                for(std::size_t i=0;i<source.size();++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>(whole[i]),
                             std::bit_cast<std::uint64_t>(source[i]));
            } else {
                const auto D=static_cast<std::int64_t>((bits.size()-1)/2);
                // Completely different j-ascending conceptual high-rate grid;
                // separate from production polyphase n-ascending indexing.
                for(std::int64_t m=0;m<total;++m) {
                    const auto q=D+m*M;
                    long double expected=0.0L;
                    for(std::size_t j=0;j<bits.size();++j) {
                        const auto high=q-static_cast<std::int64_t>(j);
                        if(high%L!=0)continue; // inserted high-grid zero
                        auto n=high/L;
                        const auto period=2*static_cast<std::int64_t>(source.size());
                        n%=period;
                        if(n<0)n+=period;
                        const auto sampleIndex=n<static_cast<std::int64_t>(source.size())
                            ? n : period-1-n;
                        expected+=static_cast<long double>(
                            source[static_cast<std::size_t>(sampleIndex)])
                            *static_cast<long double>(
                                std::bit_cast<double>(bits[j]));
                    }
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        whole[static_cast<std::size_t>(m)]),
                        std::bit_cast<std::uint64_t>(static_cast<double>(expected)));
                }
            }
            // Absolute output-phase partition invariance; no source reset or
            // local reflection after splitting a candidate at any frame.
            const auto split=total/2;
            std::vector<double> partitioned(whole.size(),123.0);
            auto first=audio::internal::render_phase11_strict_scalar_reference(
                source,std::span<double>{partitioned}.first(
                    static_cast<std::size_t>(split)),0,L,M,bits);
            auto second=audio::internal::render_phase11_strict_scalar_reference(
                source,std::span<double>{partitioned}.subspan(
                    static_cast<std::size_t>(split)),split,L,M,bits);
            QVERIFY(first);
            QVERIFY(second);
            for(std::size_t i=0;i<whole.size();++i)
                QCOMPARE(std::bit_cast<std::uint64_t>(partitioned[i]),
                         std::bit_cast<std::uint64_t>(whole[i]));
            ++checked;
        }
    }
    QCOMPARE(checked,std::size_t{36});
    QCOMPARE(identityCount,std::size_t{6});
    QCOMPARE(filteredCount,std::size_t{30});
    // Fail closed on invalid geometry, unsupported crop and nonfinite PCM.
    const std::array<double,2> badSource{
        0.25,std::numeric_limits<double>::quiet_NaN()};
    std::array<double,1> result{123.0};
    QVERIFY(!audio::internal::render_phase11_strict_scalar_reference(
        badSource,result,0,1,1,{}));
    QCOMPARE(result[0],123.0);
    QVERIFY(!audio::internal::render_phase11_strict_scalar_reference(
        source,result,99,1,1,{}));
    QVERIFY(!audio::internal::render_phase11_strict_scalar_reference(
        source,result,0,0,1,{}));
    QVERIFY(!audio::internal::render_phase11_strict_scalar_reference(
        source,result,0,1,1,std::array<std::uint64_t,1>{0}));
}


void PlaybackSampleRateAdapterTest::finalMasterMonitoringCannotRetargetMasterRate()
{
    // This is a non-audible CONTRACT test, not a 192->48 SRC claim.
    // M17/M18 master identity is selected separately from any physical
    // output device. A mismatched device must NOT trigger direct install.
    constexpr std::array<std::int64_t, 6> masters{
        44100, 48000, 88200, 96000, 176400, 192000
    };
    constexpr std::array<std::int64_t, 8> devices{
        32000, 44100, 48000, 88200, 96000, 176400, 192000, 384000
    };
    std::size_t direct=0,needsSrc=0;
    for(const auto masterHz:masters) {
        for(const auto deviceHz:devices) {
            const auto master=rate(masterHz);
            const auto device=rate(deviceHz);
            auto routed=audio::plan_master_monitoring_rate(master,device);
            QVERIFY(routed);
            QCOMPARE(routed.value()->final_master_rate,master);
            QCOMPARE(routed.value()->physical_device_rate,device);
            const auto exact=masterHz==deviceHz;
            QCOMPARE(routed.value()->can_install_master_pcm_directly,exact);
            QCOMPARE(routed.value()->monitoring_src_required,!exact);
            if(exact)++direct;
            else ++needsSrc;
        }
    }
    QCOMPARE(direct,std::size_t{6});
    QCOMPARE(needsSrc,std::size_t{42});
    // In particular a 192 kHz master + 48 kHz soundcard remains a
    // 192 kHz MASTER. Realtime monitoring SRC must be implemented
    // separately before it can be played on that device.
    const auto at192=audio::plan_master_monitoring_rate(
        rate(192000),rate(48000));
    QVERIFY(at192);
    QCOMPARE(at192.value()->final_master_rate.value(),std::int64_t{192000});
    QVERIFY(at192.value()->monitoring_src_required);
    QVERIFY(!at192.value()->can_install_master_pcm_directly);

    // A physical interface may support rates outside the six exported
    // master rates; this does not expand M17/M18 master-rate eligibility.
    auto invalidMaster=audio::plan_master_monitoring_rate(
        rate(32000),rate(32000));
    QVERIFY(!invalidMaster);
    QVERIFY(invalidMaster.error());
    QCOMPARE(invalidMaster.error()->code(),
             core::ErrorCode::UnsupportedOperation);
}

void PlaybackSampleRateAdapterTest::isolatedOfflineMonitoringPcmSixBySixMatrix()
{
    // Integration seam only: explicitly NOT a real DAC or M16 LIVE test.
    constexpr std::array<std::int64_t,6> supported{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t N=113;
    for(const auto masterHz:supported) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        for(std::size_t ch=0;ch<2;++ch) {
            auto channel=master.value()->mutable_view().channel(ch);
            QVERIFY(channel);
            for(std::int64_t i=0;i<N;++i)
                (*channel.value())[static_cast<std::size_t>(i)] =
                    (i%23==0?-0.34:0.2)*(ch==0?1.0:-0.6);
        }
        const auto beforeL=channel_bits(*master.value(),0);
        const auto beforeR=channel_bits(*master.value(),1);
        for(const auto deviceHz:supported) {
            auto monitor=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(deviceHz),frame_count(31));
            QVERIFY2(monitor,monitor?"":monitor.error()->message().c_str());
            QCOMPARE(monitor.value()->immutable_master_rate.value(),masterHz);
            QCOMPARE(monitor.value()->immutable_master_frames.value(),N);
            QCOMPARE(monitor.value()->device_rate.value(),deviceHz);
            QCOMPARE(monitor.value()->device_src_applied,masterHz!=deviceHz);
            QVERIFY(audio::validate_monitoring_pcm_candidate(*monitor.value()));
            const auto pcm=monitor.value()->device_pcm.view();
            QCOMPARE(pcm.timebase().frame_domain_id(),
                audio::FrameDomainId::OUTPUT_RATE);
            QCOMPARE(pcm.format().sample_rate().value(),deviceHz);
            const auto expected=(N/masterHz)*deviceHz+
                ((N%masterHz)*deviceHz+masterHz-1)/masterHz;
            QCOMPARE(pcm.frame_count().value(),expected);
            for(std::size_t ch=0;ch<2;++ch) {
                auto lane=pcm.channel(ch);
                QVERIFY(lane);
                for(const double x:*lane.value()) QVERIFY(std::isfinite(x));
            }
            if(masterHz==deviceHz) {
                QCOMPARE(channel_bits(monitor.value()->device_pcm,0),beforeL);
                QCOMPARE(channel_bits(monitor.value()->device_pcm,1),beforeR);
            }
            QCOMPARE(channel_bits(*master.value(),0),beforeL);
            QCOMPARE(channel_bits(*master.value(),1),beforeR);
        }
        auto wrongDomain=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(wrongDomain);
        QVERIFY(!audio::materialize_monitoring_pcm_candidate(
            wrongDomain.value()->view(),rate(48000),frame_count(31)));
    }
    // An arbitrary OS device rate does not silently expand the experimental
    // monitor converter's six-rate matrix or the immutable master contract.
    auto fmt=audio::AudioFormat::create(
        rate(192000),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto master=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},frame_count(1));
    QVERIFY(master);
    QVERIFY(!audio::materialize_monitoring_pcm_candidate(
        master.value()->view(),rate(32000),frame_count(31)));
    // Forged and stale device ownership never reaches Qt for installation.
    auto stereoFormat=audio::AudioFormat::create(
        rate(192000),audio::ChannelLayout::STEREO_LR);
    QVERIFY(stereoFormat);
    auto owner=audio::AudioBuffer::create(
        *stereoFormat.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},frame_count(193));
    QVERIFY(owner);
    auto valid=audio::materialize_monitoring_pcm_candidate(
        owner.value()->view(),rate(48000),frame_count(31));
    QVERIFY(valid);
    auto& forged=*valid.value();
    QVERIFY(audio::validate_monitoring_pcm_candidate(forged));
    forged.device_src_applied=false;
    QVERIFY(!audio::validate_monitoring_pcm_candidate(forged));
    forged.device_src_applied=true;
    forged.immutable_master_frames=frame_count(192);
    QVERIFY(!audio::validate_monitoring_pcm_candidate(forged));
    forged.immutable_master_frames=frame_count(193);
    forged.device_rate=rate(96000);
    QVERIFY(!audio::validate_monitoring_pcm_candidate(forged));
}

void PlaybackSampleRateAdapterTest::monitoringFrameMapSixRateEndpointsAndLoops()
{
    constexpr std::array<std::int64_t,6> six{
        44100,48000,88200,96000,176400,192000};
    for(const auto masterHz:six) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::MONO_C);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},
            frame_count(193));
        QVERIFY(master);
        for(const auto deviceHz:six) {
            auto candidate=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(deviceHz),frame_count(37));
            QVERIFY(candidate);
            auto map=audio::MonitoringFrameMap::create(*candidate.value());
            QVERIFY(map);
            QCOMPARE(map.value()->master_rate().value(),masterHz);
            QCOMPARE(map.value()->device_rate().value(),deviceHz);
            QCOMPARE(map.value()->master_frames().value(),std::int64_t{193});
            QCOMPARE(map.value()->device_frames(),
                candidate.value()->device_pcm.view().frame_count());
            auto last=map.value()->master_to_device(core::FrameIndex{193});
            QVERIFY(last);
            QCOMPARE(last.value()->value(),map.value()->device_frames().value());
            std::int64_t previous=0;
            for(const auto f:{0,1,2,48,96,192,193}) {
                auto index=map.value()->master_to_device(core::FrameIndex{f});
                QVERIFY(index);
                QVERIFY(index.value()->value()>=previous);
                previous=index.value()->value();
                auto inverse=map.value()->device_to_master_cursor(*index.value());
                QVERIFY(inverse);
                QVERIFY(inverse.value()->value()>=0);
                QVERIFY(inverse.value()->value()<=193);
            }
            auto full=core::FrameRange::create(
                core::FrameIndex{0},core::FrameIndex{193});
            QVERIFY(full);
            auto mappedRegion=map.value()->master_loop_to_device(*full.value());
            QVERIFY(mappedRegion);
            QCOMPARE(mappedRegion.value()->end().value(),
                map.value()->device_frames().value());
            QVERIFY(!map.value()->master_to_device(core::FrameIndex{194}));
            QVERIFY(!map.value()->device_to_master_cursor(
                core::FrameIndex{map.value()->device_frames().value()+1}));
        }
    }
    auto fmt=audio::AudioFormat::create(rate(192000),
        audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto master=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},frame_count(193));
    QVERIFY(master);
    auto down=audio::materialize_monitoring_pcm_candidate(
        master.value()->view(),rate(48000),frame_count(29));
    QVERIFY(down);
    auto map=audio::MonitoringFrameMap::create(*down.value());
    QVERIFY(map);
    auto tooSmall=core::FrameRange::create(
        core::FrameIndex{1},core::FrameIndex{2});
    QVERIFY(tooSmall);
    QVERIFY(!map.value()->master_loop_to_device(*tooSmall.value()));
}

void PlaybackSampleRateAdapterTest::monitoringPrepareAdviceSixMasterRatesAndHeadroom()
{
    using Kind=audio::MonitoringMemoryAdviceKind;
    constexpr std::uint64_t GiB=UINT64_C(1024)*1024*1024;
    // A 3-minute stereo source: the proposed MASTER rate must NOT be
    // silently retimed by the 48 kHz monitoring output device.
    const auto sourceRate=rate(48000);
    const auto sourceFrames=frame_count(48000*180);
    auto advice=audio::advise_monitoring_master_rates_for_prepare(
        sourceRate,sourceFrames,rate(48000),
        audio::ChannelLayout::STEREO_LR,
        audio::MonitoringMemoryObservation{2*GiB,4*GiB,GiB/2});
    QVERIFY2(advice,advice?"":advice.error()->message().c_str());
    QCOMPARE(advice.value()->size(),std::size_t{6});
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    for(std::size_t i=0;i<rates.size();++i) {
        const auto& item=(*advice.value())[i];
        QCOMPARE(item.proposed_master_rate.value(),rates[i]);
        QCOMPARE(item.observed_device_rate.value(),std::int64_t{48000});
        QCOMPARE(item.estimated_master_frames.value(),rates[i]*180);
        QCOMPARE(item.resource.device_frames.value(),std::int64_t{48000*180});
        QCOMPARE(item.resource.master_pcm_bytes,
            static_cast<std::uint64_t>(rates[i]*180*16));
        QCOMPARE(item.resource.device_pcm_bytes,
            static_cast<std::uint64_t>(48000*180*16));
        QCOMPARE(item.resource.combined_pcm_bytes,
            static_cast<std::uint64_t>((rates[i]+48000)*180*16));
        QCOMPARE(item.memory_headroom_after_reserve_bytes,GiB);
        QCOMPARE(item.memory_observation.available_physical_bytes,2*GiB);
        QCOMPARE(item.memory_observation.available_commit_bytes,4*GiB);
        QCOMPARE(item.memory_observation.reserved_other_prepare_bytes,GiB/2);
        QVERIFY(!item.cpu_performance_qualified);
    }
    QCOMPARE((*advice.value())[0].kind,Kind::RECOMMENDED_MEMORY_ONLY);
    QCOMPARE((*advice.value())[5].kind,Kind::CAUTION_LIMITED_HEADROOM);
    // Initial Prepare must include experimental FIR scratch in the same
    // budget model as the subsequent fresh preallocation assessment.
    // Budget is deliberately set EXACTLY to the two-PCM lower bound: the
    // old PCM-only classifier would accept it, but rational-FIR scratch
    // makes that output genuinely too large for the reserved-safe budget.
    constexpr std::uint64_t MiB=UINT64_C(1024)*1024;
    const auto projected=audio::project_monitoring_pcm_resource_cost(
        rate(192000),frame_count(192000*180),rate(48000),
        audio::ChannelLayout::STEREO_LR);
    QVERIFY(projected);
    QVERIFY(projected.value()->experimental_fir_scratch_bytes>0);
    const auto available=projected.value()->combined_pcm_bytes+512*MiB;
    const audio::MonitoringMemoryObservation threshold{
        available,available,0};
    QCOMPARE(audio::classify_monitoring_memory(
        projected.value()->combined_pcm_bytes,threshold),
        Kind::CAUTION_LIMITED_HEADROOM);
    QCOMPARE(audio::classify_monitoring_projected_memory(
        *projected.value(),threshold,false),
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    auto thresholdAdvice=audio::advise_monitoring_master_rates_for_prepare(
        rate(192000),frame_count(192000*180),rate(48000),
        audio::ChannelLayout::STEREO_LR,threshold);
    QVERIFY(thresholdAdvice);
    QCOMPARE(thresholdAdvice.value()->size(),std::size_t{6});
    QCOMPARE((*thresholdAdvice.value())[5].kind,
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    // In contrast, a same-rate owner has no new FIR scratch budget.
    const auto identity=audio::project_monitoring_pcm_resource_cost(
        rate(48000),frame_count(48000*180),rate(48000),
        audio::ChannelLayout::STEREO_LR);
    QVERIFY(identity);
    QCOMPARE(identity.value()->experimental_fir_scratch_bytes,UINT64_C(0));
    QCOMPARE(audio::classify_monitoring_projected_memory(
        *identity.value(),threshold,false),
        audio::classify_monitoring_memory(
            identity.value()->combined_pcm_bytes,threshold));
    // A synthetic overflow must never wrap into a small, green footprint.
    auto overflow=*projected.value();
    overflow.combined_pcm_bytes=std::numeric_limits<std::uint64_t>::max();
    std::uint64_t overflowHeadroom=123;
    QCOMPARE(audio::classify_monitoring_projected_memory(
        overflow,threshold,false,&overflowHeadroom),
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    QCOMPARE(overflowHeadroom,UINT64_C(0));
    // Scarce headroom discourages high sample-rate FULL-DURATION PCM
    // without changing the selected master or claiming a CPU decision.
    auto scarce=audio::advise_monitoring_master_rates_for_prepare(
        sourceRate,sourceFrames,rate(48000),
        audio::ChannelLayout::STEREO_LR,
        audio::MonitoringMemoryObservation{GiB,2*GiB,0});
    QVERIFY(scarce);
    QCOMPARE((*scarce.value())[0].kind,Kind::RECOMMENDED_MEMORY_ONLY);
    QCOMPARE((*scarce.value())[5].kind,
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    auto fullyReserved=audio::advise_monitoring_master_rates_for_prepare(
        sourceRate,sourceFrames,rate(48000),
        audio::ChannelLayout::STEREO_LR,
        audio::MonitoringMemoryObservation{GiB,2*GiB,2*GiB});
    QVERIFY(fullyReserved);
    for(const auto& item:*fullyReserved.value())
        QCOMPARE(item.kind,Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
}

void PlaybackSampleRateAdapterTest::monitoringPrepareAdviceDurationRoundingAndUnknownState()
{
    using Kind=audio::MonitoringMemoryAdviceKind;
    auto unknown=audio::advise_monitoring_master_rates_for_prepare(
        rate(44100),frame_count(44101),rate(48000),
        audio::ChannelLayout::MONO_C,
        audio::MonitoringMemoryObservation{0,0,0});
    QVERIFY(unknown);
    QCOMPARE(unknown.value()->size(),std::size_t{6});
    QCOMPARE((*unknown.value())[1].estimated_master_frames.value(),
        std::int64_t{48002});
    QCOMPARE((*unknown.value())[5].estimated_master_frames.value(),
        std::int64_t{192005});
    for(const auto& item:*unknown.value()) {
        QCOMPARE(item.kind,Kind::UNKNOWN);
        QCOMPARE(item.memory_headroom_after_reserve_bytes,UINT64_C(0));
        QCOMPARE(item.memory_observation.available_physical_bytes,UINT64_C(0));
        QCOMPARE(item.memory_observation.available_commit_bytes,UINT64_C(0));
        QVERIFY(!item.cpu_performance_qualified);
    }
    // A commit query that fails must never produce optimistic guidance.
    auto noCommit=audio::advise_monitoring_master_rates_for_prepare(
        rate(48000),frame_count(48000),rate(48000),
        audio::ChannelLayout::STEREO_LR,
        audio::MonitoringMemoryObservation{
            UINT64_C(4)*1024*1024*1024,0,0});
    QVERIFY(noCommit);
    for(const auto& item:*noCommit.value())
        QCOMPARE(item.kind,Kind::UNKNOWN);
    auto unsupportedDevice=audio::advise_monitoring_master_rates_for_prepare(
        rate(48000),frame_count(48000),rate(32000),
        audio::ChannelLayout::STEREO_LR,
        audio::MonitoringMemoryObservation{
            UINT64_C(4)*1024*1024*1024,
            UINT64_C(4)*1024*1024*1024,0});
    QVERIFY(!unsupportedDevice);
    QCOMPARE(unsupportedDevice.error()->code(),
        core::ErrorCode::UnsupportedOperation);
}

void PlaybackSampleRateAdapterTest::monitoringPrepareAdviceBudgetBoundariesAndFreshObservations()
{
    using Kind=audio::MonitoringMemoryAdviceKind;
    using Observation=audio::MonitoringMemoryObservation;
    constexpr std::uint64_t MiB=UINT64_C(1024)*1024;
    constexpr std::uint64_t GiB=UINT64_C(1024)*1024*1024;
    // The smaller of physical and committable headroom wins. For four GiB
    // available, reserve 25% plus the caller's separate 512 MiB allocation.
    const Observation measured{4*GiB,5*GiB,512*MiB};
    std::uint64_t budget=0;
    QCOMPARE(audio::classify_monitoring_memory(0,measured,&budget),
        Kind::RECOMMENDED_MEMORY_ONLY);
    QCOMPARE(budget,GiB*5/2);
    const auto half=budget/2;
    QCOMPARE(audio::classify_monitoring_memory(half,measured),
        Kind::RECOMMENDED_MEMORY_ONLY);
    QCOMPARE(audio::classify_monitoring_memory(half+1,measured),
        Kind::CAUTION_LIMITED_HEADROOM);
    QCOMPARE(audio::classify_monitoring_memory(budget,measured),
        Kind::CAUTION_LIMITED_HEADROOM);
    QCOMPARE(audio::classify_monitoring_memory(budget+1,measured),
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    // Exactly the same result when the smaller OS counter is COMMIT.
    QCOMPARE(audio::classify_monitoring_memory(half,
        Observation{5*GiB,4*GiB,512*MiB}),
        Kind::RECOMMENDED_MEMORY_ONLY);
    // Missing OS data is NOT equivalent to zero PCM use or a green choice.
    budget=123;
    QCOMPARE(audio::classify_monitoring_memory(1,
        Observation{4*GiB,0,0},&budget),Kind::UNKNOWN);
    QCOMPARE(budget,UINT64_C(0));
    QCOMPARE(audio::classify_monitoring_memory(1,
        Observation{0,4*GiB,0}),Kind::UNKNOWN);
    // Underflow/overflow-safe arithmetic at the extreme reserve boundary.
    budget=123;
    QCOMPARE(audio::classify_monitoring_memory(1,
        Observation{256*MiB,256*MiB,0},&budget),
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    QCOMPARE(budget,UINT64_C(0));
    QCOMPARE(audio::classify_monitoring_memory(1,
        Observation{std::numeric_limits<std::uint64_t>::max(),
            std::numeric_limits<std::uint64_t>::max(),
            std::numeric_limits<std::uint64_t>::max()}),
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);

    // A prior Prepare recommendation must be recalculated against a fresh
    // OS snapshot; it is NOT a resource reservation or cached admission.
    const auto before=audio::advise_monitoring_master_rates_for_prepare(
        rate(48000),frame_count(48000*180),rate(48000),
        audio::ChannelLayout::STEREO_LR,Observation{4*GiB,5*GiB,0});
    const auto after=audio::advise_monitoring_master_rates_for_prepare(
        rate(48000),frame_count(48000*180),rate(48000),
        audio::ChannelLayout::STEREO_LR,Observation{GiB,2*GiB,0});
    const auto unavailable=audio::advise_monitoring_master_rates_for_prepare(
        rate(48000),frame_count(48000*180),rate(48000),
        audio::ChannelLayout::STEREO_LR,Observation{0,0,0});
    QVERIFY(before);
    QVERIFY(after);
    QVERIFY(unavailable);
    QCOMPARE(before.value()->size(),std::size_t{6});
    QCOMPARE(after.value()->size(),std::size_t{6});
    QCOMPARE(unavailable.value()->size(),std::size_t{6});
    QCOMPARE((*before.value())[5].kind,Kind::RECOMMENDED_MEMORY_ONLY);
    QCOMPARE((*after.value())[5].kind,
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    for(std::size_t i=0;i<6;++i) {
        QCOMPARE((*before.value())[i].resource.combined_pcm_bytes,
            (*after.value())[i].resource.combined_pcm_bytes);
        QCOMPARE((*unavailable.value())[i].kind,Kind::UNKNOWN);
        QCOMPARE((*unavailable.value())[i].memory_headroom_after_reserve_bytes,
            UINT64_C(0));
        QVERIFY(!(*before.value())[i].cpu_performance_qualified);
        QVERIFY(!(*after.value())[i].cpu_performance_qualified);
        QVERIFY(!(*unavailable.value())[i].cpu_performance_qualified);
    }
}

void PlaybackSampleRateAdapterTest::
monitoringImminentAllocationUsesFreshIncrementalBudget()
{
    using Kind=audio::MonitoringMemoryAdviceKind;
    using Observation=audio::MonitoringMemoryObservation;
    constexpr std::uint64_t MiB=UINT64_C(1024)*1024;
    // Master is already owned and charged by the OS counters. The
    // incremental device allocation is the only NEW PCM expense.
    const auto master=rate(192000);
    const auto frames=frame_count(192000*180);
    const auto first=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,rate(48000),audio::ChannelLayout::STEREO_LR,
        Observation{800*MiB,1024*MiB,0});
    QVERIFY(first);
    QCOMPARE(first.value()->resource.master_pcm_bytes,
        UINT64_C(192000)*180*2*sizeof(double));
    QCOMPARE(first.value()->resource.device_pcm_bytes,
        UINT64_C(48000)*180*2*sizeof(double));
    QVERIFY(first.value()->resource.experimental_fir_scratch_bytes>0);
    QCOMPARE(first.value()->kind,Kind::RECOMMENDED_MEMORY_ONLY);
    QCOMPARE(first.value()->observed_device_rate,rate(48000));
    QVERIFY(audio::validate_imminent_monitoring_preallocation(
        *first.value(),rate(48000),first.value()->resource));
    auto wrongRate=audio::validate_imminent_monitoring_preallocation(
        *first.value(),rate(44100),first.value()->resource);
    QVERIFY(!wrongRate);
    QCOMPARE(wrongRate.error()->code(),core::ErrorCode::InvalidState);
    auto changedResource=first.value()->resource;
    changedResource.device_pcm_bytes+=sizeof(double);
    auto wrongProjection=audio::validate_imminent_monitoring_preallocation(
        *first.value(),rate(48000),changedResource);
    QVERIFY(!wrongProjection);
    QCOMPARE(wrongProjection.error()->code(),core::ErrorCode::InvalidState);
    changedResource=first.value()->resource;
    ++changedResource.experimental_fir_scratch_bytes;
    auto wrongScratch=audio::validate_imminent_monitoring_preallocation(
        *first.value(),rate(48000),changedResource);
    QVERIFY(!wrongScratch);
    QCOMPARE(wrongScratch.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(!first.value()->existing_master_counted_as_new);
    QVERIFY(!first.value()->cpu_performance_qualified);
    QCOMPARE(audio::classify_monitoring_memory(
        first.value()->resource.combined_pcm_bytes,
        Observation{800*MiB,1024*MiB,0}),
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    const auto lower=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,rate(48000),audio::ChannelLayout::STEREO_LR,
        Observation{600*MiB,1024*MiB,0});
    const auto committed=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,rate(48000),audio::ChannelLayout::STEREO_LR,
        Observation{1024*MiB,600*MiB,0});
    const auto reserved=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,rate(48000),audio::ChannelLayout::STEREO_LR,
        Observation{800*MiB,1024*MiB,256*MiB});
    const auto missing=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,rate(48000),audio::ChannelLayout::STEREO_LR,
        Observation{0,1024*MiB,0});
    QVERIFY(lower);
    QVERIFY(committed);
    QVERIFY(reserved);
    QVERIFY(missing);
    QCOMPARE(lower.value()->kind,Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    QCOMPARE(committed.value()->kind,
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    QCOMPARE(reserved.value()->kind,
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    QCOMPARE(missing.value()->kind,Kind::UNKNOWN);
    // The allocation seam calls a caller-controlled fresh check BEFORE it
    // materializes DEVICE PCM. No implicit CPU/permission policy is chosen.
    auto fmt=audio::AudioFormat::create(
        rate(48000),audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto owned=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},
        frame_count(48));
    QVERIFY(owned);
    std::size_t checks=0;
    std::uint64_t observedMasterBytes=0;
    std::uint64_t observedDeviceBytes=0;
    auto veto=audio::materialize_monitoring_pcm_candidate(
        owned.value()->view(),rate(44100),frame_count(17),
        [&](const audio::MonitoringPcmResourceProjection& cost)
            ->core::Status {
            ++checks;
            observedMasterBytes=cost.master_pcm_bytes;
            observedDeviceBytes=cost.device_pcm_bytes;
            return core::Status::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "MONITOR_ALLOC_PREFLIGHT_TEST_VETO"});
        });
    QVERIFY(!veto);
    QCOMPARE(checks,std::size_t{1});
    QCOMPARE(observedMasterBytes,UINT64_C(48)*sizeof(double));
    QVERIFY(observedDeviceBytes>0);
    QCOMPARE(veto.error()->code(),core::ErrorCode::UnsupportedOperation);
    auto allowed=audio::materialize_monitoring_pcm_candidate(
        owned.value()->view(),rate(44100),frame_count(17),
        [&](const audio::MonitoringPcmResourceProjection&) -> core::Status {
            ++checks;
            return core::Status::success();
        });
    QVERIFY(allowed);
    QCOMPARE(checks,std::size_t{2});
    QCOMPARE(allowed.value()->immutable_master_rate,rate(48000));
    QCOMPARE(allowed.value()->device_rate,rate(44100));
    // Deterministic fault injection: simulate an allocation error without
    // exhausting the CI runner and prove master PCM is untouched.
    const auto beforeBits=channel_bits(*owned.value(),0);
    auto oom=audio::materialize_monitoring_pcm_candidate(
        owned.value()->view(),rate(44100),frame_count(17),
        [&](const audio::MonitoringPcmResourceProjection&)
            ->core::Status { throw std::bad_alloc{}; });
    QVERIFY(!oom);
    QCOMPARE(oom.error()->code(),core::ErrorCode::IoFailure);
    QCOMPARE(QString::fromStdString(oom.error()->message()),
        QStringLiteral("MONITOR_ALLOC_PREFLIGHT_OUT_OF_MEMORY"));
    QCOMPARE(channel_bits(*owned.value(),0),beforeBits);
}

void PlaybackSampleRateAdapterTest::
monitoringExplicitPreallocationDecisionContract()
{
    using Advice=audio::MonitoringPcmPreallocationAdvice;
    using Kind=audio::MonitoringMemoryAdviceKind;
    using Observation=audio::MonitoringMemoryObservation;
    constexpr std::uint64_t MiB=UINT64_C(1024)*1024;
    const auto master=rate(192000);
    const auto frames=frame_count(192000);
    const auto device=rate(48000);
    const auto layout=audio::ChannelLayout::STEREO_LR;
    const auto recommendation=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,device,layout,
        Observation{UINT64_C(4)*1024*MiB,UINT64_C(4)*1024*MiB,0});
    QVERIFY(recommendation);
    QCOMPARE(recommendation.value()->kind,Kind::RECOMMENDED_MEMORY_ONLY);
    const auto plan=recommendation.value()->resource;
    std::size_t callbackCount=0;
    const std::function<core::Status(const Advice&)> noDecision{};
    const auto missing=audio::evaluate_imminent_monitoring_preallocation(
        *recommendation.value(),device,plan,noDecision);
    QVERIFY(!missing);
    QCOMPARE(missing.error()->code(),core::ErrorCode::InvalidArgument);

    // The exact same projection is mandatory: neither a retargeted
    // device nor a new FIR scratch size may reach the policy callback.
    const auto accept=[&](const Advice&)->core::Status {
        ++callbackCount;
        return core::Status::success();
    };
    const auto otherDevice=audio::evaluate_imminent_monitoring_preallocation(
        *recommendation.value(),rate(44100),plan,accept);
    QVERIFY(!otherDevice);
    QCOMPARE(otherDevice.error()->code(),core::ErrorCode::InvalidState);
    auto changed=plan;
    changed.experimental_fir_scratch_bytes+=sizeof(double);
    const auto otherScratch=audio::evaluate_imminent_monitoring_preallocation(
        *recommendation.value(),device,changed,accept);
    QVERIFY(!otherScratch);
    QCOMPARE(otherScratch.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(callbackCount,std::size_t{0});

    const auto accepted=audio::evaluate_imminent_monitoring_preallocation(
        *recommendation.value(),device,plan,accept);
    QVERIFY(accepted);
    QCOMPARE(callbackCount,std::size_t{1});

    // A policy rejection is propagated unchanged; it cannot be turned
    // into a successful allocation by the experimental Windows seam.
    const auto deny=[&](const Advice&)->core::Status {
        ++callbackCount;
        return core::Status::failure(core::Error{
            core::ErrorCode::UnsupportedOperation,
            "MONITOR_ALLOC_POLICY_TEST_REFUSED"});
    };
    const auto rejected=audio::evaluate_imminent_monitoring_preallocation(
        *recommendation.value(),device,plan,deny);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(),core::ErrorCode::UnsupportedOperation);
    QCOMPARE(callbackCount,std::size_t{2});

    const auto needed=plan.device_pcm_bytes
        +plan.experimental_fir_scratch_bytes;
    constexpr std::uint64_t reserve=UINT64_C(512)*MiB;
    const auto caution=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,device,layout,
        Observation{reserve+needed,reserve+needed,0});
    const auto discouraged=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,device,layout,
        Observation{reserve,reserve,0});
    const auto unknown=audio::advise_imminent_monitoring_pcm_allocation(
        master,frames,device,layout,
        Observation{0,reserve+needed,0});
    QVERIFY(caution);
    QVERIFY(discouraged);
    QVERIFY(unknown);
    QCOMPARE(caution.value()->kind,Kind::CAUTION_LIMITED_HEADROOM);
    QCOMPARE(discouraged.value()->kind,
        Kind::DISCOURAGED_INSUFFICIENT_HEADROOM);
    QCOMPARE(unknown.value()->kind,Kind::UNKNOWN);
    // All three states must be seen by the explicit decision. No hidden
    // fail-open, false recommendation, or hard veto is inferred here.
    for(const auto* observed:{caution.value(),discouraged.value(),
                              unknown.value()}) {
        Kind forwarded=Kind::RECOMMENDED_MEMORY_ONLY;
        const auto checked=audio::evaluate_imminent_monitoring_preallocation(
            *observed,device,plan,[&](const Advice& input)->core::Status {
                forwarded=input.kind;
                return core::Status::failure(core::Error{
                    core::ErrorCode::UnsupportedOperation,
                    "MONITOR_ALLOC_POLICY_EXPLICIT_HOLD"});
            });
        QVERIFY(!checked);
        QCOMPARE(checked.error()->code(),core::ErrorCode::UnsupportedOperation);
        QCOMPARE(forwarded,observed->kind);
    }
}

void PlaybackSampleRateAdapterTest::monitoringVirtualLongTrackResourceMatrix()
{
    // Predict duration, owned full-track PCM footprint and experimental
    // FIR MAC count WITHOUT allocating even one long-track sample.
    // A projection is NOT measured process RSS or performance.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t seconds=180;
    std::size_t checked=0;
    for(const auto inputHz:rates) {
        const auto inputFrames=frame_count(inputHz*seconds);
        for(const auto deviceHz:rates) {
            auto cost=audio::project_monitoring_pcm_resource_cost(
                rate(inputHz),inputFrames,rate(deviceHz),
                audio::ChannelLayout::STEREO_LR);
            QVERIFY2(cost,cost?"":cost.error()->message().c_str());
            QCOMPARE(cost.value()->device_frames.value(),
                deviceHz*seconds);
            const auto inBytes=static_cast<std::uint64_t>(
                inputHz*seconds*2*sizeof(double));
            const auto outBytes=static_cast<std::uint64_t>(
                deviceHz*seconds*2*sizeof(double));
            QCOMPARE(cost.value()->master_pcm_bytes,inBytes);
            QCOMPARE(cost.value()->device_pcm_bytes,outBytes);
            QCOMPARE(cost.value()->combined_pcm_bytes,inBytes+outBytes);
            const bool rational=inputHz!=deviceHz
                && !(inputHz==44100&&deviceHz==48000)
                && !(inputHz==48000&&deviceHz==44100);
            QCOMPARE(cost.value()->experimental_fir_mac_operations.has_value(),
                     rational);
            if(rational) {
                const auto taps=static_cast<std::uint64_t>(
                    std::max<std::int64_t>(128,
                        (128*inputHz+deviceHz-1)/deviceHz));
                const auto evenTaps=taps+(taps%2);
                QCOMPARE(*cost.value()->experimental_fir_mac_operations,
                    static_cast<std::uint64_t>(deviceHz*seconds)
                        *2*evenTaps);
                const auto phases=static_cast<std::uint64_t>(
                    deviceHz/std::gcd(inputHz,deviceHz));
                QCOMPARE(cost.value()->experimental_fir_scratch_bytes,
                    (phases+1)*evenTaps*sizeof(double));
            } else {
                QCOMPARE(cost.value()->experimental_fir_scratch_bytes,
                    UINT64_C(0));
            }
            ++checked;
        }
    }
    QCOMPARE(checked,std::size_t{36});
    // A five-minute 192 -> 48 stereo monitor owns a second PCM
    // allocation; 1,152,000,000 is a two-buffer LOWER bound in bytes.
    auto fiveMinutes=audio::project_monitoring_pcm_resource_cost(
        rate(192000),frame_count(192000*300),rate(48000),
        audio::ChannelLayout::STEREO_LR);
    QVERIFY(fiveMinutes);
    QCOMPARE(fiveMinutes.value()->master_pcm_bytes,
        UINT64_C(921600000));
    QCOMPARE(fiveMinutes.value()->device_pcm_bytes,
        UINT64_C(230400000));
    QCOMPARE(fiveMinutes.value()->combined_pcm_bytes,
        UINT64_C(1152000000));
    QVERIFY(fiveMinutes.value()->experimental_fir_mac_operations);
    QCOMPARE(*fiveMinutes.value()->experimental_fir_mac_operations,
        UINT64_C(14745600000));
}

void PlaybackSampleRateAdapterTest::monitoringResourcePreflightFailsOnOverflow()
{
    // Out-of-range virtual tracks are rejected BEFORE allocating the
    // device-owned PCM; neither the master nor Windows playback is touched.
    const auto maxFrames=std::numeric_limits<std::int64_t>::max();
    auto impossibleBytes=audio::project_monitoring_pcm_resource_cost(
        rate(192000),frame_count(maxFrames/8+1),rate(192000),
        audio::ChannelLayout::MONO_C);
    QVERIFY(!impossibleBytes);
    QCOMPARE(impossibleBytes.error()->code(),core::ErrorCode::IntegerOverflow);
    auto impossibleFrames=audio::project_monitoring_pcm_resource_cost(
        rate(44100),frame_count(maxFrames/3),rate(192000),
        audio::ChannelLayout::STEREO_LR);
    QVERIFY(!impossibleFrames);
    QCOMPARE(impossibleFrames.error()->code(),
        core::ErrorCode::IntegerOverflow);
    auto wrongDevice=audio::project_monitoring_pcm_resource_cost(
        rate(48000),frame_count(48000),rate(32000),
        audio::ChannelLayout::MONO_C);
    QVERIFY(!wrongDevice);
    QCOMPARE(wrongDevice.error()->code(),
        core::ErrorCode::UnsupportedOperation);
    auto validIdentity=audio::project_monitoring_pcm_resource_cost(
        rate(44100),frame_count(44100),rate(44100),
        audio::ChannelLayout::MONO_C);
    QVERIFY(validIdentity);
    QCOMPARE(validIdentity.value()->combined_pcm_bytes,
        UINT64_C(705600));
    QVERIFY(!validIdentity.value()->experimental_fir_mac_operations);
}

void PlaybackSampleRateAdapterTest::monitoringOneSecondStereoDurationAndOwnershipSmoke()
{
    // One-second functional resource smoke, not a long-track memory/perf
    // qualification. Coverage spans upsample, downsample, frozen FIR.
    constexpr double pi=3.141592653589793238462643383279502884;
    constexpr std::array<std::pair<std::int64_t,std::int64_t>,3> cases{{
        {44100,192000},{192000,48000},{48000,44100}
    }};
    for(const auto& [masterHz,deviceHz]:cases) {
        auto fmt=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(fmt);
        auto master=audio::AudioBuffer::create(
            *fmt.value(),audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(masterHz));
        QVERIFY(master);
        for(std::size_t ch=0;ch<2;++ch) {
            auto lane=master.value()->mutable_view().channel(ch);
            QVERIFY(lane);
            for(std::int64_t i=0;i<masterHz;++i) {
                const auto sine=0.25*std::sin(2.0*pi*997.0*
                    static_cast<double>(i)/static_cast<double>(masterHz));
                const auto transient=i%9973==0?0.5:0.0;
                (*lane.value())[static_cast<std::size_t>(i)]=
                    (sine+transient)*(ch==0?1.0:-0.6);
            }
        }
        const auto originalLeft=channel_bits(*master.value(),0);
        const auto originalRight=channel_bits(*master.value(),1);
        auto predicted=audio::project_monitoring_pcm_resource_cost(
            rate(masterHz),frame_count(masterHz),rate(deviceHz),
            audio::ChannelLayout::STEREO_LR);
        QVERIFY(predicted);
        QElapsedTimer microprofile;
        microprofile.start();
        auto candidate=audio::materialize_monitoring_pcm_candidate(
            master.value()->view(),rate(deviceHz),frame_count(1024));
        const auto elapsedMs=microprofile.elapsed();
        QVERIFY2(candidate,candidate?"":candidate.error()->message().c_str());
        QCOMPARE(candidate.value()->device_pcm.view().frame_count(),
                 predicted.value()->device_frames);
        qInfo().noquote() << "MONITOR_MICROPROFILE_UNQUALIFIED"
                          << masterHz << "to" << deviceHz
                          << "frames" << deviceHz
                          << "elapsed_ms" << elapsedMs
                          << "min_pcm_bytes" << predicted.value()->combined_pcm_bytes;
        QVERIFY(audio::validate_monitoring_pcm_candidate(*candidate.value()));
        QCOMPARE(candidate.value()->immutable_master_frames.value(),masterHz);
        QCOMPARE(candidate.value()->immutable_master_rate.value(),masterHz);
        QCOMPARE(candidate.value()->device_rate.value(),deviceHz);
        QCOMPARE(candidate.value()->device_pcm.view().frame_count().value(),
            deviceHz);
        QCOMPARE(candidate.value()->device_src_applied,masterHz!=deviceHz);
        for(std::size_t ch=0;ch<2;++ch) {
            auto output=candidate.value()->device_pcm.view().channel(ch);
            QVERIFY(output);
            for(const auto sample:*output.value()) {
                QVERIFY(std::isfinite(sample));
                QVERIFY2(std::abs(sample)<2.0,
                    "MONITOR_ONE_SECOND_PCM_MAGNITUDE_UNEXPECTED");
            }
        }
        QCOMPARE(channel_bits(*master.value(),0),originalLeft);
        QCOMPARE(channel_bits(*master.value(),1),originalRight);
    }
}

void PlaybackSampleRateAdapterTest::monitoringNonFiniteInputFailsClosedForSixRateFamilies()
{
    // NaN or +/-Inf in either master stereo lane must fail BEFORE routing
    // a separately SRC'd listening PCM, even on the identity-rate path.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    const std::array<double,3> invalid{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for(const auto masterHz:rates) {
        auto fmt=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(fmt);
        auto master=audio::AudioBuffer::create(
            *fmt.value(),audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(97));
        QVERIFY(master);
        for(std::size_t ch=0;ch<2;++ch) {
            auto lane=master.value()->mutable_view().channel(ch);
            QVERIFY(lane);
            std::fill(lane.value()->begin(),lane.value()->end(),0.125);
        }
        for(std::size_t ch=0;ch<2;++ch) {
            auto lane=master.value()->mutable_view().channel(ch);
            QVERIFY(lane);
            for(const auto bad:invalid) {
                (*lane.value())[37]=bad;
                for(const auto deviceHz:{44100,48000,96000,192000}) {
                    auto rejected=audio::materialize_monitoring_pcm_candidate(
                        master.value()->view(),rate(deviceHz),frame_count(31));
                    QVERIFY(!rejected);
                    QCOMPARE(rejected.error()->code(),
                        core::ErrorCode::InvalidAudioSample);
                }
                (*lane.value())[37]=0.125;
            }
        }
    }
}

void PlaybackSampleRateAdapterTest::monitoringOversizedOfflineBlockRejectsWithoutMutatingMaster()
{
    // Bound every input-rate family before touching any SRC work. No
    // partial device candidate may escape from an invalid block request.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t oversized=
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames+1;
    for(const auto masterHz:rates) {
        auto fmt=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::MONO_C);
        QVERIFY(fmt);
        auto master=audio::AudioBuffer::create(
            *fmt.value(),audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(129));
        QVERIFY(master);
        auto lane=master.value()->mutable_view().channel(0);
        QVERIFY(lane);
        for(std::int64_t i=0;i<129;++i)
            (*lane.value())[static_cast<std::size_t>(i)]=
                i%11==0?-0.5:0.25;
        const auto before=channel_bits(*master.value(),0);
        for(const auto deviceHz:rates) {
            auto rejected=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(deviceHz),frame_count(oversized));
            QVERIFY(!rejected);
            QCOMPARE(rejected.error()->code(),
                core::ErrorCode::InvalidArgument);
            QCOMPARE(channel_bits(*master.value(),0),before);
        }
    }
}

void PlaybackSampleRateAdapterTest::monitoringPassbandGridSixBySixScreen()
{
    // Coarse spectral screening of the actual isolated MASTER-monitor entry:
    // 6x6 rates x 3 tones; no realtime/DAC, THD, passband ripple or LIVE gate.
    constexpr double pi=3.141592653589793238462643383279502884;
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    constexpr std::array<double,3> tones{180.0,1400.0,6400.0};
    constexpr std::int64_t N=2304;
    std::size_t verified=0;
    for(const auto masterHz:rates) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::MONO_C);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        for(const auto toneHz:tones) {
            auto lane=master.value()->mutable_view().channel(0);
            QVERIFY(lane);
            for(std::int64_t i=0;i<N;++i)
                (*lane.value())[static_cast<std::size_t>(i)]=
                    0.4*std::sin(2.0*pi*toneHz*
                        static_cast<double>(i)/static_cast<double>(masterHz));
            const auto masterBits=channel_bits(*master.value(),0);
            for(const auto deviceHz:rates) {
                auto candidate=audio::materialize_monitoring_pcm_candidate(
                    master.value()->view(),rate(deviceHz),frame_count(97));
                QVERIFY2(candidate,
                    candidate?"":candidate.error()->message().c_str());
                QVERIFY(audio::validate_monitoring_pcm_candidate(*candidate.value()));
                auto converted=candidate.value()->device_pcm.view().channel(0);
                QVERIFY(converted);
                const auto frames=candidate.value()->device_pcm.view()
                    .frame_count().value();
                const auto trim=std::min<std::int64_t>(160,frames/8);
                QVERIFY(frames>2*trim);
                double signalPower=0.0;
                double errorPower=0.0;
                for(std::int64_t i=trim;i<frames-trim;++i) {
                    const double ideal=0.4*std::sin(2.0*pi*toneHz*
                        static_cast<double>(i)/static_cast<double>(deviceHz));
                    const double actual=(*converted.value())[
                        static_cast<std::size_t>(i)];
                    const double delta=actual-ideal;
                    signalPower+=ideal*ideal;
                    errorPower+=delta*delta;
                }
                QVERIFY(signalPower>0.0);
                // Deliberately lenient (<8% relative RMS) exploratory
                // screen, not a mastering-grade spectral acceptance limit.
                QVERIFY2(errorPower/signalPower<0.0064,
                    "MONITOR_PASSBAND_GRID_RELATIVE_RMS_SCREEN_FAILED");
                QCOMPARE(channel_bits(*master.value(),0),masterBits);
                ++verified;
            }
        }
    }
    QCOMPARE(verified,std::size_t{108});
}

void PlaybackSampleRateAdapterTest::monitoringExtendedStopbandScreen()
{
    // These tones are above the target Nyquist but below source Nyquist.
    // This only rejects gross alias leakage; exhaustive alias tests and
    // transition-band response are independent pending qualification gates.
    constexpr double pi=3.141592653589793238462643383279502884;
    constexpr std::array<std::array<std::int64_t,3>,6> cases{{
        {88200,44100,30000},
        {96000,44100,30000},
        {176400,88200,60000},
        {192000,96000,60000},
        {192000,48000,36000},
        {192000,44100,36000}
    }};
    constexpr std::int64_t N=4096;
    for(const auto& test:cases) {
        const auto masterHz=test[0],deviceHz=test[1],toneHz=test[2];
        QVERIFY(toneHz>deviceHz/2 && toneHz<masterHz/2);
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::MONO_C);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        auto input=master.value()->mutable_view().channel(0);
        QVERIFY(input);
        for(std::int64_t i=0;i<N;++i)
            (*input.value())[static_cast<std::size_t>(i)]=
                0.4*std::sin(2.0*pi*static_cast<double>(toneHz)*
                    static_cast<double>(i)/static_cast<double>(masterHz));
        const auto before=channel_bits(*master.value(),0);
        auto monitor=audio::materialize_monitoring_pcm_candidate(
            master.value()->view(),rate(deviceHz),frame_count(113));
        QVERIFY2(monitor,monitor?"":monitor.error()->message().c_str());
        auto output=monitor.value()->device_pcm.view().channel(0);
        QVERIFY(output);
        const auto frames=monitor.value()->device_pcm.view().frame_count().value();
        const auto trim=std::min<std::int64_t>(128,frames/8);
        QVERIFY(frames>2*trim);
        double power=0.0;
        for(std::int64_t i=trim;i<frames-trim;++i) {
            const double x=(*output.value())[static_cast<std::size_t>(i)];
            power+=x*x;
        }
        const double rms=std::sqrt(power/static_cast<double>(frames-2*trim));
        QVERIFY2(rms<0.4/std::sqrt(2.0)*0.03,
            "MONITOR_EXTENDED_STOPBAND_RMS_SCREEN_FAILED");
        QCOMPARE(channel_bits(*master.value(),0),before);
    }
}

void PlaybackSampleRateAdapterTest::monitoringStereoImpulseSeparationSixBySix()
{
    // Independent left/right impulses, far apart in the input timeline:
    // no channel swaps, cross-channel contamination or gross time offset.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t N=2048;
    constexpr std::int64_t leftAt=512,rightAt=1536;
    for(const auto masterHz:rates) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        auto left=master.value()->mutable_view().channel(0);
        auto right=master.value()->mutable_view().channel(1);
        QVERIFY(left && right);
        std::fill(left.value()->begin(),left.value()->end(),0.0);
        std::fill(right.value()->begin(),right.value()->end(),0.0);
        (*left.value())[static_cast<std::size_t>(leftAt)]=0.6;
        (*right.value())[static_cast<std::size_t>(rightAt)]=-0.4;
        const auto originalL=channel_bits(*master.value(),0);
        const auto originalR=channel_bits(*master.value(),1);
        for(const auto deviceHz:rates) {
            auto monitor=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(deviceHz),frame_count(89));
            QVERIFY2(monitor,monitor?"":monitor.error()->message().c_str());
            auto actualL=monitor.value()->device_pcm.view().channel(0);
            auto actualR=monitor.value()->device_pcm.view().channel(1);
            QVERIFY(actualL && actualR);
            const auto size=actualL.value()->size();
            QCOMPARE(size,actualR.value()->size());
            std::size_t peakL=0,peakR=0;
            double powerL=0.0,powerR=0.0;
            for(std::size_t i=0;i<size;++i) {
                const auto x=std::abs((*actualL.value())[i]);
                const auto y=std::abs((*actualR.value())[i]);
                if(x>powerL){powerL=x;peakL=i;}
                if(y>powerR){powerR=y;peakR=i;}
            }
            QVERIFY2(powerL>0.02 && powerR>0.02,
                "MONITOR_STEREO_IMPULSE_MISSING");
            const auto expectedL=static_cast<std::int64_t>(
                std::llround(static_cast<double>(leftAt)*deviceHz/masterHz));
            const auto expectedR=static_cast<std::int64_t>(
                std::llround(static_cast<double>(rightAt)*deviceHz/masterHz));
            QVERIFY2(std::abs(static_cast<std::int64_t>(peakL)-expectedL)<=3
                && std::abs(static_cast<std::int64_t>(peakR)-expectedR)<=3,
                "MONITOR_STEREO_IMPULSE_POSITION_SCREEN_FAILED");
            QVERIFY2(std::abs((*actualR.value())[peakL])<1e-12
                && std::abs((*actualL.value())[peakR])<1e-12,
                "MONITOR_STEREO_CROSSTALK_SCREEN_FAILED");
            QCOMPARE(channel_bits(*master.value(),0),originalL);
            QCOMPARE(channel_bits(*master.value(),1),originalR);
        }
    }
}

void PlaybackSampleRateAdapterTest::monitoringExperimentalPassbandAndAliasScreen()
{
    // SCREENING ONLY: independent mathematical sine reference. This is
    // not the spectral acceptance gate or a real-device listening test.
    // Frozen 44.1<->48 FIR kernels are deliberately excluded.
    constexpr double pi=3.141592653589793238462643383279502884;
    constexpr std::int64_t N=4096;
    constexpr std::array<std::pair<std::int64_t,std::int64_t>,6> passband{{
        {44100,96000},{48000,88200},{88200,44100},
        {96000,48000},{176400,96000},{192000,48000}
    }};
    for(const auto& [masterHz,deviceHz]:passband) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::MONO_C);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        auto input=master.value()->mutable_view().channel(0);
        QVERIFY(input);
        for(std::int64_t i=0;i<N;++i)
            (*input.value())[static_cast<std::size_t>(i)]=
                0.4*std::sin(2.0*pi*1000.0*
                    static_cast<double>(i)/static_cast<double>(masterHz));
        const auto masterBits=channel_bits(*master.value(),0);
        auto converted=audio::materialize_monitoring_pcm_candidate(
            master.value()->view(),rate(deviceHz),frame_count(127));
        QVERIFY2(converted,converted?"":converted.error()->message().c_str());
        auto samples=converted.value()->device_pcm.view().channel(0);
        QVERIFY(samples);
        const auto outputFrames=converted.value()->device_pcm.view()
            .frame_count().value();
        const auto trim=std::min<std::int64_t>(128,outputFrames/8);
        QVERIFY(outputFrames>2*trim);
        double referencePower=0.0;
        double residualPower=0.0;
        for(std::int64_t i=trim;i<outputFrames-trim;++i) {
            const auto t=static_cast<double>(i)/
                static_cast<double>(deviceHz);
            const double reference=0.4*std::sin(2.0*pi*1000.0*t);
            const auto measured=(*samples.value())[static_cast<std::size_t>(i)];
            const auto residual=measured-reference;
            referencePower+=reference*reference;
            residualPower+=residual*residual;
        }
        QVERIFY(referencePower>0.0);
        // Coarse relative RMS error guard (<5%). Does NOT constitute
        // flatness, THD, phase, imaging or platform audio qualification.
        QVERIFY2(residualPower/referencePower<0.0025,
            "EXPERIMENTAL_MONITOR_1KHZ_PASSBAND_SCREEN_FAILED");
        QCOMPARE(channel_bits(*master.value(),0),masterBits);
    }

    constexpr std::array<std::pair<std::int64_t,std::int64_t>,2> stopband{{
        {96000,44100},{192000,48000}
    }};
    for(const auto& [masterHz,deviceHz]:stopband) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::MONO_C);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        auto input=master.value()->mutable_view().channel(0);
        QVERIFY(input);
        constexpr double toneHz=36000.0;
        for(std::int64_t i=0;i<N;++i)
            (*input.value())[static_cast<std::size_t>(i)]=
                0.4*std::sin(2.0*pi*toneHz*
                    static_cast<double>(i)/static_cast<double>(masterHz));
        const auto masterBits=channel_bits(*master.value(),0);
        auto converted=audio::materialize_monitoring_pcm_candidate(
            master.value()->view(),rate(deviceHz),frame_count(193));
        QVERIFY2(converted,converted?"":converted.error()->message().c_str());
        auto samples=converted.value()->device_pcm.view().channel(0);
        QVERIFY(samples);
        const auto frames=converted.value()->device_pcm.view()
            .frame_count().value();
        const auto trim=std::min<std::int64_t>(128,frames/8);
        QVERIFY(frames>2*trim);
        double outputPower=0.0;
        for(std::int64_t i=trim;i<frames-trim;++i) {
            const auto x=(*samples.value())[static_cast<std::size_t>(i)];
            outputPower+=x*x;
        }
        // Input sine RMS is 0.4/sqrt(2). Reject clearly unsuppressed
        // stopband leakage (>2% RMS), without claiming a full alias gate.
        const double rms=std::sqrt(
            outputPower/static_cast<double>(frames-2*trim));
        QVERIFY2(rms<0.4/std::sqrt(2.0)*0.02,
            "EXPERIMENTAL_MONITOR_36KHZ_ALIAS_SCREEN_FAILED");
        QCOMPARE(channel_bits(*master.value(),0),masterBits);
    }
}

void PlaybackSampleRateAdapterTest::monitoringFullSixBySixSignalBlockInvariance()
{
    // 36 actual monitoring paths with a transient-rich non-DC signal.
    // Offline processing partition must not affect PCM samples at any rate.
    // This is NOT spectral quality, physical DAC or M16 LIVE qualification.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t N=193;
    std::size_t verified=0;
    for(const auto masterHz:rates) {
        auto format=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto master=audio::AudioBuffer::create(
            *format.value(),audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(master);
        for(std::size_t ch=0;ch<2;++ch) {
            auto samples=master.value()->mutable_view().channel(ch);
            QVERIFY(samples);
            for(std::int64_t i=0;i<N;++i) {
                const double transient=i%37==0?-0.687:
                    i%19==0?0.493:i%7==0?-0.217:0.131;
                (*samples.value())[static_cast<std::size_t>(i)]=
                    transient*(ch==0?1.0:-0.625);
            }
        }
        const auto beforeLeft=channel_bits(*master.value(),0);
        const auto beforeRight=channel_bits(*master.value(),1);
        for(const auto deviceHz:rates) {
            auto small=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(deviceHz),frame_count(7));
            auto large=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(deviceHz),frame_count(257));
            QVERIFY2(small,small?"":small.error()->message().c_str());
            QVERIFY2(large,large?"":large.error()->message().c_str());
            QVERIFY(audio::validate_monitoring_pcm_candidate(*small.value()));
            QVERIFY(audio::validate_monitoring_pcm_candidate(*large.value()));
            QCOMPARE(small.value()->immutable_master_rate.value(),masterHz);
            QCOMPARE(large.value()->immutable_master_rate.value(),masterHz);
            QCOMPARE(small.value()->device_rate.value(),deviceHz);
            QCOMPARE(large.value()->device_rate.value(),deviceHz);
            QCOMPARE(small.value()->device_src_applied,masterHz!=deviceHz);
            QCOMPARE(large.value()->device_src_applied,masterHz!=deviceHz);
            QCOMPARE(small.value()->device_pcm.view().frame_count(),
                large.value()->device_pcm.view().frame_count());
            const auto expected=(N/masterHz)*deviceHz+
                ((N%masterHz)*deviceHz+masterHz-1)/masterHz;
            QCOMPARE(small.value()->device_pcm.view().frame_count().value(),
                expected);
            for(std::size_t ch=0;ch<2;++ch) {
                QCOMPARE(channel_bits(small.value()->device_pcm,ch),
                         channel_bits(large.value()->device_pcm,ch));
                if(masterHz==deviceHz)
                    QCOMPARE(channel_bits(small.value()->device_pcm,ch),
                             ch==0?beforeLeft:beforeRight);
            }
            QCOMPARE(channel_bits(*master.value(),0),beforeLeft);
            QCOMPARE(channel_bits(*master.value(),1),beforeRight);
            ++verified;
        }
    }
    QCOMPARE(verified,std::size_t{36});
}

void PlaybackSampleRateAdapterTest::monitoring44100SixDeviceRatesRemainBlockInvariant()
{
    // Actual final-master monitoring entrypoint, not generic SOURCE SRC.
    // Different offline block sizes must never change listening PCM.
    // Neither render has M16 LIVE/M17 terminal certification.
    constexpr std::array<std::int64_t,6> devices{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t N=441;
    auto format=audio::AudioFormat::create(
        rate(44100),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    auto master=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},frame_count(N));
    QVERIFY(master);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=master.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        for(std::int64_t i=0;i<N;++i) {
            const double sample=i%17==0?-0.4:i%13==0?0.3:0.2;
            (*lane.value())[static_cast<std::size_t>(i)] =
                sample*(ch==0?1.0:-0.375);
        }
    }
    const auto beforeLeft=channel_bits(*master.value(),0);
    const auto beforeRight=channel_bits(*master.value(),1);
    for(const auto deviceHz:devices) {
        auto small=audio::materialize_monitoring_pcm_candidate(
            master.value()->view(),rate(deviceHz),frame_count(7));
        auto large=audio::materialize_monitoring_pcm_candidate(
            master.value()->view(),rate(deviceHz),frame_count(257));
        QVERIFY2(small,small?"":small.error()->message().c_str());
        QVERIFY2(large,large?"":large.error()->message().c_str());
        QVERIFY(audio::validate_monitoring_pcm_candidate(*small.value()));
        QVERIFY(audio::validate_monitoring_pcm_candidate(*large.value()));
        QCOMPARE(small.value()->immutable_master_rate.value(),
            std::int64_t{44100});
        QCOMPARE(large.value()->immutable_master_rate.value(),
            std::int64_t{44100});
        QCOMPARE(small.value()->immutable_master_frames.value(),N);
        QCOMPARE(large.value()->immutable_master_frames.value(),N);
        QCOMPARE(small.value()->device_rate.value(),deviceHz);
        QCOMPARE(large.value()->device_rate.value(),deviceHz);
        QCOMPARE(small.value()->device_src_applied,deviceHz!=44100);
        QCOMPARE(large.value()->device_src_applied,deviceHz!=44100);
        QCOMPARE(small.value()->device_pcm.view().frame_count(),
                 large.value()->device_pcm.view().frame_count());
        for(std::size_t ch=0;ch<2;++ch)
            QCOMPARE(channel_bits(small.value()->device_pcm,ch),
                     channel_bits(large.value()->device_pcm,ch));
        QCOMPARE(channel_bits(*master.value(),0),beforeLeft);
        QCOMPARE(channel_bits(*master.value(),1),beforeRight);
    }
}

void PlaybackSampleRateAdapterTest::monitoringBorrowsMasterWithoutSourceAlias()
{
    // Compare the new borrowed OUTPUT-domain path bitwise against the
    // historical SOURCE scratch path. Frozen 44.1/48 FIR stays identical.
    // No real-time, spectral or Windows device qualification is inferred.
    constexpr std::array<std::pair<std::int64_t,std::int64_t>,4> pairs{{
        {44100,48000},{48000,44100},{192000,48000},{96000,96000}
    }};
    constexpr std::int64_t N=193;
    for(const auto& [masterHz,deviceHz]:pairs) {
        auto fmt=audio::AudioFormat::create(
            rate(masterHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(fmt);
        auto master=audio::AudioBuffer::create(*fmt.value(),
            audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},
            frame_count(N));
        auto source=audio::AudioBuffer::create(*fmt.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},
            frame_count(N));
        QVERIFY(master && source);
        for(std::size_t ch=0;ch<2;++ch) {
            auto out=master.value()->mutable_view().channel(ch);
            auto scratch=source.value()->mutable_view().channel(ch);
            QVERIFY(out && scratch);
            for(std::int64_t i=0;i<N;++i) {
                const double x=(i%19==0?-0.817:0.21)*(ch==0?1.0:-0.5);
                (*out.value())[static_cast<std::size_t>(i)]=x;
                (*scratch.value())[static_cast<std::size_t>(i)]=x;
            }
        }
        const auto beforeL=channel_bits(*master.value(),0);
        const auto beforeR=channel_bits(*master.value(),1);
        auto expected=audio::materialize_full_resource_output_rate_candidate(
            source.value()->view(),rate(deviceHz),frame_count(17));
        QVERIFY(expected);
        auto direct=audio::materialize_output_master_monitoring_rate_candidate(
            master.value()->view(),rate(deviceHz),frame_count(17));
        QVERIFY(direct);
        QCOMPARE(channel_bits(direct.value()->pcm,0),
            channel_bits(expected.value()->pcm,0));
        QCOMPARE(channel_bits(direct.value()->pcm,1),
            channel_bits(expected.value()->pcm,1));
        QCOMPARE(channel_bits(*master.value(),0),beforeL);
        QCOMPARE(channel_bits(*master.value(),1),beforeR);
        QVERIFY(!audio::materialize_full_resource_output_rate_candidate(
            master.value()->view(),rate(deviceHz),frame_count(17)));
        QVERIFY(!audio::materialize_output_master_monitoring_rate_candidate(
            source.value()->view(),rate(deviceHz),frame_count(17)));
        auto owned=audio::materialize_monitoring_pcm_candidate(
            master.value()->view(),rate(deviceHz),frame_count(17));
        QVERIFY(owned);
        QCOMPARE(channel_bits(owned.value()->device_pcm,0),
            channel_bits(expected.value()->pcm,0));
        QCOMPARE(channel_bits(owned.value()->device_pcm,1),
            channel_bits(expected.value()->pcm,1));
    }
}

void PlaybackSampleRateAdapterTest::monitoringMasterSnapshotFailClosedAcrossSixRatePairs()
{
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    for(const auto src:rates) {
        auto fmt=audio::AudioFormat::create(rate(src),audio::ChannelLayout::MONO_C);
        QVERIFY(fmt);
        auto master=audio::AudioBuffer::create(*fmt.value(),
            audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},frame_count(193));
        QVERIFY(master);
        for(const auto dst:rates) {
            auto candidate=audio::materialize_monitoring_pcm_candidate(
                master.value()->view(),rate(dst),frame_count(64));
            QVERIFY(candidate);
            auto map=audio::MonitoringFrameMap::create(*candidate.value());
            QVERIFY(map);
            core::PlaybackSnapshot snapshot;
            snapshot.state=core::PlaybackState::STOPPED;
            snapshot.duration=map.value()->device_frames();
            snapshot.position=core::FrameIndex{map.value()->device_frames().value()};
            auto display=map.value()->project_display_snapshot(snapshot,std::nullopt);
            QVERIFY(display);
            QCOMPARE(display.value()->master_display_position.value(),
                std::int64_t{193});
            QCOMPARE(display.value()->master_duration.value(),std::int64_t{193});
            QCOMPARE(display.value()->device_rate.value(),dst);
            QCOMPARE(display.value()->master_rate.value(),src);
            auto masterLoop=core::FrameRange::create(
                core::FrameIndex{0},core::FrameIndex{193});
            QVERIFY(masterLoop);
            auto deviceLoop=map.value()->master_loop_to_device(*masterLoop.value());
            QVERIFY(deviceLoop);
            snapshot.loop=*deviceLoop.value();
            auto valid=map.value()->project_display_snapshot(snapshot,*masterLoop.value());
            QVERIFY(valid);
            QVERIFY(valid.value()->requested_master_loop.has_value());
            QCOMPARE(*valid.value()->requested_master_loop,*masterLoop.value());
            QVERIFY(!map.value()->project_display_snapshot(snapshot,std::nullopt));
            snapshot.loop.reset();
            QVERIFY(!map.value()->project_display_snapshot(snapshot,*masterLoop.value()));
            // 386 may be a VALID device duration (e.g. 96 -> 192 kHz).
            // Mutate the actual expected duration instead of assuming a
            // constant is invalid across the six-by-six conversion matrix.
            snapshot.duration=frame_count(
                map.value()->device_frames().value()+1);
            QVERIFY(!map.value()->project_display_snapshot(snapshot,std::nullopt));
            snapshot.duration=map.value()->device_frames();
            snapshot.audibleRealization.realizationId=core::RealizationId{77};
            QVERIFY(!map.value()->project_display_snapshot(snapshot,std::nullopt));
        }
    }
}

void PlaybackSampleRateAdapterTest::offlineCandidateMatchesFrozenPlaybackKernelBitwise()
{
    for(const auto hz: {44100,48000}) {
        const auto target=hz==44100?48000:44100;
        constexpr std::int64_t N=441;
        std::vector<std::uint64_t> interleaved;
        interleaved.reserve(static_cast<std::size_t>(N*2));
        auto format=audio::AudioFormat::create(
            rate(hz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto pcm=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(pcm);
        auto left=pcm.value()->mutable_view().channel(0);
        auto right=pcm.value()->mutable_view().channel(1);
        QVERIFY(left&&right);
        for(std::int64_t i=0;i<N;++i) {
            const double l=i==0?4.0:(i%17==0?-0.75:0.125);
            const double rr=i==N-1?-2.0:(i%13==0?0.8125:-0.0625);
            (*left.value())[static_cast<std::size_t>(i)]=l;
            (*right.value())[static_cast<std::size_t>(i)]=rr;
            interleaved.push_back(std::bit_cast<std::uint64_t>(l));
            interleaved.push_back(std::bit_cast<std::uint64_t>(rr));
        }
        auto reader=open_f64_stereo(static_cast<std::uint32_t>(hz),interleaved);
        auto playback=adapter(hz,target,N);
        auto golden=output_buffer(playback,0,
            playback.output_frame_count().value());
        QVERIFY(playback.read_frames(*reader,core::FrameIndex{0},
            golden.mutable_view()));

        for(const auto block: {1,17,257,1024}) {
            auto candidate=audio::materialize_full_resource_output_rate_candidate(
                pcm.value()->view(),rate(target),frame_count(block));
            QVERIFY2(candidate,candidate?"":candidate.error()->message().c_str());
            QVERIFY(candidate.value()->used_src);
            QCOMPARE(candidate.value()->source_frames.value(),N);
            QCOMPARE(candidate.value()->pcm.view().format().sample_rate().value(),
                std::int64_t{target});
            QCOMPARE(candidate.value()->pcm.view().frame_count(),
                playback.output_frame_count());
            QCOMPARE(candidate.value()->pcm.view().timebase().frame_domain_id(),
                audio::FrameDomainId::OUTPUT_RATE);
            QCOMPARE(candidate.value()->kernel_sha256,playback.kernel_sha256());
            for(std::size_t channel=0;channel<2;++channel)
                QCOMPARE(channel_bits(candidate.value()->pcm,channel),
                         channel_bits(golden,channel));
        }
        auto srcCandidate=audio::materialize_full_resource_output_rate_candidate(
            pcm.value()->view(),rate(hz),frame_count(17));
        QVERIFY(srcCandidate);
        QVERIFY(!srcCandidate.value()->used_src);
        for(std::size_t channel=0;channel<2;++channel)
            QCOMPARE(channel_bits(srcCandidate.value()->pcm,channel),
                     channel_bits(*pcm.value(),channel));

        auto candidate96=audio::materialize_full_resource_output_rate_candidate(
            pcm.value()->view(),rate(96000),frame_count(17));
        QVERIFY(candidate96);
        QVERIFY(candidate96.value()->used_src);
        auto overBlock=audio::materialize_full_resource_output_rate_candidate(
            pcm.value()->view(),rate(target),frame_count(1025));
        QVERIFY(!overBlock);
        auto segment=pcm.value()->view().subview(
            core::FrameIndex{1},frame_count(N-1));
        QVERIFY(segment);
        auto partial=audio::materialize_full_resource_output_rate_candidate(
            *segment.value(),rate(target),frame_count(17));
        QVERIFY(!partial);
        QCOMPARE(channel_bits(*pcm.value(),0).size(),std::size_t{N});
    }
}


void PlaybackSampleRateAdapterTest::finalOutputExperimentalSpectralScreenSixBySix()
{
    // Prequalification screen of the ACTUAL Final Output SOURCE -> MASTER
    // candidate (not merely the monitoring wrapper). These bounds detect
    // gross passband/alias failures; they are NOT a mastering-grade ripple,
    // transition-band, true-peak, listening or physical Windows certificate.
    // Frozen 44.1 <-> 48 kHz kernels are excluded and remain bitwise frozen.
    constexpr double pi=3.141592653589793238462643383279502884;
    constexpr std::array<std::int64_t,6> hz{
        44100,48000,88200,96000,176400,192000};
    constexpr std::array<double,3> passHz{250.0,2800.0,11800.0};
    constexpr std::int64_t inputFrames=3072;
    std::size_t screenedPassbands=0;
    std::size_t screenedStopbands=0;
    for(const auto sourceHz:hz) {
        auto fmt=audio::AudioFormat::create(
            rate(sourceHz),audio::ChannelLayout::MONO_C);
        QVERIFY(fmt);
        auto source=audio::AudioBuffer::create(
            *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(inputFrames));
        QVERIFY(source);
        auto lane=source.value()->mutable_view().channel(0);
        QVERIFY(lane);
        for(const auto masterHz:hz) {
            const bool frozen=(sourceHz==44100&&masterHz==48000)
                ||(sourceHz==48000&&masterHz==44100);
            if(sourceHz==masterHz||frozen)continue;
            const auto expectedFrames=
                (inputFrames*masterHz+sourceHz-1)/sourceHz;
            // Exclude reflected transient edges in OUTPUT coordinates,
            // accounting for the experimental FIR's source-rate tap span.
            const double ratio=std::max(
                1.0,static_cast<double>(sourceHz)/masterHz);
            auto taps=static_cast<std::int64_t>(
                std::ceil(128.0*ratio));
            taps+=taps%2;
            const auto trim=std::max<std::int64_t>(
                96,static_cast<std::int64_t>(std::ceil(
                    0.65*static_cast<double>(taps)*masterHz/sourceHz))+16);
            QVERIFY(expectedFrames>2*trim+64);
            for(const auto tone:passHz) {
                // These tones are strictly inside every output passband.
                QVERIFY(tone<0.8*masterHz/2.0);
                for(std::int64_t i=0;i<inputFrames;++i)
                    (*lane.value())[static_cast<std::size_t>(i)]=
                        0.4*std::sin(2.0*pi*tone*i/sourceHz);
                const auto before=channel_bits(*source.value(),0);
                auto candidate=audio::materialize_full_resource_output_rate_candidate(
                    source.value()->view(),rate(masterHz),frame_count(127));
                QVERIFY2(candidate,candidate?"":candidate.error()->message().c_str());
                QVERIFY(candidate.value()->used_src);
                QVERIFY(candidate.value()->kernel_sha256.empty());
                QCOMPARE(candidate.value()->source_rate,rate(sourceHz));
                QCOMPARE(candidate.value()->output_rate,rate(masterHz));
                QCOMPARE(candidate.value()->pcm.view().frame_count().value(),
                         expectedFrames);
                auto samples=candidate.value()->pcm.view().channel(0);
                QVERIFY(samples);
                double referencePower=0.0;
                double residualPower=0.0;
                for(std::int64_t i=trim;i<expectedFrames-trim;++i) {
                    const double ideal=0.4*std::sin(
                        2.0*pi*tone*i/masterHz);
                    const double error=(*samples.value())[
                        static_cast<std::size_t>(i)]-ideal;
                    referencePower+=ideal*ideal;
                    residualPower+=error*error;
                }
                QVERIFY(referencePower>0.0);
                QVERIFY2(residualPower/referencePower<0.0009,
                    "FINAL_MASTER_RATIONAL_FIR_PASSBAND_RMS_SCREEN_FAILED");
                QCOMPARE(channel_bits(*source.value(),0),before);
                ++screenedPassbands;
            }
            if(sourceHz>masterHz) {
                // Above target Nyquist and below original Nyquist:
                // a pure source sine must not alias at appreciable level.
                // Mid-stopband only; transitions are still unqualified.
                const double stopTone=0.25*
                    static_cast<double>(masterHz+sourceHz);
                QVERIFY(stopTone>masterHz/2.0);
                QVERIFY(stopTone<sourceHz/2.0);
                for(std::int64_t i=0;i<inputFrames;++i)
                    (*lane.value())[static_cast<std::size_t>(i)]=
                        0.4*std::sin(2.0*pi*stopTone*i/sourceHz);
                const auto before=channel_bits(*source.value(),0);
                auto candidate=audio::materialize_full_resource_output_rate_candidate(
                    source.value()->view(),rate(masterHz),frame_count(113));
                QVERIFY2(candidate,candidate?"":candidate.error()->message().c_str());
                QCOMPARE(candidate.value()->pcm.view().frame_count().value(),
                         expectedFrames);
                auto samples=candidate.value()->pcm.view().channel(0);
                QVERIFY(samples);
                double power=0.0;
                for(std::int64_t i=trim;i<expectedFrames-trim;++i) {
                    const double value=(*samples.value())[
                        static_cast<std::size_t>(i)];
                    power+=value*value;
                }
                const double rms=std::sqrt(
                    power/static_cast<double>(expectedFrames-2*trim));
                QVERIFY2(rms<0.4/std::sqrt(2.0)*0.03,
                    "FINAL_MASTER_RATIONAL_FIR_STOPBAND_RMS_SCREEN_FAILED");
                QCOMPARE(channel_bits(*source.value(),0),before);
                ++screenedStopbands;
            }
        }
    }
    QCOMPARE(screenedPassbands,std::size_t{84}); // 28 new pairs x 3 tones
    QCOMPARE(screenedStopbands,std::size_t{14}); // 15 down, minus frozen pair
}


void PlaybackSampleRateAdapterTest::fullResourceSrcCancellationAbortsBeforePublication()
{
    // An immutable SOURCE is never replaced by an unfinished Final Output,
    // including cancellation in a long rational FIR or a frozen 44.1/48
    // block loop. The output remains private; no half-filled PCM escapes.
    struct RateCase {std::int64_t source; std::int64_t target;};
    constexpr std::array<RateCase,4> cases{{
        {192000,48000}, // experimental downsample, 512 taps
        {44100,192000}, // experimental upsample
        {44100,48000},  // frozen playback kernels, unchanged FP math
        {48000,48000}   // bit-exact identity copy with bounded checks
    }};
    for(const auto row:cases) {
        auto format=audio::AudioFormat::create(
            rate(row.source),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto source=audio::AudioBuffer::create(
            *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(2049));
        QVERIFY(source);
        for(std::size_t channel=0;channel<2;++channel) {
            auto lane=source.value()->mutable_view().channel(channel);
            QVERIFY(lane);
            for(std::size_t i=0;i<2049;++i)
                (*lane.value())[i]=(i%31==0 ? -0.4 : 0.125)
                    *(channel==0?1.0:-0.5);
        }
        const auto left=channel_bits(*source.value(),0);
        const auto right=channel_bits(*source.value(),1);
        std::size_t polls=0;
        const audio::FullResourceOutputRateCancel before=[&] {
            ++polls;
            return true;
        };
        auto early=audio::materialize_full_resource_output_rate_candidate(
            source.value()->view(),rate(row.target),frame_count(17),before);
        QVERIFY(!early);
        QCOMPARE(early.error()->code(),core::ErrorCode::InvalidState);
        QCOMPARE(polls,std::size_t{1});
        polls=0;
        const audio::FullResourceOutputRateCancel during=[&] {
            return ++polls>=5;
        };
        auto stopped=audio::materialize_full_resource_output_rate_candidate(
            source.value()->view(),rate(row.target),frame_count(17),during);
        QVERIFY(!stopped);
        QCOMPARE(stopped.error()->code(),core::ErrorCode::InvalidState);
        QVERIFY(polls>=std::size_t{5});
        QCOMPARE(channel_bits(*source.value(),0),left);
        QCOMPARE(channel_bits(*source.value(),1),right);

        const audio::FullResourceOutputRateCancel never=[] {return false;};
        auto baseline=audio::materialize_full_resource_output_rate_candidate(
            source.value()->view(),rate(row.target),frame_count(17));
        auto observed=audio::materialize_full_resource_output_rate_candidate(
            source.value()->view(),rate(row.target),frame_count(17),never);
        QVERIFY(baseline&&observed);
        QCOMPARE(channel_bits(baseline.value()->pcm,0),
                 channel_bits(observed.value()->pcm,0));
        QCOMPARE(channel_bits(baseline.value()->pcm,1),
                 channel_bits(observed.value()->pcm,1));
        QCOMPARE(channel_bits(*source.value(),0),left);
        QCOMPARE(channel_bits(*source.value(),1),right);
    }
    // Monitoring uses the same bounded SRC but borrows an immutable
    // OUTPUT_RATE master instead of a SOURCE_PROCESSING_RATE precursor.
    auto format=audio::AudioFormat::create(
        rate(192000),audio::ChannelLayout::MONO_C);
    QVERIFY(format);
    auto master=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},frame_count(2049));
    QVERIFY(master);
    auto lane=master.value()->mutable_view().channel(0);
    QVERIFY(lane);
    std::fill(lane.value()->begin(),lane.value()->end(),0.25);
    const auto original=channel_bits(*master.value(),0);
    std::size_t checks=0;
    const audio::FullResourceOutputRateCancel stop=[&]{
        return ++checks>=5;
    };
    auto rejected=audio::materialize_output_master_monitoring_rate_candidate(
        master.value()->view(),rate(48000),frame_count(17),stop);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(checks>=std::size_t{5});
    QCOMPARE(channel_bits(*master.value(),0),original);
}

void PlaybackSampleRateAdapterTest::experimentalFullResourceSixBySixRateMatrix()
{
    // NONQUALIFYING functional test; spectral and physical gates pending.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    constexpr std::int64_t N=97;
    std::size_t verified=0;
    for(const auto srcHz:rates) {
        auto format=audio::AudioFormat::create(
            rate(srcHz),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto pcm=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(N));
        QVERIFY(pcm);
        auto l=pcm.value()->mutable_view().channel(0);
        auto r=pcm.value()->mutable_view().channel(1);
        QVERIFY(l&&r);
        for(std::int64_t i=0;i<N;++i) {
            (*l.value())[static_cast<std::size_t>(i)]=0.125;
            (*r.value())[static_cast<std::size_t>(i)]=-0.25;
        }
        for(const auto dstHz:rates) {
            auto a=audio::materialize_full_resource_output_rate_candidate(
                pcm.value()->view(),rate(dstHz),frame_count(1));
            auto b=audio::materialize_full_resource_output_rate_candidate(
                pcm.value()->view(),rate(dstHz),frame_count(257));
            QVERIFY2(a,a?"":a.error()->message().c_str());
            QVERIFY2(b,b?"":b.error()->message().c_str());
            const auto frames=(N*dstHz+srcHz-1)/srcHz;
            QCOMPARE(a.value()->pcm.view().frame_count().value(),frames);
            QCOMPARE(b.value()->pcm.view().frame_count().value(),frames);
            QCOMPARE(a.value()->used_src,srcHz!=dstHz);
            for(std::size_t ch=0;ch<2;++ch) {
                QCOMPARE(channel_bits(a.value()->pcm,ch),
                         channel_bits(b.value()->pcm,ch));
                auto samples=a.value()->pcm.view().channel(ch);
                QVERIFY(samples);
                const double expected=ch==0?0.125:-0.25;
                // The two historical 44.1/48 kernels have their own
                // frozen bitwise oracle and must not be requalified by a
                // stricter NEW-kernel DC tolerance in this matrix.
                const bool frozenPair=(srcHz==44100&&dstHz==48000)
                    || (srcHz==48000&&dstHz==44100);
                for(const double v:*samples.value()) {
                    QVERIFY2(std::isfinite(v),
                        "All 36 SRC pairs must generate finite output.");
                    if(!frozenPair)
                        QVERIFY2(std::abs(v-expected)<2e-12,
                            "New rational SRC must preserve constant PCM.");
                }
            }
            ++verified;
        }
    }
    QCOMPARE(verified,std::size_t{36});
}

void PlaybackSampleRateAdapterTest::policyIdentityAndFrameMap()
{
    auto identity = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(44'100),
            rate(44'100),
            audio::ChannelLayout::STEREO_LR,
            frame_count(441),
        });
    QVERIFY(!identity);
    QCOMPARE(identity.error()->code(), core::ErrorCode::UnsupportedOperation);
    QCOMPARE(
        QString::fromStdString(identity.error()->message()),
        QStringLiteral("PLAYBACK_RATE_PAIR_UNSUPPORTED_V1"));

    auto unsupported = audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{
            rate(96'000),
            rate(48'000),
            audio::ChannelLayout::STEREO_LR,
            frame_count(960),
        });
    QVERIFY(!unsupported);
    QCOMPARE(unsupported.error()->code(), core::ErrorCode::UnsupportedOperation);

    auto forward = adapter(44'100, 48'000, 441);
    QCOMPARE(forward.interpolation_factor(), std::int64_t{160});
    QCOMPARE(forward.decimation_factor(), std::int64_t{147});
    QCOMPARE(forward.output_frame_count().value(), std::int64_t{480});
    QCOMPARE(
        forward.map_input_frame_to_output(core::FrameIndex{1}).value()->value(),
        std::int64_t{2});
    QCOMPARE(
        forward.map_output_frame_to_input_cursor(core::FrameIndex{160})
            .value()
            ->value(),
        std::int64_t{147});

    auto interval = core::FrameRange::create(
        core::FrameIndex{10}, core::FrameIndex{20});
    QVERIFY(interval);
    auto mapped = forward.map_input_range_to_output(*interval.value());
    QVERIFY(mapped);
    QCOMPARE(mapped.value()->begin().value(), std::int64_t{11});
    QCOMPARE(mapped.value()->end().value(), std::int64_t{22});

    auto reverse = adapter(48'000, 44'100, 480);
    QCOMPARE(reverse.interpolation_factor(), std::int64_t{147});
    QCOMPARE(reverse.decimation_factor(), std::int64_t{160});
    QCOMPARE(reverse.output_frame_count().value(), std::int64_t{441});
}

void PlaybackSampleRateAdapterTest::assetsAreFrozenAndSymmetric()
{
    auto forward = adapter(44'100, 48'000, 10);
    QCOMPARE(forward.kernel_size(), std::size_t{30'721});
    QCOMPARE(forward.group_delay_high_rate_frames(), std::int64_t{15'360});
    QCOMPARE(
        QString::fromUtf8(forward.kernel_sha256()),
        QStringLiteral(
            "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4"));
    for (std::size_t index = 0; index < forward.kernel_size(); ++index) {
        QCOMPARE(
            forward.coefficient_bits(index),
            forward.coefficient_bits(forward.kernel_size() - 1U - index));
    }

    auto reverse = adapter(48'000, 44'100, 10);
    QCOMPARE(reverse.kernel_size(), std::size_t{28'225});
    QCOMPARE(reverse.group_delay_high_rate_frames(), std::int64_t{14'112});
    QCOMPARE(
        QString::fromUtf8(reverse.kernel_sha256()),
        QStringLiteral(
            "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22"));
    for (std::size_t index = 0; index < reverse.kernel_size(); ++index) {
        QCOMPARE(
            reverse.coefficient_bits(index),
            reverse.coefficient_bits(reverse.kernel_size() - 1U - index));
    }
}

void PlaybackSampleRateAdapterTest::constantAndChunkInvariance()
{
    constexpr std::int64_t kInputFrames = 900;
    std::vector<std::uint64_t> interleaved;
    interleaved.reserve(static_cast<std::size_t>(kInputFrames) * 2U);
    for (std::int64_t frame = 0; frame < kInputFrames; ++frame) {
        interleaved.push_back(std::bit_cast<std::uint64_t>(0.25));
        interleaved.push_back(std::bit_cast<std::uint64_t>(-0.5));
    }

    auto control = std::make_shared<ReaderControl>();
    auto wholeReader = open_f64_stereo(44'100, interleaved, control);
    auto converter = adapter(44'100, 48'000, kInputFrames);
    const auto outputFrames = converter.output_frame_count().value();
    QVERIFY(outputFrames <= audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    auto whole = output_buffer(converter, 0, outputFrames);
    QVERIFY(converter.read_frames(
        *wholeReader, core::FrameIndex{0}, whole.mutable_view()));
    const auto wholeLeft = channel_bits(whole, 0);
    const auto wholeRight = channel_bits(whole, 1);

    const std::vector<std::int64_t> partition{
        1, 17, 257, outputFrames - 275};
    auto partitionReader = open_f64_stereo(44'100, interleaved);
    const auto partitionLeft = render_partitioned(
        converter, *partitionReader, partition, 0);
    auto secondPartitionReader = open_f64_stereo(44'100, interleaved);
    const auto partitionRight = render_partitioned(
        converter, *secondPartitionReader, partition, 1);
    QCOMPARE(partitionLeft, wholeLeft);
    QCOMPARE(partitionRight, wholeRight);
    QVERIFY(control->largestReadRequest <= 4096U);

    auto left = whole.view().channel(0);
    auto right = whole.view().channel(1);
    QVERIFY(left);
    QVERIFY(right);
    for (std::size_t frame = 0; frame < left.value()->size(); ++frame) {
        QVERIFY(std::abs((*left.value())[frame] - 0.25) < 2.0e-6);
        QVERIFY(std::abs((*right.value())[frame] + 0.5) < 4.0e-6);
    }
}

void PlaybackSampleRateAdapterTest::oneFrameBoundaryAndStereoIndependence()
{
    const std::vector<std::uint64_t> oneFrame{
        std::bit_cast<std::uint64_t>(0.125),
        std::bit_cast<std::uint64_t>(-0.25),
    };
    auto oneReader = open_f64_stereo(48'000, oneFrame);
    auto reverse = adapter(48'000, 44'100, 1);
    QCOMPARE(reverse.output_frame_count().value(), std::int64_t{1});
    auto oneOutput = output_buffer(reverse, 0, 1);
    QVERIFY(reverse.read_frames(
        *oneReader, core::FrameIndex{0}, oneOutput.mutable_view()));
    auto left = oneOutput.view().channel(0);
    auto right = oneOutput.view().channel(1);
    QVERIFY(left);
    QVERIFY(right);
    QVERIFY(std::abs((*left.value())[0] - 0.125) < 2.0e-6);
    QVERIFY(std::abs((*right.value())[0] + 0.25) < 2.0e-6);

    std::vector<std::uint64_t> impulse(64U * 2U, UINT64_C(0));
    impulse[20U] = std::bit_cast<std::uint64_t>(1.0);
    auto impulseReader = open_f64_stereo(44'100, impulse);
    auto forward = adapter(44'100, 48'000, 64);
    auto impulseOutput = output_buffer(
        forward, 0, forward.output_frame_count().value());
    QVERIFY(forward.read_frames(
        *impulseReader, core::FrameIndex{0}, impulseOutput.mutable_view()));
    auto silentRight = impulseOutput.view().channel(1);
    QVERIFY(silentRight);
    for (const auto sample : *silentRight.value()) {
        QCOMPARE(std::bit_cast<std::uint64_t>(sample), UINT64_C(0));
    }
}

void PlaybackSampleRateAdapterTest::reverseDirectionAndAbsoluteSeekInvariance()
{
    constexpr std::int64_t kInputFrames = 1'000;
    std::vector<std::uint64_t> interleaved;
    interleaved.reserve(static_cast<std::size_t>(kInputFrames) * 2U);
    for (std::int64_t frame = 0; frame < kInputFrames; ++frame) {
        const double left = static_cast<double>((frame % 101) - 50) / 100.0;
        const double right = static_cast<double>((frame % 79) - 39) / 100.0;
        interleaved.push_back(std::bit_cast<std::uint64_t>(left));
        interleaved.push_back(std::bit_cast<std::uint64_t>(right));
    }
    auto converter = adapter(48'000, 44'100, kInputFrames);
    const auto outputFrames = converter.output_frame_count().value();
    auto wholeReader = open_f64_stereo(48'000, interleaved);
    auto whole = output_buffer(converter, 0, outputFrames);
    QVERIFY(converter.read_frames(
        *wholeReader, core::FrameIndex{0}, whole.mutable_view()));
    const auto wholeLeft = channel_bits(whole, 0);
    const auto wholeRight = channel_bits(whole, 1);

    const std::vector<std::int64_t> partition{
        3, 127, 509, outputFrames - 639};
    auto leftReader = open_f64_stereo(48'000, interleaved);
    auto rightReader = open_f64_stereo(48'000, interleaved);
    QCOMPARE(
        render_partitioned(converter, *leftReader, partition, 0),
        wholeLeft);
    QCOMPARE(
        render_partitioned(converter, *rightReader, partition, 1),
        wholeRight);

    constexpr std::int64_t kSeekStart = 317;
    constexpr std::int64_t kSeekFrames = 91;
    auto seekReader = open_f64_stereo(48'000, interleaved);
    auto seekBlock = output_buffer(converter, kSeekStart, kSeekFrames);
    QVERIFY(converter.read_frames(
        *seekReader,
        core::FrameIndex{kSeekStart},
        seekBlock.mutable_view()));
    const auto seekLeft = channel_bits(seekBlock, 0);
    const auto seekRight = channel_bits(seekBlock, 1);
    QCOMPARE(
        seekLeft,
        std::vector<std::uint64_t>(
            wholeLeft.begin() + kSeekStart,
            wholeLeft.begin() + kSeekStart + kSeekFrames));
    QCOMPARE(
        seekRight,
        std::vector<std::uint64_t>(
            wholeRight.begin() + kSeekStart,
            wholeRight.begin() + kSeekStart + kSeekFrames));
}

void PlaybackSampleRateAdapterTest::decodeWindowIsTrackDurationIndependent()
{
    constexpr std::int64_t kInputFrames = 100'000;
    std::vector<std::uint64_t> interleaved(
        static_cast<std::size_t>(kInputFrames) * 2U,
        std::bit_cast<std::uint64_t>(0.0));
    auto control = std::make_shared<ReaderControl>();
    auto reader = open_f64_stereo(44'100, interleaved, control);
    auto converter = adapter(44'100, 48'000, kInputFrames);
    constexpr std::int64_t kOutputStart = 50'000;
    auto block = output_buffer(
        converter,
        kOutputStart,
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    QVERIFY(converter.read_frames(
        *reader, core::FrameIndex{kOutputStart}, block.mutable_view()));
    QVERIFY(control->totalBytesRead < 50'000U);
    QVERIFY(control->largestReadRequest <= 4096U);

    auto oversized = output_buffer(
        converter,
        0,
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames + 1);
    auto rejected = converter.read_frames(
        *reader, core::FrameIndex{0}, oversized.mutable_view());
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), core::ErrorCode::InvalidArgument);
}

void PlaybackSampleRateAdapterTest::conservativeInputDependenciesMatchIndependentTapOracle()
{
    // Independent tap oracle: enumerate rational convolution coefficients
    // by interpolation phase; reflect the logical SOURCE frame separately.
    // Do not call production convolution_window(), cursor or enclosure.
    const auto independentlyReflected = [](std::int64_t logical,
                                          std::int64_t sourceFrames) {
        if (sourceFrames == 1) return std::int64_t{0};
        const auto period = sourceFrames * 2;
        auto index = logical % period;
        if (index < 0) index += period;
        return index < sourceFrames ? index : period - 1 - index;
    };
    for (const auto& [inputRate, outputRate] :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        for (const auto sourceFrames :
             std::array<std::int64_t, 5>{{1, 2, 19, 257, 2'000}}) {
            auto converter = adapter(inputRate, outputRate, sourceFrames);
            const auto outputFrames = converter.output_frame_count().value();
            const auto blockLength = std::min<std::int64_t>(128, outputFrames);
            const std::array<std::pair<std::int64_t, std::int64_t>, 4> ranges{{
                {0, 1},
                {outputFrames / 2, outputFrames / 2 + 1},
                {outputFrames - 1, outputFrames},
                {0, blockLength},
            }};
            for (const auto& [begin, end] : ranges) {
                auto interval = core::FrameRange::create(
                    core::FrameIndex{begin}, core::FrameIndex{end});
                QVERIFY(interval);
                auto enclosure = converter.conservative_input_enclosure_for_output_range(
                    *interval.value());
                QVERIFY(enclosure);
                std::int64_t minimum = sourceFrames;
                std::int64_t maximum = -1;
                for (auto outputFrame = begin; outputFrame < end; ++outputFrame) {
                    const auto q = outputFrame * converter.decimation_factor()
                        + converter.group_delay_high_rate_frames();
                    const auto phase = q % converter.interpolation_factor();
                    for (auto coefficient = phase;
                         coefficient < static_cast<std::int64_t>(converter.kernel_size());
                         coefficient += converter.interpolation_factor()) {
                        const auto logical =
                            (q - coefficient) / converter.interpolation_factor();
                        const auto sourceIndex = independentlyReflected(
                            logical, sourceFrames);
                        minimum = std::min(minimum, sourceIndex);
                        maximum = std::max(maximum, sourceIndex);
                    }
                }
                QCOMPARE(enclosure.value()->begin().value(), minimum);
                QCOMPARE(enclosure.value()->end().value(), maximum + 1);
            }
        }
    }
    // Negative oracle: output cursor zero DOES NOT prove that only
    // SOURCE frame zero contributes to the already-consumed output frame.
    auto converter = adapter(44'100, 48'000, 2'000);
    auto firstOutput = core::FrameRange::create(
        core::FrameIndex{0}, core::FrameIndex{1});
    QVERIFY(firstOutput);
    auto dependencies = converter.conservative_input_enclosure_for_output_range(
        *firstOutput.value());
    QVERIFY(dependencies);
    auto cursor = converter.map_output_frame_to_input_cursor(core::FrameIndex{1});
    QVERIFY(cursor);
    QVERIFY(dependencies.value()->end().value() > cursor.value()->value());
}

void PlaybackSampleRateAdapterTest::conservativeInputDependenciesRejectInvalidRanges()
{
    auto converter = adapter(44'100, 48'000, 2'000);
    const auto outputFrames = converter.output_frame_count().value();
    const std::array<std::pair<std::int64_t, std::int64_t>, 4> invalid{{
        {-1, 1}, {0, 0}, {outputFrames - 1, outputFrames + 1},
        {0, audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames + 1},
    }};
    for (const auto& [begin, end] : invalid) {
        auto interval = core::FrameRange::create(
            core::FrameIndex{begin}, core::FrameIndex{end});
        // FrameRange may reject impossible ranges earlier than the adapter.
        if (!interval) continue;
        auto rejected = converter.conservative_input_enclosure_for_output_range(
            *interval.value());
        QVERIFY(!rejected);
        QCOMPARE(rejected.error()->code(), core::ErrorCode::InvalidArgument);
    }
}

void PlaybackSampleRateAdapterTest::noFutureDependencyPrefixAgainstIndependentOracle()
{
    // Independent chronological oracle: enumerate every FIR coefficient
    // contribution of every NOT-YET-CONSUMED output frame. It deliberately
    // does not use the new production source-frontier formula.
    const auto reflectIndependent = [](std::int64_t logical,
                                      std::int64_t inputFrames) {
        if (inputFrames == 1) return std::int64_t{0};
        const auto period = inputFrames * 2;
        auto phase = logical % period;
        if (phase < 0) phase += period;
        return phase < inputFrames ? phase : period - 1 - phase;
    };
    for (const auto& [inputRate, outputRate] :
         std::array<std::pair<std::int64_t, std::int64_t>, 2>{{
             {44'100, 48'000}, {48'000, 44'100}}}) {
        for (const auto inputFrames :
             std::array<std::int64_t, 4>{{1, 2, 257, 2'000}}) {
            auto converter = adapter(inputRate, outputRate, inputFrames);
            const auto outputFrames = converter.output_frame_count().value();
            const std::array<std::int64_t, 5> consumedPositions{{
                0, outputFrames / 4, outputFrames / 2,
                outputFrames - 1, outputFrames}};
            std::int64_t lastRetired = 0;
            for (const auto consumed : consumedPositions) {
                auto boundary = converter.no_future_dependency_source_prefix(
                    core::FrameIndex{consumed});
                QVERIFY(boundary);
                const auto retired = boundary.value()->value();
                QVERIFY(retired >= lastRetired);
                QVERIFY(retired <= inputFrames);
                lastRetired = retired;
                if (consumed == outputFrames) {
                    QCOMPARE(retired, inputFrames);
                    continue;
                }
                std::int64_t earliestFuture = inputFrames;
                for (auto outputFrame = consumed;
                     outputFrame < outputFrames; ++outputFrame) {
                    const auto q = outputFrame * converter.decimation_factor()
                        + converter.group_delay_high_rate_frames();
                    const auto phase = q % converter.interpolation_factor();
                    for (auto tap = phase;
                         tap < static_cast<std::int64_t>(converter.kernel_size());
                         tap += converter.interpolation_factor()) {
                        const auto logical =
                            (q - tap) / converter.interpolation_factor();
                        earliestFuture = std::min(earliestFuture,
                            reflectIndependent(logical, inputFrames));
                    }
                }
                // Any source sample 'retired' by the helper must be absent
                // from all future SRC output frames. A lower bound may be
                // conservative, never above the exact independent oracle.
                QVERIFY(retired <= earliestFuture);
            }
            auto beforeZero = converter.no_future_dependency_source_prefix(
                core::FrameIndex{-1});
            QVERIFY(!beforeZero);
            QCOMPARE(beforeZero.error()->code(),
                     core::ErrorCode::InvalidArgument);
            auto pastEnd = converter.no_future_dependency_source_prefix(
                core::FrameIndex{outputFrames + 1});
            QVERIFY(!pastEnd);
            QCOMPARE(pastEnd.error()->code(),
                     core::ErrorCode::InvalidArgument);
        }
    }
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PlaybackSampleRateAdapterTest)

#include "test_playback_sample_rate_adapter.moc"
