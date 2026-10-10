#include "mastering_chain_state.hpp"

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
    void stableInstanceIdsDuringEdits();
    void rejectionOfNilOrDuplicateIdentities();
    void gainAndEqParameterUpdates();
    void bypassAndActiveState();
    void executionBindingsGeneration();
    void optInStereoMsStateAndBindings();
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

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::MasteringChainStateTest)

#include "test_mastering_chain_state.moc"
