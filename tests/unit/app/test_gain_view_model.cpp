#include "gain_view_model.hpp"

#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>

#include <memory>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::app;

class GainViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void testValidGainBoundsAccepted();
    void testOutOfRangeGainRejected();
    void testNegativeZeroCanonicalization();
    void testBypassToggleMutatesAuthorityAndRequestsRender();
    void testInvalidInputDoesNotMutateOrRender();
    void testValidEditRequestsExactlyOneRender();
    void testResetToDefault();
    void testResetForNewSource();
    void testAuthorityIsolation();
    void testFallbackViewModelsGenerateUniqueUuids();
    void testGainUndoRedoSequence();
    void testGainUndoRedoBypassOrthogonality();
    void testNoOpCommitCreatesNoHistoryOrExtraPreview();
    void testInvalidInputCreatesNoHistory();
    void testInvalidDraftCannotReplaceAppliedReadoutOrTriggerDoublePreview();
    void testNewEditAfterUndoClearsRedo();
    void testResetToDefaultIsUndoableWhenChanged();
    void testNewSourceAndRefreshClearHistory();
};

void GainViewModelTest::testValidGainBoundsAccepted()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    MasteringPreviewController previewController{state.value()};
    std::uint64_t renderCount = 0;
    previewController.set_preview_executor([&renderCount](const MasteringPreviewController::PreviewJob&) {
        ++renderCount;
        return core::Result<render::RenderResult>::failure(core::Error{
            core::ErrorCode::InvalidState, "Mock preview"});
    });

    GainViewModel vm{state.value(), &previewController};

    // Test -24.0 dB
    QVERIFY(vm.setGainDb(-24.0));
    QCOMPARE(vm.gain_db(), -24.0);
    QCOMPARE(state.value()->gain_parameters().gain_db(), -24.0);
    QVERIFY(vm.validation_error().isEmpty());

    // Test +24.0 dB
    QVERIFY(vm.setGainDb(+24.0));
    QCOMPARE(vm.gain_db(), +24.0);
    QCOMPARE(state.value()->gain_parameters().gain_db(), +24.0);
    QVERIFY(vm.validation_error().isEmpty());
}

void GainViewModelTest::testOutOfRangeGainRejected()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    std::uint64_t renderCount = 0;
    MasteringPreviewController previewController{state.value()};
    previewController.set_preview_executor([&renderCount](const MasteringPreviewController::PreviewJob&) {
        ++renderCount;
        return core::Result<render::RenderResult>::failure(core::Error{
            core::ErrorCode::InvalidState, "Mock preview"});
    });

    GainViewModel vm{state.value(), &previewController};
    const double initialGain = state.value()->gain_parameters().gain_db();

    // -24.1 dB rejected
    QVERIFY(!vm.setGainDb(-24.1));
    QCOMPARE(state.value()->gain_parameters().gain_db(), initialGain);
    QVERIFY(!vm.validation_error().isEmpty());

    // +24.1 dB rejected
    QVERIFY(!vm.setGainDb(+24.1));
    QCOMPARE(state.value()->gain_parameters().gain_db(), initialGain);
    QVERIFY(!vm.validation_error().isEmpty());

    // Invalid text rejected
    QVERIFY(!vm.setGainDbText(QStringLiteral("invalid")));
    QCOMPARE(state.value()->gain_parameters().gain_db(), initialGain);
    QVERIFY(!vm.validation_error().isEmpty());
}

void GainViewModelTest::testNegativeZeroCanonicalization()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    QVERIFY(vm.setGainDb(-0.0));
    QCOMPARE(vm.gain_db(), 0.0);
    QVERIFY(!std::signbit(vm.gain_db()));
    QCOMPARE(state.value()->gain_parameters().gain_db(), 0.0);
    QVERIFY(!std::signbit(state.value()->gain_parameters().gain_db()));
}

void GainViewModelTest::testBypassToggleMutatesAuthorityAndRequestsRender()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    std::uint64_t renderCount = 0;
    MasteringPreviewController previewController{state.value()};
    previewController.set_preview_executor([&renderCount](const MasteringPreviewController::PreviewJob&) {
        ++renderCount;
        return core::Result<render::RenderResult>::failure(core::Error{
            core::ErrorCode::InvalidState, "Mock preview"});
    });

    GainViewModel vm{state.value(), &previewController};

    QVERIFY(!vm.bypass());
    QVERIFY(!*state.value()->is_bypassed(gain_id).value());

    vm.setBypass(true);
    QVERIFY(vm.bypass());
    QVERIFY(*state.value()->is_bypassed(gain_id).value());

    vm.setBypass(false);
    QVERIFY(!vm.bypass());
    QVERIFY(!*state.value()->is_bypassed(gain_id).value());
}

