#include "../render/render_test_support.hpp"

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
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;

[[nodiscard]] std::unique_ptr<StereoMsModule> checkpoint_module(
    const ModuleRegistry& registry,
    const StereoMsParameters& parameters)
{
    const auto descriptor = registry.find_descriptor("rgsml.dsp.stereo-ms");
    Q_ASSERT(descriptor);
    auto result = StereoMsModule::create(descriptor.value()->get(), parameters);
    Q_ASSERT(result);
    return std::move(*result.value());
}

class StereoMsCheckpointTest final : public QObject {
    Q_OBJECT

private slots:
    void exactResumeBothCrossoverModesAndCutLocations();
    void rejectsCorruptionMetadataAndWrongPositionAtomically();
    void effectiveSonicFingerprintApplicability();
    void unpreparedUnboundAndEndedStreamPolicies();
};

void StereoMsCheckpointTest::exactResumeBothCrossoverModesAndCutLocations()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    constexpr std::size_t count = 65;
    std::array<double, count> left{};
    std::array<double, count> right{};
    for (std::size_t n = 0; n < count; ++n) {
        left[n] = (n == 0 ? 1.0 : 0.0)
            + 0.25 * std::sin(0.17 * static_cast<double>(n));
        right[n] = (n == 2 ? -0.75 : 0.0)
            + 0.45 * std::cos(0.13 * static_cast<double>(n));
    }
    const auto layout = rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(count))};

    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        for (double beta_percent : {0.0, 25.0, 50.0, 100.0}) {
            auto p = StereoMsParameters::create(
                -1.0, 2.0, false, mode, 120.0, beta_percent);
            QVERIFY(p);
            auto direct = checkpoint_module(*registry.value(), *p.value());
            QVERIFY(direct->prepare(spec));
            auto full_in = make_buffer(layout, 0, left, right);
            auto full_out = make_buffer(layout, 0, left, right);
            QVERIFY(full_in);
            QVERIFY(full_out);
            QVERIFY(direct->process(
                full_in.value()->view(), full_out.value()->mutable_view(),
                DspProcessContext{frame_range(0, static_cast<std::int64_t>(count)),
                                  true, true}));
            const auto expected_l = *full_out.value()->view().channel(0).value();
            const auto expected_r = *full_out.value()->view().channel(1).value();

            for (std::size_t cut : {1U, 2U, 3U, 11U, 12U, 13U, 31U, 64U}) {
                auto prefix = checkpoint_module(*registry.value(), *p.value());
                auto resumed = checkpoint_module(*registry.value(), *p.value());
                QVERIFY(prefix->prepare(spec));
                QVERIFY(resumed->prepare(spec));

                const auto prefix_l = std::span<const double>{left}.first(cut);
                const auto prefix_r = std::span<const double>{right}.first(cut);
                auto input0 = make_buffer(layout, 0, prefix_l, prefix_r);
                auto output0 = make_buffer(layout, 0, prefix_l, prefix_r);
                QVERIFY(input0);
                QVERIFY(output0);
                QVERIFY(prefix->process(
                    input0.value()->view(), output0.value()->mutable_view(),
                    DspProcessContext{frame_range(0, static_cast<std::int64_t>(cut)),
                                      true, false}));
                const auto out0_l = *output0.value()->view().channel(0).value();
                const auto out0_r = *output0.value()->view().channel(1).value();
                for (std::size_t j = 0; j < cut; ++j) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(out0_l[j]),
                             std::bit_cast<std::uint64_t>(expected_l[j]));
                    QCOMPARE(std::bit_cast<std::uint64_t>(out0_r[j]),
                             std::bit_cast<std::uint64_t>(expected_r[j]));
                }

                const auto checkpoint = prefix->runtime_checkpoint();
                QVERIFY(checkpoint);
                QVERIFY(checkpoint.value()->next_input_frame.has_value());
                QCOMPARE(checkpoint.value()->next_input_frame->value(),
                         static_cast<std::int64_t>(cut));
                QCOMPARE(checkpoint.value()->payload.size(), std::size_t{136});
                QVERIFY(resumed->restore_runtime_checkpoint(*checkpoint.value()));

                const auto suffix_l = std::span<const double>{left}.subspan(cut);
                const auto suffix_r = std::span<const double>{right}.subspan(cut);
                auto input1 = make_buffer(layout, static_cast<std::int64_t>(cut),
                                          suffix_l, suffix_r);
                auto output1 = make_buffer(layout, static_cast<std::int64_t>(cut),
                                           suffix_l, suffix_r);
                QVERIFY(input1);
                QVERIFY(output1);
                QVERIFY(resumed->process(
                    input1.value()->view(), output1.value()->mutable_view(),
                    DspProcessContext{
                        frame_range(static_cast<std::int64_t>(cut),
                                    static_cast<std::int64_t>(count)),
                        false, true}));
                const auto out1_l = *output1.value()->view().channel(0).value();
                const auto out1_r = *output1.value()->view().channel(1).value();
                for (std::size_t j = cut; j < count; ++j) {
                    QCOMPARE(std::bit_cast<std::uint64_t>(out1_l[j-cut]),
                             std::bit_cast<std::uint64_t>(expected_l[j]));
                    QCOMPARE(std::bit_cast<std::uint64_t>(out1_r[j-cut]),
                             std::bit_cast<std::uint64_t>(expected_r[j]));
                }
            }
        }
    }
}

