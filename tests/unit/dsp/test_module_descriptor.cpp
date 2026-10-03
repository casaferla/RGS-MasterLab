#include "test_support.hpp"

#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using dsp_support::error_category;

static_assert(std::is_abstract_v<IModule>);
static_assert(!std::is_copy_constructible_v<IModule>);
static_assert(!std::is_move_constructible_v<IModule>);
static_assert(std::is_same_v<
    decltype(&IModule::descriptor),
    const ModuleDescriptor& (IModule::*)() const noexcept>);
static_assert(std::is_same_v<
    decltype(&IModule::runtime_requirements),
    rgsml::core::Result<DspRuntimeRequirements> (IModule::*)(
        const DspProcessSpec&) const>);
static_assert(std::is_same_v<
    decltype(&IModule::prepare),
    rgsml::core::Status (IModule::*)(const DspProcessSpec&)>);
static_assert(std::is_same_v<
    decltype(&IModule::reset),
    void (IModule::*)() noexcept>);
static_assert(std::is_same_v<
    decltype(&IModule::process),
    rgsml::core::Status (IModule::*)(
        rgsml::audio::AudioBufferView,
        rgsml::audio::MutableAudioBufferView,
        const DspProcessContext&)>);

template <typename T>
[[nodiscard]] bool equal_span(std::span<const T> actual, const std::vector<T>& expected)
{
    return std::ranges::equal(actual, expected);
}

class ModuleDescriptorTest final : public QObject {
    Q_OBJECT

private slots:
    void validatesAndCanonicalizes();
    void rejectsInvalidDescriptors();
    void catalogMatchesFrozenMatrix();
};

void ModuleDescriptorTest::validatesAndCanonicalizes()
{
    auto descriptor = ModuleDescriptor::create(ModuleDescriptorSpec{
        "rgsml.dsp.test-module",
        "rgsml.dsp.test-module.display-name",
        {ModuleCategory::OUTPUT, ModuleCategory::UTILITY},
        {ProcessingStage::MASTER, ProcessingStage::RESTORE_PREP},
        {ChainSegment::MANUAL, ChainSegment::PRE_MASTER_CONDITIONING},
        true,
        true,
        PlacementClass::INLINE_CHAIN,
        std::nullopt,
        {"rgsml.dsp.zzz", "rgsml.dsp.aaa"},
        {},
        {},
        {},
        false,
        false,
        std::nullopt,
        std::nullopt});
    QVERIFY(descriptor.value() != nullptr);
    QVERIFY(equal_span(
        descriptor.value()->categories(),
        std::vector{ModuleCategory::UTILITY, ModuleCategory::OUTPUT}));
    QVERIFY(equal_span(
        descriptor.value()->allowed_stages(),
        std::vector{ProcessingStage::RESTORE_PREP, ProcessingStage::MASTER}));
    QVERIFY(equal_span(
        descriptor.value()->allowed_segments(),
        std::vector{ChainSegment::PRE_MASTER_CONDITIONING, ChainSegment::MANUAL}));
    QVERIFY(equal_span(
        descriptor.value()->must_precede(),
        std::vector<std::string>{"rgsml.dsp.aaa", "rgsml.dsp.zzz"}));
}

