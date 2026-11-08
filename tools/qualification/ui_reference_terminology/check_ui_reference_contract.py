#!/usr/bin/env python3
"""RGS MasterLab 2026-11-08 PO frozen REFERENCE UI terminology gate.

Read-only audit. No DSP, persistence or project schema migrations. Windows/Linux
stdlib-only script; use the immutable pre-UX commit as the comparison base.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
BASE = "db1f59e168c3b851a3fcf6b487b2ada9e23e1a30"

def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def fail(message: str) -> None:
    raise AssertionError(message)

def must_have(path: str, snippets: tuple[str, ...]) -> None:
    contents = text(path)
    for snippet in snippets:
        if snippet not in contents:
            fail(f"{path}: required PO UI contract missing: {snippet}")

def must_avoid(path: str, snippets: tuple[str, ...]) -> None:
    contents = text(path)
    for snippet in snippets:
        if snippet in contents:
            fail(f"{path}: legacy public-GOLD wording remains: {snippet}")

def immutable_source(path: str) -> None:
    previous = subprocess.check_output(
        ["git", "show", f"{BASE}:{path}"], cwd=ROOT)
    # Compare canonical committed blobs, not checkout bytes: Windows checkout
    # may use CRLF while git show always returns the LF-normalized blob.
    current = subprocess.check_output(
        ["git", "show", f"HEAD:{path}"], cwd=ROOT)
    if previous != current:
        fail(f"PROTECTED_PERSISTENCE_OR_DSP_CHANGED: {path}")

def only_public_cpp_text_changed(path: str, substitutions: tuple[tuple[str,str],...]) -> None:
    original = subprocess.check_output(
        ["git", "show", f"{BASE}:{path}"], cwd=ROOT).decode("utf-8")
    expected = original
    for old, new in substitutions:
        if expected.count(old) != 1:
            fail(f"Unable to locate unique baseline C++ public wording {path} {old}")
        expected = expected.replace(old,new)
    if text(path) != expected:
        fail(f"UNAUTHORIZED_NONPRESENTATION_CPP_CHANGE: {path}")

def run() -> None:
    protected_paths = (
        "core/src/processing_state.cpp",
        "project/include/rgsml/project/project_document.hpp",
        "app/src/project_session_view_model.cpp",
        "tests/unit/project/test_project_json.cpp",
        "app/src/gold_selection_view_model.hpp",
        "app/src/audition_source_selector.hpp",
    )
    for path in protected_paths:
        immutable_source(path)

    only_public_cpp_text_changed(
        "app/src/gold_selection_view_model.cpp",
        (("Choose one local Gold WAV file.",
          "Choose one local Reference WAV file."),))
    only_public_cpp_text_changed(
        "app/src/audition_source_selector.cpp",
        (("Gold must be a file distinct from Source.",
          "Reference must be a file distinct from Source."),
         ("Gold must not be an alias of the Source file.",
          "Reference must not be an alias of the Source file.")))

    must_have("core/src/processing_state.cpp",
              ("ProcessingState::GOLD", 'return "GOLD";', 'token == "GOLD"'))
    must_have("project/include/rgsml/project/project_document.hpp",
              ('referenceType{"GOLD"}',))
    must_have("tests/unit/project/test_project_json.cpp",
              ('"GOLD"', 'region_gold_chain_roundtrip'))
    must_have("app/src/gold_selection_view_model.hpp",
              ("GoldSelectionViewModel", "hasGold", "selectGold"))
    must_have("ui/qml/Main.qml", (
        'objectName: "menuOpenGold"; text: "Open Reference"',
        'objectName: "menuClearGold"; text: "Clear Reference"',
        'title: "Open Reference WAV"',
        'text: "Reference / AutoMatch"',
        "minimumControlWidth: 166",
        '"GOLD_AUTOMATCH"',  # unchanged internal view token
        'goldSelection.selectGold(selectedFile)', # unchanged API
        '"NO REFERENCE LOADED"',
        'elide: Text.ElideNone',
        '"Reference: not loaded"',
    ))
    must_avoid("ui/qml/Main.qml", (
        '"Open Gold Reference"', '"Clear Gold Reference"',
        '"NO GOLD REFERENCE LOADED"', 'text: "Gold / Automatch"',
        '"Gold: not loaded"',
    ))
    must_have("ui/qml/AuditionSourceSelector.qml", (
        'text: "REFERENCE"', 'minimumControlWidth: 112',
        'Accessible.name: "Audition Reference"',
        'auditionSelector.activeTarget === "GOLD"', # unchanged contract
        "auditionSelector.selectGold()", # unchanged API
        'objectName: "auditionGoldButton"', # unchanged ID
    ))
    must_avoid("ui/qml/AuditionSourceSelector.qml",
               ('text: "GOLD"', '"Audition Gold Reference"'))
    must_have("ui/qml/M19M21AnalysisWorkbenchPreview.qml", (
        '{name:"REFERENCE",key:"GOLD",color:"#D19562"}',
        'objectName: "analysisScopeBadge_" + (modelData.key || modelData.name)',
        'text:"REFERENCE SCOPE IS INDEPENDENT"',
        '"Δ PREPARED → REFERENCE"', '"Δ PROCESSED → REFERENCE"',
        'modelData.gold', # unchanged data key
    ))
    must_have("ui/qml/M21MetricsComparisonPreview.qml", (
        "PREPARED | REFERENCE | PROCESSED",
        "Independent REFERENCE analysis scope",
        "Δ PREPARED→REFERENCE",
        "Δ PROCESSED→REFERENCE",
        'root.presentation.scopes[1]', # unchanged data
        "modelData.gold", # unchanged data key
        'objectName: "m21GoldScope"', # unchanged QML test identity
    ))
    # All actually user-facing text/title/a11y properties must not expose
    # technical GOLD. Key names, styles/tone="gold", IDs and enum values are
    # intentionally excluded from this presentation-only inspection.
    targets = (
        "ui/qml/Main.qml",
        "ui/qml/AuditionSourceSelector.qml",
        "ui/qml/M19M21AnalysisWorkbenchPreview.qml",
        "ui/qml/M21MetricsComparisonPreview.qml",
    )
    assignment = re.compile(r'^\s*(?:text|title|Accessible\.name|Accessible\.description)\s*:')
    legacy = re.compile(r'\b(?:GOLD|Gold)\b')
    for path in targets:
        for number, line in enumerate(text(path).splitlines(), 1):
            if assignment.search(line) and legacy.search(line):
                # A value carrying the raw technical token must be mapped
                # (e.g. auditionSelector.activeTarget or scopeWarning).
                if re.search(r'["\']GOLD["\']', line) and "===" in line:
                    continue
                fail(f"{path}:{number}: raw GOLD in UI property: {line.strip()}")
    print("RGS_REFERENCE_UI_PUBLIC_TERMINOLOGY_AND_FROZEN_GOLD_CONTRACT_PASS")

if __name__ == "__main__":
    try:
        run()
    except (AssertionError, OSError, subprocess.CalledProcessError) as e:
        print(f"RGS_REFERENCE_UI_CONTRACT_FAILURE: {e}", file=sys.stderr)
        sys.exit(1)
