#include "dsp_chain_adapter_model.hpp"
#include "dsp_module_adapter.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "mastering_chain_state.hpp"

#include <rgsml/dsp/module_registry.hpp>

#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include <memory>

namespace rgsml::app::tests {

class DspModuleAdapterTest final : public QObject {
    Q_OBJECT

private slots:
    void testAdapterInventoryAndOrder();
    void testStableInstanceIds();
    void testConfigurationStateRules();
    void testBypassOrthogonality();
    void testHistoryCapabilities();
    void testAbBypassDispatch();
    void testResetSemantics();
    void testLiveChangePolicy();
    void testSelectionSwitchingPreservesStateAndHistory();
    void testNewSourceResetAndRefreshSynchronization();
    void testSelectionByStableModuleInstanceId();
    void testSelectedIndexIsProjectionOfSelectedIdentity();
    void testAdapterOrderMatchesMasteringChainState();
    void testProjectOpenRehydrationReconcilesSelectedInstanceId();
};

void DspModuleAdapterTest::testAdapterInventoryAndOrder()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainStateRes = MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId);
    QVERIFY(chainStateRes);
    auto chainState = std::move(*chainStateRes.value());

    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};

    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    const auto modules = chainModel.modules();
    QCOMPARE(modules.size(), 2);

    auto* gainAdapter = qobject_cast<DspModuleAdapter*>(modules[0].value<QObject*>());
    auto* eqAdapter = qobject_cast<DspModuleAdapter*>(modules[1].value<QObject*>());

    QVERIFY(gainAdapter != nullptr);
    QVERIFY(eqAdapter != nullptr);

    QCOMPARE(gainAdapter->type_id(), QStringLiteral("rgsml.dsp.gain"));
    QCOMPARE(gainAdapter->display_name(), QStringLiteral("Input Gain"));
    QCOMPARE(gainAdapter->editor_content_key(), QStringLiteral("INPUT_GAIN"));

    QCOMPARE(eqAdapter->type_id(), QStringLiteral("rgsml.dsp.parametric-eq"));
    QCOMPARE(eqAdapter->display_name(), QStringLiteral("Parametric EQ"));
    QCOMPARE(eqAdapter->editor_content_key(), QStringLiteral("PARAMETRIC_EQ"));
}

void DspModuleAdapterTest::testStableInstanceIds()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    const auto modules = chainModel.modules();
    auto* gainAdapter = qobject_cast<DspModuleAdapter*>(modules[0].value<QObject*>());
    auto* eqAdapter = qobject_cast<DspModuleAdapter*>(modules[1].value<QObject*>());

    QCOMPARE(gainAdapter->instance_id(), QString::fromStdString(chainState.gain_instance_id().to_string()));
    QCOMPARE(eqAdapter->instance_id(), QString::fromStdString(chainState.eq_instance_id().to_string()));
}

void DspModuleAdapterTest::testConfigurationStateRules()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    auto* gainAdapter = chainModel.active_module();
    chainModel.setSelectedIndex(1);
    auto* eqAdapter = chainModel.active_module();

    // Initial canonical default states
    QCOMPARE(gainAdapter->configuration_state(), QStringLiteral("Default"));
    QCOMPARE(eqAdapter->configuration_state(), QStringLiteral("Default"));

    // Modify gain -> Manual
    gainVM.setGainDb(2.5);
    QCOMPARE(gainAdapter->configuration_state(), QStringLiteral("Manual"));

    // Add EQ band -> Manual
    eqVM.addBand();
    QCOMPARE(eqAdapter->configuration_state(), QStringLiteral("Manual"));

    // Reset EQ -> Default
    eqVM.resetToFlat();
    QCOMPARE(eqAdapter->configuration_state(), QStringLiteral("Default"));

    // Reset Gain -> Default
    gainVM.resetToDefault();
    QCOMPARE(gainAdapter->configuration_state(), QStringLiteral("Default"));
}

void DspModuleAdapterTest::testBypassOrthogonality()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    auto* gainAdapter = chainModel.active_module();
    chainModel.setSelectedIndex(1);
    auto* eqAdapter = chainModel.active_module();

    // Default configuration + bypass does NOT convert configuration state to Manual
    gainAdapter->setBypass(true);
    QVERIFY(gainAdapter->bypass());
    QCOMPARE(gainAdapter->configuration_state(), QStringLiteral("Default"));

    eqAdapter->setBypass(true);
    QVERIFY(eqAdapter->bypass());
    QCOMPARE(eqAdapter->configuration_state(), QStringLiteral("Default"));
}

