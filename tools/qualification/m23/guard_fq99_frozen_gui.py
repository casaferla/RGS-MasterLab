#!/usr/bin/env python3
"""M23 nonrelease guard: keep every pre-existing FQ99 GUI byte-for-byte.

New M19-M21 UI is permitted only as the exact reviewed additive wiring.
The guard compares the real Git blobs against the FQ99 source commit,
not screenshots, render-dependent dimensions, or test-specific exceptions.
A new edit to a qualified GUI must be separately approved by the PO and
qualified; never silently extend the allowlist to make the CI green.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

FQ99 = "5e4871d5849e4121b0612c2142d1e051d6c9a3d2"

SHARED = {
    "ui/qml/DspChainSelector.qml",
    "ui/qml/DspEditorHost.qml",
    "ui/qml/DspWorkspace.qml",
    "ui/qml/Main.qml",
}
# Two PO-approved analysis-specific QML entries and the sole approved
# Input Gain-hero layout adaptation are additive to the FQ99 GUI.
NEW = {
    "ui/qml/AnalysisStatusEditor.qml",
    "ui/qml/AnalysisApprovedContent.qml",
    "ui/qml/StudioReadOnlyMeasurement.qml",
    "ui/qml/AnalysisModuleIcon.qml",
}
ALLOWED = SHARED | NEW
# L1-M24 PO 2026-11-14 integration authorizes the already verified, exact
# P2 I2 opt-in GUI tree as an additive future qualification candidate.
# Unlike the older strict FQ99 delta, this path pins ALL touched QML blobs
# byte-for-byte to the independently CI-green P2 research Product HEAD.
# It does not call this GUI FQ99-byte-identical and grants no FQ or release.
P2_INTEGRATION_SOURCE = "bd148a88a882dba10ff09f0190d57faa61a7e980"
P2_ADDITIONAL = {
    "ui/qml/StreamingTruePeakLimiterEditor.qml",
    "ui/qml/StudioIcon.qml",
    "ui/qml/StudioParameterSlider.qml",
    "ui/qml/TruePeakMeterEditor.qml",
    "ui/qml/TruePeakSharedRail.qml",
}
P2_EXPECTED_GUI_DELTA = ALLOWED | P2_ADDITIONAL



def git(*args: str) -> bytes:
    p = subprocess.run(
        ["git", *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=False,
    )
    if p.returncode:
        raise AssertionError(
            "GIT_READ_FAILED " + " ".join(args) + ": "
            + p.stderr.decode("utf-8", "replace")[:200]
        )
    return p.stdout


def blob(rev: str, path: str) -> str:
    return git("show", rev + ":" + path).decode("utf-8")


def remove_exact(text: str, original: str, replacement: str = "",
                 count: int = 1) -> str:
    actual = text.count(original)
    if actual != count:
        raise AssertionError(
            "FROZEN_GUI_ADDENDUM_MISMATCH expected="
            + str(count) + " actual=" + str(actual)
            + " token=" + repr(original[:56])
        )
    return text.replace(original, replacement)


def normalize(path: str, text: str) -> str:
    if path == "ui/qml/DspChainSelector.qml":
        # PO GO 2026-11-10: precise text-only correction for M16-parity
        # ALWAYS-ON service accessibility. Identical row dim, icon glow,
        # silhouettes, geometry, input events and module ownership.
        # Restore exact frozen wording before comparing FQ99 bytes.
        text = remove_exact(text, '''                            // M16/M19/M20/M21 are permanent observers.
                            // A read-only service must NEVER claim Manual
                            // settings just because it has no Default state.
                            Accessible.name: rowItem.moduleAdapter
                                ? rowItem.moduleAdapter.displayName + " · "
                                    + (rowItem.permanentlyLit
                                        ? "Always on · Read-only"
                                        : rowItem.isDefaultState ? "Default" : "Manual")
                                : "Module icon"
                            ToolTip.text: rowItem.moduleAdapter
                                ? rowItem.moduleAdapter.displayName + " · "
                                    + (rowItem.permanentlyLit
                                        ? "Always on · Read-only analysis / service"
                                        : rowItem.isDefaultState
                                            ? "Default settings" : "Manual non-default settings")
                                    + (rowItem.isBypassed ? " · Bypassed" : "")
                                : "Module"
''', '''                            Accessible.name: rowItem.moduleAdapter
                                ? rowItem.moduleAdapter.displayName + " · "
                                    + (rowItem.isDefaultState ? "Default" : "Manual")
                                : "Module icon"
                            ToolTip.text: rowItem.moduleAdapter
                                ? rowItem.moduleAdapter.displayName + " · "
                                    + (rowItem.isDefaultState
                                        ? "Default settings" : "Manual non-default settings")
                                    + (rowItem.isBypassed ? " · Bypassed" : "")
                                : "Module"
''')
        # Exact PO-authorized additional vector CHILD. The original
        # StudioIcon Canvas, geometry, icon-kind routing and dim/glow
        # formulas are byte-identical to the previously qualified GUI.
        text = remove_exact(text, '''                            // Three approved analysis glyphs share the
                            // EXISTING 16px StudioIcon slot, tooltip, focus,
                            // brightness and bypass semantics. Canvas has no
                            // paths for these new kinds; a vector Shape child
                            // supplies the crisp silhouette without moving
                            // the qualified legacy icon geometry.
                            AnalysisModuleIcon {
                                id: analysisVectorIcon
                                objectName: "dspChainAnalysisVectorIcon_" + rowItem.modelIndex
                                anchors.fill: parent
                                kind: moduleSymbol.kind
                                strokeColor: moduleSymbol.strokeColor
                                glowEnabled: moduleSymbol.glowEnabled
                                visible: kind === "analysis-levels"
                                    || kind === "analysis-spectrum"
                                    || kind === "analysis-delta"
                            }
''')
        # Recover the preexisting M19-M21 additive wiring first.
        return remove_exact(text, '''        case "M19_ANALYSIS": return "analysis-levels"
        case "M20_ANALYSIS": return "analysis-spectrum"
        case "M21_ANALYSIS": return "analysis-delta"
''')
    if path == "ui/qml/DspEditorHost.qml":
        text = remove_exact(text, "    property var analysisStatusViewModel: null\n")
        text = remove_exact(
            text,
            '''            || root.activeModule.editorContentKey === "MASTER_MONITOR"
            || root.activeModule.editorContentKey === "M19_ANALYSIS"
            || root.activeModule.editorContentKey === "M20_ANALYSIS"
            || root.activeModule.editorContentKey === "M21_ANALYSIS")''',
            '''            || root.activeModule.editorContentKey === "MASTER_MONITOR")''',
        )
        return remove_exact(text, '''        // M19–M21: three persistent read-only pages, like the qualified
        // M16 TruePeakMeterEditor. The existing ColumnLayout is the only
        // geometry owner: hidden pages are excluded from its allocation.
        // Pages belong to the host throughout its dock/floating lifetime,
        // preserving view state across navigation without create/destroy.
        AnalysisStatusEditor {
            id: m19LevelAnalysisEditor
            objectName: "m19LevelProductAnalysisEditor"
            panelIndex: 0
            statusViewModel: root.analysisStatusViewModel
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "M19_ANALYSIS" : false
            enabled: visible
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
        }

        AnalysisStatusEditor {
            id: m20TonalStereoAnalysisEditor
            objectName: "m20TonalStereoProductAnalysisEditor"
            panelIndex: 1
            statusViewModel: root.analysisStatusViewModel
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "M20_ANALYSIS" : false
            enabled: visible
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
        }

        AnalysisStatusEditor {
            id: m21MetricsDeltaAnalysisEditor
            objectName: "m21MetricsDeltaProductAnalysisEditor"
            panelIndex: 2
            statusViewModel: root.analysisStatusViewModel
            visible: root.activeModule
                ? root.activeModule.editorContentKey === "M21_ANALYSIS" : false
            enabled: visible
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.preferredHeight: 0
        }

''')
    if path == "ui/qml/DspWorkspace.qml":
        text = remove_exact(text, "    property var analysisStatusViewModel: null\n")
        return remove_exact(text, "            analysisStatusViewModel: root.analysisStatusViewModel\n", count=2)
    if path == "ui/qml/Main.qml":
        return remove_exact(
            text,
            '''            analysisStatusViewModel: typeof m21ProductStatusViewModel !== "undefined"
                ? m21ProductStatusViewModel : null
''',
        )
    raise AssertionError("UNAPPROVED_SHARED_GUI_PATH " + path)


def exact_p2_integrated_gui_pin() -> bool:
    # Probe ancestry without hiding other Git errors; a positive match
    # selects the exact 2026-11-13 P2 Product GUI source pin.
    ancestry = subprocess.run(
        ["git", "merge-base", "--is-ancestor",
         P2_INTEGRATION_SOURCE, "HEAD"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if ancestry.returncode != 0:
        return False
    git("cat-file", "-e", P2_INTEGRATION_SOURCE + "^{commit}")
    changed = set(
        git("diff", "--name-only", FQ99, "HEAD", "--", "ui/qml")
        .decode("utf-8").splitlines()
    )
    if changed != P2_EXPECTED_GUI_DELTA:
        raise AssertionError(
            "INTEGRATED_P2_GUI_DELTA_SCOPE_MISMATCH expected="
            + repr(sorted(P2_EXPECTED_GUI_DELTA))
            + " actual=" + repr(sorted(changed)))
    # The prior P2 research I2 native Qt GUI build/screenshots were green
    # on exactly these bytes. No unrelated or speculative edits to any
    # previously qualified common QML file can pass this integration pin.
    for path in sorted(P2_EXPECTED_GUI_DELTA):
        if blob("HEAD", path) != blob(P2_INTEGRATION_SOURCE, path):
            raise AssertionError("INTEGRATED_P2_GUI_BLOB_UNPINNED " + path)
    if blob("HEAD", "ui/CMakeLists.txt") != blob(
        P2_INTEGRATION_SOURCE, "ui/CMakeLists.txt"
    ):
        raise AssertionError("INTEGRATED_P2_QML_REGISTRY_UNPINNED")
    # Existing FQ99 Main/Workspace remain unchanged except the previously
    # reviewed three analyzer-property links, which the original guard
    # must still be able to subtract byte-for-byte.
    for path in ("ui/qml/DspWorkspace.qml", "ui/qml/Main.qml"):
        if normalize(path, blob("HEAD", path)) != blob(FQ99, path):
            raise AssertionError("QUALIFIED_GUI_EXISTING_CONTENT_CHANGED " + path)
    print("FQ99_PLUS_P2_INTEGRATED_GUI_PIN=PASS "
          "exact_prior_green_P2_QML_blobs=YES "
          "legacy_Main_Workspace_FQ99_normalized=YES "
          "release_qualification=NO")
    return True


def main() -> int:
    # checkout@v4 in the focused workflow uses fetch-depth: 0. Never
    # substitute a network request, a moving main branch, or a guessed SHA.
    git("cat-file", "-e", FQ99 + "^{commit}")
    if exact_p2_integrated_gui_pin():
        return 0
    paths = set(
        git("diff", "--name-only", FQ99, "HEAD", "--", "ui/qml")
        .decode("utf-8").splitlines()
    )
    unexpected = paths - ALLOWED
    if unexpected:
        raise AssertionError("QUALIFIED_GUI_FILE_CHANGED " + ", ".join(sorted(unexpected)))
    if paths != ALLOWED:
        raise AssertionError(
            "APPROVED_ANALYSIS_WIRING_MISSING expected="
            + repr(sorted(ALLOWED)) + " actual=" + repr(sorted(paths))
        )
    for path in sorted(SHARED):
        current = blob("HEAD", path)
        old = blob(FQ99, path)
        if normalize(path, current) != old:
            raise AssertionError("QUALIFIED_GUI_EXISTING_CONTENT_CHANGED " + path)

    # The QML registration also must be additive only, no old registrations
    # removed, shuffled, or replaced.
    current_cmake = blob("HEAD", "ui/CMakeLists.txt")
    old_cmake = blob(FQ99, "ui/CMakeLists.txt")
    for approved_qml in (
        "        qml/AnalysisStatusEditor.qml\n",
        "        qml/StudioReadOnlyMeasurement.qml\n",
        "        qml/AnalysisApprovedContent.qml\n",
        "        qml/AnalysisModuleIcon.qml\n",
    ):
        current_cmake = remove_exact(current_cmake, approved_qml)
    if current_cmake != old_cmake:
        raise AssertionError("QUALIFIED_GUI_QML_REGISTRY_CHANGED")

    for approved in NEW:
        if not Path(approved).is_file():
            raise AssertionError("APPROVED_ANALYSIS_QML_MISSING " + approved)

    # The originally qualified M19 read-only widget is changed only to
    # allow its 36px Input Gain label to wrap at the compact editor width.
    # Normalize precisely that PO-approved layout change back to FQ99.
    current_readout = blob("HEAD", "ui/qml/StudioReadOnlyMeasurement.qml")
    current_readout = remove_exact(
        current_readout,
        "    implicitHeight: root.heroReadout ? 102 : 48",
        "    implicitHeight: root.heroReadout ? 84 : 48")
    current_readout = remove_exact(
        current_readout,
        "            width: parent.width; height: root.heroReadout ? 30 : 14",
        "            width: parent.width; height: root.heroReadout ? 16 : 14")
    current_readout = remove_exact(
        current_readout,
        "            wrapMode: root.heroReadout ? Text.WordWrap : Text.NoWrap\n"
        "            verticalAlignment: Text.AlignVCenter\n"
        "            elide: root.heroReadout ? Text.ElideNone : Text.ElideRight",
        "            elide: Text.ElideRight")
    if current_readout != blob(FQ99, "ui/qml/StudioReadOnlyMeasurement.qml"):
        raise AssertionError("QUALIFIED_STUDIO_READOUT_UNAPPROVED_CHANGE")

    approved_content = blob("HEAD", "ui/qml/AnalysisApprovedContent.qml")
    for required in (
        'objectName: "m19LevelsPage"',
        'objectName: "m20TonalStereoPage"',
        'objectName: "m21ComparisonPage"',
        'heroReadout: true',
        'dataAvailable: root.hasNumber(',
        'rawText: root.metricText(',
        'm20StaticPsd',
        'root.metricText(m21MetricRow.observed.',
        'StudioTooltipDwell {',
        'VERIFIED ANALYSIS UNAVAILABLE',
    ):
        if required not in approved_content:
            raise AssertionError("APPROVED_ANALYSIS_CONTENT_MISSING " + required)
    # PO explicitly authorized genuine numerical development UI on
    # 2026-11-10. This remains additive only: FQ99 shared geometry and
    # vector slots are still byte-protected by normalize() above.
    # No demo values or literal bypass of the actual Product source.
    for forbidden in ("fixtureMode", "presentation: ({", "dataAvailable: true"):
        if forbidden in approved_content:
            raise AssertionError("UNAUTHORIZED_PRODUCT_NUMERIC_DISPLAY " + forbidden)
    print(
        "FQ99_FROZEN_GUI_GUARD=PASS "
        "qualified_widgets_byte_identical=YES "
        "approved_new_analysis_additions_only=YES"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print("FQ99_FROZEN_GUI_GUARD=FAIL", exc, file=sys.stderr)
        sys.exit(1)
