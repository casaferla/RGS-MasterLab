#include "../render/render_test_support.hpp"
#include "test_support.hpp"

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

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;
using dsp_support::error_category;

[[nodiscard]] const ModuleDescriptor& spatial_descriptor(const ModuleRegistry& reg)
{
    return reg.find_descriptor("rgsml.dsp.stereo-ms").value()->get();
}

[[nodiscard]] std::unique_ptr<StereoMsModule> make_module(
    const ModuleRegistry& reg, StereoMsParameters params)
{
    auto result = StereoMsModule::create(spatial_descriptor(reg), params);
    Q_ASSERT(result);
    return std::move(*result.value());
}

class StereoMsModuleTest final : public QObject {
    Q_OBJECT

private slots:
    void rejectsMismatchedDescriptorAndUnpreparedUse();
    void monoBitExactAcrossNonEffectiveSettings();
    void stereoBroadbandAndNoHiddenCompensation();
    void sideMuteDominatesAllStoredSideFields();
    void activeCrossoversProduceFrozenSettlingWithoutAllpassSubstitution();
    void rejectsInvalidContextAndNonFiniteInput();
};

void StereoMsModuleTest::rejectsMismatchedDescriptorAndUnpreparedUse()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto defaults = StereoMsParameters::create_default();
    QVERIFY(defaults);
    auto wrong = StereoMsModule::create(
        registry.value()->find_descriptor("rgsml.dsp.gain").value()->get(),
        *defaults.value());
    QVERIFY(!wrong);
    QCOMPARE(wrong.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);

    auto module = make_module(*registry.value(), *defaults.value());
    const std::array src{0.5, -0.5};
    auto in = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, src);
    auto out = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, src);
    QVERIFY(in);
    QVERIFY(out);
    const auto before = module->process(
        in.value()->view(), out.value()->mutable_view(),
        DspProcessContext{frame_range(0, 2), true, true});
    QVERIFY(!before);
    QCOMPARE(error_category(*before.error()), std::string_view{"DSP_MODULE_NOT_PREPARED"});
}

void StereoMsModuleTest::monoBitExactAcrossNonEffectiveSettings()
{
    const std::array<double, 12> src{
        0.0, -0.0,
        std::bit_cast<double>(UINT64_C(0x0000000000000001)),
        std::bit_cast<double>(UINT64_C(0x8000000000000001)),
        std::numeric_limits<double>::min(),
        -std::numeric_limits<double>::min(),
        std::nextafter(1.0, 0.0), 1.0,
        std::nextafter(1.0, 2.0), 2.0, -2.0,
        std::bit_cast<double>(UINT64_C(0x7fefffffffffffff))};
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto in = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 17, src);
    QVERIFY(in);
    for (const auto mode : {MonoBassMode::OFF, MonoBassMode::LR12, MonoBassMode::LR24}) {
        for (bool muted : {false, true}) {
            auto parameters = StereoMsParameters::create(
                12.0, -24.0, muted, mode, 300.0, 0.0);
            QVERIFY(parameters);
            auto module = make_module(*registry.value(), *parameters.value());
            const DspProcessSpec spec{
                format(rgsml::audio::ChannelLayout::MONO_C),
                rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                frame_count(12)};
            const auto req = module->runtime_requirements(spec);
            QVERIFY(req);
            QCOMPARE(req.value()->algorithmic_latency_frames.value(), std::int64_t{0});
            QCOMPARE(req.value()->effective_tail_frames.value(), std::int64_t{0});
            QVERIFY(module->prepare(spec));
            module->reset();
            auto out = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 17, src);
            QVERIFY(out);
            QVERIFY(module->process(
                in.value()->view(), out.value()->mutable_view(),
                DspProcessContext{frame_range(17, 29), true, true}));
            QCOMPARE(bits(in.value()->view()), bits(out.value()->view()));
        }
    }
}

