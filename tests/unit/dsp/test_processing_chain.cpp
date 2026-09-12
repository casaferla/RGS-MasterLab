#include "test_support.hpp"

#include <rgsml/dsp/processing_chain.hpp>

#include "internal/revision.hpp"

#include <QtTest/QTest>

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using dsp_support::error_category;
using dsp_support::make_descriptor;
using dsp_support::make_id;

[[nodiscard]] std::vector<ModuleInstance> snapshot(const ProcessingChain& chain)
{
    return {chain.instances().begin(), chain.instances().end()};
}

class ProcessingChainTest final : public QObject {
    Q_OBJECT

private slots:
    void moduleInstanceControlState();
    void addMoveDuplicateBypassRemove();
    void failuresAreAtomic();
    void hardAdvisoryAndSingleActiveConstraints();
    void terminalOrderAndRevisionOverflow();
};

void ProcessingChainTest::moduleInstanceControlState()
{
    const auto id = make_id("00000000-0000-0000-0000-000000000001");
    auto instance = ModuleInstance::create_manual(id, "rgsml.dsp.gain");
    QVERIFY(instance.value() != nullptr);
    QVERIFY(instance.value()->enabled());
    QVERIFY(instance.value()->active());
    QVERIFY(!instance.value()->user_bypass());
    QVERIFY(!instance.value()->controller_suspended());
    QVERIFY(!instance.value()->domain_suspended());
    QCOMPARE(instance.value()->provenance(), ModuleProvenance::MANUAL);
    QCOMPARE(instance.value()->owner(), ModuleOwner::USER);
    QCOMPARE(instance.value()->link_state(), ModuleLinkState::UNLINKED);
    QVERIFY(!instance.value()->semantic_node_id().has_value());

    instance.value()->set_controller_suspended(true);
    QVERIFY(!instance.value()->active());
    QVERIFY(!instance.value()->user_bypass());
    instance.value()->set_controller_suspended(false);
    instance.value()->set_domain_suspended(true);
    QVERIFY(!instance.value()->active());
    instance.value()->set_domain_suspended(false);
    instance.value()->set_enabled(false);
    QVERIFY(!instance.value()->active());
    instance.value()->set_enabled(true);
    instance.value()->set_user_bypass(true);
    QVERIFY(!instance.value()->active());

    const auto newId = make_id("00000000-0000-0000-0000-000000000002");
    const auto duplicate = instance.value()->duplicate_with_id(newId);
    QCOMPARE(duplicate.instance_id(), newId);
    QVERIFY(duplicate.user_bypass());
    QCOMPARE(duplicate.parameter_state(), instance.value()->parameter_state());

    auto badOwner = ModuleInstance::create(ModuleInstanceSpec{
        id,
        "rgsml.dsp.gain",
        true,
        false,
        false,
        false,
        static_cast<ModuleProvenance>(99),
        ModuleOwner::USER,
        ModuleLinkState::UNLINKED,
        std::nullopt,
        {}});
    QVERIFY(badOwner.error() != nullptr);
    QCOMPARE(
        error_category(*badOwner.error()),
        std::string_view{"MODULE_OWNERSHIP_INVALID"});

    auto badLink = ModuleInstance::create(ModuleInstanceSpec{
        id,
        "rgsml.dsp.gain",
        true,
        false,
        false,
        false,
        ModuleProvenance::MANUAL,
        ModuleOwner::USER,
        static_cast<ModuleLinkState>(99),
        std::nullopt,
        {}});
    QVERIFY(badLink.error() != nullptr);
    QCOMPARE(
        error_category(*badLink.error()),
        std::string_view{"MODULE_LINK_STATE_INVALID"});
}

void ProcessingChainTest::addMoveDuplicateBypassRemove()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry.value() != nullptr);
    auto chain = ProcessingChain::create(
        *registry.value(),
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    QVERIFY(chain.value() != nullptr);
    QCOMPARE(chain.value()->revision(), std::uint64_t{0});
    QVERIFY(chain.value()->instances().empty());

    const auto gain = make_id("00000000-0000-0000-0000-000000000011");
    const auto compressor = make_id("00000000-0000-0000-0000-000000000012");
    const auto equalizer = make_id("00000000-0000-0000-0000-000000000013");
    const auto gainCopy = make_id("00000000-0000-0000-0000-000000000014");

    QVERIFY(chain.value()->add(gain, "rgsml.dsp.gain", 0));
    QVERIFY(chain.value()->add(compressor, "rgsml.dsp.compressor", 1));
    QVERIFY(chain.value()->add(equalizer, "rgsml.dsp.parametric-eq", 1));
    QCOMPARE(chain.value()->revision(), std::uint64_t{3});
    QCOMPARE(chain.value()->instances()[1].instance_id(), equalizer);

    QVERIFY(chain.value()->move(gain, 2));
    QCOMPARE(chain.value()->instances()[2].instance_id(), gain);
    QCOMPARE(chain.value()->revision(), std::uint64_t{4});
    QVERIFY(chain.value()->move(gain, 2));
    QCOMPARE(chain.value()->revision(), std::uint64_t{4});

    QVERIFY(chain.value()->set_user_bypass(gain, true));
    QCOMPARE(chain.value()->revision(), std::uint64_t{5});
    QVERIFY(chain.value()->set_user_bypass(gain, true));
    QCOMPARE(chain.value()->revision(), std::uint64_t{5});
    QVERIFY(chain.value()->duplicate(gain, gainCopy, 3));
    QCOMPARE(chain.value()->revision(), std::uint64_t{6});
    QVERIFY(chain.value()->instances()[3].user_bypass());
    QCOMPARE(chain.value()->instances()[3].instance_id(), gainCopy);
    QVERIFY(chain.value()->set_user_bypass(gainCopy, false));
    QCOMPARE(chain.value()->revision(), std::uint64_t{7});
    QVERIFY(chain.value()->instances()[2].user_bypass());
    QVERIFY(!chain.value()->instances()[3].user_bypass());

    auto found = chain.value()->find_instance(gainCopy);
    QVERIFY(found.value() != nullptr);
    QCOMPARE(found.value()->get().module_type_id(), std::string_view{"rgsml.dsp.gain"});
    QVERIFY(chain.value()->remove(equalizer));
    QCOMPARE(chain.value()->revision(), std::uint64_t{8});
    QCOMPARE(chain.value()->instances().size(), std::size_t{3});
}