void StereoMsCheckpointTest::rejectsCorruptionMetadataAndWrongPositionAtomically()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto p = StereoMsParameters::create(2.0, -1.0, false,
                                         MonoBassMode::LR24, 120.0, 50.0);
    QVERIFY(p);
    const auto layout = rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(8)};
    auto source = checkpoint_module(*registry.value(), *p.value());
    auto destination = checkpoint_module(*registry.value(), *p.value());
    QVERIFY(source->prepare(spec));
    QVERIFY(destination->prepare(spec));
    const std::array<double, 8> left{0.25, 0.0, -0.5, 0.75, 0.13, -0.2, 0.1, 0.3};
    const std::array<double, 8> right{-0.15, 0.4, 0.2, 0.1, 0.6, 0.7, -0.1, 0.05};
    auto input0 = make_buffer(layout, 0, std::span<const double>{left}.first(4),
                             std::span<const double>{right}.first(4));
    auto output0 = make_buffer(layout, 0, std::span<const double>{left}.first(4),
                              std::span<const double>{right}.first(4));
    QVERIFY(input0);
    QVERIFY(output0);
    QVERIFY(source->process(
        input0.value()->view(), output0.value()->mutable_view(),
        DspProcessContext{frame_range(0, 4), true, false}));
    const auto saved = source->runtime_checkpoint();
    QVERIFY(saved);
    const auto original = *saved.value();
    QVERIFY(destination->restore_runtime_checkpoint(original));
    const auto before = destination->runtime_checkpoint();
    QVERIFY(before);

    auto check_reject = [&](const DspRuntimeCheckpoint& invalid) {
        const auto status = destination->restore_runtime_checkpoint(invalid);
        QVERIFY(!status);
        const auto after = destination->runtime_checkpoint();
        QVERIFY(after);
        QCOMPARE(*after.value(), *before.value());
    };

    auto bad = original;
    bad.payload[0] ^= 0x01U;
    check_reject(bad);
    bad = original;
    bad.payload.resize(135);
    check_reject(bad);
    bad = original;
    // Recalculate FNV footer for a forged payload containing a nonfinite
    // state; checksum alone must never make nonfinite recursive state legal.
    constexpr std::uint64_t nan_bits = UINT64_C(0x7ff8000000000000);
    for (std::size_t i = 0; i < 8; ++i) {
        bad.payload[i] = static_cast<std::uint8_t>((nan_bits >> (8 * i)) & 0xffU);
    }
    std::uint64_t checksum = UINT64_C(14695981039346656037);
    for (std::size_t i = 0; i < 128; ++i) {
        checksum ^= bad.payload[i];
        checksum *= UINT64_C(1099511628211);
    }
    for (std::size_t i = 0; i < 8; ++i) {
        bad.payload[128+i] = static_cast<std::uint8_t>((checksum >> (8*i)) & 0xffU);
    }
    check_reject(bad);
    bad = original;
    bad.algorithm_version = "9.9.9";
    check_reject(bad);
    bad = original;
    bad.parameter_schema_id = "wrong/schema";
    check_reject(bad);
    bad = original;
    bad.backend_identity = "another/backend";
    check_reject(bad);
    bad = original;
    bad.sonic_fingerprint += "corrupted";
    check_reject(bad);
    bad = original;
    bad.next_input_frame = rgsml::core::FrameIndex{99};
    check_reject(bad);
    bad = original;
    bad.audio_format = format(layout, 44100);
    check_reject(bad);

    // Prior valid state survives every rejected restore, including checksum
    // and metadata failures, and produces the exact same next block.
    auto a = make_buffer(layout, 4, std::span<const double>{left}.last(4),
                        std::span<const double>{right}.last(4));
    auto out_expected = make_buffer(layout, 4,
                                   std::span<const double>{left}.last(4),
                                   std::span<const double>{right}.last(4));
    auto out_actual = make_buffer(layout, 4,
                                 std::span<const double>{left}.last(4),
                                 std::span<const double>{right}.last(4));
    QVERIFY(a);
    QVERIFY(out_expected);
    QVERIFY(out_actual);
    const DspProcessContext last{frame_range(4,8), false, true};
    QVERIFY(source->process(a.value()->view(),
                            out_expected.value()->mutable_view(), last));
    QVERIFY(destination->process(a.value()->view(),
                                 out_actual.value()->mutable_view(), last));
    QCOMPARE(bits(out_expected.value()->view()), bits(out_actual.value()->view()));
}

