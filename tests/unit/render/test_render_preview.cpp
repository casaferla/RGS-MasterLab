#include "render_test_support.hpp"

#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QtTest/QTest>

#include <array>
#include <bit>
#include <cmath>
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

[[nodiscard]] rgsml::dsp::ParametricEqParameters bell_eq(double freq_hz, double gain_db, double q)
{
    const auto band_id = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = rgsml::dsp::EqBandParameters::create(
        band_id,
        true,
        rgsml::dsp::EqFilterType::BELL,
        rgsml::dsp::EqRouting::STEREO,
        rgsml::dsp::BellPayload{freq_hz, gain_db, q});
    Q_ASSERT(band);
    auto eq_params = rgsml::dsp::ParametricEqParameters::create({*band.value()});
    Q_ASSERT(eq_params);
    return *eq_params.value();
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
    void parametricEqPreviewAndCausalPrerollEquivalence();
    void mixedGainAndEqOrderAndBypassIdentity();
    void activeMixedGainAndEqChainIntegration();
    void bindingOrderInvariance();
    void eqSignatureContent();
    void errorOrderingActiveUnsupportedModuleFailsTruthfully();
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

    const rgsml::dsp::ModuleExecutionBinding binding{id, gain(6.0)};
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

    // Orphan binding
    const auto orphan_id = make_id("20000000-0000-0000-0000-000000000099");
    const rgsml::dsp::ModuleExecutionBinding orphan_binding{orphan_id, gain(0.0)};
    auto orphan_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding, orphan_binding}, frame_count(7));
    QVERIFY(!orphan_req);

    // Payload mismatch
    const auto eq_id = make_id("20000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));
    const rgsml::dsp::ModuleExecutionBinding mismatch_binding{eq_id, gain(0.0)};
    auto mismatch_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding, mismatch_binding}, frame_count(7));
    QVERIFY(!mismatch_req);
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
    QCOMPARE(result.value()->signatures().size(), std::size_t{1});
    QCOMPARE(
        result.value()->signatures().front().disposition,
        rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY);
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
    QCOMPARE(result.value()->signatures().size(), std::size_t{2});
    QCOMPARE(result.value()->signatures()[0].instance_id, first);
    QCOMPARE(result.value()->signatures()[1].instance_id, second);
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

void RenderPreviewTest::parametricEqPreviewAndCausalPrerollEquivalence()
{
    // Generate state-revealing input signal
    std::vector<double> left(1000U);
    std::vector<double> right(1000U);
    for (std::size_t i = 0; i < 1000U; ++i) {
        left[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    const auto source_bits_before = bits(source.value()->view());
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto eq_id = make_id("25000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 0));

    const auto eq_params = bell_eq(1000.0, 6.0, 1.414);
    const rgsml::dsp::ModuleExecutionBinding binding{eq_id, eq_params};

    // Full render from 0 to 1000
    auto full_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1000), chain,
        {binding}, frame_count(64));
    QVERIFY(full_req);
    auto full_res = rgsml::render::render_preview(*full_req.value(), *registry.value());
    QVERIFY(full_res);

    // Mid-window render from 500 to 1000 (with causal preroll from 0 to 500) across block sizes 1, 7, 64, 257
    auto full_subview = full_res.value()->view().subview(
        rgsml::core::FrameIndex{500}, frame_count(500));
    QVERIFY(full_subview);
    const auto expected_bits = bits(*full_subview.value());

    for (const auto block : {1, 7, 64, 257}) {
        auto mid_req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(500, 1000), chain,
            {binding}, frame_count(block));
        QVERIFY(mid_req);
        auto mid_res = rgsml::render::render_preview(*mid_req.value(), *registry.value());
        QVERIFY(mid_res);
        QCOMPARE(bits(mid_res.value()->view()), expected_bits);
    }

    // Qualification requirement 4: Verify Source bits are identical AFTER all renders
    QCOMPARE(bits(source.value()->view()), source_bits_before);
}

void RenderPreviewTest::mixedGainAndEqOrderAndBypassIdentity()
{
    const std::array left{0.5, -0.25, 0.75, -0.5};
    const std::array right{-0.5, 0.25, -0.75, 0.5};
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto gain_id = make_id("26000000-0000-0000-0000-000000000001");
    const auto eq_id = make_id("26000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));

    const auto gain_params = gain(6.0);
    const auto eq_params = bell_eq(1000.0, 3.0, 0.707);

    // Bypass both EQ and Gain -> exact identity
    QVERIFY(chain.set_user_bypass(gain_id, true));
    QVERIFY(chain.set_user_bypass(eq_id, true));

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 4), chain,
        {{gain_id, gain_params}, {eq_id, eq_params}}, frame_count(64));
    QVERIFY(req);
    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);
    QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));
    QCOMPARE(res.value()->signatures().size(), std::size_t{2});
    QCOMPARE(res.value()->signatures()[0].disposition, rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY);
    QCOMPARE(res.value()->signatures()[1].disposition, rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY);
}

