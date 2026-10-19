#include "mastering_chain_state.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_playback_frame_map.hpp"
#include "terminal_source_control_bridge.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include "playback_transport_view_model.hpp"
#include "../platform/fake_playback_service.hpp"
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include "terminal_limiter_view_model.hpp"
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

#include <rgsml/core/error.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <QtTest/QTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include <variant>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::app;
using namespace rgsml::dsp;
using rgsml::core::Uuid;

[[nodiscard]] Uuid test_uuid(const char* text)
{
    return *Uuid::parse(text).value();
}

[[nodiscard]] ModuleInstanceId test_instance_id(const char* text)
{
    return *ModuleInstanceId::from_uuid(test_uuid(text)).value();
}

class MasteringChainStateTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultTopologyAndCanonicalOrder();
    void productStartupHasFourModulesWithStereoMsInitiallyBypassed();
    void stableInstanceIdsDuringEdits();
    void rejectionOfNilOrDuplicateIdentities();
    void gainAndEqParameterUpdates();
    void bypassAndActiveState();
    void executionBindingsGeneration();
    void optInStereoMsStateAndBindings();
    void restoredFourNodeStereoMsPreservesTypedIdentityAndBypass();
    void terminalLimiterProductStateAndActualPcm();
    void limiterCeilingViewModelDraftAndValidation();
    void terminalOutputAdmissionRejectsObsoleteOwnersAndCeiling();
    void terminalOutputFrameMapPreservesSourceLoopAndSeekCoordinates();
    void terminalPrivateWorkerStagesOnlyCurrentOwnedOutput();
};

void MasteringChainStateTest::defaultTopologyAndCanonicalOrder()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto chain_id = test_uuid("10000000-0000-4000-8000-000000000001");
    const auto gain_id = test_instance_id("10000000-0000-4000-8000-000000000010");
    const auto eq_id = test_instance_id("10000000-0000-4000-8000-000000000020");
    const auto comp_id = test_instance_id("10000000-0000-4000-8000-000000000030");

    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id, comp_id);
    QVERIFY(state);

    QCOMPARE(state.value()->chain_id(), chain_id);
    QCOMPARE(state.value()->module_count(), std::size_t{3});
    const auto instances = state.value()->instances();
    QCOMPARE(instances.size(), std::size_t{3});

    // Index 0: Input Gain
    QCOMPARE(instances[0].module_type_id(), std::string_view("rgsml.dsp.gain"));
    QCOMPARE(instances[0].instance_id(), gain_id);

    // Index 1: Parametric EQ
    QCOMPARE(instances[1].module_type_id(), std::string_view("rgsml.dsp.parametric-eq"));
    QCOMPARE(instances[1].instance_id(), eq_id);

    // Index 2: Compressor
    QCOMPARE(instances[2].module_type_id(), std::string_view("rgsml.dsp.compressor"));
    QCOMPARE(instances[2].instance_id(), comp_id);

    // Verify newly materialized Compressor is initially user-bypassed
    auto comp_bypass = state.value()->is_bypassed(comp_id);
    QVERIFY(comp_bypass);
    QVERIFY(*comp_bypass.value());
}

void MasteringChainStateTest::productStartupHasFourModulesWithStereoMsInitiallyBypassed()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chainId = test_uuid("70000000-0000-4000-8000-000000000001");
    const auto gainId = test_instance_id("70000000-0000-4000-8000-000000000011");
    const auto eqId = test_instance_id("70000000-0000-4000-8000-000000000012");
    const auto compId = test_instance_id("70000000-0000-4000-8000-000000000014");
    const auto stereoId = test_instance_id("70000000-0000-4000-8000-000000000015");
    auto state = MasteringChainState::create_product_default(
        *registry.value(), chainId, gainId, eqId, compId, stereoId);
    QVERIFY(state);
    QCOMPARE(state.value()->module_count(), std::size_t{4});
    const auto instances = state.value()->instances();
    QCOMPARE(instances[0].module_type_id(), std::string_view("rgsml.dsp.gain"));
    QCOMPARE(instances[1].module_type_id(), std::string_view("rgsml.dsp.parametric-eq"));
    QCOMPARE(instances[2].module_type_id(), std::string_view("rgsml.dsp.compressor"));
    QCOMPARE(instances[3].module_type_id(), std::string_view("rgsml.dsp.stereo-ms"));
    QCOMPARE(instances[0].instance_id(), gainId);
    QCOMPARE(instances[1].instance_id(), eqId);
    QCOMPARE(instances[2].instance_id(), compId);
    QCOMPARE(instances[3].instance_id(), stereoId);
    QCOMPARE(state.value()->stereo_ms_instance_id(), std::optional{stereoId});
    const auto defaults = StereoMsParameters::create_default();
    QVERIFY(defaults);
    QCOMPARE(state.value()->stereo_ms_parameters(), std::optional{*defaults.value()});
    auto stereoBypass = state.value()->is_bypassed(stereoId);
    QVERIFY(stereoBypass);
    QVERIFY(*stereoBypass.value());
    QCOMPARE(state.value()->execution_bindings().size(), std::size_t{4});
    QCOMPARE(state.value()->execution_bindings()[3].instance_id, stereoId);
    QVERIFY(std::holds_alternative<StereoMsParameters>(
        state.value()->execution_bindings()[3].parameters));

    // Guard against accidentally converting old 3-node project factories
    // to four nodes or mutating historically saved chain identities.
    auto historical = MasteringChainState::create_default(
        *registry.value(), chainId, gainId, eqId, compId);
    QVERIFY(historical);
    QCOMPARE(historical.value()->module_count(), std::size_t{3});
    QVERIFY(!historical.value()->stereo_ms_instance_id());
    QVERIFY(!historical.value()->stereo_ms_parameters());

    // User may enable this real module later; startup bypass is not a
    // permanently disabled or decorative editor-only entry.
    QVERIFY(state.value()->set_user_bypass(stereoId, false));
    stereoBypass = state.value()->is_bypassed(stereoId);
    QVERIFY(stereoBypass);
    QVERIFY(!*stereoBypass.value());
}

void MasteringChainStateTest::stableInstanceIdsDuringEdits()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto chain_id = test_uuid("a0000000-0000-4000-8000-000000000001");
    const auto custom_gain_id = test_instance_id("a1111111-1111-4111-8111-111111111111");
    const auto custom_eq_id = test_instance_id("a2222222-2222-4222-8222-222222222222");
    const auto custom_comp_id = test_instance_id("a3333333-3333-4333-8333-333333333333");

    auto state = MasteringChainState::create_default(
        *registry.value(), chain_id, custom_gain_id, custom_eq_id, custom_comp_id);
    QVERIFY(state);

    QCOMPARE(state.value()->gain_instance_id(), custom_gain_id);
    QCOMPARE(state.value()->eq_instance_id(), custom_eq_id);
    QCOMPARE(state.value()->compressor_instance_id(), custom_comp_id);

    // Edit Gain parameters
    auto new_gain = GainParameters::create(-6.0);
    QVERIFY(new_gain);
    QVERIFY(state.value()->set_gain_parameters(*new_gain.value()));

    // Edit EQ parameters
    auto new_eq = ParametricEqParameters::create_legacy_default();
    QVERIFY(new_eq);
    QVERIFY(state.value()->set_parametric_eq_parameters(*new_eq.value()));

    // Edit Compressor parameters
    auto new_comp = CompressorParameters::create(CompressorDetectorMode::PEAK, CompressorChannelLink::LINKED_MAX, -18.0, 3.0);
    QVERIFY(new_comp);
    QVERIFY(state.value()->set_compressor_parameters(*new_comp.value()));

    // Verify IDs remain stable and non-nil
    QCOMPARE(state.value()->gain_instance_id(), custom_gain_id);
    QCOMPARE(state.value()->eq_instance_id(), custom_eq_id);
    QCOMPARE(state.value()->compressor_instance_id(), custom_comp_id);
}