void DspModuleAdapterTest::testHistoryCapabilities()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    auto* gainAdapter = chainModel.active_module();
    chainModel.setSelectedIndex(1);
    auto* eqAdapter = chainModel.active_module();

    QVERIFY(!gainAdapter->history_supported());
    QVERIFY(!gainAdapter->can_undo());
    QVERIFY(!gainAdapter->can_redo());

    QVERIFY(eqAdapter->history_supported());
    QVERIFY(!eqAdapter->can_undo());
    QVERIFY(!eqAdapter->can_redo());

    eqVM.addBand();
    QVERIFY(eqAdapter->can_undo());
    QVERIFY(!eqAdapter->can_redo());

    eqAdapter->undo();
    QCOMPARE(eqVM.band_count(), 1);
    QVERIFY(!eqAdapter->can_undo());
    QVERIFY(eqAdapter->can_redo());

    eqAdapter->redo();
    QCOMPARE(eqVM.band_count(), 2);
    QVERIFY(eqAdapter->can_undo());
    QVERIFY(!eqAdapter->can_redo());
}

void DspModuleAdapterTest::testAbBypassDispatch()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    auto* gainAdapter = chainModel.active_module();
    chainModel.setSelectedIndex(1);
    auto* eqAdapter = chainModel.active_module();

    gainAdapter->setBypass(true);
    QVERIFY(gainVM.bypass());
    QVERIFY(!eqVM.bypass());

    eqAdapter->setBypass(true);
    QVERIFY(gainVM.bypass());
    QVERIFY(eqVM.bypass());

    gainAdapter->setBypass(false);
    QVERIFY(!gainVM.bypass());
    QVERIFY(eqVM.bypass());
}

void DspModuleAdapterTest::testResetSemantics()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    auto* gainAdapter = chainModel.active_module();
    chainModel.setSelectedIndex(1);
    auto* eqAdapter = chainModel.active_module();

    gainVM.setGainDb(-6.0);
    eqVM.addBand();

    gainAdapter->resetToDefault();
    QCOMPARE(gainVM.gain_db(), 0.0);
    QCOMPARE(eqVM.band_count(), 2);

    eqAdapter->resetToDefault();
    QCOMPARE(eqVM.band_count(), 1);
    QVERIFY(eqVM.is_default());
}

void DspModuleAdapterTest::testLiveChangePolicy()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    const auto modules = chainModel.modules();
    auto* gainAdapter = qobject_cast<DspModuleAdapter*>(modules[0].value<QObject*>());
    auto* eqAdapter = qobject_cast<DspModuleAdapter*>(modules[1].value<QObject*>());

    QCOMPARE(gainAdapter->live_change_policy(), QStringLiteral("PREPARED_REALIZATION_HOT_SWAP"));
    QCOMPARE(eqAdapter->live_change_policy(), QStringLiteral("PREPARED_REALIZATION_HOT_SWAP"));
}

void DspModuleAdapterTest::testSelectionSwitchingPreservesStateAndHistory()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    gainVM.setGainDb(1.5);
    eqVM.addBand();
    const quint64 genBeforeSwitch = eqVM.preview_generation();

    chainModel.setSelectedIndex(1);
    QCOMPARE(chainModel.selected_index(), 1);
    QCOMPARE(gainVM.gain_db(), 1.5);
    QCOMPARE(eqVM.band_count(), 2);
    QVERIFY(eqVM.can_undo());
    QCOMPARE(eqVM.preview_generation(), genBeforeSwitch);

    chainModel.setSelectedIndex(0);
    QCOMPARE(chainModel.selected_index(), 0);
    QCOMPARE(gainVM.gain_db(), 1.5);
    QCOMPARE(eqVM.band_count(), 2);
    QVERIFY(eqVM.can_undo());
    QCOMPARE(eqVM.preview_generation(), genBeforeSwitch);
}

void DspModuleAdapterTest::testNewSourceResetAndRefreshSynchronization()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    gainVM.setGainDb(3.0);
    eqVM.addBand();

    chainModel.resetForNewSource();

    QCOMPARE(gainVM.gain_db(), 0.0);
    QCOMPARE(eqVM.band_count(), 1);
    QVERIFY(eqVM.is_default());

    const auto modules = chainModel.modules();
    auto* gainAdapter = qobject_cast<DspModuleAdapter*>(modules[0].value<QObject*>());
    auto* eqAdapter = qobject_cast<DspModuleAdapter*>(modules[1].value<QObject*>());

    QCOMPARE(gainAdapter->configuration_state(), QStringLiteral("Default"));
    QCOMPARE(eqAdapter->configuration_state(), QStringLiteral("Default"));
}