void RenderPreviewTest::activeMixedGainAndEqChainIntegration()
{
    // Qualification requirement 1: Active mixed Gain + EQ
    std::vector<double> left(100U);
    std::vector<double> right(100U);
    for (std::size_t i = 0; i < 100U; ++i) {
        left[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto gain_id = make_id("28000000-0000-0000-0000-000000000001");
    const auto eq_id = make_id("28000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));

    const rgsml::dsp::ModuleExecutionBinding gain_binding{gain_id, gain(6.0)};
    const rgsml::dsp::ModuleExecutionBinding eq_binding{eq_id, bell_eq(1000.0, 6.0, 1.414)};

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {gain_binding, eq_binding}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);

    // Verify non-identity output
    QVERIFY(bits(res.value()->view()) != bits(source.value()->view()));

    // Verify signatures appear in ProcessingChain order with PROCESSED disposition
    const auto& sigs = res.value()->signatures();
    QCOMPARE(sigs.size(), std::size_t{2});

    QCOMPARE(sigs[0].instance_id, gain_id);
    QCOMPARE(sigs[0].type_id, std::string{"rgsml.dsp.gain"});
    QCOMPARE(sigs[0].disposition, rgsml::render::ModuleExecutionDisposition::PROCESSED);

    QCOMPARE(sigs[1].instance_id, eq_id);
    QCOMPARE(sigs[1].type_id, std::string{"rgsml.dsp.parametric-eq"});
    QCOMPARE(sigs[1].disposition, rgsml::render::ModuleExecutionDisposition::PROCESSED);
}

void RenderPreviewTest::bindingOrderInvariance()
{
    // Qualification requirement 2: Binding collection order MUST NOT control execution
    std::vector<double> left(100U);
    std::vector<double> right(100U);
    for (std::size_t i = 0; i < 100U; ++i) {
        left[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto gain_id = make_id("29000000-0000-0000-0000-000000000001");
    const auto eq_id = make_id("29000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));

    const rgsml::dsp::ModuleExecutionBinding gain_binding{gain_id, gain(6.0)};
    const rgsml::dsp::ModuleExecutionBinding eq_binding{eq_id, bell_eq(1000.0, 6.0, 1.414)};

    // Supply bindings in forward order (gain -> eq)
    auto req_forward = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {gain_binding, eq_binding}, frame_count(64));
    QVERIFY(req_forward);
    auto res_forward = rgsml::render::render_preview(*req_forward.value(), *registry.value());
    QVERIFY(res_forward);

    // Supply bindings in REVERSED order (eq -> gain)
    auto req_reversed = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {eq_binding, gain_binding}, frame_count(64));
    QVERIFY(req_reversed);
    auto res_reversed = rgsml::render::render_preview(*req_reversed.value(), *registry.value());
    QVERIFY(res_reversed);

    // Output must match exactly
    QCOMPARE(bits(res_reversed.value()->view()), bits(res_forward.value()->view()));

    // Execution signatures must still be in chain order (gain_id -> eq_id)
    const auto& sigs = res_reversed.value()->signatures();
    QCOMPARE(sigs.size(), std::size_t{2});
    QCOMPARE(sigs[0].instance_id, gain_id);
    QCOMPARE(sigs[1].instance_id, eq_id);
}

void RenderPreviewTest::eqSignatureContent()
{
    // Qualification requirement 3: EQ signature contains ONLY enabled bands
    const auto band_enabled_id = *rgsml::core::Uuid::parse("30000000-0000-0000-0000-000000000001").value();
    const auto band_disabled_id = *rgsml::core::Uuid::parse("30000000-0000-0000-0000-000000000002").value();

    auto enabled_band = rgsml::dsp::EqBandParameters::create(
        band_enabled_id,
        true,
        rgsml::dsp::EqFilterType::BELL,
        rgsml::dsp::EqRouting::STEREO,
        rgsml::dsp::BellPayload{1000.0, 6.0, 0.707});
    QVERIFY(enabled_band);

    auto disabled_band = rgsml::dsp::EqBandParameters::create(
        band_disabled_id,
        false,
        rgsml::dsp::EqFilterType::LOW_SHELF,
        rgsml::dsp::EqRouting::LEFT,
        rgsml::dsp::ShelfPayload{100.0, -12.0, 1.0});
    QVERIFY(disabled_band);

    auto eq_params = rgsml::dsp::ParametricEqParameters::create({*enabled_band.value(), *disabled_band.value()});
    QVERIFY(eq_params);

    const std::array samples{0.25, -0.5, 0.5};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto eq_id = make_id("30000000-0000-0000-0000-000000000003");
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 0));

    const rgsml::dsp::ModuleExecutionBinding binding{eq_id, *eq_params.value()};

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 3), chain,
        {binding}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);

    const auto& sigs = res.value()->signatures();
    QCOMPARE(sigs.size(), std::size_t{1});
    QCOMPARE(sigs[0].instance_id, eq_id);

    const auto* eq_payload = std::get_if<rgsml::render::ParametricEqExecutionSignaturePayload>(&sigs[0].payload);
    QVERIFY(eq_payload != nullptr);

    // Verify only 1 enabled band appears and disabled band is absent
    QCOMPARE(eq_payload->enabled_bands.size(), std::size_t{1});

    const auto& band_sig = eq_payload->enabled_bands[0];
    QCOMPARE(band_sig.filter_type, rgsml::dsp::EqFilterType::BELL);
    QCOMPARE(band_sig.routing, rgsml::dsp::EqRouting::STEREO);

    const auto* bell = std::get_if<rgsml::dsp::BellPayload>(&band_sig.payload);
    QVERIFY(bell != nullptr);
    QCOMPARE(bell->frequency_hz, 1000.0);
    QCOMPARE(bell->gain_db, 6.0);
    QCOMPARE(bell->q, 0.707);
}

void RenderPreviewTest::errorOrderingActiveUnsupportedModuleFailsTruthfully()
{
    const std::array samples{0.25};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto comp_id = make_id("27000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 0));

    // Notice no binding is provided for compressor in RenderRequest
    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1), chain,
        {}, frame_count(64));
    QVERIFY(req); // Request creation succeeds because compressor is not a supported parameterized builtin

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(!res);
    QCOMPARE(res.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
    QCOMPARE(std::string_view{res.error()->message()}, std::string_view{"An active chain node has no production implementation."});
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::RenderPreviewTest)

#include "test_render_preview.moc"