void MasteringChainStateTest::rejectionOfNilOrDuplicateIdentities()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto valid_chain = test_uuid("b0000000-0000-4000-8000-000000000001");
    const auto valid_gain = test_instance_id("b1111111-1111-4111-8111-111111111111");
    const auto valid_eq = test_instance_id("b2222222-2222-4222-8222-222222222222");
    const auto valid_comp = test_instance_id("b3333333-3333-4333-8333-333333333333");

    // Nil chain ID rejected
    QVERIFY(!MasteringChainState::create_default(*registry.value(), Uuid{}, valid_gain, valid_eq, valid_comp));

    // Constructing StrongId with nil UUID is rejected
    auto nil_strong_id = ModuleInstanceId::from_uuid(Uuid{});
    QVERIFY(!nil_strong_id);
    QCOMPARE(nil_strong_id.error()->code(), rgsml::core::ErrorCode::InvalidUuid);

    // Duplicate instance IDs rejected
    auto rejected_dup = MasteringChainState::create_default(*registry.value(), valid_chain, valid_gain, valid_gain, valid_comp);
    QVERIFY(!rejected_dup);
    QCOMPARE(rejected_dup.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
}

void MasteringChainStateTest::gainAndEqParameterUpdates()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto chain_id = test_uuid("c0000000-0000-4000-8000-000000000001");
    const auto gain_id = test_instance_id("c1111111-1111-4111-8111-111111111111");
    const auto eq_id = test_instance_id("c2222222-2222-4222-8222-222222222222");
    const auto comp_id = test_instance_id("c3333333-3333-4333-8333-333333333333");

    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id, comp_id);
    QVERIFY(state);

    QCOMPARE(state.value()->gain_parameters().gain_db(), 0.0);

    auto updated_gain = GainParameters::create(4.5);
    QVERIFY(updated_gain);
    QVERIFY(state.value()->set_gain_parameters(*updated_gain.value()));
    QCOMPARE(state.value()->gain_parameters().gain_db(), 4.5);

    const auto id1 = test_uuid("00000000-0000-4000-8000-000000000001");
    auto band = EqBandParameters::create(
        id1, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{2000.0, -3.0, 1.0});
    QVERIFY(band);

    auto custom_eq = ParametricEqParameters::create({*band.value()});
    QVERIFY(custom_eq);

    QVERIFY(state.value()->set_parametric_eq_parameters(*custom_eq.value()));
    QCOMPARE(state.value()->parametric_eq_parameters(), *custom_eq.value());

    auto updated_comp = CompressorParameters::create(CompressorDetectorMode::PEAK, CompressorChannelLink::DUAL_MONO, -12.0, 4.0);
    QVERIFY(updated_comp);
    QVERIFY(state.value()->set_compressor_parameters(*updated_comp.value()));
    QCOMPARE(state.value()->compressor_parameters(), *updated_comp.value());
}

void MasteringChainStateTest::bypassAndActiveState()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto chain_id = test_uuid("d0000000-0000-4000-8000-000000000001");
    const auto gain_id = test_instance_id("d1111111-1111-4111-8111-111111111111");
    const auto eq_id = test_instance_id("d2222222-2222-4222-8222-222222222222");
    const auto comp_id = test_instance_id("d3333333-3333-4333-8333-333333333333");

    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id, comp_id);
    QVERIFY(state);

    // Initial state: Gain & EQ not bypassed, Compressor user-bypassed
    auto bypass_res = state.value()->is_bypassed(gain_id);
    QVERIFY(bypass_res);
    QVERIFY(!*bypass_res.value());

    bypass_res = state.value()->is_bypassed(comp_id);
    QVERIFY(bypass_res);
    QVERIFY(*bypass_res.value());

    auto gain_inst = state.value()->gain_instance();
    QVERIFY(gain_inst);
    QVERIFY((*gain_inst.value()).get().active());

    // Toggle Compressor bypass off
    QVERIFY(state.value()->set_user_bypass(comp_id, false));

    bypass_res = state.value()->is_bypassed(comp_id);
    QVERIFY(bypass_res);
    QVERIFY(!*bypass_res.value());

    auto comp_inst = state.value()->compressor_instance();
    QVERIFY(comp_inst);
    QVERIFY((*comp_inst.value()).get().active());
}

void MasteringChainStateTest::executionBindingsGeneration()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto chain_id = test_uuid("e0000000-0000-4000-8000-000000000001");
    const auto gain_id = test_instance_id("e1111111-1111-4111-8111-111111111111");
    const auto eq_id = test_instance_id("e2222222-2222-4222-8222-222222222222");
    const auto comp_id = test_instance_id("e3333333-3333-4333-8333-333333333333");

    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id, comp_id);
    QVERIFY(state);

    auto gain_params = GainParameters::create(-3.0);
    QVERIFY(gain_params);
    QVERIFY(state.value()->set_gain_parameters(*gain_params.value()));

    const auto bindings = state.value()->execution_bindings();
    QCOMPARE(bindings.size(), std::size_t{3});

    // Binding 0: Gain
    QCOMPARE(bindings[0].instance_id, gain_id);
    QVERIFY(std::holds_alternative<GainParameters>(bindings[0].parameters));
    QCOMPARE(std::get<GainParameters>(bindings[0].parameters).gain_db(), -3.0);

    // Binding 1: Parametric EQ
    QCOMPARE(bindings[1].instance_id, eq_id);
    QVERIFY(std::holds_alternative<ParametricEqParameters>(bindings[1].parameters));
    QCOMPARE(std::get<ParametricEqParameters>(bindings[1].parameters), state.value()->parametric_eq_parameters());

    // Binding 2: Compressor
    QCOMPARE(bindings[2].instance_id, comp_id);
    QVERIFY(std::holds_alternative<CompressorParameters>(bindings[2].parameters));
    QCOMPARE(std::get<CompressorParameters>(bindings[2].parameters), state.value()->compressor_parameters());
}

void MasteringChainStateTest::optInStereoMsStateAndBindings()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chain_id = test_uuid("f0000000-0000-4000-8000-000000000001");
    const auto gain_id = test_instance_id("f1111111-1111-4111-8111-111111111111");
    const auto eq_id = test_instance_id("f2222222-2222-4222-8222-222222222222");
    const auto comp_id = test_instance_id("f3333333-3333-4333-8333-333333333333");
    const auto ms_id = test_instance_id("f4444444-4444-4444-8444-444444444444");
    auto ms = StereoMsParameters::create(
        -3.0, 8.0, false, MonoBassMode::LR24, 250.0, 25.0);
    QVERIFY(ms);
    auto legacy = MasteringChainState::create_default(
        *registry.value(), chain_id, gain_id, eq_id, comp_id);
    QVERIFY(legacy);
    QCOMPARE(legacy.value()->module_count(), std::size_t{3});
    QVERIFY(!legacy.value()->stereo_ms_instance_id());
    QVERIFY(!legacy.value()->stereo_ms_parameters());
    QVERIFY(!legacy.value()->set_stereo_ms_parameters(*ms.value()));
    QCOMPARE(legacy.value()->execution_bindings().size(), std::size_t{3});
    for (const auto duplicate : {gain_id, eq_id, comp_id}) {
        auto bad = MasteringChainState::create_with_stereo_ms(
            *registry.value(), chain_id, gain_id, eq_id, comp_id,
            duplicate, *ms.value(), false);
        QVERIFY(!bad);
        QCOMPARE(bad.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
    }
    auto state = MasteringChainState::create_with_stereo_ms(
        *registry.value(), chain_id, gain_id, eq_id, comp_id,
        ms_id, *ms.value(), true);
    QVERIFY(state);
    QCOMPARE(state.value()->module_count(), std::size_t{4});
    QCOMPARE(state.value()->instances()[3].instance_id(), ms_id);
    QCOMPARE(state.value()->instances()[3].module_type_id(),
             std::string_view("rgsml.dsp.stereo-ms"));
    QVERIFY(state.value()->stereo_ms_instance());
    QCOMPARE(state.value()->stereo_ms_instance_id(), std::optional{ms_id});
    QCOMPARE(state.value()->stereo_ms_parameters(), std::optional{*ms.value()});
    auto bypass = state.value()->is_bypassed(ms_id);
    QVERIFY(bypass);
    QVERIFY(*bypass.value());
    const auto bindings = state.value()->execution_bindings();
    QCOMPARE(bindings.size(), std::size_t{4});
    QCOMPARE(bindings[3].instance_id, ms_id);
    QCOMPARE(std::get<StereoMsParameters>(bindings[3].parameters), *ms.value());
    auto changed = StereoMsParameters::create(
        6.0, -3.0, true, MonoBassMode::OFF, 120.0, 100.0);
    QVERIFY(changed);
    QVERIFY(state.value()->set_stereo_ms_parameters(*changed.value()));
    QCOMPARE(std::get<StereoMsParameters>(bindings[3].parameters), *ms.value());
    QCOMPARE(std::get<StereoMsParameters>(
        state.value()->execution_bindings()[3].parameters), *changed.value());
    QVERIFY(state.value()->set_user_bypass(ms_id, false));
    bypass = state.value()->is_bypassed(ms_id);
    QVERIFY(bypass);
    QVERIFY(!*bypass.value());
}

