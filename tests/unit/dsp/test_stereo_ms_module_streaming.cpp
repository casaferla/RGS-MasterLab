#include "../render/render_test_support.hpp"

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/stereo_ms_module.hpp>

#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;

[[nodiscard]] std::unique_ptr<StereoMsModule> make_active(
    const ModuleRegistry& registry,
    MonoBassMode mode, double low_width = 25.0)
{
    const auto parameters = StereoMsParameters::create(
        0.0, 0.0, false, mode, 120.0, low_width);
    Q_ASSERT(parameters);
    const auto descriptor = registry.find_descriptor("rgsml.dsp.stereo-ms");
    Q_ASSERT(descriptor);
    auto result = StereoMsModule::create(
        descriptor.value()->get(), *parameters.value());
    Q_ASSERT(result);
    return std::move(*result.value());
}

class StereoMsModuleStreamingTest final : public QObject {
    Q_OBJECT

private slots:
    void allPartitionsMatchContinuousAudio();
    void rejectedBlocksPreserveOutputAndFilterState();
    void rejectedStreamOrderAndReset();
};

void StereoMsModuleStreamingTest::allPartitionsMatchContinuousAudio()
{
    auto reg = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    std::array<double, 128> left{};
    std::array<double, 128> right{};
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = 0.5 * std::sin(0.07 * static_cast<double>(i))
                + 0.2 * std::cos(0.02 * static_cast<double>(i));
        right[i] = 0.6 * std::cos(0.11 * static_cast<double>(i))
                 - 0.1 * std::sin(0.03 * static_cast<double>(i));
    }
    const auto layout = rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(128)};
    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        for (const double low_width : {0.0, 25.0, 100.0}) {
            auto continuous = make_active(*reg.value(), mode, low_width);
            auto partitioned = make_active(*reg.value(), mode, low_width);
            QVERIFY(continuous->prepare(spec));
            QVERIFY(partitioned->prepare(spec));
            auto full_in = make_buffer(layout, 0, left, right);
            auto full_out = make_buffer(layout, 0, left, right);
            QVERIFY(full_in);
            QVERIFY(full_out);
            QVERIFY(continuous->process(
                full_in.value()->view(), full_out.value()->mutable_view(),
                DspProcessContext{frame_range(0, 128), true, true}));
            const auto full_left = *full_out.value()->view().channel(0).value();
            const auto full_right = *full_out.value()->view().channel(1).value();

            constexpr std::array<std::size_t, 9> boundaries{
                1, 2, 3, 7, 33, 64, 65, 127, 128};
            std::size_t start = 0;
            for (const auto end : boundaries) {
                const auto a = std::span<const double>{left}.subspan(start, end - start);
                const auto b = std::span<const double>{right}.subspan(start, end - start);
                auto input = make_buffer(layout, static_cast<std::int64_t>(start), a, b);
                auto output = make_buffer(layout, static_cast<std::int64_t>(start), a, b);
                QVERIFY(input);
                QVERIFY(output);
                QVERIFY(partitioned->process(
                    input.value()->view(), output.value()->mutable_view(),
                    DspProcessContext{
                        frame_range(static_cast<std::int64_t>(start),
                                    static_cast<std::int64_t>(end)),
                        start == 0, end == left.size()}));
                const auto out_l = *output.value()->view().channel(0).value();
                const auto out_r = *output.value()->view().channel(1).value();
                for (std::size_t j = 0; j < end - start; ++j) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(out_l[j]),
                             std::bit_cast<std::uint64_t>(full_left[start + j]));
                    QCOMPARE(std::bit_cast<std::uint64_t>(out_r[j]),
                             std::bit_cast<std::uint64_t>(full_right[start + j]));
                }
                start = end;
            }
        }
    }
}