void ModuleDescriptorTest::rejectsInvalidDescriptors()
{
    const auto valid = [] {
        return ModuleDescriptorSpec{
            "rgsml.dsp.valid",
            "rgsml.dsp.valid.display-name",
            {ModuleCategory::UTILITY},
            {ProcessingStage::MASTER},
            {ChainSegment::MANUAL},
            true,
            true,
            PlacementClass::INLINE_CHAIN,
            std::nullopt,
            {},
            {},
            {},
            {},
            false,
            false,
            std::nullopt,
            std::nullopt};
    };
    const auto rejected = [](ModuleDescriptorSpec spec) {
        const auto result = ModuleDescriptor::create(std::move(spec));
        return result.error() != nullptr
            && error_category(*result.error()) == "MODULE_PLACEMENT_INVALID";
    };

    for (const auto* invalidId : {
             "",
             "rgsml.dsp.",
             "rgsml.dsp.Bad",
             "rgsml.dsp.bad_underscore",
             "rgsml.dsp.-bad",
             "rgsml.dsp.bad-"}) {
        auto spec = valid();
        spec.type_id = invalidId;
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.display_name_key.clear();
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.categories.clear();
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.allowed_stages.clear();
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.allowed_segments.clear();
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.categories.push_back(ModuleCategory::UTILITY);
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.must_precede = {"rgsml.dsp.other", "rgsml.dsp.other"};
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.allowed_segments = {ChainSegment::REPAIR};
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.terminal_slot = TerminalSlot::DITHER;
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.placement_class = PlacementClass::TERMINAL_SLOT;
        spec.allowed_segments = {ChainSegment::TERMINAL};
        spec.terminal_slot = TerminalSlot::DITHER;
        spec.duplicable = false;
        spec.single_active_instance = false;
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.must_precede = {spec.type_id};
        QVERIFY(rejected(std::move(spec)));
    }
    {
        auto spec = valid();
        spec.must_precede = {"rgsml.dsp.other"};
        spec.must_follow = {"rgsml.dsp.other"};
        QVERIFY(rejected(std::move(spec)));
    }
}

