#!/usr/bin/env python3
"""M17 GR after M18-M22 dual-green: exact-source, frozen, isolated UI guard.

No main merge; no sound DSP, SRC, true-peak meter, Product registry or
persisted GOLD token may change. Qt tests must validate visual behavior.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
M18_M22_BASE = "3aca7bdf4d96269d86e6690617642b876dcb3cbc"
PO_M17_GR = "365c91924dc04a73a6eaa0981b880db5383e0731"
# These blobs are exact original PR #85 approved UI and PO source contract.
ORIGINAL = {
    "docs/architecture/M17_UNIFIED_FULL_GR_HISTORY_PO_DELTA_20261105.md":
        "17c38ac525db9e9af702b576a648b360b1607491",
    "ui/qml/DspEditorHost.qml":
        "015bff4375e9920f3a98c26b8969475e43f147d9",
    "ui/qml/DspWorkspace.qml":
        "550505f9c3ccc5ac1e0557a9c3753e7627c6cbb3",
    "ui/qml/TruePeakGrHistoryGraph.qml":
        "97bf8128c362d48b882d671821de7ce3ad50f95c",
    "ui/qml/TruePeakLimiterEditor.qml":
        "1533859335ea5e15e66b6a84ee59aec0e789c2bf",
    "ui/qml/TruePeakMeterEditor.qml":
        "1aa406aee2d4a3ba4b4223cd246e5f52fb7b5ce6",
}
# Approved six-hunk contextual graft atop the frozen M18-M22+REFERENCE head.
GRAFTED = {
    "ui/qml/Main.qml":"4b4af77e532accb9f0e7742ba55a01f1d29ed57d",
    "tests/ui_smoke/test_source_metadata_panel.cpp":
        "c9f28a952e0b9081a68a09cf164a7d799e0561d1",
}
BRIDGE = {
    "docs/architecture/M17_GR_AFTER_M22_REBASE_20261108.md",
    "tools/qualification/m17_gr_history/verify_after_m22.py",
    ".github/workflows/m17-gr-after-m22-focused.yml",
}

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)

def check(ok, message):
    if not ok:
        raise RuntimeError(message)

def blob(ref, path):
    return git("show", f"{ref}:{path}")

def main():
    check(subprocess.call(
        ["git","merge-base","--is-ancestor",M18_M22_BASE,"HEAD"],
        cwd=ROOT)==0,
        "M18-M22 last dual-OS CI#7 is not an ancestor")
    changed=set(git("diff","--name-only",M18_M22_BASE,"HEAD")
                .decode().splitlines())
    permitted=set(ORIGINAL)|set(GRAFTED)|BRIDGE
    check(changed==permitted,
          "M17 overlay has missing or unapproved paths: "+
          repr({"missing":sorted(permitted-changed),
                "unexpected":sorted(changed-permitted)}))
    for path,sha in ORIGINAL.items():
        actual=git("rev-parse",f"HEAD:{path}").decode().strip()
        check(actual==sha,
              f"PO original M17 GR blob changed: {path}: {actual}")
        check(blob("HEAD",path)==blob(PO_M17_GR,path),
              "PO PR85 source drift: "+path)
    for path,sha in GRAFTED.items():
        actual=git("rev-parse",f"HEAD:{path}").decode().strip()
        check(actual==sha,
              f"reviewed GR/REFERENCE/M18 graft changed: {path}: {actual}")

    mainqml=blob("HEAD","ui/qml/Main.qml").decode()
    for marker in (
        "m17GrTimelineFraction",
        "m17GrTimelineScope",
        "waveformOverview.positionFrames / waveformOverview.durationFrames",
        "limiterOfflineGrProvider",
        "Open Reference",
        "Clear Reference",
        "GOLD_AUTOMATCH",
        "m18ProductExportViewModel",
    ):
        check(marker in mainqml, "M17 shared waveform/REFERENCE/M18 missing: "+marker)
    for path in (
        "ui/qml/Main.qml","ui/qml/DspEditorHost.qml",
        "ui/qml/DspWorkspace.qml","ui/qml/TruePeakMeterEditor.qml",
        "ui/qml/TruePeakLimiterEditor.qml",
    ):
        content=blob("HEAD",path).decode()
        check("limiterGrProvider" not in content,
              "obsolete LIVE GR QML provider still in "+path)

    graph=blob("HEAD","ui/qml/TruePeakGrHistoryGraph.qml").decode()
    for marker in (
        "offlineSeriesValid", "offlineSeriesAvailable",
        "playheadAvailable", "timelineFraction", "timelineScope",
        "frameEndExclusive", "ctx.createLinearGradient(",
        "ctx.fillRect(cursorX - 1", '#F2B632',
    ):
        check(marker in graph, "M17 authentic full-history graph absent: "+marker)
    for disallowed in (
        "verifiedHistory","backendVerified","verifiedSeriesAvailable",
        "Timer {",
    ):
        check(disallowed not in graph,
              "obsolete or fabricated GR Live source: "+disallowed)

    smoke=blob("HEAD","tests/ui_smoke/test_source_metadata_panel.cpp").decode()
    for marker in (
        "void SourceMetadataPanelSmokeTest::truePeakLimiterLayoutCandidate()",
        "referenceTerminologyOnRealMain",
        "m17_gr_history_SYNTHETIC_cursor25.png",
        "m17_gr_history_SYNTHETIC_cursor75.png",
        "m17_gr_history_EDITOR_SYNTHETIC_cursor25.png",
        "m17_gr_history_EDITOR_SYNTHETIC_cursor75.png",
        "M17 full editor raster failed to reflect GR playhead seek",
        "QVERIFY(!grPlot->property(\"playheadAvailable\").toBool());",
        "NON-CONFORMING",
    ):
        check(marker in smoke, "Qt M17/REFERENCE regression missing: "+marker)

    # Scope is strictly UI/QtTests/docs/workflow. The rest of the canonical
    # DSP, Phase-6, M18/M22 Product and serialized GOLD code equals CI#7.
    for protected in (
        "app/src/main.cpp","app/src/internal/m18_product_complete_binding.hpp",
        "app/src/internal/m21_product_owner_analysis.hpp",
        "app/src/internal/m22_product_verified_dispatch.hpp",
        "render/src/internal/m22_request_plan_candidate.hpp",
        "core/src/processing_state.cpp",
        "ui/CMakeLists.txt",
        "project/src/project_snapshot.cpp",
        "tests/integration/app/test_project_session.cpp",
    ):
        check(blob("HEAD",protected)==blob(M18_M22_BASE,protected),
              "frozen Product/DSP/Project/Qt registration changed: "+protected)
    print("M17_GR_HISTORY_AFTER_M18_M22_PROVENANCE_PASS")
    print("Isolated preintegration ONLY; no Main merge/FQ/physical acceptance")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError,OSError,subprocess.CalledProcessError) as exc:
        print("M17_GR_REBASE_GUARD_FAILURE:",exc,file=sys.stderr)
        sys.exit(1)