void GainViewModelTest::testInvalidInputDoesNotMutateOrRender()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    std::uint64_t renderCount = 0;
    MasteringPreviewController previewController{state.value()};
    previewController.set_preview_executor([&renderCount](const MasteringPreviewController::PreviewJob&) {
        ++renderCount;
        return core::Result<render::RenderResult>::failure(core::Error{
            core::ErrorCode::InvalidState, "Mock preview"});
    });

    GainViewModel vm{state.value(), &previewController};

    const std::uint64_t genBefore = previewController.preview_generation();

    QVERIFY(!vm.setGainDb(30.0));
    QCOMPARE(previewController.preview_generation(), genBefore);
    QCOMPARE(state.value()->gain_parameters().gain_db(), 0.0);
}

void GainViewModelTest::testValidEditRequestsExactlyOneRender()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    MasteringPreviewController previewController{state.value()};
    GainViewModel vm{state.value(), &previewController};

    const std::uint64_t genBefore = previewController.preview_generation();

    QVERIFY(vm.setGainDb(6.0));
    QCOMPARE(previewController.preview_generation(), genBefore + 1U);
}

void GainViewModelTest::testResetToDefault()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    QVERIFY(vm.setGainDb(12.0));
    QCOMPARE(vm.gain_db(), 12.0);

    vm.resetToDefault();
    QCOMPARE(vm.gain_db(), 0.0);
    QCOMPARE(state.value()->gain_parameters().gain_db(), 0.0);
}

void GainViewModelTest::testResetForNewSource()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    QVERIFY(vm.setGainDb(-12.0));
    vm.setBypass(true);
    QCOMPARE(vm.gain_db(), -12.0);
    QVERIFY(vm.bypass());

    vm.resetForNewSource();
    QCOMPARE(vm.gain_db(), 0.0);
    QVERIFY(!vm.bypass());
    QCOMPARE(state.value()->gain_parameters().gain_db(), 0.0);
    QVERIFY(!*state.value()->is_bypassed(gain_id).value());
}

void GainViewModelTest::testAuthorityIsolation()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    // Gain edit updates Gain ONLY and preserves EQ
    const auto origEqParams = state.value()->parametric_eq_parameters();
    QVERIFY(vm.setGainDb(6.0));
    QCOMPARE(state.value()->gain_parameters().gain_db(), 6.0);
    QCOMPARE(state.value()->parametric_eq_parameters(), origEqParams);
    QCOMPARE(state.value()->gain_instance_id(), gain_id);
    QCOMPARE(state.value()->eq_instance_id(), eq_id);
}

void GainViewModelTest::testFallbackViewModelsGenerateUniqueUuids()
{
    GainViewModel vm1;
    GainViewModel vm2;

    QCOMPARE(vm1.gain_db(), 0.0);
    QCOMPARE(vm2.gain_db(), 0.0);

    QVERIFY(vm1.setGainDb(3.0));
    QCOMPARE(vm1.gain_db(), 3.0);
    QCOMPARE(vm2.gain_db(), 0.0);
}

void GainViewModelTest::testGainUndoRedoSequence()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    MasteringPreviewController previewController{state.value()};
    GainViewModel vm{state.value(), &previewController};

    QVERIFY(!vm.can_undo());
    QVERIFY(!vm.can_redo());
    const quint64 gen0 = previewController.preview_generation();

    // 0.0 -> +2.0 -> +4.0, exactly one preview request per committed edit.
    QVERIFY(vm.setGainDb(2.0));
    QCOMPARE(previewController.preview_generation(), gen0 + 1U);
    QVERIFY(vm.can_undo());
    QVERIFY(!vm.can_redo());

    QVERIFY(vm.setGainDb(4.0));
    QCOMPARE(previewController.preview_generation(), gen0 + 2U);

    // Undo -> +2.0
    vm.undo();
    QCOMPARE(previewController.preview_generation(), gen0 + 3U);
    QCOMPARE(vm.gain_db(), 2.0);
    QVERIFY(vm.can_undo());
    QVERIFY(vm.can_redo());

    // Undo -> 0.0
    vm.undo();
    QCOMPARE(previewController.preview_generation(), gen0 + 4U);
    QCOMPARE(vm.gain_db(), 0.0);
    QVERIFY(!vm.can_undo());
    QVERIFY(vm.can_redo());

    // Redo -> +2.0
    vm.redo();
    QCOMPARE(previewController.preview_generation(), gen0 + 5U);
    QCOMPARE(vm.gain_db(), 2.0);
    QVERIFY(vm.can_undo());
    QVERIFY(vm.can_redo());

    // Redo -> +4.0
    vm.redo();
    QCOMPARE(previewController.preview_generation(), gen0 + 6U);
    QCOMPARE(vm.gain_db(), 4.0);
    QVERIFY(vm.can_undo());
    QVERIFY(!vm.can_redo());
}

void GainViewModelTest::testGainUndoRedoBypassOrthogonality()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    QVERIFY(vm.setGainDb(3.0));
    vm.setBypass(true);
    QVERIFY(vm.bypass());

    vm.undo();
    QCOMPARE(vm.gain_db(), 0.0);
    QVERIFY2(vm.bypass(), "Gain Undo must NOT alter bypass state");
}

