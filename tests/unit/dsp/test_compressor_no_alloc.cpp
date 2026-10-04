#include "../render/render_test_support.hpp"
#include "test_support.hpp"

#include <rgsml/dsp/compressor_module.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace {
thread_local std::atomic<std::size_t> t_alloc_count{0};
thread_local bool t_monitor_allocations{false};
}

void* operator new(std::size_t size) {
    if (t_monitor_allocations) {
        t_alloc_count.fetch_add(1, std::memory_order_relaxed);
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;

class CompressorNoAllocTest final : public QObject {
    Q_OBJECT

private slots:
    void zeroAllocationsDuringSteadyStateProcessAndFinalize();
};

void CompressorNoAllocTest::zeroAllocationsDuringSteadyStateProcessAndFinalize()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    const auto& desc = registry.value()->find_descriptor("rgsml.dsp.compressor").value()->get();

    auto params = *CompressorParameters::create_default().value();
    auto mod = std::move(*CompressorModule::create(desc, params).value());

    const DspProcessSpec spec{format(rgsml::audio::ChannelLayout::STEREO_LR, 48000), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(512)};
    QVERIFY(mod->prepare(spec));

    // Warm-up call
    std::vector<double> left(512U, 0.5);
    std::vector<double> right(512U, -0.5);
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto out_buf = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);

    QVERIFY(mod->process(in_buf.value()->view(), out_buf.value()->mutable_view(), DspProcessContext{frame_range(0, 512), true, false}));

    // Pre-allocate buffer views, frame ranges, and context objects for all 100 process calls and finalize
    auto in_b = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 512, left, right);
    auto out_b = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 512, left, right);
    const auto in_view = in_b.value()->view();
    const auto out_view = out_b.value()->mutable_view();

    std::vector<rgsml::dsp::DspProcessContext> process_contexts;
    process_contexts.reserve(100);
    for (std::size_t i = 1; i <= 100; ++i) {
        const std::int64_t start_f = static_cast<std::int64_t>(i * 512);
        process_contexts.push_back(DspProcessContext{frame_range(start_f, start_f + 512), false, false});
    }

    std::vector<double> zero240(240U, 0.0);
    auto fin_out = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 101 * 512, zero240, zero240);
    const auto fin_out_view = fin_out.value()->mutable_view();
    const std::int64_t fin_start = 101 * 512;
    const rgsml::dsp::DspProcessContext fin_context{frame_range(fin_start, fin_start + 240), false, true};

    // Execute 100 steady-state process calls with exact isolated allocation monitoring
    for (std::size_t i = 0; i < 100; ++i) {
        t_monitor_allocations = false;
        t_alloc_count.store(0, std::memory_order_relaxed);

        t_monitor_allocations = true;
        const auto status = mod->process(in_view, out_view, process_contexts[i]);
        t_monitor_allocations = false;

        const std::size_t alloc_delta = t_alloc_count.load(std::memory_order_relaxed);
        if (alloc_delta > 0) {
            std::cout << "[NO_ALLOC_FAILURE] PROCESS call_index=" << (i + 1)
                      << " alloc_count=" << alloc_delta << "\n";
        }
        QVERIFY(status);
        QCOMPARE(alloc_delta, std::size_t{0});
    }

    // Execute finalize call with exact isolated allocation monitoring
    t_monitor_allocations = false;
    t_alloc_count.store(0, std::memory_order_relaxed);

    t_monitor_allocations = true;
    const auto fin_status = mod->finalize(fin_out_view, fin_context);
    t_monitor_allocations = false;

    const std::size_t fin_alloc_delta = t_alloc_count.load(std::memory_order_relaxed);
    if (fin_alloc_delta > 0) {
        std::cout << "[NO_ALLOC_FAILURE] FINALIZE alloc_count=" << fin_alloc_delta << "\n";
    }
    QVERIFY(fin_status);
    QCOMPARE(fin_alloc_delta, std::size_t{0});
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::CompressorNoAllocTest)

#include "test_compressor_no_alloc.moc"
