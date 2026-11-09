#!/usr/bin/env python3
"""M17→M19→M20→M21→MONITOR Product navigation source guard.

Nonrelease smoke. Does not certify numerical M19/M20/M21 Phase-6.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_visual_order_and_no_chain_mutation() -> None:
    c = read("app/src/dsp_chain_adapter_model.cpp")
    before = c.index("// Terminal instance is separate from the MANUAL")
    m17 = c.index("terminalLimiterState_ && terminalLimiterViewModel_", before)
    new = c.index("for(const int milestone:{19,20,21})", m17)
    after = c.index("if (terminalMonitorViewModel_)", new)
    assert m17 < new < after
    assert "ProductAnalysisModuleAdapter" in c
    assert "MasteringChainState::insert" not in c
    adapter = read("app/src/dsp_module_adapter.hpp")
    part = adapter[adapter.index("class ProductAnalysisModuleAdapter"):
                   adapter.index("class MonitorServiceModuleAdapter")]
    for term in ("non_processing_service() const noexcept override{return true;}",
                 "bypass_supported() const noexcept override{return false;}",
                 "void setBypass(bool) override {}",
                 "QStringLiteral(\"Not yet qualified\")"):
        assert term in part, term


def test_product_binds_status_without_demo() -> None:
    main = read("app/src/main.cpp")
    assert "bindAnalysisStatusViewModel(\n        &m21ProductStatusViewModel)" in main
    assert "m21ProductStatusViewModel" in read("ui/qml/Main.qml")
    editor = read("ui/qml/AnalysisStatusEditor.qml")
    assert "productionAuthorized === true" in editor
    assert "VERIFIED ANALYSIS UNAVAILABLE" in editor
    assert "selectPanelSource(" in editor
    assert "fixtureMode" not in editor
    # Windows A/B #23/#24/#25 demonstrated that even an inactive NEW
    # QQuickItem/Loader anywhere inside FQ99's DspEditorHost tree
    # makes the legacy Parametric EQ stop shrinking to dock allocation.
    # M16 remains the prior validated non-processing analysis service;
    # new M19-21 content must be hosted independently in Main dock slot.
    host = read("ui/qml/DspEditorHost.qml")
    main_qml = read("ui/qml/Main.qml")
    assert 'objectName: "m19M21ProductAnalysisLoader"' not in host
    assert 'objectName: "m19M21ProductAnalysisOverlay"' not in host
    assert 'AnalysisStatusEditor {' not in host
    assert 'objectName: "m19M21DockSlotAnalysisLoader"' in main_qml
    assert 'parent: root.activeDockSlot()' in main_qml
    assert 'active: analysisSelected && !root.editorDetached' in main_qml
    assert 'objectName: "m19M21ProductAnalysisEditor"' in main_qml
    assert main_qml.index('objectName: "m19M21DockSlotAnalysisLoader"') < main_qml.index(
        'id: floatingModuleWindow'
    )
    ui = read("ui/CMakeLists.txt")
    assert "qml/AnalysisStatusEditor.qml" in ui
    assert "qml/M19M21AnalysisWorkbenchPreview.qml" not in ui
    preview = read("ui/qml/M19M21AnalysisWorkbenchPreview.qml")
    assert "fixtureMode: true" in preview


def test_export_permission_unchanged() -> None:
    qml = read("ui/qml/Main.qml")
    # FQ99 Main status-bar chrome is frozen: export diagnostics belong to
    # evidence and ViewModels, not a new layout-consuming status label.
    assert "root.appM18ExportModel.canExport" in qml
    assert 'objectName: "m18ExportEligibilityDiagnostic"' not in qml
    binder = read("app/src/internal/m18_product_complete_binding.hpp")
    assert "can_offer_verified_master(" in binder
    assert "fanout.m17_verified_epoch_current()" in binder
    assert "verified==c.candidates.staged()" in binder
    assert "export_readiness_reason(" in binder


def test_original_source_open_guard_preserved() -> None:
    qml = read("ui/qml/Main.qml")
    # The unqualified Ctrl+O Shortcut would mutate an already qualified
    # Main.qml. Retain its existing guarded Open Source button instead.
    assert 'id: sourceOpenButton' in qml
    assert 'enabled: !root.monitorExclusive' in qml
    assert "onClicked: sourceDialog.open()" in qml
    assert 'objectName: "openSourceStandardShortcut"' not in qml


if __name__ == "__main__":
    for test in (
        test_visual_order_and_no_chain_mutation,
        test_product_binds_status_without_demo,
        test_export_permission_unchanged,
        test_original_source_open_guard_preserved,
    ):
        test()
    print("M19_M21_PRODUCT_NAVIGATION_SOURCE_GUARD_NONRELEASE_PASS")
