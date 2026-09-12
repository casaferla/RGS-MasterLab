#include "expected_bits.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/dsp/gain_module.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rgsml::tests {
namespace {

[[nodiscard]] std::uint64_t ulp_distance(std::uint64_t left, std::uint64_t right)
{
    return left > right ? left - right : right - left;
}

[[nodiscard]] rgsml::audio::AudioBuffer buffer_from_bits(
    const std::array<std::uint64_t, 6>& values)
{
    auto rate = rgsml::core::SampleRate::create(48'000);
    auto format = rgsml::audio::AudioFormat::create(
        *rate.value(), rgsml::audio::ChannelLayout::MONO_C);
    auto frames = rgsml::core::FrameCount::create(6);
    auto buffer = rgsml::audio::AudioBuffer::create(
        *format.value(), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        rgsml::core::FrameIndex{0}, *frames.value());
    auto plane = buffer.value()->mutable_view().channel(0);
    std::transform(values.begin(), values.end(), plane.value()->begin(), [](std::uint64_t bits) {
        return std::bit_cast<double>(bits);
    });
    return std::move(*buffer.value());
}

class GainGoldenTest final : public QObject {
    Q_OBJECT

private slots:
    void independentMpfrVectorsWithinFourUlp();
    void zeroDbIsExactForAllFiniteClasses();
};

void GainGoldenTest::independentMpfrVectorsWithinFourUlp()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    const auto& descriptor = registry.value()
        ->find_descriptor("rgsml.dsp.gain").value()->get();
    auto input = buffer_from_bits(gain_golden::kInputBits);
    auto frames = rgsml::core::FrameCount::create(6);
    auto range = rgsml::core::FrameRange::create(
        rgsml::core::FrameIndex{0}, rgsml::core::FrameIndex{6});

    for (const auto& vector : gain_golden::kGoldenVectors) {
        auto parameters = rgsml::dsp::GainParameters::create(vector.gain_db);
        auto module = rgsml::dsp::GainModule::create(descriptor, *parameters.value());
        auto output = buffer_from_bits(gain_golden::kInputBits);
        const rgsml::dsp::DspProcessSpec spec{
            input.view().format(),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            *frames.value()};
        QVERIFY((*module.value())->prepare(spec));
        QVERIFY((*module.value())->process(
            input.view(), output.mutable_view(),
            rgsml::dsp::DspProcessContext{*range.value(), true, true}));
        const auto actual = *output.view().channel(0).value();
        for (std::size_t index = 0; index < actual.size(); ++index) {
            const auto actual_bits = std::bit_cast<std::uint64_t>(actual[index]);
            const auto expected_bits = vector.output_bits[index];
            QVERIFY2(ulp_distance(actual_bits, expected_bits) <= 4U,
                "Gain output differs by more than the frozen 4-ULP oracle tolerance.");
            if ((expected_bits << 1U) == 0U || expected_bits < UINT64_C(0x0010000000000000)
                || (expected_bits >= UINT64_C(0x8000000000000000)
                    && expected_bits < UINT64_C(0x8010000000000000))) {
                QCOMPARE(actual_bits >> 63U, expected_bits >> 63U);
            }
        }
    }
}

void GainGoldenTest::zeroDbIsExactForAllFiniteClasses()
{
    const std::array<std::uint64_t, 6> identity_bits{
        UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
        UINT64_C(0x0000000000000001), UINT64_C(0x8000000000000001),
        UINT64_C(0x7fefffffffffffff), UINT64_C(0xffefffffffffffff)};
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    const auto& descriptor = registry.value()
        ->find_descriptor("rgsml.dsp.gain").value()->get();
    auto parameters = rgsml::dsp::GainParameters::create(-0.0);
    auto module = rgsml::dsp::GainModule::create(descriptor, *parameters.value());
    auto input = buffer_from_bits(identity_bits);
    auto output = buffer_from_bits(gain_golden::kInputBits);
    auto frames = rgsml::core::FrameCount::create(6);
    auto range = rgsml::core::FrameRange::create(
        rgsml::core::FrameIndex{0}, rgsml::core::FrameIndex{6});
    QVERIFY((*module.value())->prepare({
        input.view().format(),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        *frames.value()}));
    QVERIFY((*module.value())->process(
        input.view(), output.mutable_view(),
        {*range.value(), true, true}));
    const auto actual = *output.view().channel(0).value();
    for (std::size_t index = 0; index < actual.size(); ++index) {
        QCOMPARE(std::bit_cast<std::uint64_t>(actual[index]), identity_bits[index]);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::GainGoldenTest)

#include "test_gain_golden.moc"
