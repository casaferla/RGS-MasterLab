#include "render_test_support.hpp"

#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace render_support;

[[nodiscard]] rgsml::dsp::ProcessingChain empty_chain(
    const rgsml::dsp::ModuleRegistry& registry)
{
    auto chain = rgsml::dsp::ProcessingChain::create(
        registry,
        {rgsml::dsp::ProcessingStage::MASTER, rgsml::dsp::ChainSegment::MANUAL});
    Q_ASSERT(chain);
    return std::move(*chain.value());
}

[[nodiscard]] rgsml::dsp::GainParameters gain(double value)
{
    auto parameters = rgsml::dsp::GainParameters::create(value);
    Q_ASSERT(parameters);
    return *parameters.value();
}

class RenderPreviewTest final : public QObject {
    Q_OBJECT

private slots:
    void validatesBindingsAndWindowAtomically();
    void emptyAndBypassedChainsAreExactIdentity();
    void activeGainIsChunkInvariantAndSourceImmutable();
    void multipleGainsUseFrozenSnapshotOrder();
    void requiredListeningPartitionsMatchForAllGains();
    void activeUnavailableModuleFails();
    void resultLifetimeIsIndependent();
};

void RenderPreviewTest::validatesBindingsAndWindowAtomically()
{
    const std::array samples{0.25, -0.5, 1.0};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("20000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));

    auto missing = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain, {}, frame_count(7));
    QVERIFY(!missing);

    const rgsml::render::GainParameterBinding binding{id, gain(6.0)};
    auto duplicate = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding, binding}, frame_count(7));
    QVERIFY(!duplicate);

    auto outside = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(99, 103), chain,
        {binding}, frame_count(7));
    QVERIFY(!outside);

    auto zero_block = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding}, frame_count(0));
    QVERIFY(!zero_block);

    auto compressor = empty_chain(*registry.value());
    const auto compressor_id = make_id("20000000-0000-0000-0000-000000000002");
    QVERIFY(compressor.add(compressor_id, "rgsml.dsp.compressor", 0));
    auto mismatch = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), compressor,
        {{compressor_id, gain(0.0)}}, frame_count(7));
    QVERIFY(!mismatch);
}

void RenderPreviewTest::emptyAndBypassedChainsAreExactIdentity()
{
    const std::array left{0.0, -0.0, 0.5, -1.25, std::bit_cast<double>(UINT64_C(1))};
    const std::array right{-0.0, 0.0, -0.5, 1.25, std::bit_cast<double>(UINT64_C(0x8000000000000001))};
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 50, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    for (const auto block : {1, 7, 64, 257}) {
        auto request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(50, 55), chain, {}, frame_count(block));
        QVERIFY(request);
        auto result = rgsml::render::render_preview(*request.value(), *registry.value());
        QVERIFY(result);
        QCOMPARE(bits(result.value()->view()), bits(source.value()->view()));
    }

    auto partial_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(51, 54), chain, {}, frame_count(7));
    auto partial_result = rgsml::render::render_preview(
        *partial_request.value(), *registry.value());
    auto source_partial = source.value()->view().subview(
        rgsml::core::FrameIndex{51}, frame_count(3));
    QVERIFY(partial_result);
    QCOMPARE(partial_result.value()->view().absolute_range(), frame_range(51, 54));
    QCOMPARE(bits(partial_result.value()->view()), bits(*source_partial.value()));

    const auto id = make_id("21000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.set_user_bypass(id, true));
    auto request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(50, 55), chain,
        {{id, gain(24.0)}}, frame_count(1));
    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(result);
    QCOMPARE(bits(result.value()->view()), bits(source.value()->view()));
    QCOMPARE(result.value()->gain_signatures().size(), std::size_t{1});
    QCOMPARE(
        result.value()->gain_signatures().front().disposition,
        rgsml::render::GainExecutionDisposition::BYPASS_IDENTITY);
}

