#include "test_support.hpp"

#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/frame_time.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <string_view>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using dsp_support::error_category;
using dsp_support::make_id;

[[nodiscard]] std::vector<ModuleInstance> replay_trace(
    const ModuleRegistry& registry,
    std::uint64_t& revision)
{
    auto chain = ProcessingChain::create(
        registry,
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    const auto gain = make_id("10000000-0000-0000-0000-000000000001");
    const auto compressor = make_id("10000000-0000-0000-0000-000000000002");
    const auto equalizer = make_id("10000000-0000-0000-0000-000000000003");
    const auto duplicateGain = make_id("10000000-0000-0000-0000-000000000004");

    if (!chain.value()->add(gain, "rgsml.dsp.gain", 0)
        || !chain.value()->add(compressor, "rgsml.dsp.compressor", 1)
        || !chain.value()->add(equalizer, "rgsml.dsp.parametric-eq", 1)
        || !chain.value()->move(compressor, 0)
        || !chain.value()->duplicate(gain, duplicateGain, 3)
        || !chain.value()->set_user_bypass(duplicateGain, true)
        || !chain.value()->remove(equalizer)) {
        revision = 0;
        return {};
    }
    revision = chain.value()->revision();
    return {chain.value()->instances().begin(), chain.value()->instances().end()};
}

class StructuralIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void deterministicPublicApiTrace();
    void limiterTerminalBypassAndFactorySemantics();
    void testHarnessBypassCopiesPcmWithZeroLatency();
};

void StructuralIntegrationTest::deterministicPublicApiTrace()
{
    auto firstRegistry = ModuleRegistry::create_dsp_package_v1();
    auto secondRegistry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(firstRegistry.value() != nullptr);
    QVERIFY(secondRegistry.value() != nullptr);
    QVERIFY(std::ranges::equal(
        firstRegistry.value()->descriptors(),
        secondRegistry.value()->descriptors()));
    QCOMPARE(firstRegistry.value()->descriptors().size(), std::size_t{11});
    QCOMPARE(firstRegistry.value()->factory_count(), std::size_t{5});

    std::uint64_t firstRevision = 0;
    std::uint64_t secondRevision = 0;
    const auto first =
        replay_trace(*firstRegistry.value(), firstRevision);
    const auto second =
        replay_trace(*secondRegistry.value(), secondRevision);
    QCOMPARE(first, second);
    QCOMPARE(firstRevision, secondRevision);
    QCOMPARE(firstRevision, std::uint64_t{7});
    QCOMPARE(first.size(), std::size_t{3});

    for (const auto& instance : first) {
        auto descriptor =
            firstRegistry.value()->find_descriptor(instance.module_type_id());
        QVERIFY(descriptor.value() != nullptr);
        QCOMPARE(
            descriptor.value()->get().type_id(),
            instance.module_type_id());
        auto module = firstRegistry.value()->create_module(instance.module_type_id());
        if (instance.module_type_id() == "rgsml.dsp.gain"
            || instance.module_type_id() == "rgsml.dsp.parametric-eq"
            || instance.module_type_id() == "rgsml.dsp.compressor") {
            QVERIFY(module.value() != nullptr);
        } else {
            QVERIFY(module.error() != nullptr);
            QCOMPARE(
                error_category(*module.error()),
                std::string_view{"MODULE_IMPLEMENTATION_UNAVAILABLE"});
        }
    }
}

