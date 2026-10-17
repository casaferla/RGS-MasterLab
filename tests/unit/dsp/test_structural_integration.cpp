#include "test_support.hpp"

#include <rgsml/dsp/processing_chain.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
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

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StructuralIntegrationTest)

#include "test_structural_integration.moc"