void StereoMsModuleStreamingTest::rejectedBlocksPreserveOutputAndFilterState()
{
    auto reg = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    const auto layout = rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(4)};
    const std::array<double, 4> first_left{0.25, -0.1, 0.0, 0.4};
    const std::array<double, 4> first_right{-0.2, 0.1, 0.05, 0.3};
    const std::array<double, 4> good_left{0.13, 0.05, 0.25, -0.15};
    const std::array<double, 4> good_right{-0.08, 0.09, 0.14, -0.23};

    for (auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        auto baseline = make_active(*reg.value(), mode);
        auto subject = make_active(*reg.value(), mode);
        QVERIFY(baseline->prepare(spec));
        QVERIFY(subject->prepare(spec));
        auto first_input = make_buffer(layout, 0, first_left, first_right);
        auto first_output_a = make_buffer(layout, 0, first_left, first_right);
        auto first_output_b = make_buffer(layout, 0, first_left, first_right);
        QVERIFY(first_input);
        QVERIFY(first_output_a);
        QVERIFY(first_output_b);
        const DspProcessContext first_context{frame_range(0, 4), true, false};
        QVERIFY(baseline->process(
            first_input.value()->view(), first_output_a.value()->mutable_view(),
            first_context));
        QVERIFY(subject->process(
            first_input.value()->view(), first_output_b.value()->mutable_view(),
            first_context));

        const auto check_failed = [&](double invalid_value,
                                      std::string_view expected_category) {
            std::array<double, 4> bad_left = good_left;
            bad_left[2] = invalid_value;
            auto bad_input = make_buffer(layout, 4, bad_left, good_right);
            auto bad_output = make_buffer(layout, 4, good_left, good_right);
            QVERIFY(bad_input);
            QVERIFY(bad_output);
            const auto before = bits(bad_output.value()->view());
            const auto status = subject->process(
                bad_input.value()->view(), bad_output.value()->mutable_view(),
                DspProcessContext{frame_range(4, 8), false, true});
            QVERIFY(!status);
            QCOMPARE(error_category(*status.error()), expected_category);
            QCOMPARE(bits(bad_output.value()->view()), before);
        };
        check_failed(std::numeric_limits<double>::infinity(),
                     "NONFINITE_INPUT_SAMPLE");
        // Finite input can still overflow Mid arithmetic or recursive states.
        check_failed(std::numeric_limits<double>::max(),
                     "NONFINITE_OUTPUT_SAMPLE");

        auto retry_input = make_buffer(layout, 4, good_left, good_right);
        auto retry_output = make_buffer(layout, 4, good_left, good_right);
        auto baseline_output = make_buffer(layout, 4, good_left, good_right);
        QVERIFY(retry_input);
        QVERIFY(retry_output);
        QVERIFY(baseline_output);
        const DspProcessContext retry_context{frame_range(4, 8), false, true};
        QVERIFY(baseline->process(
            retry_input.value()->view(), baseline_output.value()->mutable_view(),
            retry_context));
        QVERIFY(subject->process(
            retry_input.value()->view(), retry_output.value()->mutable_view(),
            retry_context));
        QCOMPARE(bits(retry_output.value()->view()),
                 bits(baseline_output.value()->view()));
    }
}

void StereoMsModuleStreamingTest::rejectedStreamOrderAndReset()
{
    auto reg = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    auto module = make_active(*reg.value(), MonoBassMode::LR24);
    const auto layout = rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(4)};
    QVERIFY(module->prepare(spec));
    const std::array<double, 4> samples{0.25, 0.0, 0.0, 0.0};
    auto source = make_buffer(layout, 0, samples, samples);
    auto destination = make_buffer(layout, 0, samples, samples);
    QVERIFY(source);
    QVERIFY(destination);
    const auto before = bits(destination.value()->view());
    QVERIFY(!module->process(
        source.value()->view(), destination.value()->mutable_view(),
        DspProcessContext{frame_range(0, 4), false, false}));
    QCOMPARE(bits(destination.value()->view()), before);
    QVERIFY(module->process(
        source.value()->view(), destination.value()->mutable_view(),
        DspProcessContext{frame_range(0, 4), true, true}));
    const auto accepted = bits(destination.value()->view());
    QVERIFY(!module->process(
        source.value()->view(), destination.value()->mutable_view(),
        DspProcessContext{frame_range(0, 4), true, true}));
    QCOMPARE(bits(destination.value()->view()), accepted);
    module->reset();
    auto reset_output = make_buffer(layout, 0, samples, samples);
    QVERIFY(reset_output);
    QVERIFY(module->process(
        source.value()->view(), reset_output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 4), true, true}));
    QCOMPARE(bits(reset_output.value()->view()), accepted);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsModuleStreamingTest)

#include "test_stereo_ms_module_streaming.moc"
