#include "../../unit/audio/wav_test_support.hpp"
#include "../../unit/render/render_test_support.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QtTest/QTest>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace rgsml::tests {

class RenderWavIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void wavReaderCanonicalSourceToOwnedRenderResult();
};

void RenderWavIntegrationTest::wavReaderCanonicalSourceToOwnedRenderResult()
{
    using namespace wav_support;
    using namespace render_support;

    struct Case final {
        std::uint16_t channels;
        std::uint32_t sample_rate;
        std::vector<std::int64_t> interleaved_codes;
    };
    const std::array cases{
        Case{1U, 48'000U, {-16'384, -8'192, 0, 24'576}},
        Case{2U, 44'100U, {
            -16'384, 16'384,
            -8'192, 8'192,
            0, 1,
            24'576, -24'576}},
    };

    for (const auto& item : cases) {
        const auto payload = pcm_payload(item.interleaved_codes, 16U);
        const auto bytes = make_wav(
            1U, 16U, item.channels, item.sample_rate, payload);
        auto control = std::make_shared<ReaderControl>();
        auto reader = rgsml::audio::WavReader::open(memory_reader(bytes, control));
        QVERIFY(reader);

    auto source = rgsml::audio::AudioBuffer::create(
        (*reader.value())->info().audio_format(),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        rgsml::core::FrameIndex{0},
        (*reader.value())->info().frame_count());
    QVERIFY(source);
    auto read = (*reader.value())->read_frames(
        rgsml::core::FrameIndex{0}, source.value()->mutable_view());
    QVERIFY(read);
    QCOMPARE(read.value()->value(), std::int64_t{4});
    const auto source_bits = render_support::bits(source.value()->view());

    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = rgsml::dsp::ProcessingChain::create(
        *registry.value(),
        {rgsml::dsp::ProcessingStage::MASTER, rgsml::dsp::ChainSegment::MANUAL});
    const auto id = make_id("25000000-0000-0000-0000-000000000001");
    QVERIFY(chain.value()->add(id, "rgsml.dsp.gain", 0));
    auto parameters = rgsml::dsp::GainParameters::create(6.0);
    auto request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 4), *chain.value(),
        {{id, *parameters.value()}}, render_support::frame_count(257));
    QVERIFY(request);
    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(result);

    QCOMPARE(result.value()->view().format(), source.value()->view().format());
    QCOMPARE(result.value()->view().absolute_range(), frame_range(0, 4));
    QCOMPARE(result.value()->view().frame_count().value(), std::int64_t{4});
    QCOMPARE(render_support::bits(source.value()->view()), source_bits);
    const auto factor = 1.9952623149688795;
    for (std::size_t channel = 0; channel < item.channels; ++channel) {
        const auto input = *source.value()->view().channel(channel).value();
        const auto output = *result.value()->view().channel(channel).value();
        for (std::size_t frame = 0; frame < input.size(); ++frame) {
            QCOMPARE(output[frame], input[frame] * factor);
        }
    }
    QVERIFY((*reader.value())->close());
    }
}

}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::RenderWavIntegrationTest)

#include "test_render_wav.moc"