void StereoMsCheckpointTest::effectiveSonicFingerprintApplicability()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    for (auto layout : {rgsml::audio::ChannelLayout::MONO_C,
                        rgsml::audio::ChannelLayout::STEREO_LR}) {
        const DspProcessSpec spec{
            format(layout), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            frame_count(4)};
        auto a = StereoMsParameters::create(
            -3.0, 4.0, true, MonoBassMode::LR12, 120.0, 75.0);
        auto b = StereoMsParameters::create(
            -3.0, -24.0, true, MonoBassMode::LR24, 300.0, 0.0);
        QVERIFY(a);
        QVERIFY(b);
        auto mod_a = checkpoint_module(*registry.value(), *a.value());
        auto mod_b = checkpoint_module(*registry.value(), *b.value());
        QVERIFY(mod_a->prepare(spec));
        QVERIFY(mod_b->prepare(spec));
        auto cp_a = mod_a->runtime_checkpoint();
        auto cp_b = mod_b->runtime_checkpoint();
        QVERIFY(cp_a);
        QVERIFY(cp_b);
        QCOMPARE(cp_a.value()->sonic_fingerprint, cp_b.value()->sonic_fingerprint);
        QVERIFY(mod_b->restore_runtime_checkpoint(*cp_a.value()));
        QVERIFY(cp_a.value()->payload.empty());
    }

    // All six stored fields are non-effective on mono, even Mid gain.
    {
        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::MONO_C),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4)};
        auto p = StereoMsParameters::create(
            12.0, -24.0, false, MonoBassMode::LR24, 300.0, 0.0);
        auto neutral = StereoMsParameters::create_default();
        QVERIFY(p);
        QVERIFY(neutral);
        auto one = checkpoint_module(*registry.value(), *p.value());
        auto two = checkpoint_module(*registry.value(), *neutral.value());
        QVERIFY(one->prepare(spec));
        QVERIFY(two->prepare(spec));
        auto cp = one->runtime_checkpoint();
        QVERIFY(cp);
        auto baseline = two->runtime_checkpoint();
        QVERIFY(baseline);
        QCOMPARE(cp.value()->sonic_fingerprint, baseline.value()->sonic_fingerprint);
        QVERIFY(two->restore_runtime_checkpoint(*cp.value()));
    }

    // OFF mode: cutoff and Low Width remain stored but non-effective.
    {
        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::STEREO_LR),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4)};
        auto x = StereoMsParameters::create(
            -2.0, 3.0, false, MonoBassMode::OFF, 40.0, 0.0);
        auto y = StereoMsParameters::create(
            -2.0, 3.0, false, MonoBassMode::OFF, 300.0, 100.0);
        QVERIFY(x);
        QVERIFY(y);
        auto m1 = checkpoint_module(*registry.value(), *x.value());
        auto m2 = checkpoint_module(*registry.value(), *y.value());
        QVERIFY(m1->prepare(spec));
        QVERIFY(m2->prepare(spec));
        auto cp = m1->runtime_checkpoint();
        QVERIFY(cp);
        auto second = m2->runtime_checkpoint();
        QVERIFY(second);
        QCOMPARE(cp.value()->sonic_fingerprint, second.value()->sonic_fingerprint);
        QVERIFY(m2->restore_runtime_checkpoint(*cp.value()));
    }

    // Mid gain is effective under Side mute; low-width is effective in LR.
    {
        const DspProcessSpec spec{
            format(rgsml::audio::ChannelLayout::STEREO_LR),
            rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4)};
        auto p0 = StereoMsParameters::create(
            0.0, 0.0, true, MonoBassMode::LR12, 120.0, 50.0);
        auto p1 = StereoMsParameters::create(
            1.0, 0.0, true, MonoBassMode::LR12, 120.0, 50.0);
        auto lr0 = StereoMsParameters::create(
            0.0, 0.0, false, MonoBassMode::LR24, 120.0, 50.0);
        auto lr1 = StereoMsParameters::create(
            0.0, 0.0, false, MonoBassMode::LR24, 120.0, 100.0);
        QVERIFY(p0);
        QVERIFY(p1);
        QVERIFY(lr0);
        QVERIFY(lr1);
        auto m0 = checkpoint_module(*registry.value(), *p0.value());
        auto m1 = checkpoint_module(*registry.value(), *p1.value());
        auto m2 = checkpoint_module(*registry.value(), *lr0.value());
        auto m3 = checkpoint_module(*registry.value(), *lr1.value());
        QVERIFY(m0->prepare(spec));
        QVERIFY(m1->prepare(spec));
        QVERIFY(m2->prepare(spec));
        QVERIFY(m3->prepare(spec));
        auto cp0 = m0->runtime_checkpoint();
        auto cp1 = m1->runtime_checkpoint();
        auto cp2 = m2->runtime_checkpoint();
        auto cp3 = m3->runtime_checkpoint();
        QVERIFY(cp0);
        QVERIFY(cp1);
        QVERIFY(cp2);
        QVERIFY(cp3);
        QVERIFY(cp0.value()->sonic_fingerprint != cp1.value()->sonic_fingerprint);
        QVERIFY(cp2.value()->sonic_fingerprint != cp3.value()->sonic_fingerprint);
        QVERIFY(!m3->restore_runtime_checkpoint(*cp2.value()));
    }
}