void MasteringChainStateTest::restoredFourNodeStereoMsPreservesTypedIdentityAndBypass()
{
    auto registryResult = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registryResult);
    auto registry = std::make_shared<const ModuleRegistry>(
        *registryResult.value());
    const auto chainId = test_uuid("a2000000-0000-4000-8000-000000000001");
    const auto gainId = test_instance_id("a2000000-0000-4000-8000-000000000010");
    const auto eqId = test_instance_id("a2000000-0000-4000-8000-000000000020");
    const auto compId = test_instance_id("a2000000-0000-4000-8000-000000000030");
    const auto msId = test_instance_id("a2000000-0000-4000-8000-000000000040");
    const auto wrongId = test_instance_id("a2000000-0000-4000-8000-000000000050");
    auto ms = StereoMsParameters::create(
        -2.5, 4.0, false, MonoBassMode::LR12, 160.0, 50.0);
    QVERIFY(ms);
    auto candidate = MasteringChainState::create_with_stereo_ms(
        *registry, chainId, gainId, eqId, compId, msId, *ms.value(), true);
    QVERIFY(candidate);
    auto preservedGain = GainParameters::create(-4.0);
    QVERIFY(preservedGain);
    QVERIFY(candidate.value()->set_gain_parameters(*preservedGain.value()));

    const auto restoreChain = [&]() {
        const auto items = candidate.value()->instances();
        return ProcessingChain::restore(
            *registry, {ProcessingStage::MASTER, ChainSegment::MANUAL},
            candidate.value()->chain().revision(),
            std::vector<ModuleInstance>(items.begin(), items.end()));
    };
    {
        auto chain = restoreChain();
        QVERIFY(chain);
        auto restored = MasteringChainState::restore(
            registry, chainId, std::move(*chain.value()),
            gainId, candidate.value()->gain_parameters(),
            eqId, candidate.value()->parametric_eq_parameters(),
            compId, candidate.value()->compressor_parameters(),
            msId, *ms.value());
        QVERIFY(restored);
        QCOMPARE(restored.value()->module_count(), std::size_t{4});
        QCOMPARE(restored.value()->chain().revision(),
                 candidate.value()->chain().revision());
        QCOMPARE(restored.value()->stereo_ms_instance_id(), std::optional{msId});
        QCOMPARE(restored.value()->stereo_ms_parameters(), std::optional{*ms.value()});
        QCOMPARE(restored.value()->gain_parameters(), *preservedGain.value());
        QCOMPARE(restored.value()->instances()[3].instance_id(), msId);
        QCOMPARE(restored.value()->instances()[3].module_type_id(),
                 std::string_view("rgsml.dsp.stereo-ms"));
        const auto msBypass = restored.value()->is_bypassed(msId);
        QVERIFY(msBypass);
        QVERIFY(*msBypass.value());
        const auto bindings = restored.value()->execution_bindings();
        QCOMPARE(bindings.size(), std::size_t{4});
        QCOMPARE(bindings[3].instance_id, msId);
        QCOMPARE(std::get<StereoMsParameters>(bindings[3].parameters), *ms.value());
    }
    // Fail closed on dropped carrier, mismatched ID, or a fourth node
    // accidentally supplied to the legacy three-module overload.
    {
        auto chain = restoreChain();
        QVERIFY(chain);
        QVERIFY(!MasteringChainState::restore(
            registry, chainId, std::move(*chain.value()),
            gainId, candidate.value()->gain_parameters(),
            eqId, candidate.value()->parametric_eq_parameters(),
            compId, candidate.value()->compressor_parameters()));
    }
    {
        auto chain = restoreChain();
        QVERIFY(chain);
        QVERIFY(!MasteringChainState::restore(
            registry, chainId, std::move(*chain.value()),
            gainId, candidate.value()->gain_parameters(),
            eqId, candidate.value()->parametric_eq_parameters(),
            compId, candidate.value()->compressor_parameters(),
            wrongId, *ms.value()));
    }
    {
        auto chain = restoreChain();
        QVERIFY(chain);
        QVERIFY(!MasteringChainState::restore(
            registry, chainId, std::move(*chain.value()),
            gainId, candidate.value()->gain_parameters(),
            eqId, candidate.value()->parametric_eq_parameters(),
            compId, candidate.value()->compressor_parameters(),
            msId, std::nullopt));
    }
}

