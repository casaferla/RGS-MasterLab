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

# Explicit PO 2026-11-15: these exact QML blobs are the approved M18
# WAV-output UX and the additional PO-observed compact vertical-resize fix.
# The new Stereo/M-S blob removes the second hard breakpoint and its
# 2026-11-15 L1-M24 compact-height correction bounds the preferred controls
# well to the actual 272lp residual instead of preserving a 4lp overflow.
# This explicit SHA pin is restricted to THAT reviewed QML change; prior
# FQ99 and P2 GUI provenance is not reclassified as qualified.
# Every blob is separately pinned by Git object ID and must rerun native Qt CI.
# This is a NEW NONRELEASE candidate, not a statement of FQ99 identity.
# PO-approved post-FQ101 presentation-only update: M17 is displayed as
# Export TP Limiter; WAV Export now precedes Prepare. Its DspEditorHost
# blob below is exact-pinned again; pre-existing FQ99/P2 geometry and
# rendering gates stay mandatory. No new FQ status is asserted.
# M18 physical B01 2026-11-15: exact-pinned diagnostic-only UI delta.
# Show the existing Product exportReadinessReason in the fixed receipt slot,
# without changing verified identity, consent, DSP, layout or button authority.
# PO 2026-11-16: exact-pinned one-click WAV export UX from verified
# M17 preview/listen. The only newly actionable affordance restores the
# original MANUAL DEVICE owner, revalidates native M18 canExport, then
# opens SaveFile. Nonconforming, recovering or stale owners are excluded.
# No export consent, DSP authority, QML geometry or layout is relaxed.
# CI #33 startup root cause: Main assigned an unknown property to the
# intermediate DspWorkspace. Exact docked AND floating forwarding is now
# separately SHA-pinned, without touching DSP or M18 Product authority.
# PO physical 2026-11-16: exact-pinned M19/M20 source controls moved into
# canonical host top-right DSP action slot; redundant M21 controls removed.
# Compact native cards preserve readout fonts, full 9-row M21 table and
# the genuine log-scaled M20 spectrum. No source/authenticator is relaxed.
# Other original FQ99/P2 controls remain pinned to their prior green base.
# PO 2026-11-18: SOURCE and REFERENCE are different, preserved waveform
# presentations. Gold audition draws its selected WAV in gold with an
# independent seek/zoom viewport. M17's verified PCM is retained through
# audio-only target switches; no Project/DSP/Export contract was relaxed.
# PO 2026-11-16: M17 UI verb is Render Master (and Preview Master),
# while PREPARED stays a distinct audio pipeline scope. Only UI wording.
# PO-approved 2026-11-16 Streaming TP Limiter controls and Gain icon:
# exact-pin numeric 120x32 wells, reset/action row, removal of Research Margin
# and experimental copy, and the single horizontal slider pictogram.
# M16 diagnostic data authorization does NOT imply release Gate G.
# Exact PO-observed 2026-11-16 M19–M21 usability changes:
# ordered source selectors/table, genuine worker-activity label, and
# logarithmically aligned M20 frequency tick positions. These two
# pre-existing additive analysis files are exact-pinned, NOT released
# or declared FQ99-identical. M16 Gate G remains separate.
# PO accepted 2026-11-18 Reference region/A-B, protected M17 restore,
# WAV receipt wording and family-vector controls. The six changed GUI blobs
# below are exact-pinned; FQ99/P2 reference baselines remain untouched.
M18_WAV_OUTPUT_UI_BLOBS = {
    "ui/qml/AuditionSourceSelector.qml": "1225337c5ca5f652cb01a022e3475be5afa1aa74",
    "ui/qml/StudioButton.qml": "04abee78e09e9d6e9cc8f53b7957eca3691fc7a8",
    "ui/qml/DspChainSelector.qml": "2973190d6293ab1dbb766fe03aac6a6b19f081f2",
    "ui/qml/AnalysisApprovedContent.qml": "c7c4cc4c58c0a3e9c598f2d4c65c3374a405df0a",
    "ui/qml/AnalysisStatusEditor.qml": "6fd80287caa32e5ec180c91b3933e8529293a55b",
    # PO-directed pre-M24 adversarial repair F05: unchanged native M18
    # ticket/consent and FQ99 geometry; only the recovery-only monitor
    # predicate is added to the already approved Return + Export path.
    # Review against the prior pinned blob: exactly five QML lines replaced,
    # no other Main.qml line changed. Same exact Git-blob enforcement.
    "ui/qml/Main.qml": "3e222232b54305994970b073e16a977c63ccaf47",
    "ui/qml/DspEditorHost.qml": "8028ac7e80b70cfb1a5f97a0bbfb118872359919",
    "ui/qml/StreamingTruePeakLimiterEditor.qml": "221ea17a9b42ddf49c5aa667b33514297e9cb3a6",
    "ui/qml/StudioIcon.qml": "4dd165406dd1130dce6434bcb248df3211a11477",
    "ui/qml/StereoMsEditor.qml": "c56054770d120ca211189a3aff5c3bac4c10672a",
    "ui/qml/DspWorkspace.qml": "b354658f9164258b41c9cac3a6cfd2d42e421802",
    "ui/qml/TruePeakMeterEditor.qml": "71cb0fe236ea683a027f78713449661fc7ebdb1e",
    "ui/qml/TruePeakLimiterEditor.qml": "a92485ba49571dd6e2259d41c79ff51157574161",
}
M18_APPROVED_GUI_DELTA = P2_EXPECTED_GUI_DELTA | set(M18_WAV_OUTPUT_UI_BLOBS)



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
    if changed != M18_APPROVED_GUI_DELTA:
        raise AssertionError(
            "INTEGRATED_M18_GUI_DELTA_SCOPE_MISMATCH expected="
            + repr(sorted(M18_APPROVED_GUI_DELTA))
            + " actual=" + repr(sorted(changed)))
    # The PO-authorized M18 and vertical-resize UI blobs must match their
    # independently inspected exact Git object IDs. This cannot silently
    # approve arbitrary future changes to previously qualified shared GUI.
    # Everything else remains byte-identical to the CI-green P2 I2 source.
    # PO physical replay proved that even SMOOTH scaling of shared
    # margins/header creates incorrect alignment. The common header is
    # EXACTLY the unchanged Parametric-EQ 12lp inset, 8lp vertical gaps
    # and 40lp row for all DSPs at every host height. The only allowed
    # adaptation lives below it. P2's private 390px hard jump is gone.
    host_qml=blob("HEAD", "ui/qml/DspEditorHost.qml")
    stereo_qml=blob("HEAD", "ui/qml/StereoMsEditor.qml")
    p2_qml=blob("HEAD", "ui/qml/StreamingTruePeakLimiterEditor.qml")
    if ("root.height < 470" in host_qml
        or "hostSpaceFactor" in host_qml
        or "root.height < 380" in stereo_qml
        or "height < 390" in p2_qml or "root.compact ?" in p2_qml):
        raise AssertionError("M18_PO_VERTICAL_HEADER_OR_P2_JUMP_REINTRODUCED")
    if ('anchors.margins: 12' not in host_qml
        or 'Layout.minimumHeight: 40' not in host_qml
        or 'Layout.maximumHeight: 40' not in host_qml
        or 'verticalSpaceFactor' not in stereo_qml
        or 'verticalSpaceFactor' not in p2_qml
        or 'm18WavOutputRightAlignSpacer' not in host_qml):
        raise AssertionError("M18_PO_EQ_HEADER_PARITY_OR_WAV_ALIGNMENT_MISSING")
    # Exact PO 2026-11-15 resize rule: containers grow to the right /
    # bottom from the compact-minimum TOP-LEFT, never translate because
    # a higher window adds top or left padding. Guard both source
    # properties and real scene coordinates (main.cpp Qt Product gate).
    if ('        spacing: 4\n\n        RowLayout' not in stereo_qml
        or 'Layout.minimumHeight: 30' not in stereo_qml
        or 'Layout.preferredHeight: 30' not in stereo_qml
        or 'Layout.maximumHeight: 30' not in stereo_qml
        or '        anchors.margins: 8\n        spacing: 5' not in p2_qml
        or '                anchors.margins: 9\n' not in p2_qml
        or 'anchors.margins: 8 + ' in p2_qml
        or 'anchors.margins: 9 + ' in p2_qml):
        raise AssertionError("PO_RESIZABLE_WELL_MIN_TOP_LEFT_DRIFT_REINTRODUCED")
    for path in sorted(M18_APPROVED_GUI_DELTA):
        if path in M18_WAV_OUTPUT_UI_BLOBS:
            actual = git("rev-parse", "HEAD:" + path).decode().strip()
            if actual != M18_WAV_OUTPUT_UI_BLOBS[path]:
                raise AssertionError("M18_WAV_UI_EXACT_BLOB_NOT_APPROVED " + path)
            if blob("HEAD", path) == blob(P2_INTEGRATION_SOURCE, path):
                raise AssertionError("M18_WAV_UI_NO_REAL_CHANGE " + path)
        elif blob("HEAD", path) != blob(P2_INTEGRATION_SOURCE, path):
            raise AssertionError("INTEGRATED_P2_GUI_BLOB_UNPINNED " + path)
    if blob("HEAD", "ui/CMakeLists.txt") != blob(
        P2_INTEGRATION_SOURCE, "ui/CMakeLists.txt"
    ):
        raise AssertionError("INTEGRATED_P2_QML_REGISTRY_UNPINNED")
    # Preserve the old FQ99 delta verification using the prior CI-green
    # P2 source, not by misrepresenting the new M18 UI as unchanged.
    for path in ("ui/qml/DspWorkspace.qml", "ui/qml/Main.qml"):
        if normalize(path, blob(P2_INTEGRATION_SOURCE, path)) != blob(FQ99, path):
            raise AssertionError("PREEXISTING_P2_GUI_BREAKS_FQ99 " + path)
    # The earlier FQ99/P2 baseline remains known while the five new exact
    # M18 blobs require their own current Windows Qt compile/screenshot gate.
    print("FQ99_PLUS_P2_M18_WAV_UI_PIN=PASS "
          "exact_prior_green_P2_GUI_base=YES "
          "exact_PO_M18_UI_blobs=YES "
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
    ):
        if required not in approved_content:
            raise AssertionError("APPROVED_ANALYSIS_CONTENT_MISSING " + required)
    status_editor = blob("HEAD", "ui/qml/AnalysisStatusEditor.qml")
    for required in (
        'objectName: "analysisVerificationStatus"',
        'VERIFIED ANALYSIS UNAVAILABLE',
        'MEASUREMENTS AVAILABLE',
    ):
        if required not in status_editor:
            raise AssertionError("APPROVED_ANALYSIS_HEADER_STATUS_MISSING " + required)
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
