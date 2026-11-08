#!/usr/bin/env python3
"""Read-only provenance guard for isolated M18-M22 + REFERENCE UI review.
Does not authorize Product registration, CI/PO FQ, or any main integration.
"""
from pathlib import Path
import subprocess
import sys
import difflib

ROOT = Path(__file__).resolve().parents[3]
BASE = "5a5b9b7e4b01779832e32662d5c6b66b23087ce9"  # M22 CI#12 SUCCESS
UI = "b7a30ad2fb22a9d718a3e75adbf8d337bbccf6dd"  # REFERENCE CI#6 SUCCESS

IMPORTED = (
    "ui/qml/AnalysisModuleDockHostPreview.qml",
    "ui/qml/AuditionSourceSelector.qml",
    "ui/qml/M19LevelsPanelPreview.qml",
    "ui/qml/M19M21AnalysisWorkbenchPreview.qml",
    "ui/qml/M20TonalStereoPanelPreview.qml",
    "ui/qml/M21MetricsComparisonPreview.qml",
    "ui/qml/M21MetricsDeltaPanelPreview.qml",
    "ui/qml/Main.qml",
    "ui/qml/StudioReadOnlyMeasurement.qml",
    "app/src/audition_source_selector.cpp",
    "app/src/gold_selection_view_model.cpp",
    "app/src/playback_transport_view_model.cpp",
    "platform/windows/src/windows_resource_identity.cpp",
    "app/src/internal/m21_independent_panels_review_candidate.hpp",
    "app/src/internal/m21_qt_snapshot_review_candidate.hpp",
    "app/src/internal/m21_review_epoch_store_candidate.hpp",
    "app/src/internal/m21_ui_readonly_projection_candidate.hpp",
    "tests/ui_smoke/test_source_metadata_panel.cpp",
    "tests/unit/app/test_m21_qml_typed_convergence.cpp",
    "tests/unit/app/test_m21_ui_readonly_projection_candidate.cpp",
    "tools/qualification/m19_m21_gui/CMakeLists.txt",
    "tools/qualification/m19_m21_gui/analysis_workbench_screenshot.cpp",
    "tools/qualification/m21/qt_preview/m21_qml_preview_smoke.cpp",
    "tools/qualification/ui_reference_terminology/check_ui_reference_contract.py",
    "docs/architecture/M19_M21_INDEPENDENT_QT_REVIEW_FRAMES_20261108.md",
    "docs/architecture/M19_M21_INDEPENDENT_ROUTING_QT_REVIEW_20261108.md",
    "docs/architecture/M19_M21_PRIVATE_REVIEW_EPOCH_GATE_20261108.md",
    "docs/architecture/M19_M21_QML_TYPED_CONVERGENCE_ISOLATED_GATE_20261108.md",
    "docs/architecture/M19_M21_TYPED_UI_EVIDENCE_PROJECTION_20261108.md",
    ".github/workflows/m19-m21-analysis-gui-screenshots.yml",
    ".github/workflows/m19-m21-qml-typed-convergence.yml",
    ".github/workflows/m19-m21-typed-ui-projection.yml",
)
ADDED = {
    "tests/CMakeLists.txt",
    "tools/qualification/m18_m22_bridge/verify_bridge.py",
    ".github/workflows/m18-m22-reference-ui-bridge.yml",
    "docs/architecture/M18_M22_REFERENCE_GUI_ISOLATED_BRIDGE_20261108.md",
    "app/src/internal/m21_product_qt_readiness_bridge.hpp",
    "tests/integration/app/test_project_session.cpp",
}
PROTECTED = (
    "core/src/processing_state.cpp",
    "project/include/rgsml/project/project_document.hpp",
    "project/src/project_snapshot.cpp",
    "project/src/internal/canonical_json.cpp",
        "render/src/internal/m22_request_plan_candidate.hpp",
    "app/src/internal/m21_product_owner_analysis.hpp",
    "app/src/internal/m21_product_publication_inbox.hpp",
)

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)

def blob(ref, path):
    return git("show", ref + ":" + path)

def check(ok, msg):
    if not ok:
        raise RuntimeError(msg)