void ProcessingChainTest::failuresAreAtomic()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    auto chain = ProcessingChain::create(
        *registry.value(),
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    QVERIFY(chain.value() != nullptr);
    const auto id = make_id("00000000-0000-0000-0000-000000000021");
    const auto other = make_id("00000000-0000-0000-0000-000000000022");
    QVERIFY(chain.value()->add(id, "rgsml.dsp.gain", 0));

    const auto before = snapshot(*chain.value());
    const auto revision = chain.value()->revision();
    const auto verify_unchanged = [&] {
        QCOMPARE(snapshot(*chain.value()), before);
        QCOMPARE(chain.value()->revision(), revision);
    };

    auto unknown = chain.value()->add(other, "rgsml.dsp.unknown", 1);
    QVERIFY(unknown.error() != nullptr);
    QCOMPARE(error_category(*unknown.error()), std::string_view{"MODULE_TYPE_NOT_FOUND"});
    verify_unchanged();

    auto duplicateId = chain.value()->add(id, "rgsml.dsp.gain", 1);
    QVERIFY(duplicateId.error() != nullptr);
    QCOMPARE(
        error_category(*duplicateId.error()),
        std::string_view{"DUPLICATE_MODULE_INSTANCE_ID"});
    verify_unchanged();

    auto badPosition = chain.value()->move(id, 1);
    QVERIFY(badPosition.error() != nullptr);
    verify_unchanged();

    auto wrongStage = chain.value()->add(other, "rgsml.dsp.dc-offset", 1);
    QVERIFY(wrongStage.error() != nullptr);
    QCOMPARE(
        error_category(*wrongStage.error()),
        std::string_view{"MODULE_STAGE_NOT_ALLOWED"});
    verify_unchanged();

    auto wrongPlacement =
        chain.value()->add(other, "rgsml.dsp.true-peak-limiter", 1);
    QVERIFY(wrongPlacement.error() != nullptr);
    QCOMPARE(
        error_category(*wrongPlacement.error()),
        std::string_view{"MODULE_SEGMENT_NOT_ALLOWED"});
    verify_unchanged();

    auto missingRemove = chain.value()->remove(other);
    QVERIFY(missingRemove.error() != nullptr);
    QCOMPARE(
        error_category(*missingRemove.error()),
        std::string_view{"MODULE_INSTANCE_NOT_FOUND"});
    verify_unchanged();

    auto missingBypass = chain.value()->set_user_bypass(other, true);
    QVERIFY(missingBypass.error() != nullptr);
    verify_unchanged();
}

