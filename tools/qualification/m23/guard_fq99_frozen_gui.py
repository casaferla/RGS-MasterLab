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
NEW = {"ui/qml/AnalysisStatusEditor.qml"}
ALLOWED = SHARED | NEW


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
        return remove_exact(text, '''        case "M19_ANALYSIS": return "module-peak-meter"
        case "M20_ANALYSIS": return "module-stereo"
        case "M21_ANALYSIS": return "module-peak-meter"
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
        return text
    if path == "ui/qml/DspWorkspace.qml":
        text = remove_exact(text, "    property var analysisStatusViewModel: null\n")
        return remove_exact(text, "            analysisStatusViewModel: root.analysisStatusViewModel\n", count=2)
    if path == "ui/qml/Main.qml":
        # New analysis is hosted in the existing MAIN dock slot, never
        # in the previously qualified DSP host scenegraph.
        text = remove_exact(text, '''    // M19–M21: new analysis content lives at the DOCK-SLOT layer.
    // Do NOT insert any child (even an inactive Loader) into the FQ99
    // DspEditorHost scenegraph: the native minimum-window test proves
    // such new children perturb the qualified Parametric EQ allocation.
    // This overlay is strictly limited to NEW analysis selections.
    Loader {
        id: dockedProductAnalysis
        objectName: "m19M21DockSlotAnalysisLoader"
        parent: root.activeDockSlot()
        readonly property bool analysisSelected:
            root.statusDspKey === "M19_ANALYSIS"
            || root.statusDspKey === "M20_ANALYSIS"
            || root.statusDspKey === "M21_ANALYSIS"
        active: analysisSelected && !root.editorDetached
        visible: active
        asynchronous: false
        z: 10
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
        anchors.topMargin: 60
        sourceComponent: Component {
            AnalysisStatusEditor {
                objectName: "m19M21ProductAnalysisEditor"
                panelIndex: root.statusDspKey === "M20_ANALYSIS" ? 1
                    : root.statusDspKey === "M21_ANALYSIS" ? 2 : 0
                statusViewModel: realDspWorkspace.analysisStatusViewModel
            }
        }
    }

''')
        return remove_exact(
            text,
            '''            analysisStatusViewModel: typeof m21ProductStatusViewModel !== "undefined"
                ? m21ProductStatusViewModel : null
''',
        )
    raise AssertionError("UNAPPROVED_SHARED_GUI_PATH " + path)


def main() -> int:
    # checkout@v4 in the focused workflow uses fetch-depth: 0. Never
    # substitute a network request, a moving main branch, or a guessed SHA.
    git("cat-file", "-e", FQ99 + "^{commit}")
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
    current_cmake = remove_exact(
        current_cmake, "        qml/AnalysisStatusEditor.qml\n"
    )
    if current_cmake != old_cmake:
        raise AssertionError("QUALIFIED_GUI_QML_REGISTRY_CHANGED")

    if not Path("ui/qml/AnalysisStatusEditor.qml").is_file():
        raise AssertionError("APPROVED_NEW_ANALYSIS_PANEL_MISSING")
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
