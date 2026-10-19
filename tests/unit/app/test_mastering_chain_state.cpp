#include "mastering_chain_state.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_candidate_gate.hpp"
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
    // The snapshot is still immutable and privately owned by its consumer;
    // invalidation does not make it an audible Processed realization.
    QCOMPARE(snap->rendered.manual_owner.get(),owner.get());
}


}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::MasteringChainStateTest)

#include "test_mastering_chain_state.moc"