void StructuralIntegrationTest::limiterTerminalBypassAndFactorySemantics()
{
    const auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto terminal=ProcessingChain::create(*registry.value(),
        {ProcessingStage::MASTER,ChainSegment::TERMINAL});
    QVERIFY(terminal);
    const auto limiter=make_id("10000000-0000-0000-0000-000000000017");
    QVERIFY(terminal.value()->add(limiter,"rgsml.dsp.true-peak-limiter",0));
    QCOMPARE(terminal.value()->instances().size(),std::size_t{1});
    QVERIFY(terminal.value()->instances()[0].active());

    // BYPASS is an instance/chain state, never a silent default Ceiling or
    // an active limiter pretending to use zero reduction. Renderer must SKIP
    // inactive instances, with no D-frame drain nor whole-resource trim.
    const auto initialRevision=terminal.value()->revision();
    QVERIFY(terminal.value()->set_user_bypass(limiter,true));
    QVERIFY(!terminal.value()->instances()[0].active());
    QVERIFY(terminal.value()->instances()[0].user_bypass());
    QCOMPARE(terminal.value()->revision(),initialRevision+1);
    const auto stillRegistered=registry.value()->create_module("rgsml.dsp.true-peak-limiter");
    QVERIFY(stillRegistered);
    QVERIFY(terminal.value()->set_user_bypass(limiter,false));
    QVERIFY(terminal.value()->instances()[0].active());
    QCOMPARE(terminal.value()->revision(),initialRevision+2);

    auto manual=ProcessingChain::create(*registry.value(),
        {ProcessingStage::MASTER,ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto id=make_id("10000000-0000-0000-0000-000000000018");
    QVERIFY(!manual.value()->add(id,"rgsml.dsp.true-peak-limiter",0));
    QVERIFY(!terminal.value()->duplicate(limiter,id,1));
    const auto idTwo=make_id("10000000-0000-0000-0000-000000000019");
    QVERIFY(!terminal.value()->add(idTwo,"rgsml.dsp.true-peak-limiter",1));
}

void StructuralIntegrationTest::testHarnessBypassCopiesPcmWithZeroLatency()
{
    // Test-only inactive routing contract; not proof of product renderer yet.
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain=ProcessingChain::create(*registry.value(),
        {ProcessingStage::MASTER,ChainSegment::TERMINAL});
    QVERIFY(chain);
    const auto id=make_id("10000000-0000-0000-0000-000000000027");
    QVERIFY(chain.value()->add(id,"rgsml.dsp.true-peak-limiter",0));
    QVERIFY(chain.value()->set_user_bypass(id,true));
    const std::array<double,5> left{4.0,-0.0,-1.0,0.125,+0.0};
    const std::array<double,5> right{-2.0,+0.0,1.25,-0.25,-0.0};
    std::array<std::array<double,5>,2> bypass{};
    int processedNodes=0;
    if(chain.value()->instances()[0].active()) {
        ++processedNodes; // Inactive terminal must never invoke DSP.
    } else {
        std::copy(left.begin(),left.end(),bypass[0].begin());
        std::copy(right.begin(),right.end(),bypass[1].begin());
    }
    QCOMPARE(processedNodes,0);
    for(std::size_t i=0;i<left.size();++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(bypass[0][i]),
            std::bit_cast<std::uint64_t>(left[i]));
        QCOMPARE(std::bit_cast<std::uint64_t>(bypass[1][i]),
            std::bit_cast<std::uint64_t>(right[i]));
    }
    QCOMPARE(bypass[0].size(),left.size()); // Zero effective drain / trim.
    QVERIFY(chain.value()->set_user_bypass(id,false));
    QVERIFY(chain.value()->instances()[0].active());

    // Active contrast: normal module incurs D=228 before N real frames.
    const auto fmt=audio::AudioFormat::create(
        *core::SampleRate::create(44100).value(),
        audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto module=registry.value()->create_module("rgsml.dsp.true-peak-limiter");
    QVERIFY(module);
    constexpr std::int64_t n=5,d=228;
    const auto count=[](std::int64_t x) {
        return *core::FrameCount::create(x).value();
    };
    QVERIFY(module.value()->get()->prepare(DspProcessSpec{
        *fmt.value(),audio::FrameDomainId::OUTPUT_RATE,count(512)}));
    auto input=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},count(n));
    auto initialOutput=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},count(n));
    auto tail=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{n},count(d));
    QVERIFY(input && initialOutput && tail);
    for(std::size_t ch=0;ch<2;++ch) {
        auto channel=input.value()->mutable_view().channel(ch);
        QVERIFY(channel);
        for(std::size_t i=0;i<left.size();++i)
            (*channel.value())[i]=ch==0?left[i]:right[i];
    }
    const auto first=core::FrameRange::create(
        core::FrameIndex{0},core::FrameIndex{n});
    const auto last=core::FrameRange::create(
        core::FrameIndex{n},core::FrameIndex{n+d});
    QVERIFY(first && last);
    QVERIFY(module.value()->get()->process(
        input.value()->view(),initialOutput.value()->mutable_view(),
        DspProcessContext{*first.value(),true,false}));
    QVERIFY(module.value()->get()->finalize(
        tail.value()->mutable_view(),DspProcessContext{*last.value(),false,true}));
    for(std::size_t ch=0;ch<2;++ch) {
        const auto placeholders=initialOutput.value()->view().channel(ch);
        const auto real=tail.value()->view().channel(ch);
        QVERIFY(placeholders && real);
        for(const double x:*placeholders.value())
            QCOMPARE(std::bit_cast<std::uint64_t>(x),
                std::bit_cast<std::uint64_t>(+0.0));
        for(std::size_t i=0;i<left.size();++i)
            QVERIFY(std::isfinite((*real.value())[static_cast<std::size_t>(d-n)+i]));
    }
    const auto firstProcessed=tail.value()->view().channel(0);
    QVERIFY(firstProcessed);
    QVERIFY(std::abs((*firstProcessed.value())[static_cast<std::size_t>(d-n)])
        <std::abs(left[0])); // Cannot mistake active limiter for bypass.
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StructuralIntegrationTest)

#include "test_structural_integration.moc"
