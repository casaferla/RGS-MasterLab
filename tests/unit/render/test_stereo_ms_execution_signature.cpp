#include "render_test_support.hpp"

#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <rgsml/render/stereo_ms_execution_signature.hpp>

#include <QtTest/QTest>

#include <optional>
#include <string_view>
#include <variant>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace rgsml::render;
using namespace render_support;

class StereoMsExecutionSignatureTest final : public QObject {
    Q_OBJECT
private slots:
    void exactActiveSixFieldsAndIdentity();
    void monoExcludesAllSonicFields();
    void sideMuteDominatesStoredGainAndCrossover();
    void offExcludesInactiveCutoffAndLowWidth();
    void bypassHasIdentityWithoutEffectiveSonicFields();
    void wrongDescriptorFailsClosed();
};

void StereoMsExecutionSignatureTest::exactActiveSixFieldsAndIdentity()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto descriptor=registry.value()->find_descriptor("rgsml.dsp.stereo-ms");
    QVERIFY(descriptor);
    const auto id=make_id("28500000-0000-0000-0000-000000000001");
    auto params=StereoMsParameters::create(-3.0,8.0,false,
                                          MonoBassMode::LR24,300.0,25.0);
    QVERIFY(params);
    auto signature=make_stereo_ms_execution_signature(descriptor.value()->get(),
        id,*params.value(),rgsml::audio::ChannelLayout::STEREO_LR,false);
    QVERIFY(signature);
    QCOMPARE(signature.value()->instance_id,id);
    QCOMPARE(signature.value()->type_id,std::string{"rgsml.dsp.stereo-ms"});
    QCOMPARE(signature.value()->algorithm_version,std::string{"1.0.0"});
    QCOMPARE(signature.value()->parameter_schema_id,
             std::string{"rgsml.dsp.stereo-ms.parameters/1.0.0"});
    QCOMPARE(signature.value()->disposition,ModuleExecutionDisposition::PROCESSED);
    const auto* p=std::get_if<StereoMsExecutionSignaturePayload>(
        &signature.value()->payload);
    QVERIFY(p != nullptr);
    QVERIFY(p->mid_gain_db == std::optional<double>{-3.0});
    QVERIFY(p->side_gain_db == std::optional<double>{8.0});
    QVERIFY(p->side_muted == std::optional<bool>{false});
    QVERIFY(p->mono_bass_mode == std::optional<MonoBassMode>{MonoBassMode::LR24});
    QVERIFY(p->mono_bass_cutoff_hz == std::optional<double>{300.0});
    QVERIFY(p->low_band_width_percent == std::optional<double>{25.0});
}

void StereoMsExecutionSignatureTest::monoExcludesAllSonicFields()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto d=registry.value()->find_descriptor("rgsml.dsp.stereo-ms");
    QVERIFY(d);
    const auto id=make_id("28500000-0000-0000-0000-000000000002");
    auto a=StereoMsParameters::create(12.0,-24.0,true,
                                    MonoBassMode::LR12,40.0,0.0);
    auto b=StereoMsParameters::create(-12.0,12.0,false,
                                    MonoBassMode::LR24,300.0,100.0);
    QVERIFY(a);
    QVERIFY(b);
    auto sa=make_stereo_ms_execution_signature(d.value()->get(),id,*a.value(),
        rgsml::audio::ChannelLayout::MONO_C,false);
    auto sb=make_stereo_ms_execution_signature(d.value()->get(),id,*b.value(),
        rgsml::audio::ChannelLayout::MONO_C,false);
    QVERIFY(sa);
    QVERIFY(sb);
    QVERIFY(*sa.value() == *sb.value());
    const auto* p=std::get_if<StereoMsExecutionSignaturePayload>(
        &sa.value()->payload);
    QVERIFY(p != nullptr);
    QVERIFY(!p->mid_gain_db && !p->side_gain_db && !p->side_muted
            && !p->mono_bass_mode && !p->mono_bass_cutoff_hz
            && !p->low_band_width_percent);
}

