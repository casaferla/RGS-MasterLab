#include "m22_authentic_q0_conversion_candidate.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/core/frame_time.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#ifndef RGSML_M22_SOURCE_DIR
#error "M22 original FIR fixture repository directory required"
#endif

namespace rgsml::tests {
class M22AuthenticQ0CandidateTest final : public QObject {
    Q_OBJECT
private:
    [[nodiscard]] static std::vector<std::uint8_t> frozen(
        const std::string& name) {
        const auto path=std::filesystem::path{RGSML_M22_SOURCE_DIR}
            /"resources"/"fir"/name;
        std::ifstream file{path,std::ios::binary};
        if(!file)return {};
        return {std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()};
    }
    [[nodiscard]] static core::Result<audio::AudioBuffer> source(
        std::int64_t hz,audio::ChannelLayout layout,
        core::FrameIndex start=core::FrameIndex{0},
        audio::FrameDomainId domain=
            audio::FrameDomainId::SOURCE_PROCESSING_RATE) {
        auto rate=core::SampleRate::create(hz);
        if(!rate) return core::Result<audio::AudioBuffer>::failure(
            *rate.error());
        auto fmt=audio::AudioFormat::create(*rate.value(),layout);
        if(!fmt)return core::Result<audio::AudioBuffer>::failure(
            *fmt.error());
        auto count=core::FrameCount::create(257);
        if(!count)return core::Result<audio::AudioBuffer>::failure(
            *count.error());
        auto pcm=audio::AudioBuffer::create(*fmt.value(),domain,
                                           start,*count.value());
        if(!pcm)return pcm;
        for(std::size_t ch=0;ch<pcm.value()->format().channel_count();++ch) {
            auto plane=pcm.value()->mutable_view().channel(ch);
            if(!plane)return core::Result<audio::AudioBuffer>::failure(
                *plane.error());
            for(std::size_t i=0;i<plane.value()->size();++i)
                (*plane.value())[i]=
                    static_cast<double>((i+ch*7)%19)/40.0-0.2;
        }
        return pcm;
    }
private slots:
    void realPinnedQ0BothDirectionsExactReplay() {
        using namespace rgsml::m22_private;
        struct Rate final {
            std::int64_t from;
            std::int64_t to;
            const char* bin;
        };
        constexpr std::array<Rate,2> pairs{{
            {44100,48000,"RGSMSR1_L160_M147.bin"},
            {48000,44100,"RGSMSR1_L147_M160.bin"}
        }};
        for(const auto& pair:pairs)for(const auto layout:{
                audio::ChannelLayout::MONO_C,
                audio::ChannelLayout::STEREO_LR}) {
            const auto input=source(pair.from,layout);
            QVERIFY(input);
            const auto outputHz=core::SampleRate::create(pair.to);
            const auto block=core::FrameCount::create(128);
            QVERIFY(outputHz&&block);
            const auto bytes=frozen(pair.bin);
            QVERIFY2(bytes.size()>1000,"Real original Q0 asset unavailable");
            const auto once=materialize_authentic_q0_source_candidate(
                input.value()->view(),*outputHz.value(),*block.value(),bytes);
            QVERIFY2(once,once?"":once.error()->message().c_str());
            const auto twice=materialize_authentic_q0_source_candidate(
                input.value()->view(),*outputHz.value(),*block.value(),bytes);
            QVERIFY2(twice,twice?"":twice.error()->message().c_str());
            QVERIFY(once.value()->used_frozen_q0);
            QVERIFY(!once.value()->authorizes_release());
            QCOMPARE(once.value()->frozen_q0_sha256.size(),std::size_t{64});
            QCOMPARE(once.value()->pre_terminal_pcm.kernel_sha256,
                     twice.value()->pre_terminal_pcm.kernel_sha256);
            const auto expected=static_cast<std::int64_t>(
                (257*pair.to+pair.from-1)/pair.from);
            const auto out=once.value()->pre_terminal_pcm.pcm.view();
            const auto replay=twice.value()->pre_terminal_pcm.pcm.view();
            QCOMPARE(out.timebase().frame_domain_id(),
                     audio::FrameDomainId::OUTPUT_RATE);
            QCOMPARE(out.absolute_start_frame().value(),std::int64_t{0});
            QCOMPARE(out.frame_count().value(),expected);
            QCOMPARE(out.format().sample_rate().value(),pair.to);
            QCOMPARE(out.format().channel_layout(),layout);
            for(std::size_t ch=0;ch<out.format().channel_count();++ch) {
                const auto a=out.channel(ch);
                const auto b=replay.channel(ch);
                QVERIFY(a&&b);
                QCOMPARE(a.value()->size(),b.value()->size());
                for(std::size_t i=0;i<a.value()->size();++i) {
                    QVERIFY(std::isfinite((*a.value())[i]));
                    QCOMPARE(std::bit_cast<std::uint64_t>((*a.value())[i]),
                             std::bit_cast<std::uint64_t>((*b.value())[i]));
                }
            }
            const auto cropLen=core::FrameCount::create(19);
            QVERIFY(cropLen);
            const auto crop=out.subview(core::FrameIndex{3},*cropLen.value());
            QVERIFY(crop);
            QCOMPARE(crop.value()->absolute_start_frame().value(),
                     std::int64_t{3});
            for(std::size_t ch=0;ch<out.format().channel_count();++ch) {
                const auto full=out.channel(ch);
                const auto part=crop.value()->channel(ch);
                QVERIFY(full&&part);
                QCOMPARE(part.value()->data(),full.value()->data()+3);
                QCOMPARE(part.value()->size(),std::size_t{19});
            }
        }
    }
    void refusalAndIdentityDoNotBypassQ0() {
        using namespace rgsml::m22_private;
        const auto input=source(48000,audio::ChannelLayout::MONO_C);
        QVERIFY(input);
        const auto rate=core::SampleRate::create(44100);
        const auto same=core::SampleRate::create(48000);
        const auto block=core::FrameCount::create(128);
        QVERIFY(rate&&same&&block);
        const auto real=frozen("RGSMSR1_L147_M160.bin");
        const auto wrong=frozen("RGSMSR1_L160_M147.bin");
        QVERIFY(!real.empty()&&!wrong.empty());
        QVERIFY(!materialize_authentic_q0_source_candidate(
            input.value()->view(),*rate.value(),*block.value(),{}));
        QVERIFY(!materialize_authentic_q0_source_candidate(
            input.value()->view(),*rate.value(),*block.value(),wrong));
        std::vector<std::uint8_t> bad=real;
        bad[bad.size()/2]^=std::uint8_t{1};
        QVERIFY(!materialize_authentic_q0_source_candidate(
            input.value()->view(),*rate.value(),*block.value(),bad));
        QVERIFY(!materialize_authentic_q0_source_candidate(
            input.value()->view(),*same.value(),*block.value(),real));
        const auto identity=materialize_authentic_q0_source_candidate(
            input.value()->view(),*same.value(),*block.value(),{});
        QVERIFY(identity);
        QVERIFY(!identity.value()->used_frozen_q0);
        QVERIFY(!identity.value()->authorizes_release());
        QCOMPARE(identity.value()->pre_terminal_pcm.pcm.view().frame_count(),
                 input.value()->view().frame_count());
        const auto nonzero=source(48000,audio::ChannelLayout::MONO_C,
                                  core::FrameIndex{7});
        QVERIFY(nonzero);
        QVERIFY(!materialize_authentic_q0_source_candidate(
            nonzero.value()->view(),*rate.value(),*block.value(),real));
        const auto wrongDomain=source(48000,audio::ChannelLayout::MONO_C,
            core::FrameIndex{0},audio::FrameDomainId::OUTPUT_RATE);
        QVERIFY(wrongDomain);
        QVERIFY(!materialize_authentic_q0_source_candidate(
            wrongDomain.value()->view(),*rate.value(),*block.value(),real));
        QVERIFY(!materialize_authentic_q0_source_candidate(
            input.value()->view(),*rate.value(),*block.value(),real,
            []{return true;}));
        // Retire immediately after a real strictly scalar full-resource
        // progress callback. No privately allocated partial PCM may
        // escape or be treated as a valid M22 stage after cancellation.
        bool stopAfterProgress=false;
        std::int64_t observed=0;
        const auto retired=materialize_authentic_q0_source_candidate(
            input.value()->view(),*rate.value(),*block.value(),real,
            [&]{return stopAfterProgress;},
            [&](std::int64_t done,std::int64_t) {
                if(done>0) {observed=done;stopAfterProgress=true;}
            });
        QVERIFY(observed>0);
        QVERIFY(!retired);
        // A nonfinite input must fail *before* the first output
        // publication, even with genuine frozen coefficients.
        auto nanInput=source(48000,audio::ChannelLayout::MONO_C);
        QVERIFY(nanInput);
        auto nanChannel=nanInput.value()->mutable_view().channel(0);
        QVERIFY(nanChannel);
        (*nanChannel.value())[41]=
            std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!materialize_authentic_q0_source_candidate(
            nanInput.value()->view(),*rate.value(),*block.value(),real));
    }
};
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::M22AuthenticQ0CandidateTest)
#include "test_m22_authentic_q0_candidate.moc"