void StereoMsModuleTest::stereoBroadbandAndNoHiddenCompensation()
{
    const std::array left{1.0, 1.0, 0.5, -0.5, 0.0};
    const std::array right{1.0, -1.0, -0.5, 0.5, 0.0};
    auto reg = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    auto params = StereoMsParameters::create(0.0, 6.0);
    QVERIFY(params);
    auto mod = make_module(*reg.value(), *params.value());
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(5)};
    QVERIFY(mod->prepare(spec));
    auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto output = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    QVERIFY(input);
    QVERIFY(output);
    QVERIFY(mod->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 5), true, true}));
    const auto l = *output.value()->view().channel(0).value();
    const auto r = *output.value()->view().channel(1).value();
    const double side_gain = std::pow(10.0, 6.0 / 20.0);
    QVERIFY(std::abs(l[0] - 1.0) < 1e-12);
    QVERIFY(std::abs(r[0] - 1.0) < 1e-12);
    QVERIFY(std::abs(l[1] - side_gain) < 1e-12);
    QVERIFY(std::abs(r[1] + side_gain) < 1e-12);
    QVERIFY(std::abs(l[2] - side_gain * 0.5) < 1e-12);
    QVERIFY(std::abs(r[2] + side_gain * 0.5) < 1e-12);
    QVERIFY(l[4] == 0.0);
    QVERIFY(r[4] == 0.0);
    // No limiter or loudness makeup: anti-phase peak is allowed above 1.0.
    QVERIFY(l[1] > 1.0);
}

void StereoMsModuleTest::sideMuteDominatesAllStoredSideFields()
{
    const std::array left{1.0, 0.5, -0.5, 2.0};
    const std::array right{-1.0, 0.5, 0.25, -2.0};
    auto reg = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    auto a = StereoMsParameters::create(
        -3.0, -24.0, true, MonoBassMode::OFF, 40.0, 100.0);
    auto b = StereoMsParameters::create(
        -3.0, 12.0, true, MonoBassMode::LR24, 300.0, 0.0);
    QVERIFY(a);
    QVERIFY(b);
    auto ma = make_module(*reg.value(), *a.value());
    auto mb = make_module(*reg.value(), *b.value());
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(4)};
    QVERIFY(ma->prepare(spec));
    QVERIFY(mb->prepare(spec));
    auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto oa = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto ob = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    QVERIFY(input);
    QVERIFY(oa);
    QVERIFY(ob);
    const DspProcessContext context{frame_range(0, 4), true, true};
    QVERIFY(ma->process(input.value()->view(), oa.value()->mutable_view(), context));
    QVERIFY(mb->process(input.value()->view(), ob.value()->mutable_view(), context));
    QCOMPARE(bits(oa.value()->view()), bits(ob.value()->view()));
    const auto l = *oa.value()->view().channel(0).value();
    const auto r = *oa.value()->view().channel(1).value();
    const double gain = std::pow(10.0, -3.0 / 20.0);
    for (std::size_t i = 0; i < left.size(); ++i) {
        QVERIFY(l[i] == r[i]);
        const double expected = (left[i] * 0.5 + right[i] * 0.5) * gain;
        QVERIFY(std::abs(l[i] - expected) < 1e-13);
    }
    QVERIFY(l[0] == 0.0);
    QVERIFY(l[3] == 0.0);
}