void MasteringChainStateTest::terminalLimiterProductStateAndActualPcm()
{
    auto reg=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    const auto id=test_instance_id("7e000000-0000-4000-8000-000000000017");
    auto state=TerminalLimiterState::create_default(*reg.value(),id);
    QVERIFY(state);
    QCOMPARE(state.value()->chain().context().stage,ProcessingStage::MASTER);
    QCOMPARE(state.value()->chain().context().segment,ChainSegment::TERMINAL);
    QCOMPARE(state.value()->chain().instances().size(),std::size_t{1});
    QCOMPARE(state.value()->chain().instances()[0].module_type_id(),
        std::string_view{"rgsml.dsp.true-peak-limiter"});
    QCOMPARE(state.value()->instance_id(),id);
    QVERIFY(state.value()->bypassed());
    auto original=state.value()->parameters();
    for(double bad:{-12.01,0.01,std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::quiet_NaN()}) {
        QVERIFY(!state.value()->set_ceiling_db_tp(bad));
        QCOMPARE(state.value()->parameters(),original);
    }
    QVERIFY(state.value()->set_ceiling_db_tp(-12.0));
    auto bindings=state.value()->execution_bindings();
    QCOMPARE(bindings.size(),std::size_t{1});
    QCOMPARE(bindings[0].instance_id,id);
    QCOMPARE(std::get<TruePeakLimiterParameters>(bindings[0].parameters)
        .ceiling_db_tp(),-12.0);

    auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    auto fmt=rgsml::audio::AudioFormat::create(*rate.value(),
        rgsml::audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto count=core::FrameCount::create(301);
    auto block=core::FrameCount::create(17);
    QVERIFY(count && block);
    auto pcm=rgsml::audio::AudioBuffer::create(*fmt.value(),
        rgsml::audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{1000},*count.value());
    QVERIFY(pcm);
    for(std::size_t ch=0;ch<2;++ch) {
        auto plane=pcm.value()->mutable_view().channel(ch);
        QVERIFY(plane);
        for(std::size_t i=0;i<301;++i) {
            const double x=i==0?4.0:(i==300?-3.0:(i%37==0?2.0:0.0625));
            (*plane.value())[i]=ch==0?x:-0.75*x;
        }
    }
    auto byp=rgsml::render::render_true_peak_limiter_bound_terminal_candidate(
        pcm.value()->view(),state.value()->chain(),bindings,
        *reg.value(),*block.value());
    QVERIFY(byp);
    QVERIFY(byp.value()->checked.terminal.bypassed);
    QVERIFY(!byp.value()->checked.final_check.has_value());
    for(std::size_t ch=0;ch<2;++ch) {
        auto a=pcm.value()->view().channel(ch);
        auto b=byp.value()->checked.terminal.compensated_pcm.view().channel(ch);
        QVERIFY(a && b);
        for(std::size_t i=0;i<301;++i)
            QCOMPARE(std::bit_cast<std::uint64_t>((*a.value())[i]),
                     std::bit_cast<std::uint64_t>((*b.value())[i]));
    }
    QVERIFY(state.value()->set_user_bypass(false));
    auto active=rgsml::render::render_true_peak_limiter_bound_terminal_candidate(
        pcm.value()->view(),state.value()->chain(),
        state.value()->execution_bindings(),*reg.value(),*block.value());
    QVERIFY2(active,active?"":active.error()->message().c_str());
    QVERIFY(!active.value()->checked.terminal.bypassed);
    QVERIFY(active.value()->checked.final_check.has_value());
    QCOMPARE(active.value()->limiter_instance_id,id);
    QCOMPARE(active.value()->terminal_chain_revision,state.value()->chain().revision());
    QVERIFY(active.value()->checked.final_check->after_linear_peak
        <=std::pow(10.0,-12.0/20.0));
    QCOMPARE(active.value()->checked.terminal.applied_gain_frames.size(),std::size_t{301});
    QVERIFY(state.value()->set_ceiling_db_tp(-6.0));
    QCOMPARE(std::get<TruePeakLimiterParameters>(bindings[0].parameters)
        .ceiling_db_tp(),-12.0);
    auto updated=state.value()->execution_bindings();
    QCOMPARE(std::get<TruePeakLimiterParameters>(updated[0].parameters)
        .ceiling_db_tp(),-6.0);
    QCOMPARE(state.value()->instance_id(),id);
}



void MasteringChainStateTest::limiterCeilingViewModelDraftAndValidation()
{
    auto reg=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(reg);
    const auto id=test_instance_id("7a000000-0000-4000-8000-000000000017");
    auto product=TerminalLimiterState::create_default(*reg.value(),id);
    QVERIFY(product);
    int acceptedChanges=0;
    TerminalLimiterViewModel vm{product.value(),[&acceptedChanges]{
        ++acceptedChanges;
    }};
    QCOMPARE(vm.ceiling_db_tp(),-1.0);
    QCOMPARE(product.value()->parameter_revision(),std::uint64_t{0});
    QCOMPARE(vm.ceiling_db_tp_text(),QStringLiteral("-1.00"));
    QVERIFY(vm.bypass());
    QCOMPARE(acceptedChanges,0);
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-6.0));
    QCOMPARE(vm.draft_ceiling_db_tp(),-6.0);
    QCOMPARE(vm.ceiling_db_tp_text(),QStringLiteral("-6.00"));
    QCOMPARE(vm.ceiling_db_tp(),-1.0);
    QCOMPARE(product.value()->execution_bindings()[0].instance_id,id);
    QCOMPARE(std::get<TruePeakLimiterParameters>(
        product.value()->execution_bindings()[0].parameters).ceiling_db_tp(),-1.0);
    QCOMPARE(acceptedChanges,0);

    QVERIFY(!vm.setDraftFieldText(QStringLiteral("ceilingDbTp"),
        QStringLiteral("not a number")));
    QCOMPARE(vm.validation_field(),QStringLiteral("ceilingDbTp"));
    QVERIFY(!vm.validation_message().isEmpty());
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.ceiling_db_tp(),-1.0);
    QCOMPARE(acceptedChanges,0);
    QVERIFY(!vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-13.0));
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.ceiling_db_tp(),-1.0);
    vm.cancelDraft();
    QCOMPARE(vm.draft_ceiling_db_tp(),-1.0);
    QVERIFY(vm.validation_field().isEmpty());
    QVERIFY(vm.validation_message().isEmpty());

    QVERIFY(vm.setDraftFieldText(QStringLiteral("ceilingDbTp"),
        QStringLiteral(" -9.50 ")));
    QCOMPARE(vm.draft_ceiling_db_tp(),-9.5);
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.ceiling_db_tp(),-9.5);
    QCOMPARE(product.value()->parameters().ceiling_db_tp(),-9.5);
    QCOMPARE(product.value()->parameter_revision(),std::uint64_t{1});
    QCOMPARE(acceptedChanges,1);
    QVERIFY(vm.commitDraft());
    QCOMPARE(product.value()->parameter_revision(),std::uint64_t{1});
    QCOMPARE(acceptedChanges,1);
    vm.setBypass(false);
    QVERIFY(!vm.bypass());
    QCOMPARE(acceptedChanges,2);
    vm.setBypass(false);
    QCOMPARE(acceptedChanges,2);
    vm.setBypass(true);
    QVERIFY(vm.bypass());
    QCOMPARE(acceptedChanges,3);
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-2.0));
    vm.cancelDraft();
    QCOMPARE(vm.draft_ceiling_db_tp(),-9.5);
    QCOMPARE(vm.ceiling_db_tp(),-9.5);
    QCOMPARE(acceptedChanges,3);
    QVERIFY(!vm.setDraftFieldText(QStringLiteral("unrelated"),QStringLiteral("-6.0")));
    QCOMPARE(vm.ceiling_db_tp(),-9.5);
    QCOMPARE(acceptedChanges,3);
}