def main():
    check(subprocess.call(["git", "merge-base", "--is-ancestor", BASE, "HEAD"],
                          cwd=ROOT) == 0, "M22 frozen base is not an ancestor")
    paths = set(git("diff", "--name-only", BASE, "HEAD").decode().splitlines())
    permitted = set(IMPORTED) | ADDED
    check(paths <= permitted, "Unapproved paths in bridge: " + repr(sorted(paths - permitted)))
    check(paths == permitted, "Bridge scope incomplete: " + repr(sorted(permitted - paths)))
    for path in IMPORTED:
        check(blob("HEAD", path) == blob(UI, path),
              "Imported blob diverged from CI#6: " + path)
    for path in PROTECTED:
        check(blob("HEAD", path) == blob(BASE, path),
              "Protected canonical M22/Product/state changed: " + path)

    # The real M22 ProductSession and M21 worker tests may only gain
    # assertions; never drop/alter historical Product gates to go green.
    test_path = "tests/integration/app/test_project_session.cpp"
    baseline = blob(BASE, test_path).decode().splitlines(keepends=True)
    current = blob("HEAD", test_path).decode().splitlines(keepends=True)
    changes = difflib.SequenceMatcher(a=baseline, b=current, autojunk=False)
    check(all(tag in ("equal", "insert") for tag,*_ in changes.get_opcodes()),
          "Existing M22 ProductSession assertions deleted or altered")
    joined = "".join(current)
    for marker in (
        '#include "internal/m21_product_qt_readiness_bridge.hpp"',
        "using QtReadiness=app::internal::M21ProductQtReadinessBridge;",
        "privatePanels.current_product_owner",
        "MISSING",
        "panel.value(\"levels\").toMap().isEmpty()",
        "QtReadiness::still_current(reroutedPanels",
    ):
        check(marker in joined, "M21 real-owner Qt regression absent: "+marker)
    bridge = blob("HEAD",
        "app/src/internal/m21_product_qt_readiness_bridge.hpp").decode()
    for marker in ("M21ProductPublicationInbox",
                   "view_current(", "current_product_owner",
                   "product_ui_authorized{false}",
                   "masked_unpublished_envelope",
                   "routing.is_current_route(",
                   "privateQualityOnly",
                   "m21_private::ui_candidate::quality_label(",
                   "metric==3",
                   "s.lifecycle==Lifecycle::current",
                   "M18ProductExportIdentity owner_identity",
                   "now.owner_identity==expected.owner_identity",
                   "now.identities==expected.identities"):
        check(marker in bridge, "Private UI owner guard missing: "+marker)

    cmake = blob("HEAD", "tests/CMakeLists.txt")
    original = blob(BASE, "tests/CMakeLists.txt")
    begin = b"# L1-M19/M20/M21 preproduction read-only typed UI envelope."
    end = b"# L1-M20 read-only real AudioBuffer spectral/stereo analysis."
    check(cmake.count(begin) == 1 and cmake.count(end) == 1,
          "Isolated M19-21 CMake block missing or duplicated")
    start = cmake.index(begin)
    finish = cmake.index(end, start)
    check(cmake[:start] + cmake[finish:] == original,
          "M22 CMake changed outside isolated UI test registration")
    mainqml = blob("HEAD", "ui/qml/Main.qml").decode()
    for required in ('text: "Open Reference"', 'text: "Clear Reference"',
                     'text: "Reference / AutoMatch"', '"NO REFERENCE LOADED"',
                     'goldSelection.selectGold(selectedFile)', '"GOLD_AUTOMATCH"'):
        check(required in mainqml, "Public UI/internal compatibility failed: " + required)
    audition = blob("HEAD", "ui/qml/AuditionSourceSelector.qml").decode()
    check('text: "REFERENCE"' in audition and
          'auditionSelector.activeTarget === "GOLD"' in audition,
          "Audition public REFERENCE/internal GOLD mismatch")
    check(b'QML_FILES' in blob("HEAD", "ui/CMakeLists.txt"),
          "Product UI registration file unexpectedly absent")
    check(blob("HEAD", "ui/CMakeLists.txt") == blob(BASE, "ui/CMakeLists.txt"),
          "Production QML registration altered")
    print("RGS_M18_M22_REFERENCE_UI_ISOLATED_IMPORT_PROVENANCE_PASS")
    print("Not Product active; no FQ, no PO physical acceptance, no main merge")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print("BRIDGE_PROVENANCE_FAILURE:", error, file=sys.stderr)
        sys.exit(1)