void ProcessingChainTest::hardAdvisoryAndSingleActiveConstraints()
{
    auto orderedRegistry = ModuleRegistry::create({
        {make_descriptor(
             "rgsml.dsp.alpha",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             false,
             {"rgsml.dsp.beta"}),
         nullptr},
        {make_descriptor("rgsml.dsp.beta"), nullptr}});
    QVERIFY(orderedRegistry.value() != nullptr);
    auto ordered = ProcessingChain::create(
        *orderedRegistry.value(),
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    const auto alpha = make_id("00000000-0000-0000-0000-000000000031");
    const auto beta = make_id("00000000-0000-0000-0000-000000000032");
    QVERIFY(ordered.value()->add(beta, "rgsml.dsp.beta", 0));
    const auto revision = ordered.value()->revision();
    auto wrongOrder = ordered.value()->add(alpha, "rgsml.dsp.alpha", 1);
    QVERIFY(wrongOrder.error() != nullptr);
    QCOMPARE(
        error_category(*wrongOrder.error()),
        std::string_view{"MODULE_ORDER_VIOLATION"});
    QCOMPARE(ordered.value()->revision(), revision);
    QVERIFY(ordered.value()->add(alpha, "rgsml.dsp.alpha", 0));
    auto moveWrong = ordered.value()->move(alpha, 1);
    QVERIFY(moveWrong.error() != nullptr);
    QCOMPARE(ordered.value()->instances()[0].instance_id(), alpha);

    auto advisoryRegistry = ModuleRegistry::create({
        {make_descriptor(
             "rgsml.dsp.recommended",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             false,
             {},
             {},
             {"rgsml.dsp.other"}),
         nullptr},
        {make_descriptor("rgsml.dsp.other"), nullptr}});
    auto advisory = ProcessingChain::create(
        *advisoryRegistry.value(),
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    const auto recommended = make_id("00000000-0000-0000-0000-000000000033");
    const auto other = make_id("00000000-0000-0000-0000-000000000034");
    QVERIFY(advisory.value()->add(other, "rgsml.dsp.other", 0));
    QVERIFY(advisory.value()->add(recommended, "rgsml.dsp.recommended", 1));

    auto singleRegistry = ModuleRegistry::create({
        {make_descriptor(
             "rgsml.dsp.single",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             true),
         nullptr}});
    auto single = ProcessingChain::create(
        *singleRegistry.value(),
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    const auto first = make_id("00000000-0000-0000-0000-000000000035");
    const auto second = make_id("00000000-0000-0000-0000-000000000036");
    QVERIFY(single.value()->add(first, "rgsml.dsp.single", 0));
    QVERIFY(single.value()->set_user_bypass(first, true));
    QVERIFY(single.value()->add(second, "rgsml.dsp.single", 1));
    const auto singleRevision = single.value()->revision();
    auto twoActive = single.value()->set_user_bypass(first, false);
    QVERIFY(twoActive.error() != nullptr);
    QCOMPARE(
        error_category(*twoActive.error()),
        std::string_view{"MODULE_SINGLE_ACTIVE_VIOLATION"});
    QCOMPARE(single.value()->revision(), singleRevision);
    QVERIFY(single.value()->instances()[0].user_bypass());

    auto noBypassRegistry = ModuleRegistry::create({
        {make_descriptor(
             "rgsml.dsp.fixed",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             false),
         nullptr}});
    auto noBypass = ProcessingChain::create(
        *noBypassRegistry.value(),
        {ProcessingStage::MASTER, ChainSegment::MANUAL});
    const auto fixed = make_id("00000000-0000-0000-0000-000000000037");
    QVERIFY(noBypass.value()->add(fixed, "rgsml.dsp.fixed", 0));
    const auto fixedRevision = noBypass.value()->revision();
    auto rejectedBypass = noBypass.value()->set_user_bypass(fixed, true);
    QVERIFY(rejectedBypass.error() != nullptr);
    QCOMPARE(
        error_category(*rejectedBypass.error()),
        std::string_view{"MODULE_NOT_BYPASSABLE"});
    QCOMPARE(noBypass.value()->revision(), fixedRevision);
}

void ProcessingChainTest::terminalOrderAndRevisionOverflow()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    auto terminal = ProcessingChain::create(
        *registry.value(),
        {ProcessingStage::MASTER, ChainSegment::TERMINAL});
    QVERIFY(terminal.value() != nullptr);
    const auto dither = make_id("00000000-0000-0000-0000-000000000041");
    const auto limiter = make_id("00000000-0000-0000-0000-000000000042");
    QVERIFY(terminal.value()->add(dither, "rgsml.dsp.dither", 0));
    const auto revision = terminal.value()->revision();
    auto invalidOrder =
        terminal.value()->add(limiter, "rgsml.dsp.true-peak-limiter", 1);
    QVERIFY(invalidOrder.error() != nullptr);
    QCOMPARE(
        error_category(*invalidOrder.error()),
        std::string_view{"MODULE_ORDER_VIOLATION"});
    QCOMPARE(terminal.value()->revision(), revision);
    QVERIFY(terminal.value()->add(
        limiter,
        "rgsml.dsp.true-peak-limiter",
        0));
    const auto limiterCopy =
        make_id("00000000-0000-0000-0000-000000000043");
    const auto terminalRevision = terminal.value()->revision();
    auto rejectedDuplicate = terminal.value()->duplicate(
        limiter,
        limiterCopy,
        1);
    QVERIFY(rejectedDuplicate.error() != nullptr);
    QCOMPARE(
        error_category(*rejectedDuplicate.error()),
        std::string_view{"MODULE_NOT_DUPLICABLE"});
    QCOMPARE(terminal.value()->revision(), terminalRevision);

    const auto overflow =
        internal::next_revision(std::numeric_limits<std::uint64_t>::max());
    QVERIFY(overflow.error() != nullptr);
    QCOMPARE(
        error_category(*overflow.error()),
        std::string_view{"CHAIN_REVISION_OVERFLOW"});
    const auto next = internal::next_revision(41U);
    QVERIFY(next.value() != nullptr);
    QCOMPARE(*next.value(), std::uint64_t{42});

    auto invalidContext = ProcessingChain::create(
        *registry.value(),
        {ProcessingStage::RESTORE_PREP, ChainSegment::MANUAL});
    QVERIFY(invalidContext.error() != nullptr);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ProcessingChainTest)

#include "test_processing_chain.moc"