void MasteringChainStateTest::terminalOutputAdmissionRejectsObsoleteOwnersAndCeiling()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto manual=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto id=test_instance_id("7b000000-0000-4000-8000-000000000017");
    auto terminal=TerminalLimiterState::create_default(*registry.value(),id);
    QVERIFY(terminal);
    const auto sampleRate=*core::SampleRate::create(44100).value();
    const auto outputRate=*core::SampleRate::create(48000).value();
    auto fmt=audio::AudioFormat::create(
        sampleRate,audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto n=*core::FrameCount::create(441).value();
    auto block=*core::FrameCount::create(17).value();
    auto source=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},n);
    QVERIFY(source);
    for(std::size_t ch=0;ch<2;++ch) {
        auto data=source.value()->mutable_view().channel(ch);
        QVERIFY(data);
        for(std::size_t i=0;i<441;++i)
            (*data.value())[i]=i==0 ? (ch==0?4.0:-3.0)
                                    : (ch==0?0.25:-0.125);
    }
    auto request=render::RenderRequest::create(
        source.value()->view(),source.value()->view().absolute_range(),
        *manual.value(),{},block);
    QVERIFY(request);
    auto manualRender=render::render_preview(
        *request.value(),*registry.value());
    QVERIFY(manualRender);
    auto owner=std::make_shared<const render::RenderResult>(
        std::move(*manualRender.value()));
    const auto identity=core::RealizationId{77};
    TerminalCandidateGate gate;

    auto missing=gate.capture(owner,{},identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(!missing);
    QVERIFY(!gate.staged());

    auto job=gate.capture(owner,owner,identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(job);
    QCOMPARE(job.value()->terminal_parameter_revision,std::uint64_t{0});
    auto old=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),n,manual.value()->revision(),
        outputRate,terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),block);
    QVERIFY(old);
    const auto chainRev=terminal.value()->chain().revision();
    QVERIFY(terminal.value()->set_ceiling_db_tp(-6.0));
    QCOMPARE(terminal.value()->chain().revision(),chainRev);
    QCOMPARE(terminal.value()->parameter_revision(),std::uint64_t{1});
    // Even an A -> B -> A Ceiling edit must not resurrect old PCM:
    // exact parameter equality is NOT an adequate staleness certificate.
    QVERIFY(terminal.value()->set_ceiling_db_tp(-1.0));
    QVERIFY(terminal.value()->set_ceiling_db_tp(-6.0));
    QCOMPARE(terminal.value()->parameter_revision(),std::uint64_t{3});
    QVERIFY(!gate.accept(*job.value(),std::move(*old.value()),
        owner,owner,identity,*manual.value(),*terminal.value()));
    QVERIFY(!gate.staged());

    auto job2=gate.capture(owner,owner,identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(job2);
    auto swapped=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),n,manual.value()->revision(),
        outputRate,terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),block);
    QVERIFY(swapped);
    QVERIFY(!gate.accept(*job2.value(),std::move(*swapped.value()),
        owner,{},identity,*manual.value(),*terminal.value()));
    QVERIFY(!gate.staged());

    auto job3=gate.capture(owner,owner,identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(job3);
    auto produced=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),n,manual.value()->revision(),
        outputRate,terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),block);
    QVERIFY(produced);
    auto newId=core::RealizationId{78};
    QVERIFY(!gate.accept(*job3.value(),std::move(*produced.value()),
        owner,owner,newId,*manual.value(),*terminal.value()));
    QVERIFY(!gate.staged());

    auto job4=gate.capture(owner,owner,identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(job4);
    auto accepted=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),n,manual.value()->revision(),
        outputRate,terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),block);
    QVERIFY(accepted);
    QVERIFY(gate.accept(*job4.value(),std::move(*accepted.value()),
        owner,owner,identity,*manual.value(),*terminal.value()));
    auto snap=gate.staged();
    QVERIFY(snap);
    QCOMPARE(snap->original_manual_realization_id.value,std::uint64_t{77});
    QCOMPARE(snap->prepared_owner.get(),owner.get());
    QCOMPARE(snap->rendered.manual_owner.get(),owner.get());
    QVERIFY(snap->rendered.used_candidate_src);
    QCOMPARE(snap->rendered.terminal.checked.terminal
        .compensated_pcm.view().frame_count().value(),std::int64_t{480});
    QVERIFY(snap->rendered.terminal.checked.terminal.bypassed);
    QVERIFY(!snap->rendered.terminal.checked.final_check.has_value());
    QVERIFY(snap->rendered.terminal.checked.terminal.applied_gain_frames.empty());

    gate.invalidate();
    QVERIFY(!gate.staged());
    // A real ACTIVE completed terminal candidate must carry M16 final check
    // and one genuine OUTPUT_RATE gain witness per rendered frame.
    QVERIFY(terminal.value()->set_user_bypass(false));
    auto activeJob=gate.capture(owner,owner,identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(activeJob);
    auto activeOutput=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),n,manual.value()->revision(),
        outputRate,terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),block);
    QVERIFY(activeOutput);
    QVERIFY(gate.accept(*activeJob.value(),std::move(*activeOutput.value()),
        owner,owner,identity,*manual.value(),*terminal.value()));
    auto activeStaged=gate.staged();
    QVERIFY(activeStaged);
    QVERIFY(!activeStaged->rendered.terminal.checked.terminal.bypassed);
    QVERIFY(activeStaged->rendered.terminal.checked.final_check.has_value());
    QCOMPARE(activeStaged->rendered.terminal.checked.terminal
        .applied_gain_frames.size(),std::size_t{480});
    gate.invalidate();
    QVERIFY(!gate.staged());
    // Even an invalid replacement request must withdraw a previously
    // admitted terminal candidate. No old candidate can remain publishable.
    auto newCapture=gate.capture(owner,owner,identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(newCapture);
    auto candidateAgain=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),n,manual.value()->revision(),
        outputRate,terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),block);
    QVERIFY(candidateAgain);
    QVERIFY(gate.accept(*newCapture.value(),std::move(*candidateAgain.value()),
        owner,owner,identity,*manual.value(),*terminal.value()));
    QVERIFY(gate.staged());
    auto invalidReplacement=gate.capture(owner,{},identity,*manual.value(),
        *terminal.value(),outputRate);
    QVERIFY(!invalidReplacement);
    QVERIFY(!gate.staged());
    // The snapshot is still immutable and privately owned by its consumer;
    // invalidation does not make it an audible Processed realization.
    QCOMPARE(snap->rendered.manual_owner.get(),owner.get());
}