void StereoMsExecutionSignatureTest::sideMuteDominatesStoredGainAndCrossover()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto d=registry.value()->find_descriptor("rgsml.dsp.stereo-ms");
    QVERIFY(d);
    const auto id=make_id("28500000-0000-0000-0000-000000000003");
    auto a=StereoMsParameters::create(-3.0,8.0,true,
                                    MonoBassMode::LR24,300.0,25.0);
    auto b=StereoMsParameters::create(-3.0,-24.0,true,
                                    MonoBassMode::OFF,40.0,100.0);
    QVERIFY(a);
    QVERIFY(b);
    auto sa=make_stereo_ms_execution_signature(d.value()->get(),id,*a.value(),
        rgsml::audio::ChannelLayout::STEREO_LR,false);
    auto sb=make_stereo_ms_execution_signature(d.value()->get(),id,*b.value(),
        rgsml::audio::ChannelLayout::STEREO_LR,false);
    QVERIFY(sa);
    QVERIFY(sb);
    QVERIFY(*sa.value() == *sb.value());
    const auto* p=std::get_if<StereoMsExecutionSignaturePayload>(
        &sa.value()->payload);
    QVERIFY(p);
    QVERIFY(p->mid_gain_db == std::optional<double>{-3.0});
    QVERIFY(p->side_muted == std::optional<bool>{true});
    QVERIFY(!p->side_gain_db && !p->mono_bass_mode
            && !p->mono_bass_cutoff_hz && !p->low_band_width_percent);
}

void StereoMsExecutionSignatureTest::offExcludesInactiveCutoffAndLowWidth()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto d=registry.value()->find_descriptor("rgsml.dsp.stereo-ms");
    QVERIFY(d);
    const auto id=make_id("28500000-0000-0000-0000-000000000004");
    auto a=StereoMsParameters::create(-3.0,8.0,false,
                                    MonoBassMode::OFF,40.0,0.0);
    auto b=StereoMsParameters::create(-3.0,8.0,false,
                                    MonoBassMode::OFF,300.0,100.0);
    QVERIFY(a);
    QVERIFY(b);
    auto sa=make_stereo_ms_execution_signature(d.value()->get(),id,*a.value(),
        rgsml::audio::ChannelLayout::STEREO_LR,false);
    auto sb=make_stereo_ms_execution_signature(d.value()->get(),id,*b.value(),
        rgsml::audio::ChannelLayout::STEREO_LR,false);
    QVERIFY(sa);
    QVERIFY(sb);
    QVERIFY(*sa.value() == *sb.value());
    const auto* p=std::get_if<StereoMsExecutionSignaturePayload>(
        &sa.value()->payload);
    QVERIFY(p);
    QVERIFY(p->mono_bass_mode == std::optional<MonoBassMode>{MonoBassMode::OFF});
    QVERIFY(!p->mono_bass_cutoff_hz && !p->low_band_width_percent);
}

void StereoMsExecutionSignatureTest::bypassHasIdentityWithoutEffectiveSonicFields()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto d=registry.value()->find_descriptor("rgsml.dsp.stereo-ms");
    QVERIFY(d);
    const auto id=make_id("28500000-0000-0000-0000-000000000005");
    auto a=StereoMsParameters::create(-3.0,8.0,false,
                                    MonoBassMode::LR12,120.0,50.0);
    auto b=StereoMsParameters::create(12.0,-24.0,true,
                                    MonoBassMode::LR24,300.0,0.0);
    QVERIFY(a);
    QVERIFY(b);
    auto sa=make_stereo_ms_execution_signature(d.value()->get(),id,*a.value(),
        rgsml::audio::ChannelLayout::STEREO_LR,true);
    auto sb=make_stereo_ms_execution_signature(d.value()->get(),id,*b.value(),
        rgsml::audio::ChannelLayout::STEREO_LR,true);
    QVERIFY(sa);
    QVERIFY(sb);
    QVERIFY(*sa.value() == *sb.value());
    QCOMPARE(sa.value()->disposition,ModuleExecutionDisposition::BYPASS_IDENTITY);
    const auto* p=std::get_if<StereoMsExecutionSignaturePayload>(
        &sa.value()->payload);
    QVERIFY(p);
    QVERIFY(!p->mid_gain_db && !p->side_gain_db && !p->side_muted
            && !p->mono_bass_mode && !p->mono_bass_cutoff_hz
            && !p->low_band_width_percent);
}

void StereoMsExecutionSignatureTest::wrongDescriptorFailsClosed()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto d=registry.value()->find_descriptor("rgsml.dsp.gain");
    QVERIFY(d);
    auto p=StereoMsParameters::create_default();
    QVERIFY(p);
    const auto id=make_id("28500000-0000-0000-0000-000000000006");
    auto result=make_stereo_ms_execution_signature(d.value()->get(),id,*p.value(),
        rgsml::audio::ChannelLayout::STEREO_LR,false);
    QVERIFY(!result);
    QCOMPARE(result.error()->code(),rgsml::core::ErrorCode::InvalidArgument);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsExecutionSignatureTest)
#include "test_stereo_ms_execution_signature.moc"