void RenderPreviewTest::activeGainIsChunkInvariantAndSourceImmutable()
{
    const std::array samples{
        0.0, -0.0, 0.125, -0.25, 0.5, -1.0, 1.25, -2.0,
        0.03125, -0.0625, 0.75, -0.875, 0.2, -0.4, 0.8, -1.6, 0.1};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, samples);
    const auto before = bits(source.value()->view());
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("22000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));

    std::vector<std::uint64_t> reference;
    for (const auto block : {1, 7, 64, 257}) {
        auto request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(10, 27), chain,
            {{id, gain(6.0)}}, frame_count(block));
        auto result = rgsml::render::render_preview(*request.value(), *registry.value());
        QVERIFY(result);
        if (reference.empty()) {
            reference = bits(result.value()->view());
        } else {
            QCOMPARE(bits(result.value()->view()), reference);
        }
        QCOMPARE(result.value()->render_window(), frame_range(10, 27));
        QCOMPARE(result.value()->frame_domain_id(), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE);
        QCOMPARE(result.value()->chain_revision(), std::uint64_t{1});
    }
    QCOMPARE(bits(source.value()->view()), before);
}

void RenderPreviewTest::multipleGainsUseFrozenSnapshotOrder()
{
    const std::array samples{0.5, -0.25, 1.25, -2.0};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto first = make_id("23000000-0000-0000-0000-000000000001");
    const auto second = make_id("23000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(first, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(second, "rgsml.dsp.gain", 1));

    auto request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 4), chain,
        {{first, gain(6.0)}, {second, gain(-12.0)}}, frame_count(7));
    QVERIFY(request);
    QVERIFY(chain.set_user_bypass(first, true));

    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(result);
    const auto actual = *result.value()->view().channel(0).value();
    const auto plus_six = 1.9952623149688795;
    const auto minus_twelve = 0.251188643150958;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        QCOMPARE(actual[index], (samples[index] * plus_six) * minus_twelve);
    }
    QCOMPARE(result.value()->gain_signatures().size(), std::size_t{2});
    QCOMPARE(result.value()->gain_signatures()[0].instance_id, first);
    QCOMPARE(result.value()->gain_signatures()[1].instance_id, second);
    QCOMPARE(result.value()->gain_signatures()[0].gain_db, 6.0);
    QCOMPARE(result.value()->gain_signatures()[1].gain_db, -12.0);
}

void RenderPreviewTest::requiredListeningPartitionsMatchForAllGains()
{
    std::vector<double> samples(600U);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = static_cast<double>(
            static_cast<std::int64_t>(index % 97U) - 48) / 128.0;
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("23500000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));

    for (const auto gain_db : {0.0, 6.0, -12.0}) {
        std::vector<std::uint64_t> reference;
        for (const auto block : {64, 257}) {
            auto request = rgsml::render::RenderRequest::create(
                source.value()->view(), frame_range(0, 600), chain,
                {{id, gain(gain_db)}}, frame_count(block));
            auto result = rgsml::render::render_preview(
                *request.value(), *registry.value());
            QVERIFY(result);
            if (reference.empty()) {
                reference = bits(result.value()->view());
            } else {
                QCOMPARE(bits(result.value()->view()), reference);
            }
        }
    }
}

void RenderPreviewTest::activeUnavailableModuleFails()
{
    const std::array samples{0.25};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    QVERIFY(chain.add(
        make_id("24000000-0000-0000-0000-000000000001"),
        "rgsml.dsp.compressor", 0));
    auto request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1), chain, {}, frame_count(1));
    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(!result);
    QCOMPARE(result.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
}

void RenderPreviewTest::resultLifetimeIsIndependent()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto result = [&registry] {
        const std::array samples{0.25, -0.5};
        auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
        auto chain = empty_chain(*registry.value());
        auto request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 2), chain, {}, frame_count(64));
        return rgsml::render::render_preview(*request.value(), *registry.value());
    }();
    QVERIFY(result);
    QCOMPARE(
        bits(result.value()->view()),
        std::vector<std::uint64_t>({
            std::bit_cast<std::uint64_t>(0.25),
            std::bit_cast<std::uint64_t>(-0.5)}));
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::RenderPreviewTest)

#include "test_render_preview.moc"