void MasteringChainStateTest::terminalOutputFrameMapPreservesSourceLoopAndSeekCoordinates()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto manual=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto limiterId=test_instance_id("7c000000-0000-4000-8000-000000000017");
    auto limiter=TerminalLimiterState::create_default(*registry.value(),limiterId);
    QVERIFY(limiter);
    const auto sourceRate=*core::SampleRate::create(44100).value();
    const auto outputRate=*core::SampleRate::create(48000).value();
    const auto n=*core::FrameCount::create(441).value();
    const auto block=*core::FrameCount::create(17).value();
    auto fmt=audio::AudioFormat::create(
        sourceRate,audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto pcm=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},n);
    QVERIFY(pcm);
    auto job=render::RenderRequest::create(
        pcm.value()->view(),pcm.value()->view().absolute_range(),
        *manual.value(),{},block);
    QVERIFY(job);
    auto manualRendered=render::render_preview(*job.value(),*registry.value());
    QVERIFY(manualRendered);
    auto owner=std::make_shared<const render::RenderResult>(
        std::move(*manualRendered.value()));
    TerminalCandidateGate gate;
    for(const auto rateHz:{48000,44100}) {
        const auto target=rateHz==48000?outputRate:sourceRate;
        auto capture=gate.capture(owner,owner,core::RealizationId{91},
            *manual.value(),*limiter.value(),target);
        QVERIFY(capture);
        auto completed=render::render_true_peak_limiter_owned_output_rate_candidate(
            owner,*manual.value(),n,manual.value()->revision(),target,
            limiter.value()->chain(),limiter.value()->execution_bindings(),
            *registry.value(),block);
        QVERIFY2(completed,completed?"":completed.error()->message().c_str());
        QVERIFY(gate.accept(*capture.value(),std::move(*completed.value()),
            owner,owner,core::RealizationId{91},
            *manual.value(),*limiter.value()));
        auto accepted=gate.staged();
        QVERIFY(accepted);
        auto map=TerminalPlaybackFrameMap::create(*accepted);
        QVERIFY(map);
        const bool src=rateHz==48000;
        QCOMPARE(map.value()->converted(),src);
        QCOMPARE(map.value()->source_frames().value(),std::int64_t{441});
        QCOMPARE(map.value()->output_frames().value(),
                 std::int64_t{src?480:441});
        for(const auto from: {0,5,10,15,20,147,300,441}) {
            auto mapped=map.value()->source_to_output(core::FrameIndex{from});
            QVERIFY(mapped);
            // The exact half-open-ceil map is frozen by the playback SRC
            // implementation. Check BEFORE / INSIDE / AFTER Loop Region.
            const std::int64_t expected=src?(from*160+146)/147:from;
            QCOMPARE(mapped.value()->value(),expected);
        }
        auto range=core::FrameRange::create(core::FrameIndex{10},
                                            core::FrameIndex{20});
        QVERIFY(range);
        auto mappedLoop=map.value()->source_loop_to_output(*range.value());
        QVERIFY(mappedLoop);
        QCOMPARE(mappedLoop.value()->begin().value(),src?11:10);
        QCOMPARE(mappedLoop.value()->end().value(),src?22:20);
        auto endLoop=core::FrameRange::create(core::FrameIndex{300},
                                              core::FrameIndex{441});
        QVERIFY(endLoop);
        auto mappedEnd=map.value()->source_loop_to_output(*endLoop.value());
        QVERIFY(mappedEnd);
        QCOMPARE(mappedEnd.value()->begin().value(),src?327:300);
        QCOMPARE(mappedEnd.value()->end().value(),src?480:441);
        auto sourceBefore=map.value()->output_to_source_display(core::FrameIndex{0});
        QVERIFY(sourceBefore);
        QCOMPARE(sourceBefore.value()->value(),std::int64_t{0});
        // Cursor inverse is display-only and must not be repurposed as
        // M16 live-sink consumption evidence.
        auto displayInside=map.value()->output_to_source_display(
            core::FrameIndex{src?160:147});
        QVERIFY(displayInside);
        QCOMPARE(displayInside.value()->value(),std::int64_t{147});
        auto displayAfterLoop=map.value()->output_to_source_display(
            core::FrameIndex{src?327:300});
        QVERIFY(displayAfterLoop);
        QCOMPARE(displayAfterLoop.value()->value(),std::int64_t{300});
        auto sourceAfter=map.value()->output_to_source_display(
            core::FrameIndex{src?480:441});
        QVERIFY(sourceAfter);
        QCOMPARE(sourceAfter.value()->value(),std::int64_t{441});
        auto badSource=map.value()->source_to_output(core::FrameIndex{-1});
        QVERIFY(!badSource);
        QVERIFY(!map.value()->source_to_output(core::FrameIndex{442}));
        QVERIFY(!map.value()->output_to_source_display(
            core::FrameIndex{src?481:442}));
        auto badLoop=core::FrameRange::create(core::FrameIndex{440},
                                             core::FrameIndex{442});
        QVERIFY(badLoop);
        QVERIFY(!map.value()->source_loop_to_output(*badLoop.value()));

        // Run the pure APP control plane against an exact OUTPUT_RATE fake
        // session. Product PROCESSED remains SOURCE-owned and unchanged.
        FakePlaybackService playback;
        playback.state=core::PlaybackState::STOPPED;
        playback.position=core::FrameIndex{0};
        playback.duration=map.value()->output_frames();
        playback.audibleRealization=core::AudibleRealizationState{
            core::AudibleHandoffPhase::NEW,core::RealizationId{9001}};
        QVERIFY(!TerminalSourceControlBridge::create(
            gate,playback,core::RealizationId{0}));
        QVERIFY(!TerminalSourceControlBridge::create(
            gate,playback,core::RealizationId{91})); // manual ID is not terminal
        QVERIFY(!TerminalSourceControlBridge::create(
            gate,playback,core::RealizationId{9002})); // wrong audible ID
        auto control=TerminalSourceControlBridge::create(
            gate,playback,core::RealizationId{9001});
        QVERIFY(control);
        QVERIFY(control.value()->set_source_loop(*range.value()));
        QCOMPARE(playback.loop,std::optional{*mappedLoop.value()});
        for(const auto sourceFrame:{0,5,11,17,20,30,300,441}) {
            QVERIFY(control.value()->seek_source(core::FrameIndex{sourceFrame}));
            auto mapped=map.value()->source_to_output(core::FrameIndex{sourceFrame});
            QVERIFY(mapped);
            QCOMPARE(playback.position,*mapped.value());
            auto snapshot=control.value()->source_snapshot();
            QVERIFY(snapshot);
            QCOMPARE(snapshot.value()->duration.value(),std::int64_t{441});
            QCOMPARE(snapshot.value()->loop,std::optional{*range.value()});
            // At SRC boundaries an inverse display cursor may be the
            // preceding SOURCE frame; never use it as exact TP-LIVE proof.
            const auto inverse=map.value()->output_to_source_display(playback.position);
            QVERIFY(inverse);
            QCOMPARE(snapshot.value()->position,*inverse.value());
        }
        auto invalidSource=playback.position;
        QVERIFY(!control.value()->seek_source(core::FrameIndex{442}));
        QCOMPARE(playback.position,invalidSource);
        const auto beforeLoop=playback.loop;
        QVERIFY(!control.value()->set_source_loop(*badLoop.value()));
        QCOMPARE(playback.loop,beforeLoop);
        // An output-session owner change must suppress every old command,
        // including a seek while PLAYING, without touching the new owner.
        playback.state=core::PlaybackState::PLAYING;
        playback.audibleRealization=core::AudibleRealizationState{
            core::AudibleHandoffPhase::OLD,core::RealizationId{9001}};
        QVERIFY(!control.value()->seek_source(core::FrameIndex{11}));
        QCOMPARE(playback.position,invalidSource);
        playback.audibleRealization=core::AudibleRealizationState{
            core::AudibleHandoffPhase::NEW,core::RealizationId{9002}};
        QVERIFY(!control.value()->set_source_loop(std::nullopt));
        QCOMPARE(playback.loop,beforeLoop);
        playback.audibleRealization=core::AudibleRealizationState{
            core::AudibleHandoffPhase::NEW,core::RealizationId{9001}};
        playback.state=core::PlaybackState::PAUSED;
        QVERIFY(control.value()->set_source_loop(std::nullopt));
        QVERIFY(!playback.loop);
        auto paused=control.value()->source_snapshot();
        QVERIFY(paused);
        QCOMPARE(paused.value()->state,core::PlaybackState::PAUSED);
        QVERIFY(control.value()->set_source_loop(*range.value()));
        // An unannounced external loop mutation must not be represented
        // as the approved SOURCE loop.
        playback.loop.reset();
        QVERIFY(!control.value()->source_snapshot());
        playback.loop=*mappedLoop.value();
        QVERIFY(control.value()->source_snapshot());
        gate.invalidate();
        QVERIFY(!control.value()->seek_source(core::FrameIndex{11}));
        QVERIFY(!control.value()->set_source_loop(std::nullopt));
        QVERIFY(!control.value()->source_snapshot());
        QCOMPARE(playback.loop,std::optional{*mappedLoop.value()});
        control.value()->detach();
        QVERIFY(!control.value()->seek_source(core::FrameIndex{5}));

        // The prior stale-owner test deliberately invalidated the gate.
        // A new private session MUST use a freshly captured and accepted
        // candidate; retaining the old snapshot must not resurrect it.
        if(src) {
            QVERIFY(!gate.staged());
            auto sessionCapture=gate.capture(owner,owner,
                core::RealizationId{91},*manual.value(),*limiter.value(),target);
            QVERIFY(sessionCapture);
            auto sessionOutput=
                render::render_true_peak_limiter_owned_output_rate_candidate(
                    owner,*manual.value(),n,manual.value()->revision(),target,
                    limiter.value()->chain(),limiter.value()->execution_bindings(),
                    *registry.value(),block);
            QVERIFY(sessionOutput);
            QVERIFY(gate.accept(*sessionCapture.value(),
                std::move(*sessionOutput.value()),owner,owner,
                core::RealizationId{91},*manual.value(),*limiter.value()));
            const auto sessionAccepted=gate.staged();
            QVERIFY(sessionAccepted);
            QVERIFY(sessionAccepted!=accepted);
            auto manualService=std::make_unique<FakePlaybackService>();
            auto& manualAudible=*manualService;
            PlaybackTransportViewModel terminalTransport{std::move(manualService)};
            manualAudible.state=core::PlaybackState::STOPPED;
            manualAudible.duration=n;
            manualAudible.audibleRealization=core::AudibleRealizationState{
                core::AudibleHandoffPhase::NEW,core::RealizationId{91}};
            int installations=0;
            std::shared_ptr<const void> observedLifetime;
            bool validOutputPcm=true;
            TerminalAudioSessionGate installer(
                gate,manualAudible,
                [&](audio::AudioBufferView pcmView,
                    std::shared_ptr<const void> lifetime,
                    core::RealizationId published) -> core::Status {
                    ++installations;
                    validOutputPcm = validOutputPcm
                        &&pcmView.timebase().frame_domain_id()
                            ==audio::FrameDomainId::OUTPUT_RATE
                        &&pcmView.frame_count().value()==480
                        &&pcmView.format().sample_rate().value()==48000;
                    observedLifetime=lifetime;
                    manualAudible.duration=pcmView.frame_count();
                    manualAudible.position=core::FrameIndex{0};
                    manualAudible.loop.reset();
                    manualAudible.audibleRealization=core::AudibleRealizationState{
                        core::AudibleHandoffPhase::NEW,published};
                    return core::Status::success();
                });
            QVERIFY(!installer.prepare_private_session(
                owner,owner,core::RealizationId{91},core::RealizationId{0}));
            QVERIFY(!installer.prepare_private_session(
                owner,owner,core::RealizationId{91},core::RealizationId{91}));
            QCOMPARE(installations,0);
            int issues=0;
            TerminalPrivatePublicationCoordinator privatePublisher{
                gate,installer,terminalTransport,
                [&](std::shared_ptr<const render::RenderResult> p,
                    std::shared_ptr<const render::RenderResult> m,
                    std::optional<core::RealizationId> id) {
                    ++issues;
                    if(p!=owner || m!=owner || id!=core::RealizationId{91})
                        return core::Result<core::RealizationId>::failure(
                            core::Error{core::ErrorCode::InvalidState,
                                "Wrong central identity issuer owner."});
                    return core::Result<core::RealizationId>::success(
                        core::RealizationId{9100});
                }};
            manualAudible.state=core::PlaybackState::PLAYING;
            QVERIFY(!privatePublisher.install_private(
                owner,owner,core::RealizationId{91},*range.value()));
            QCOMPARE(issues,0); // no ID may be consumed before preflight
            QCOMPARE(installations,0);
            manualAudible.state=core::PlaybackState::STOPPED;
            manualAudible.position=core::FrameIndex{147};
            auto published=privatePublisher.install_private(
                owner,owner,core::RealizationId{91},*range.value());
            QVERIFY(published);
            QCOMPARE(*published.value(),core::RealizationId{9100});
            QCOMPARE(issues,1);
            QCOMPARE(manualAudible.position.value(),std::int64_t{160});
            QCOMPARE(manualAudible.loop,std::optional{*mappedLoop.value()});
            QVERIFY(installer.has_session());
            // Refuse replacing an active terminal owner without retirement.
            QVERIFY(!installer.prepare_private_session(
                owner,owner,core::RealizationId{91},core::RealizationId{9500}));
            QVERIFY(installer.has_session());
            QCOMPARE(manualAudible.audibleRealization.realizationId,
                     std::optional{core::RealizationId{9100}});
            // A live terminal mapping cannot be unbound before retiring
            // the actual audible output; no SOURCE fallback is legal yet.
            QVERIFY(!terminalTransport.detach_private_terminal_source_controls(
                installer));
            QCOMPARE(installations,1);
            QVERIFY(validOutputPcm);
            QVERIFY(observedLifetime.get()
                ==static_cast<const void*>(sessionAccepted.get()));
            QVERIFY(installer.seek_source(core::FrameIndex{300}));
            QCOMPARE(manualAudible.position.value(),std::int64_t{327});
            QVERIFY(installer.set_source_loop(*range.value()));
            QCOMPARE(manualAudible.loop,std::optional{*mappedLoop.value()});
            auto installedSnapshot=installer.source_snapshot();
            QVERIFY(installedSnapshot);
            QCOMPARE(installedSnapshot.value()->position.value(),
                     std::int64_t{300});
            auto displayed=terminalTransport.source_display_snapshot();
            QVERIFY(displayed);
            QCOMPARE(displayed.value()->position.value(),std::int64_t{300});
            QCOMPARE(displayed.value()->duration.value(),std::int64_t{441});
            auto raw=terminalTransport.playback_snapshot();
            QVERIFY(raw);
            QCOMPARE(raw.value()->position.value(),std::int64_t{327});
            QCOMPARE(raw.value()->duration->value(),std::int64_t{480});
            QVERIFY(!terminalTransport.seek_target_frame(core::FrameIndex{20}));
            QVERIFY(terminalTransport.seek_source_frame(core::FrameIndex{20}));
            QCOMPARE(manualAudible.position.value(),std::int64_t{22});
            QVERIFY(terminalTransport.set_loop_source_range(*range.value()));
            QCOMPARE(manualAudible.loop,std::optional{*mappedLoop.value()});
            QVERIFY(installer.set_source_loop(std::nullopt));
            QVERIFY(!manualAudible.loop);
            // A different audible identity can take over: old transport
            // Pause/Stop/clear must never send commands to the new owner.
            manualAudible.state=core::PlaybackState::PLAYING;
            manualAudible.audibleRealization=core::AudibleRealizationState{
                core::AudibleHandoffPhase::NEW,core::RealizationId{9898}};
            const auto pausedBefore=manualAudible.pauseCalls;
            const auto stoppedBefore=manualAudible.stopCalls;
            const auto clearsBefore=manualAudible.clearCalls;
            terminalTransport.pause();
            terminalTransport.stop();
            QVERIFY(!terminalTransport.stop_and_clear());
            QCOMPARE(manualAudible.pauseCalls,pausedBefore);
            QCOMPARE(manualAudible.stopCalls,stoppedBefore);
            QCOMPARE(manualAudible.clearCalls,clearsBefore);
            QCOMPARE(manualAudible.audibleRealization.realizationId,
                     std::optional{core::RealizationId{9898}});
            // Revocation can abandon its own lease, NOT the alien PCM.
            QVERIFY(!privatePublisher.revoke_private());
            QVERIFY(!installer.has_session());
            QCOMPARE(manualAudible.audibleRealization.realizationId,
                     std::optional{core::RealizationId{9898}});
            QCOMPARE(manualAudible.clearCalls,clearsBefore);
            QVERIFY(!terminalTransport.detach_private_terminal_source_controls(
                installer));
            // Simulate exact owner returning to STOPPED, then a logical
            // invalidation. Unbinding is forbidden while PCM remains live.
            manualAudible.state=core::PlaybackState::STOPPED;
            manualAudible.audibleRealization=core::AudibleRealizationState{
                core::AudibleHandoffPhase::NEW,core::RealizationId{9100}};
            installer.detach();
            const auto lastOutputPosition=manualAudible.position;
            QVERIFY(!terminalTransport.seek_source_frame(core::FrameIndex{11}));
            QVERIFY(!terminalTransport.set_loop_source_range(*range.value()));
            QVERIFY(!terminalTransport.source_display_snapshot());
            QCOMPARE(manualAudible.position,lastOutputPosition);
            auto unchangedRaw=terminalTransport.playback_snapshot();
            QVERIFY(unchangedRaw);
            QCOMPARE(unchangedRaw.value()->position,lastOutputPosition);
            QVERIFY(!terminalTransport.stop_and_clear());
            QVERIFY(!terminalTransport.detach_private_terminal_source_controls(
                installer));
            QCOMPARE(manualAudible.position,lastOutputPosition);
            // Only after an independent exact terminal output retire
            // (NO_SOURCE + no identity) may normal manual transport resume.
            QVERIFY(manualAudible.clear());
            QVERIFY(terminalTransport.detach_private_terminal_source_controls(
                installer));
            QVERIFY(terminalTransport.detach_private_terminal_source_controls(
                installer));
            QVERIFY(!installer.has_session());
            QVERIFY(!installer.seek_source(core::FrameIndex{20}));

            // Fresh exact-owned terminal installation must be fully retired
            // by the coordinator (device CLEAR and expired SOURCE lease).
            manualAudible.state=core::PlaybackState::STOPPED;
            manualAudible.position=core::FrameIndex{147};
            manualAudible.duration=n;
            manualAudible.audibleRealization=core::AudibleRealizationState{
                core::AudibleHandoffPhase::NEW,core::RealizationId{91}};
            TerminalAudioSessionGate exactGate{
                gate,manualAudible,
                [&](audio::AudioBufferView output,std::shared_ptr<const void>,
                    core::RealizationId id) -> core::Status {
                    manualAudible.position=core::FrameIndex{0};
                    manualAudible.duration=output.frame_count();
                    manualAudible.state=core::PlaybackState::STOPPED;
                    manualAudible.loop.reset();
                    manualAudible.audibleRealization=core::AudibleRealizationState{
                        core::AudibleHandoffPhase::NEW,id};
                    return core::Status::success();
                }};
            TerminalPrivatePublicationCoordinator exactPublisher{
                gate,exactGate,terminalTransport,
                [&](std::shared_ptr<const render::RenderResult>,
                    std::shared_ptr<const render::RenderResult>,
                    std::optional<core::RealizationId>) {
                    return core::Result<core::RealizationId>::success(
                        core::RealizationId{9200});
                }};
            auto second=exactPublisher.install_private(
                owner,owner,core::RealizationId{91},*range.value());
            QVERIFY(second);
            QCOMPARE(manualAudible.position.value(),std::int64_t{160});
            QCOMPARE(manualAudible.loop,std::optional{*mappedLoop.value()});
            const auto clearsExact=manualAudible.clearCalls;
            QVERIFY(exactPublisher.revoke_private());
            QCOMPARE(manualAudible.clearCalls,clearsExact+1);
            QCOMPARE(manualAudible.state,core::PlaybackState::NO_SOURCE);
            QVERIFY(!manualAudible.audibleRealization.realizationId);
            QVERIFY(!exactGate.has_session());
            QVERIFY(exactPublisher.revoke_private());
            QVERIFY(!terminalTransport.source_display_snapshot());

            // Failure after a callback reports success MUST NOT clear a
            // different audible identity acquired by another publisher.
            manualAudible.state=core::PlaybackState::STOPPED;
            manualAudible.duration=n;
            manualAudible.audibleRealization=core::AudibleRealizationState{
                core::AudibleHandoffPhase::NEW,core::RealizationId{91}};
            TerminalAudioSessionGate wrongIdentity(
                gate,manualAudible,
                [&](audio::AudioBufferView, std::shared_ptr<const void>,
                    core::RealizationId) -> core::Status {
                    manualAudible.duration=*core::FrameCount::create(480).value();
                    manualAudible.audibleRealization=core::AudibleRealizationState{
                        core::AudibleHandoffPhase::NEW,core::RealizationId{9999}};
                    return core::Status::success();
                });
            const auto clears=manualAudible.clearCalls;
            QVERIFY(!wrongIdentity.prepare_private_session(
                owner,owner,core::RealizationId{91},core::RealizationId{9101}));
            QVERIFY(!wrongIdentity.has_session());
            QCOMPARE(manualAudible.clearCalls,clears);
            QCOMPARE(manualAudible.audibleRealization.realizationId,
                     std::optional{core::RealizationId{9999}});

            // A source/parameter mutation DURING the synchronous device
            // install removes its candidate and retires only our own PCM.
            manualAudible.state=core::PlaybackState::STOPPED;
            manualAudible.duration=n;
            manualAudible.audibleRealization=core::AudibleRealizationState{
                core::AudibleHandoffPhase::NEW,core::RealizationId{91}};
            TerminalAudioSessionGate changedMidInstall(
                gate,manualAudible,
                [&](audio::AudioBufferView pcmView, std::shared_ptr<const void>,
                    core::RealizationId newId) -> core::Status {
                    manualAudible.duration=pcmView.frame_count();
                    manualAudible.audibleRealization=core::AudibleRealizationState{
                        core::AudibleHandoffPhase::NEW,newId};
                    gate.invalidate();
                    return core::Status::success();
                });
            QVERIFY(!changedMidInstall.prepare_private_session(
                owner,owner,core::RealizationId{91},core::RealizationId{9102}));
            QVERIFY(!changedMidInstall.has_session());
            QCOMPARE(manualAudible.clearCalls,clears+1);
            QCOMPARE(manualAudible.state,core::PlaybackState::NO_SOURCE);
            QVERIFY(!gate.staged());
        }
        gate.invalidate();
        QVERIFY(!gate.staged());
    }
}