void StereoMsCheckpointTest::unpreparedUnboundAndEndedStreamPolicies()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto p = StereoMsParameters::create(
        0.0, 0.0, false, MonoBassMode::LR12, 120.0, 100.0);
    QVERIFY(p);
    auto module = checkpoint_module(*registry.value(), *p.value());
    QVERIFY(!module->runtime_checkpoint());
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, frame_count(4)};
    QVERIFY(module->prepare(spec));
    auto unbound = module->runtime_checkpoint();
    QVERIFY(unbound);
    QVERIFY(!unbound.value()->next_input_frame.has_value());
    QCOMPARE(unbound.value()->payload.size(), std::size_t{136});

    auto fresh = checkpoint_module(*registry.value(), *p.value());
    QVERIFY(fresh->prepare(spec));
    QVERIFY(fresh->restore_runtime_checkpoint(*unbound.value()));
    auto altered = *unbound.value();
    // A valid-checksum but nonzero unbound state is not acceptable.
    altered.payload[0] = 1U;
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (std::size_t i = 0; i < 128; ++i) {
        hash ^= altered.payload[i];
        hash *= UINT64_C(1099511628211);
    }
    for (std::size_t i = 0; i < 8; ++i) {
        altered.payload[128+i] =
            static_cast<std::uint8_t>((hash >> (8*i)) & 0xffU);
    }
    QVERIFY(!fresh->restore_runtime_checkpoint(altered));

    const std::array<double,4> frames{0.5, 0.0, 0.0, 0.0};
    auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR,
                             0, frames, frames);
    auto output = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR,
                              0, frames, frames);
    QVERIFY(input);
    QVERIFY(output);
    QVERIFY(module->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0,4), true, true}));
    QVERIFY(!module->runtime_checkpoint());
    QVERIFY(!module->restore_runtime_checkpoint(*unbound.value()));
    module->reset();
    QVERIFY(module->runtime_checkpoint());
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsCheckpointTest)

#include "test_stereo_ms_checkpoint.moc"
