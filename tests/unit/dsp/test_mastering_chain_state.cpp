#include <rgsml/core/error.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/mastering_chain_state.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <QtTest/QTest>

#include <variant>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using rgsml::core::Uuid;

class MasteringChainStateTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultTopologyAndCanonicalOrder();
    void stableInstanceIdsDuringEdits();
    void gainParameterUpdates();
    void eqParameterUpdates();
    void bypassAndActiveState();
    void executionBindingsGeneration();
    void rejectionOfDuplicateInstanceIds();
};

void MasteringChainStateTest::defaultTopologyAndCanonicalOrder()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    auto state = MasteringChainState::create_default(*registry.value());
    QVERIFY(state);

    QCOMPARE(state.value()->module_count(), std::size_t{2});
    const auto instances = state.value()->instances();
    QCOMPARE(instances.size(), std::size_t{2});

    // Index 0: Input Gain
    QCOMPARE(instances[0].module_type_id(), std::string_view("rgsml.dsp.gain"));
    QCOMPARE(instances[0].instance_id(), state.value()->gain_instance_id());

    // Index 1: Parametric EQ
    QCOMPARE(instances[1].module_type_id(), std::string_view("rgsml.dsp.parametric-eq"));
    QCOMPARE(instances[1].instance_id(), state.value()->eq_instance_id());
}

void MasteringChainStateTest::stableInstanceIdsDuringEdits()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto custom_gain_id = *ModuleInstanceId::from_uuid(*Uuid::parse("a1111111-1111-4111-8111-111111111111").value()).value();
    const auto custom_eq_id = *ModuleInstanceId::from_uuid(*Uuid::parse("a2222222-2222-4222-8222-222222222222").value()).value();

    auto state = MasteringChainState::create_default(
        *registry.value(), Uuid{}, custom_gain_id, custom_eq_id);
    QVERIFY(state);

    QCOMPARE(state.value()->gain_instance_id(), custom_gain_id);
    QCOMPARE(state.value()->eq_instance_id(), custom_eq_id);

    // Edit Gain parameters
    auto new_gain = GainParameters::create(-6.0);
    QVERIFY(new_gain);
    QVERIFY(state.value()->set_gain_parameters(*new_gain.value()));

    // Edit EQ parameters
    auto new_eq = ParametricEqParameters::create_legacy_default();
    QVERIFY(new_eq);
    QVERIFY(state.value()->set_parametric_eq_parameters(*new_eq.value()));

    // Verify IDs remain stable and non-nil
    QCOMPARE(state.value()->gain_instance_id(), custom_gain_id);
    QCOMPARE(state.value()->eq_instance_id(), custom_eq_id);
}

void MasteringChainStateTest::gainParameterUpdates()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    auto state = MasteringChainState::create_default(*registry.value());
    QVERIFY(state);

    QCOMPARE(state.value()->gain_parameters().gain_db(), 0.0);

    auto updated_gain = GainParameters::create(4.5);
    QVERIFY(updated_gain);
    QVERIFY(state.value()->set_gain_parameters(*updated_gain.value()));

    QCOMPARE(state.value()->gain_parameters().gain_db(), 4.5);
}

void MasteringChainStateTest::eqParameterUpdates()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    auto state = MasteringChainState::create_default(*registry.value());
    QVERIFY(state);

    const auto id1 = *Uuid::parse("00000000-0000-4000-8000-000000000001").value();
    auto band = EqBandParameters::create(
        id1, true, EqFilterType::BELL, EqRouting::STEREO, BellPayload{2000.0, -3.0, 1.0});
    QVERIFY(band);

    auto custom_eq = ParametricEqParameters::create({*band.value()});
    QVERIFY(custom_eq);

    QVERIFY(state.value()->set_parametric_eq_parameters(*custom_eq.value()));
    QCOMPARE(state.value()->parametric_eq_parameters(), *custom_eq.value());
}

void MasteringChainStateTest::bypassAndActiveState()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    auto state = MasteringChainState::create_default(*registry.value());
    QVERIFY(state);

    const auto& gain_id = state.value()->gain_instance_id();

    // Initial state: not bypassed
    auto bypass_res = state.value()->is_bypassed(gain_id);
    QVERIFY(bypass_res);
    QVERIFY(!*bypass_res.value());

    auto gain_inst = state.value()->gain_instance();
    QVERIFY(gain_inst);
    QVERIFY((*gain_inst.value()).get().active());

    // Toggle bypass on
    QVERIFY(state.value()->set_user_bypass(gain_id, true));

    bypass_res = state.value()->is_bypassed(gain_id);
    QVERIFY(bypass_res);
    QVERIFY(*bypass_res.value());

    gain_inst = state.value()->gain_instance();
    QVERIFY(gain_inst);
    QVERIFY(!(*gain_inst.value()).get().active());
}

void MasteringChainStateTest::executionBindingsGeneration()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    auto state = MasteringChainState::create_default(*registry.value());
    QVERIFY(state);

    auto gain_params = GainParameters::create(-3.0);
    QVERIFY(gain_params);
    QVERIFY(state.value()->set_gain_parameters(*gain_params.value()));

    const auto bindings = state.value()->execution_bindings();
    QCOMPARE(bindings.size(), std::size_t{2});

    // Binding 0: Gain
    QCOMPARE(bindings[0].instance_id, state.value()->gain_instance_id());
    QVERIFY(std::holds_alternative<GainParameters>(bindings[0].parameters));
    QCOMPARE(std::get<GainParameters>(bindings[0].parameters).gain_db(), -3.0);

    // Binding 1: Parametric EQ
    QCOMPARE(bindings[1].instance_id, state.value()->eq_instance_id());
    QVERIFY(std::holds_alternative<ParametricEqParameters>(bindings[1].parameters));
    QCOMPARE(std::get<ParametricEqParameters>(bindings[1].parameters), state.value()->parametric_eq_parameters());
}

void MasteringChainStateTest::rejectionOfDuplicateInstanceIds()
{
    auto registry = ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto same_id = *ModuleInstanceId::from_uuid(*Uuid::parse("a1111111-1111-4111-8111-111111111111").value()).value();

    auto gain_params = GainParameters::create(0.0);
    auto eq_params = ParametricEqParameters::create_legacy_default();

    auto rejected = MasteringChainState::create(
        *registry.value(), Uuid{}, same_id, *gain_params.value(), false, same_id, *eq_params.value(), false);
    QVERIFY(!rejected);
    QCOMPARE(rejected.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::MasteringChainStateTest)

#include "test_mastering_chain_state.moc"