void StereoMsModuleTest::activeCrossoversProduceFrozenSettlingWithoutAllpassSubstitution()
{
    auto reg = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    const DspProcessSpec stereo{
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(4)};
    for (auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        auto p = StereoMsParameters::create(
            0.0, 0.0, false, mode, 120.0, 100.0);
        QVERIFY(p);
        auto mod = make_module(*reg.value(), *p.value());
        auto req = mod->runtime_requirements(stereo);
        QVERIFY(req);
        const auto frozen = mode == MonoBassMode::LR12 ? 1760 : 2488;
        QCOMPARE(req.value()->pre_context_frames.value(), frozen);
        QCOMPARE(req.value()->post_context_frames.value(), frozen);
        QCOMPARE(req.value()->effective_tail_frames.value(), frozen);
        QCOMPARE(req.value()->algorithmic_latency_frames.value(), std::int64_t{0});
        QVERIFY(mod->prepare(stereo));

        // Mid-only impulse must carry the LR all-pass phase, even with
        // lowBandWidthPercent=100; a fabricated identity bypass is invalid.
        const std::array left{1.0, 0.0, 0.0, 0.0};
        auto input = make_buffer(
            rgsml::audio::ChannelLayout::STEREO_LR, 0, left, left);
        auto output = make_buffer(
            rgsml::audio::ChannelLayout::STEREO_LR, 0, left, left);
        QVERIFY(input);
        QVERIFY(output);
        QVERIFY(mod->process(input.value()->view(), output.value()->mutable_view(),
            DspProcessContext{frame_range(0, 4), true, true}));
        const auto rendered = *output.value()->view().channel(0).value();
        const auto right = *output.value()->view().channel(1).value();
        QVERIFY(std::abs(rendered[0] - 1.0) > 1e-3);
        for (std::size_t i = 0; i < left.size(); ++i) {
            QVERIFY(std::isfinite(rendered[i]));
            QCOMPARE(rendered[i], right[i]);
        }

        // Side-muted Stereo ignores crossover mode/cutoff and its settling.
        auto muted = StereoMsParameters::create(
            0.0, 12.0, true, mode, 300.0, 0.0);
        QVERIFY(muted);
        auto muted_module = make_module(*reg.value(), *muted.value());
        auto muted_req = muted_module->runtime_requirements(stereo);
        QVERIFY(muted_req);
        QCOMPARE(muted_req.value()->effective_tail_frames.value(), std::int64_t{0});
    }

    // A failed re-prepare must disarm a previously playable realization.
    auto off = StereoMsParameters::create_default();
    QVERIFY(off);
    auto mod = make_module(*reg.value(), *off.value());
    QVERIFY(mod->prepare(stereo));
    const DspProcessSpec bad{
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(0)};
    QVERIFY(!mod->prepare(bad));
    const std::array left{0.0, 0.0, 0.0, 0.0};
    auto in = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, left);
    auto out = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, left);
    QVERIFY(in);
    QVERIFY(out);
    auto status = mod->process(in.value()->view(), out.value()->mutable_view(),
                               DspProcessContext{frame_range(0, 4), true, true});
    QVERIFY(!status);
    QCOMPARE(error_category(*status.error()),
             std::string_view{"DSP_MODULE_NOT_PREPARED"});
}

void StereoMsModuleTest::rejectsInvalidContextAndNonFiniteInput()
{
    auto reg = ModuleRegistry::create_dsp_package_v1();
    auto parameters = StereoMsParameters::create_default();
    QVERIFY(reg);
    QVERIFY(parameters);
    auto mod = make_module(*reg.value(), *parameters.value());
    const DspProcessSpec spec{
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(2)};
    QVERIFY(mod->prepare(spec));
    const std::array left{0.5, -0.5};
    const std::array right{-0.5, 0.5};
    auto input = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto output = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    QVERIFY(input);
    QVERIFY(output);
    auto invalid = mod->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(1, 3), true, true});
    QVERIFY(!invalid);
    QCOMPARE(error_category(*invalid.error()),
             std::string_view{"INVALID_DSP_PROCESS_CONTEXT"});
    auto overlap = mod->process(
        input.value()->view(), input.value()->mutable_view(),
        DspProcessContext{frame_range(0, 2), true, true});
    QVERIFY(!overlap);
    QCOMPARE(error_category(*overlap.error()),
             std::string_view{"OVERLAPPING_AUDIO_VIEWS"});
    auto plane = input.value()->mutable_view().channel(0);
    (*plane.value())[1] = std::numeric_limits<double>::infinity();
    const auto original_output = bits(output.value()->view());
    auto nonfinite = mod->process(
        input.value()->view(), output.value()->mutable_view(),
        DspProcessContext{frame_range(0, 2), true, true});
    QVERIFY(!nonfinite);
    QCOMPARE(error_category(*nonfinite.error()),
             std::string_view{"NONFINITE_INPUT_SAMPLE"});
    QCOMPARE(bits(output.value()->view()), original_output);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsModuleTest)

#include "test_stereo_ms_module.moc"