void MasteringChainStateTest::terminalPrivateWorkerStagesOnlyCurrentOwnedOutput()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto manual=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    auto limiter=TerminalLimiterState::create_default(
        *registry.value(),
        test_instance_id("7c000000-0000-4000-8000-000000000057"));
    QVERIFY(limiter);
    const auto sourceRate=*core::SampleRate::create(44100).value();
    const auto outputRate=*core::SampleRate::create(48000).value();
    auto fmt=audio::AudioFormat::create(
        sourceRate,audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    const auto frames=*core::FrameCount::create(441).value();
    auto buffer=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},frames);
    QVERIFY(buffer);
    auto request=render::RenderRequest::create(
        buffer.value()->view(),buffer.value()->view().absolute_range(),
        *manual.value(),{},*core::FrameCount::create(17).value());
    QVERIFY(request);
    auto rendered=render::render_preview(*request.value(),*registry.value());
    QVERIFY(rendered);
    auto owner=std::make_shared<const render::RenderResult>(
        std::move(*rendered.value()));
    std::shared_ptr<const render::RenderResult> active=owner;
    std::optional<core::RealizationId> activeId=core::RealizationId{57};
    // Qt queued worker completion requires a QCoreApplication event loop.
    // QTEST_APPLESS_MAIN silently strands the completion in RENDERING.
    QVERIFY(QCoreApplication::instance() != nullptr);
    TerminalCandidateGate gate;
    TerminalPrivateRenderController worker{
        gate,*limiter.value(),[&]{return active;},[&]{return active;},
        [&]{return activeId;},[&]{return *manual.value();}};
    QSignalSpy completions(&worker,&TerminalPrivateRenderController::changed);
    QVERIFY(completions.isValid());
    QVERIFY(worker.request_private(outputRate));
    QTRY_COMPARE(worker.status(),QStringLiteral("READY_PRIVATE"));
    auto first=gate.staged();
    QVERIFY(first);
    QCOMPARE(first->original_manual_realization_id,core::RealizationId{57});
    QCOMPARE(first->rendered.output_rate,outputRate);
    // invalidate + RENDERING + accepted READY_PRIVATE must be observable
    // on Qt's owner-thread event dispatcher, not just in worker memory.
    QVERIFY(completions.size() >= 3);
    QCOMPARE(first->rendered.terminal.checked.terminal.compensated_pcm
        .view().frame_count().value(),std::int64_t{480});
    QVERIFY(worker.request_private(sourceRate));
    QVERIFY(!gate.staged());
    QTRY_COMPARE(worker.status(),QStringLiteral("READY_PRIVATE"));
    QVERIFY(gate.staged());
    QCOMPARE(gate.staged()->rendered.output_rate,sourceRate);
    QCOMPARE(gate.staged()->rendered.terminal.checked.terminal
        .compensated_pcm.view().frame_count().value(),std::int64_t{441});
    QVERIFY(worker.request_private(outputRate));
    activeId=core::RealizationId{58};
    worker.invalidate();
    QVERIFY(!gate.staged());
    QTest::qWait(30);
    QVERIFY(!gate.staged());
    QVERIFY(worker.request_private(outputRate));
    QTRY_COMPARE(worker.status(),QStringLiteral("READY_PRIVATE"));
    QCOMPARE(gate.staged()->original_manual_realization_id,
             core::RealizationId{58});

    // Reversible A->B->A Ceiling edits may restore the same numeric target,
    // but must not resurrect a worker job captured at the older revision.
    QVERIFY(worker.request_private(outputRate));
    QVERIFY(limiter.value()->set_ceiling_db_tp(-6.0));
    QVERIFY(limiter.value()->set_ceiling_db_tp(-1.0));
    QTRY_COMPARE(worker.status(),QStringLiteral("STALE"));
    QVERIFY(!gate.staged());
}


}  // namespace
}  // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::MasteringChainStateTest)

#include "test_mastering_chain_state.moc"
