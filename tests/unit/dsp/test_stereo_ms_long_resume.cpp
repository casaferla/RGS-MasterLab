#include "stereo_ms_independent_fixture.hpp"

#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/stereo_ms_module.hpp>
#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;
using namespace stereo_ms_fixture;

[[nodiscard]] std::unique_ptr<StereoMsModule> new_module(
    const ModuleRegistry& registry, MonoBassMode mode, double beta)
{
    auto params=StereoMsParameters::create(
        -1.0,2.0,false,mode,120.0,beta*100.0);
    Q_ASSERT(params);
    auto descriptor=registry.find_descriptor("rgsml.dsp.stereo-ms");
    Q_ASSERT(descriptor);
    auto result=StereoMsModule::create(descriptor.value()->get(),*params.value());
    Q_ASSERT(result);
    return std::move(*result.value());
}

class StereoMsLongResumeTest final : public QObject {
    Q_OBJECT
private slots:
    void longRaggedChunksAndAllCheckpointCuts();
};

void StereoMsLongResumeTest::longRaggedChunksAndAllCheckpointCuts()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto left=samples(UINT32_C(0x4D313500));
    const auto right=samples(UINT32_C(0x4D313501));
    const auto layout=rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{format(layout),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(kFrames))};
    auto input=make_buffer(layout,0,left,right);
    QVERIFY(input);
    constexpr std::array<std::size_t,8> pattern{1,7,2,11,3,12,5,31};
    constexpr std::array<std::size_t,8> cuts{1,2,3,11,12,13,31,4096};

    for (const auto mode : {MonoBassMode::LR12,MonoBassMode::LR24}) {
        for (const double beta : {0.0,0.25,0.5,0.75,1.0}) {
            auto whole=new_module(*registry.value(),mode,beta);
            QVERIFY(whole->prepare(spec));
            auto output=make_buffer(layout,0,left,right);
            QVERIFY(output);
            QVERIFY(whole->process(input.value()->view(),
                output.value()->mutable_view(),
                DspProcessContext{frame_range(0,static_cast<std::int64_t>(kFrames)),
                                  true,true}));
            const auto expected_l=*output.value()->view().channel(0).value();
            const auto expected_r=*output.value()->view().channel(1).value();

            // Every chunk boundary must preserve every binary64 output bit.
            auto chunked=new_module(*registry.value(),mode,beta);
            QVERIFY(chunked->prepare(spec));
            std::size_t from=0,step=0;
            while(from<kFrames) {
                const std::size_t to=std::min(
                    kFrames,from+pattern[step%pattern.size()]);
                const auto l=std::span<const double>{left}.subspan(from,to-from);
                const auto r=std::span<const double>{right}.subspan(from,to-from);
                auto in=make_buffer(layout,static_cast<std::int64_t>(from),l,r);
                auto out=make_buffer(layout,static_cast<std::int64_t>(from),l,r);
                QVERIFY(in);
                QVERIFY(out);
                QVERIFY(chunked->process(in.value()->view(),
                    out.value()->mutable_view(),
                    DspProcessContext{frame_range(
                        static_cast<std::int64_t>(from),
                        static_cast<std::int64_t>(to)),
                        from==0,to==kFrames}));
                const auto lo=*out.value()->view().channel(0).value();
                const auto ro=*out.value()->view().channel(1).value();
                for(std::size_t j=0;j<to-from;++j) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(lo[j]),
                             std::bit_cast<std::uint64_t>(expected_l[from+j]));
                    QCOMPARE(std::bit_cast<std::uint64_t>(ro[j]),
                             std::bit_cast<std::uint64_t>(expected_r[from+j]));
                }
                from=to;
                ++step;
            }

            // Restart from every specified internal stream position.
            for(const std::size_t cut : cuts) {
                auto prefix=new_module(*registry.value(),mode,beta);
                auto restored=new_module(*registry.value(),mode,beta);
                QVERIFY(prefix->prepare(spec));
                QVERIFY(restored->prepare(spec));
                auto input0=make_buffer(layout,0,
                    std::span<const double>{left}.first(cut),
                    std::span<const double>{right}.first(cut));
                auto output0=make_buffer(layout,0,
                    std::span<const double>{left}.first(cut),
                    std::span<const double>{right}.first(cut));
                QVERIFY(input0);
                QVERIFY(output0);
                QVERIFY(prefix->process(input0.value()->view(),
                    output0.value()->mutable_view(),
                    DspProcessContext{frame_range(0,
                        static_cast<std::int64_t>(cut)),true,false}));
                const auto prefix_l=*output0.value()->view().channel(0).value();
                const auto prefix_r=*output0.value()->view().channel(1).value();
                for(std::size_t j=0;j<cut;++j) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(prefix_l[j]),
                             std::bit_cast<std::uint64_t>(expected_l[j]));
                    QCOMPARE(std::bit_cast<std::uint64_t>(prefix_r[j]),
                             std::bit_cast<std::uint64_t>(expected_r[j]));
                }
                const auto checkpoint=prefix->runtime_checkpoint();
                QVERIFY(checkpoint);
                QVERIFY(checkpoint.value()->next_input_frame.has_value());
                QCOMPARE(checkpoint.value()->next_input_frame->value(),
                         static_cast<std::int64_t>(cut));
                QVERIFY(restored->restore_runtime_checkpoint(*checkpoint.value()));
                auto input1=make_buffer(layout,static_cast<std::int64_t>(cut),
                    std::span<const double>{left}.subspan(cut),
                    std::span<const double>{right}.subspan(cut));
                auto output1=make_buffer(layout,static_cast<std::int64_t>(cut),
                    std::span<const double>{left}.subspan(cut),
                    std::span<const double>{right}.subspan(cut));
                QVERIFY(input1);
                QVERIFY(output1);
                QVERIFY(restored->process(input1.value()->view(),
                    output1.value()->mutable_view(),
                    DspProcessContext{frame_range(
                        static_cast<std::int64_t>(cut),
                        static_cast<std::int64_t>(kFrames)),false,true}));
                const auto resumed_l=*output1.value()->view().channel(0).value();
                const auto resumed_r=*output1.value()->view().channel(1).value();
                for(std::size_t j=cut;j<kFrames;++j) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(resumed_l[j-cut]),
                             std::bit_cast<std::uint64_t>(expected_l[j]));
                    QCOMPARE(std::bit_cast<std::uint64_t>(resumed_r[j-cut]),
                             std::bit_cast<std::uint64_t>(expected_r[j]));
                }
            }
        }
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsLongResumeTest)
#include "test_stereo_ms_long_resume.moc"
