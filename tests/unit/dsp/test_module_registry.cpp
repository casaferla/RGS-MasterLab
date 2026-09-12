#include "test_support.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using dsp_support::error_category;
using dsp_support::make_descriptor;

class FakeModule final : public IModule {
public:
    explicit FakeModule(std::shared_ptr<const ModuleDescriptor> descriptor)
        : descriptor_(std::move(descriptor))
    {
    }

    const ModuleDescriptor& descriptor() const noexcept override
    {
        return *descriptor_;
    }
    rgsml::core::Result<DspRuntimeRequirements>
    runtime_requirements(const DspProcessSpec&) const override
    {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(
            rgsml::core::Error{
                rgsml::core::ErrorCode::UnsupportedOperation,
                "Test fake has no processing implementation."});
    }
    rgsml::core::Status prepare(const DspProcessSpec&) override
    {
        return rgsml::core::Status::failure(
            rgsml::core::Error{
                rgsml::core::ErrorCode::UnsupportedOperation,
                "Test fake has no processing implementation."});
    }
    void reset() noexcept override {}
    rgsml::core::Status process(
        rgsml::audio::AudioBufferView,
        rgsml::audio::MutableAudioBufferView,
        const DspProcessContext&) override
    {
        return rgsml::core::Status::failure(
            rgsml::core::Error{
                rgsml::core::ErrorCode::UnsupportedOperation,
                "Test fake has no processing implementation."});
    }

private:
    std::shared_ptr<const ModuleDescriptor> descriptor_;
};

class FakeFactory final : public IModuleFactory {
public:
    FakeFactory(
        std::string claimed_type_id,
        std::shared_ptr<const ModuleDescriptor> produced_descriptor)
        : claimed_type_id_(std::move(claimed_type_id))
        , produced_descriptor_(std::move(produced_descriptor))
    {
    }

    std::string_view module_type_id() const noexcept override
    {
        return claimed_type_id_;
    }
    rgsml::core::Result<std::unique_ptr<IModule>> create() const override
    {
        return rgsml::core::Result<std::unique_ptr<IModule>>::success(
            std::make_unique<FakeModule>(produced_descriptor_));
    }

private:
    std::string claimed_type_id_;
    std::shared_ptr<const ModuleDescriptor> produced_descriptor_;
};

[[nodiscard]] std::vector<std::string> ids(const ModuleRegistry& registry)
{
    std::vector<std::string> values;
    for (const auto& descriptor : registry.descriptors()) {
        values.emplace_back(descriptor.type_id());
    }
    return values;
}

class ModuleRegistryTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyLookupAndUnavailableCatalog();
    void deterministicConstructionAndDuplicates();
    void validatesOrderGraphAndFactoryIdentity();
    void testOnlyFactoryResolution();
};

void ModuleRegistryTest::emptyLookupAndUnavailableCatalog()
{
    auto empty = ModuleRegistry::create({});
    QVERIFY(empty.value() != nullptr);
    QVERIFY(empty.value()->descriptors().empty());
    QCOMPARE(empty.value()->factory_count(), std::size_t{0});
    auto missing = empty.value()->find_descriptor("rgsml.dsp.missing");
    QVERIFY(missing.error() != nullptr);
    QCOMPARE(error_category(*missing.error()), std::string_view{"MODULE_TYPE_NOT_FOUND"});

    auto catalog = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(catalog.value() != nullptr);
    for (const auto& descriptor : catalog.value()->descriptors()) {
        QVERIFY(!catalog.value()->has_factory(descriptor.type_id()));
        auto unavailable = catalog.value()->create_module(descriptor.type_id());
        QVERIFY(unavailable.error() != nullptr);
        QCOMPARE(
            error_category(*unavailable.error()),
            std::string_view{"MODULE_IMPLEMENTATION_UNAVAILABLE"});
    }
    auto unknown = catalog.value()->create_module("rgsml.dsp.unknown");
    QVERIFY(unknown.error() != nullptr);
    QCOMPARE(error_category(*unknown.error()), std::string_view{"MODULE_TYPE_NOT_FOUND"});
}