void DspModuleAdapterTest::testSelectionByStableModuleInstanceId()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    const QString gainInstanceId = QString::fromStdString(gainId.to_string());
    const QString eqInstanceId = QString::fromStdString(eqId.to_string());

    // Selection defaults to gainInstanceId
    QCOMPARE(chainModel.selected_instance_id(), gainInstanceId);
    QCOMPARE(chainModel.active_module()->instance_id(), gainInstanceId);

    // Select EQ by stable instance ID
    chainModel.selectModuleByInstanceId(eqInstanceId);
    QCOMPARE(chainModel.selected_instance_id(), eqInstanceId);
    QCOMPARE(chainModel.active_module()->instance_id(), eqInstanceId);
}

void DspModuleAdapterTest::testSelectedIndexIsProjectionOfSelectedIdentity()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    const QString eqInstanceId = QString::fromStdString(eqId.to_string());

    QCOMPARE(chainModel.selected_index(), 0);

    chainModel.selectModuleByInstanceId(eqInstanceId);
    QCOMPARE(chainModel.selected_index(), 1);
}

void DspModuleAdapterTest::testAdapterOrderMatchesMasteringChainState()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    const auto chainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto gainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto eqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
    const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();

    auto chainState = std::move(*MasteringChainState::create_default(*registry.value(), chainUuid, gainId, eqId).value());
    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    const auto instances = chainState.instances();
    const auto modules = chainModel.modules();

    QCOMPARE(modules.size(), static_cast<qsizetype>(instances.size()));
    for (std::size_t idx = 0; idx < instances.size(); ++idx) {
        auto* adapter = qobject_cast<DspModuleAdapter*>(modules[static_cast<qsizetype>(idx)].value<QObject*>());
        QVERIFY(adapter != nullptr);
        QCOMPARE(adapter->instance_id(), QString::fromStdString(instances[idx].instance_id().to_string()));
        QCOMPARE(adapter->type_id(), QString::fromStdString(std::string{instances[idx].module_type_id()}));
    }
}

void DspModuleAdapterTest::testProjectOpenRehydrationReconcilesSelectedInstanceId()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    // Initial pre-open chain IDs
    const auto preChainUuid = *core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto preGainUuid = *core::Uuid::parse("22222222-2222-2222-2222-222222222222").value();
    const auto preEqUuid = *core::Uuid::parse("33333333-3333-3333-3333-333333333333").value();
    const auto preGainId = *dsp::ModuleInstanceId::from_uuid(preGainUuid).value();
    const auto preEqId = *dsp::ModuleInstanceId::from_uuid(preEqUuid).value();

    auto chainStateRes = MasteringChainState::create_default(*registry.value(), preChainUuid, preGainId, preEqId);
    QVERIFY(chainStateRes);
    auto chainState = std::move(*chainStateRes.value());

    GainViewModel gainVM{&chainState, nullptr};
    EqViewModel eqVM{&chainState, nullptr};
    DspChainAdapterModel chainModel{&gainVM, &eqVM, &chainState};

    // Select EQ in initial chain
    const QString preEqInstanceId = QString::fromStdString(preEqId.to_string());
    chainModel.selectModuleByInstanceId(preEqInstanceId);
    QCOMPARE(chainModel.selected_instance_id(), preEqInstanceId);
    QCOMPARE(chainModel.selected_index(), 1);

    // Rehydrate/replace chainState with new restored IDs (e.g. from Project Open)
    const auto restoredChainUuid = *core::Uuid::parse("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa").value();
    const auto restoredGainUuid = *core::Uuid::parse("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb").value();
    const auto restoredEqUuid = *core::Uuid::parse("cccccccc-cccc-cccc-cccc-cccccccccccc").value();
    const auto restoredGainId = *dsp::ModuleInstanceId::from_uuid(restoredGainUuid).value();
    const auto restoredEqId = *dsp::ModuleInstanceId::from_uuid(restoredEqUuid).value();

    auto restoredChainStateRes = MasteringChainState::create_default(*registry.value(), restoredChainUuid, restoredGainId, restoredEqId);
    QVERIFY(restoredChainStateRes);
    chainState = std::move(*restoredChainStateRes.value());

    // Refresh model from authority after Project Open
    chainModel.refreshFromAuthority();

    const QString restoredEqInstanceId = QString::fromStdString(restoredEqId.to_string());

    // Proves that selectedInstanceId_ reconciles to the restored EQ instance ID based on module type
    QCOMPARE(chainModel.selected_instance_id(), restoredEqInstanceId);
    QCOMPARE(chainModel.active_module()->instance_id(), restoredEqInstanceId);
    QCOMPARE(chainModel.selected_index(), 1);
    QCOMPARE(chainModel.active_module()->type_id(), QStringLiteral("rgsml.dsp.parametric-eq"));
}

}  // namespace rgsml::app::tests

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    rgsml::app::tests::DspModuleAdapterTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_dsp_module_adapter.moc"