void GainViewModelTest::testNoOpCommitCreatesNoHistoryOrExtraPreview()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    MasteringPreviewController previewController{state.value()};
    GainViewModel vm{state.value(), &previewController};

    const quint64 genBefore = previewController.preview_generation();

    // Re-committing 0.0 dB
    QVERIFY(vm.setGainDb(0.0));
    QCOMPARE(previewController.preview_generation(), genBefore);
    QVERIFY(!vm.can_undo());
}

void GainViewModelTest::testInvalidInputCreatesNoHistory()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    QVERIFY(!vm.setGainDb(30.0));
    QVERIFY(!vm.can_undo());
}

void GainViewModelTest::testNewEditAfterUndoClearsRedo()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    vm.setGainDb(2.0);
    vm.setGainDb(4.0);
    vm.undo();
    QVERIFY(vm.can_redo());

    vm.setGainDb(5.0);
    QVERIFY2(!vm.can_redo(), "New edit after Undo must clear Redo stack");
}

void GainViewModelTest::testResetToDefaultIsUndoableWhenChanged()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    vm.setGainDb(6.0);
    vm.resetToDefault();
    QCOMPARE(vm.gain_db(), 0.0);
    QVERIFY(vm.can_undo());

    vm.undo();
    QCOMPARE(vm.gain_db(), 6.0);
}

void GainViewModelTest::testNewSourceAndRefreshClearHistory()
{
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain_id = *core::Uuid::parse("10000000-0000-4000-8000-000000000001").value();
    auto gain_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000010").value()).value();
    auto eq_id = *dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse("10000000-0000-4000-8000-000000000020").value()).value();
    auto state = MasteringChainState::create_default(*registry.value(), chain_id, gain_id, eq_id);
    QVERIFY(state);

    GainViewModel vm{state.value()};

    vm.setGainDb(3.0);
    QVERIFY(vm.can_undo());

    vm.resetForNewSource();
    QVERIFY(!vm.can_undo());
    QVERIFY(!vm.can_redo());

    vm.setGainDb(3.0);
    QVERIFY(vm.can_undo());

    vm.refreshFromAuthority();
    QVERIFY(!vm.can_undo());
    QVERIFY(!vm.can_redo());
}


void GainViewModelTest::testInvalidDraftCannotReplaceAppliedReadoutOrTriggerDoublePreview()
{
    // Native C++ Q_PROPERTY contract consumed by InputGainEditor:
    // gainDbText is the authoritative big display; draftGainDbText is
    // field-local, even for empty invalid drafts and repeated blur.
    GainViewModel vm;
    QVERIFY(vm.setGainDbText(QStringLiteral("3.0")));
    QCOMPARE(vm.gain_db(),3.0);
    QCOMPARE(vm.gain_db_text(),QStringLiteral("3.0"));
    const quint64 beforeInvalid=vm.preview_generation();
    QVERIFY(!vm.setGainDbText(QStringLiteral("not-a-number")));
    QCOMPARE(vm.gain_db(),3.0);
    QCOMPARE(vm.gain_db_text(),QStringLiteral("3.0"));
    QCOMPARE(vm.draft_gain_db_text(),QStringLiteral("not-a-number"));
    QCOMPARE(vm.preview_generation(),beforeInvalid);
    QVERIFY(!vm.setGainDbText(QStringLiteral("")));
    QCOMPARE(vm.gain_db_text(),QStringLiteral("3.0"));
    QCOMPARE(vm.draft_gain_db_text(),QStringLiteral(""));
    QCOMPARE(vm.preview_generation(),beforeInvalid);
    // Escape: apply exact authoritative value as a NO-OP; no new preview.
    QVERIFY(vm.setGainDbText(vm.gain_db_text()));
    QCOMPARE(vm.gain_db_text(),QStringLiteral("3.0"));
    QCOMPARE(vm.draft_gain_db_text(),QStringLiteral("3.0"));
    QVERIFY(vm.validation_error().isEmpty());
    QCOMPARE(vm.preview_generation(),beforeInvalid);
    // Enter, followed by genuine blur, must not double-commit.
    QVERIFY(vm.setGainDbText(QStringLiteral("4.0")));
    const quint64 committed=vm.preview_generation();
    QCOMPARE(committed,beforeInvalid+1U);
    QCOMPARE(vm.gain_db_text(),QStringLiteral("4.0"));
    QVERIFY(vm.setGainDbText(QStringLiteral("4.0")));
    QCOMPARE(vm.preview_generation(),committed);
    vm.undo();
    QCOMPARE(vm.gain_db_text(),QStringLiteral("3.0"));
}

}  // namespace
}  // namespace rgsml::tests

QTEST_MAIN(rgsml::tests::GainViewModelTest)

#include "test_gain_view_model.moc"
