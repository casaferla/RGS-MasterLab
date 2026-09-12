#include "../render/render_test_support.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/gain_module.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;

[[nodiscard]] const ModuleDescriptor& gain_descriptor(const ModuleRegistry& registry)
{
    return registry.find_descriptor("rgsml.dsp.gain").value()->get();
}

[[nodiscard]] std::unique_ptr<GainModule> gain_module(
    const ModuleRegistry& registry,
    double gain_db)
{
    auto parameters = GainParameters::create(gain_db);
    Q_ASSERT(parameters);
    auto module = GainModule::create(gain_descriptor(registry), *parameters.value());
    Q_ASSERT(module);
    return std::move(*module.value());
}

class GainTest final : public QObject {
    Q_OBJECT

private slots:
    void parameterValidationAndCanonicalZero();
    void descriptorFactoryAndApiValidation();
    void runtimeRequirementsAreFrozen();
    void exactIdentityAndSignedZero();
    void scalarMonoStereoAndOutOfUnity();
    void rejectsNonFiniteAndOverlap();
};

void GainTest::parameterValidationAndCanonicalZero()
{
    for (const auto accepted : {-24.0, -12.0, -0.0, 0.0, 6.0, 24.0}) {
        auto value = GainParameters::create(accepted);
        QVERIFY(value);
    }
    auto negative_zero = GainParameters::create(-0.0);
    QCOMPARE(std::bit_cast<std::uint64_t>(negative_zero.value()->gain_db()), UINT64_C(0));

    for (const auto invalid : {
             -24.000000000000004,
             24.000000000000004,
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()}) {
        auto value = GainParameters::create(invalid);
        QVERIFY(!value);
    }
    QCOMPARE(
        GainParameters::create(std::numeric_limits<double>::quiet_NaN()).error()->code(),
        rgsml::core::ErrorCode::InvalidArgument);
    QCOMPARE(
        GainParameters::create(24.000000000000004).error()->code(),
        rgsml::core::ErrorCode::OutOfRange);
}

void GainTest::descriptorFactoryAndApiValidation()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto& descriptor = gain_descriptor(*registry.value());
    QCOMPARE(descriptor.algorithm_version(), std::optional<std::string_view>{"1.0.0"});
    QCOMPARE(
        descriptor.parameter_schema_id(),
        std::optional<std::string_view>{"rgsml.dsp.gain.parameters/1.0.0"});
    QCOMPARE(registry.value()->descriptors().size(), std::size_t{11});
    QCOMPARE(registry.value()->factory_count(), std::size_t{1});
    QVERIFY(registry.value()->has_factory("rgsml.dsp.gain"));
    auto default_module = registry.value()->create_module("rgsml.dsp.gain");
    QVERIFY(default_module);
    const auto* concrete = dynamic_cast<const GainModule*>(default_module.value()->get());
    QVERIFY(concrete != nullptr);
    QCOMPARE(concrete->parameters().gain_db(), 0.0);

    auto wrong = ModuleDescriptor::create(ModuleDescriptorSpec{
        "rgsml.dsp.gain", "gain", {ModuleCategory::UTILITY},
        {ProcessingStage::MASTER}, {ChainSegment::MANUAL}, true, true,
        PlacementClass::INLINE_CHAIN, std::nullopt, {}, {}, {}, {}, false, false,
        std::nullopt, std::nullopt});
    auto zero = GainParameters::create(0.0);
    auto rejected = GainModule::create(*wrong.value(), *zero.value());
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
}

void GainTest::runtimeRequirementsAreFrozen()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    auto module = gain_module(*registry.value(), 6.0);
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(257)};
    auto requirements = module->runtime_requirements(spec);
    QVERIFY(requirements);
    QCOMPARE(requirements.value()->execution_model, DspExecutionModel::STREAMING_CAUSAL);
    QCOMPARE(requirements.value()->algorithmic_latency_frames.value(), std::int64_t{0});
    QCOMPARE(requirements.value()->look_ahead_frames.value(), std::int64_t{0});
    QCOMPARE(requirements.value()->pre_context_frames.value(), std::int64_t{0});
    QCOMPARE(requirements.value()->post_context_frames.value(), std::int64_t{0});
    QCOMPARE(requirements.value()->effective_tail_frames.value(), std::int64_t{0});
    QVERIFY(!requirements.value()->requires_prepass);
    QVERIFY(module->prepare(spec));
    module->reset();
}

void GainTest::exactIdentityAndSignedZero()
{
    const std::array samples{
        0.0,
        -0.0,
        std::bit_cast<double>(UINT64_C(1)),
        std::bit_cast<double>(UINT64_C(0x8000000000000001)),
        1.25,
        -2.0};
    auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, samples);
    auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, samples);
    auto registry = ModuleRegistry::create_dsp_package_v1();
    auto module = gain_module(*registry.value(), -0.0);
    const DspProcessSpec spec{
        input.value()->view().format(),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(samples.size()))};
    QVERIFY(module->prepare(spec));
    const DspProcessContext context{frame_range(10, 16), true, true};
    QVERIFY(module->process(input.value()->view(), output.value()->mutable_view(), context));
    QCOMPARE(bits(input.value()->view()), bits(output.value()->view()));
}

void GainTest::scalarMonoStereoAndOutOfUnity()
{
    const std::array left{0.5, -0.25, 1.25, -2.0, -0.0};
    const std::array right{-0.5, 0.25, -1.25, 2.0, 0.0};
    auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto output = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = ModuleRegistry::create_dsp_package_v1();
    auto module = gain_module(*registry.value(), 6.0);
    const DspProcessSpec spec{
        input.value()->view().format(),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(5)};
    QVERIFY(module->prepare(spec));
    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 5), true, true}));
    const auto factor = 1.9952623149688795;
    for (std::size_t channel = 0; channel < 2; ++channel) {
        const auto source = *input.value()->view().channel(channel).value();
        const auto actual = *output.value()->view().channel(channel).value();
        for (std::size_t index = 0; index < source.size(); ++index) {
            QCOMPARE(actual[index], source[index] * factor);
        }
    }
    QVERIFY(std::signbit(output.value()->view().channel(0).value()->back()));
}

void GainTest::rejectsNonFiniteAndOverlap()
{
    const std::array finite{1.0};
    auto input = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, finite);
    auto output = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, finite);
    auto registry = ModuleRegistry::create_dsp_package_v1();
    auto module = gain_module(*registry.value(), 24.0);
    const DspProcessSpec spec{
        input.value()->view().format(),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(1)};
    QVERIFY(module->prepare(spec));

    input.value()->mutable_view().channel(0).value()->front() =
        std::numeric_limits<double>::quiet_NaN();
    auto status = module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 1), true, true});
    QVERIFY(!status);
    QCOMPARE(status.error()->code(), rgsml::core::ErrorCode::InvalidAudioSample);

    input.value()->mutable_view().channel(0).value()->front() =
        std::numeric_limits<double>::max();
    status = module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 1), true, true});
    QVERIFY(!status);
    QCOMPARE(status.error()->code(), rgsml::core::ErrorCode::InvalidAudioSample);

    input.value()->mutable_view().channel(0).value()->front() = 1.0;
    auto same = input.value()->mutable_view();
    status = module->process(
        same.as_const(), same,
        DspProcessContext{frame_range(0, 1), true, true});
    QVERIFY(!status);
    QCOMPARE(status.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::GainTest)

#include "test_gain.moc"