void ModuleDescriptorTest::catalogMatchesFrozenMatrix()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry.value() != nullptr);
    QCOMPARE(registry.value()->descriptors().size(), std::size_t{11});
    QCOMPARE(registry.value()->factory_count(), std::size_t{3});

    struct Expected final {
        const char* id;
        std::vector<ModuleCategory> categories;
        std::vector<ProcessingStage> stages;
        std::vector<ChainSegment> segments;
        bool duplicable;
        PlacementClass placement;
        std::optional<TerminalSlot> slot;
        bool must_be_last;
        bool single_active;
        std::optional<std::string> algorithm;
        std::optional<std::string> schema;
    };
    const std::array<Expected, 11> expected{{
        {"rgsml.dsp.dc-offset", {ModuleCategory::RESTORATION}, {ProcessingStage::RESTORE_PREP}, {ChainSegment::REPAIR}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, std::nullopt, std::nullopt},
        {"rgsml.dsp.declip", {ModuleCategory::RESTORATION}, {ProcessingStage::RESTORE_PREP}, {ChainSegment::REPAIR}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, std::nullopt, std::nullopt},
        {"rgsml.dsp.dehum", {ModuleCategory::RESTORATION}, {ProcessingStage::RESTORE_PREP}, {ChainSegment::REPAIR}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, std::nullopt, std::nullopt},
        {"rgsml.dsp.gain", {ModuleCategory::UTILITY}, {ProcessingStage::RESTORE_PREP, ProcessingStage::MASTER}, {ChainSegment::PRE_MASTER_CONDITIONING, ChainSegment::MANUAL, ChainSegment::DNA_LINKED, ChainSegment::REF_LINKED}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, "1.0.0", "rgsml.dsp.gain.parameters/1.0.0"},
        {"rgsml.dsp.parametric-eq", {ModuleCategory::FILTER_EQ}, {ProcessingStage::RESTORE_PREP, ProcessingStage::MASTER}, {ChainSegment::REPAIR, ChainSegment::PRE_MASTER_CONDITIONING, ChainSegment::MANUAL, ChainSegment::DNA_LINKED, ChainSegment::REF_LINKED}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, "1.0.0", "rgsml.dsp.parametric-eq.parameters/1.0.0"},
        {"rgsml.dsp.compressor", {ModuleCategory::DYNAMICS}, {ProcessingStage::MASTER}, {ChainSegment::MANUAL, ChainSegment::DNA_LINKED, ChainSegment::REF_LINKED}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, "1.0.0", "rgsml.dsp.compressor.parameters/1.0.0"},
        {"rgsml.dsp.stereo-ms", {ModuleCategory::SPATIAL}, {ProcessingStage::MASTER}, {ChainSegment::MANUAL, ChainSegment::DNA_LINKED, ChainSegment::REF_LINKED}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, std::nullopt, std::nullopt},
        {"rgsml.dsp.true-peak-limiter", {ModuleCategory::DYNAMICS, ModuleCategory::OUTPUT}, {ProcessingStage::MASTER}, {ChainSegment::TERMINAL}, false, PlacementClass::TERMINAL_SLOT, TerminalSlot::FINAL_TRUE_PEAK_LIMITER, false, true, std::nullopt, std::nullopt},
        {"rgsml.dsp.dither", {ModuleCategory::OUTPUT}, {ProcessingStage::MASTER}, {ChainSegment::TERMINAL}, false, PlacementClass::TERMINAL_SLOT, TerminalSlot::DITHER, true, true, std::nullopt, std::nullopt},
        {"rgsml.dsp.dynamic-eq", {ModuleCategory::FILTER_EQ, ModuleCategory::DYNAMICS}, {ProcessingStage::RESTORE_PREP, ProcessingStage::MASTER}, {ChainSegment::PRE_MASTER_CONDITIONING, ChainSegment::MANUAL, ChainSegment::DNA_LINKED, ChainSegment::REF_LINKED}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, "1.0.0", "rgsml.dsp.dynamic-eq.parameters/1.0.0"},
        {"rgsml.dsp.transient-shaper", {ModuleCategory::DYNAMICS}, {ProcessingStage::RESTORE_PREP, ProcessingStage::MASTER}, {ChainSegment::PRE_MASTER_CONDITIONING, ChainSegment::MANUAL, ChainSegment::DNA_LINKED, ChainSegment::REF_LINKED}, true, PlacementClass::INLINE_CHAIN, std::nullopt, false, false, "1.0.0", "rgsml.dsp.transient-shaper.parameters/1.0.0"},
    }};

    for (const auto& row : expected) {
        auto result = registry.value()->find_descriptor(row.id);
        QVERIFY2(result.value() != nullptr, row.id);
        const auto& descriptor = result.value()->get();
        QCOMPARE(descriptor.type_id(), std::string_view{row.id});
        QCOMPARE(
            descriptor.display_name_key(),
            std::string{row.id} + ".display-name");
        QVERIFY(equal_span(descriptor.categories(), row.categories));
        QVERIFY(equal_span(descriptor.allowed_stages(), row.stages));
        QVERIFY(equal_span(descriptor.allowed_segments(), row.segments));
        QCOMPARE(descriptor.duplicable(), row.duplicable);
        QVERIFY(descriptor.bypassable());
        QCOMPARE(descriptor.placement_class(), row.placement);
        QCOMPARE(descriptor.terminal_slot(), row.slot);
        QCOMPARE(descriptor.must_be_last(), row.must_be_last);
        QCOMPARE(descriptor.single_active_instance(), row.single_active);
        if (row.algorithm) {
            QVERIFY(descriptor.algorithm_version().has_value());
            QCOMPARE(*descriptor.algorithm_version(), std::string_view{*row.algorithm});
            QVERIFY(descriptor.parameter_schema_id().has_value());
            QCOMPARE(*descriptor.parameter_schema_id(), std::string_view{*row.schema});
            if (descriptor.type_id() == "rgsml.dsp.gain"
                || descriptor.type_id() == "rgsml.dsp.parametric-eq"
                || descriptor.type_id() == "rgsml.dsp.compressor") {
                QVERIFY(descriptor.recommended_before().empty());
            } else {
                QCOMPARE(descriptor.recommended_before().size(), std::size_t{1});
                QCOMPARE(
                    descriptor.recommended_before().front(),
                    std::string{"rgsml.dsp.true-peak-limiter"});
            }
        } else {
            QVERIFY(!descriptor.algorithm_version().has_value());
            QVERIFY(!descriptor.parameter_schema_id().has_value());
            QVERIFY(descriptor.recommended_before().empty());
        }
        QVERIFY(descriptor.must_precede().empty());
        QVERIFY(descriptor.must_follow().empty());
        QVERIFY(descriptor.recommended_after().empty());
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::ModuleDescriptorTest)

#include "test_module_descriptor.moc"