void ModuleRegistryTest::deterministicConstructionAndDuplicates()
{
    std::vector<ModuleRegistration> forward{
        {make_descriptor("rgsml.dsp.zeta"), nullptr},
        {make_descriptor("rgsml.dsp.alpha"), nullptr},
        {make_descriptor("rgsml.dsp.middle"), nullptr}};
    auto reverse = forward;
    std::ranges::reverse(reverse);

    auto first = ModuleRegistry::create(std::move(forward));
    auto second = ModuleRegistry::create(std::move(reverse));
    QVERIFY(first.value() != nullptr);
    QVERIFY(second.value() != nullptr);
    QCOMPARE(ids(*first.value()), ids(*second.value()));
    QCOMPARE(
        ids(*first.value()),
        std::vector<std::string>({
            "rgsml.dsp.alpha",
            "rgsml.dsp.middle",
            "rgsml.dsp.zeta"}));

    auto duplicate = ModuleRegistry::create({
        {make_descriptor("rgsml.dsp.same"), nullptr},
        {make_descriptor("rgsml.dsp.same"), nullptr}});
    QVERIFY(duplicate.error() != nullptr);
    QCOMPARE(
        error_category(*duplicate.error()),
        std::string_view{"DUPLICATE_MODULE_TYPE"});
}

void ModuleRegistryTest::validatesOrderGraphAndFactoryIdentity()
{
    auto cycle = ModuleRegistry::create({
        {make_descriptor(
             "rgsml.dsp.alpha",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             false,
             {"rgsml.dsp.beta"}),
         nullptr},
        {make_descriptor(
             "rgsml.dsp.beta",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             false,
             {"rgsml.dsp.alpha"}),
         nullptr}});
    QVERIFY(cycle.error() != nullptr);
    QCOMPARE(error_category(*cycle.error()), std::string_view{"MODULE_ORDER_VIOLATION"});

    auto unknownEdge = ModuleRegistry::create({
        {make_descriptor(
             "rgsml.dsp.alpha",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             false,
             {"rgsml.dsp.absent"}),
         nullptr}});
    QVERIFY(unknownEdge.error() != nullptr);
    QCOMPARE(
        error_category(*unknownEdge.error()),
        std::string_view{"MODULE_TYPE_NOT_FOUND"});

    auto descriptor = make_descriptor("rgsml.dsp.alpha");
    const auto produced =
        std::make_shared<const ModuleDescriptor>(descriptor);
    auto mismatchFactory = std::make_shared<const FakeFactory>(
        "rgsml.dsp.beta",
        produced);
    auto mismatch = ModuleRegistry::create({
        {std::move(descriptor), std::move(mismatchFactory)}});
    QVERIFY(mismatch.error() != nullptr);
    QCOMPARE(
        error_category(*mismatch.error()),
        std::string_view{"MODULE_IMPLEMENTATION_UNAVAILABLE"});

    auto recommendationsMayCycle = ModuleRegistry::create({
        {make_descriptor(
             "rgsml.dsp.alpha",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             false,
             {},
             {},
             {"rgsml.dsp.beta"}),
         nullptr},
        {make_descriptor(
             "rgsml.dsp.beta",
             ProcessingStage::MASTER,
             ChainSegment::MANUAL,
             true,
             true,
             false,
             {},
             {},
             {"rgsml.dsp.alpha"}),
         nullptr}});
    QVERIFY(recommendationsMayCycle.value() != nullptr);
}

void ModuleRegistryTest::testOnlyFactoryResolution()
{
    auto descriptor = make_descriptor("rgsml.dsp.fake");
    const auto matching =
        std::make_shared<const ModuleDescriptor>(descriptor);
    auto factory = std::make_shared<const FakeFactory>(
        "rgsml.dsp.fake",
        matching);
    auto registry = ModuleRegistry::create({
        {std::move(descriptor), std::move(factory)}});
    QVERIFY(registry.value() != nullptr);
    QCOMPARE(registry.value()->factory_count(), std::size_t{1});
    auto module = registry.value()->create_module("rgsml.dsp.fake");
    QVERIFY(module.value() != nullptr);
    QVERIFY(*module.value() != nullptr);
    QCOMPARE((*module.value())->descriptor().type_id(), std::string_view{"rgsml.dsp.fake"});

    auto expected = make_descriptor("rgsml.dsp.expected");
    auto produced = std::make_shared<const ModuleDescriptor>(
        make_descriptor("rgsml.dsp.different"));
    auto badFactory = std::make_shared<const FakeFactory>(
        "rgsml.dsp.expected",
        produced);
    auto badRegistry = ModuleRegistry::create({
        {std::move(expected), std::move(badFactory)}});
    QVERIFY(badRegistry.value() != nullptr);
    auto rejected = badRegistry.value()->create_module("rgsml.dsp.expected");
    QVERIFY(rejected.error() != nullptr);
    QCOMPARE(
        error_category(*rejected.error()),
        std::string_view{"MODULE_IMPLEMENTATION_UNAVAILABLE"});
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ModuleRegistryTest)

#include "test_module_registry.moc"
