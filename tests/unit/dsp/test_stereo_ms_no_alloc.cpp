#include "../render/render_test_support.hpp"

#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/stereo_ms_module.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <QtTest/QTest>

#include <atomic>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <numbers>

namespace {
thread_local bool t_count_allocations{false};
thread_local std::atomic<std::size_t> t_allocations{0};
}

void* operator new(std::size_t size)
{
    if (t_count_allocations) {
        t_allocations.fetch_add(1U, std::memory_order_relaxed);
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;

class StereoMsNoAllocTest final : public QObject {
    Q_OBJECT
private slots:
    void noProcessAllocationsForAllModesLayoutsAndSideMute();
};

void StereoMsNoAllocTest::noProcessAllocationsForAllModesLayoutsAndSideMute()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto descriptor = registry.value()->find_descriptor("rgsml.dsp.stereo-ms");
    QVERIFY(descriptor);

    constexpr std::size_t block = 128;
    constexpr std::size_t blocks = 48;
    std::array<double, block> left{}, right{};
    for (std::size_t i = 0; i < block; ++i) {
        left[i] = 0.31 * std::sin(0.043 * static_cast<double>(i))
                + 0.17 * std::cos(0.014 * static_cast<double>(i));
        right[i] = -0.27 * std::sin(0.091 * static_cast<double>(i))
                 + 0.22 * std::cos(0.032 * static_cast<double>(i));
    }

    for (const auto layout : {rgsml::audio::ChannelLayout::MONO_C,
                               rgsml::audio::ChannelLayout::STEREO_LR}) {
        const DspProcessSpec spec{
            format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(static_cast<std::int64_t>(block))};
        for (const auto mode : {MonoBassMode::OFF, MonoBassMode::LR12,
                                MonoBassMode::LR24}) {
            for (const bool muted : {false, true}) {
                auto params = StereoMsParameters::create(
                    -1.0, 2.0, muted, mode, 120.0, 25.0);
                QVERIFY(params);
                auto created = StereoMsModule::create(
                    descriptor.value()->get(), *params.value());
                QVERIFY(created);
                auto& module = *created.value();
                QVERIFY(module->prepare(spec));
                for (std::size_t n = 0; n < blocks; ++n) {
                    const auto from = static_cast<std::int64_t>(n * block);
                    auto input = make_buffer(layout, from, left, right);
                    auto output = make_buffer(layout, from, left, right);
                    QVERIFY(input);
                    QVERIFY(output);
                    const DspProcessContext context{
                        frame_range(from, from + static_cast<std::int64_t>(block)),
                        n == 0, n + 1 == blocks};

                    // All Qt, buffer, view and context preparation is outside
                    // this strictly isolated measurement interval.
                    t_allocations.store(0U, std::memory_order_relaxed);
                    t_count_allocations = true;
                    const auto result = module->process(
                        input.value()->view(), output.value()->mutable_view(),
                        context);
                    t_count_allocations = false;
                    const auto delta = t_allocations.load(std::memory_order_relaxed);
                    QVERIFY2(result, "Stereo/M-S process rejected valid contiguous PCM.");
                    QCOMPARE(delta, std::size_t{0});
                }
            }
        }
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsNoAllocTest)
#include "test_stereo_ms_no_alloc.moc"
