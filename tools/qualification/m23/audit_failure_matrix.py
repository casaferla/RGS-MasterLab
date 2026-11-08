#!/usr/bin/env python3
"""M23 frozen-gap audit and CTest/JUnit evidence assembler: NO release claims.

This tool never runs audio itself, cannot mint a Product ticket, and cannot
declare M23 PASS. It distinguishes a genuine executed Qt test suite from a
mere source-code marker. It makes missing fault cases visible, not green.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from xml.etree import ElementTree as ET

SCHEMA = "rgsml.m23.failure-matrix-reconciliation/1"
VALID_COVERAGE = {"existing", "partial", "gap"}
MANDATORY = (
    "render_window", "unavailable_module", "nonfinite_dsp_input",
    "short_write", "flush_close_failure", "candidate_reopen_failure",
    "malformed_candidate", "encoded_checksum_mismatch",
    "decoded_checksum_mismatch", "existing_destination",
    "source_hardlink_alias", "gold_destination_collision",
    "commit_epoch_race", "destination_readback_mismatch",
    "malformed_project", "unknown_semantic_module",
    "project_open_atomicity", "source_gold_immutability",
    "postdecode_true_peak_pcm24", "alpha_diagnostic_report",
)
WINDOWS_ONLY = {
    "rgsml_transactional_audio_export_tests",
    "rgsml_m18_windows_candidate_delivery_tests",
}
FROZEN_FALSE = (
    "m23_closed", "physical_gate_m",
    "product_admission_authorized", "release_qualified",
)


class M23ContractError(ValueError):
    pass


def safe_path(root: Path, relative: str) -> Path:
    path = Path(relative)
    if not relative or path.is_absolute() or ".." in path.parts:
        raise M23ContractError("unsafe evidence file path")
    full = (root / path).resolve()
    if not full.is_relative_to(root.resolve()) or not full.is_file():
        raise M23ContractError("evidence path missing/escaped root: " + relative)
    return full


def read_manifest(root: Path, manifest_path: Path) -> dict:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != SCHEMA:
        raise M23ContractError("M23 frozen matrix schema changed")
    if manifest.get("status") != "RECONCILIATION_ONLY_NOT_M23_QUALIFIED":
        raise M23ContractError("M23 reconciliation claimed a qualification status")
    for key in FROZEN_FALSE:
        if manifest.get(key) is not False:
            raise M23ContractError("false M23 release/physical claim: " + key)
    cases = manifest.get("cases")
    if not isinstance(cases, list) or len(cases) != len(MANDATORY):
        raise M23ContractError("M23 matrix must cover all 20 frozen categories")
    if [c.get("category") for c in cases] != list(MANDATORY):
        raise M23ContractError("M23 categories reordered/omitted/duplicated")
    if [c.get("id") for c in cases] != [
        str(i).zfill(2) for i in range(1, len(MANDATORY)+1)
    ]:
        raise M23ContractError("M23 category IDs changed")
    cmake = safe_path(root, "tests/CMakeLists.txt").read_text(
        encoding="utf-8"
    )
    for c in cases:
        coverage = c.get("static_coverage")
        if coverage not in VALID_COVERAGE:
            raise M23ContractError("unknown coverage status: " + str(coverage))
        if not isinstance(c.get("rationale"), str) or not c["rationale"]:
            raise M23ContractError("evidence rationale absent")
        if coverage == "gap":
            if c.get("ctest") is not None or c.get("source") is not None:
                raise M23ContractError("gap cannot masquerade as source proof")
            continue
        suite, source, slot = (
            c.get("ctest"), c.get("source"), c.get("slot")
        )
        if not all(isinstance(x, str) and x for x in (
            suite, source, slot
        )):
            raise M23ContractError("existing/partial proof requires exact suite/slot")
        if ("add_test(NAME " + suite) not in cmake:
            raise M23ContractError("claimed CTest suite isn't registered: " + suite)
        source_text = safe_path(root, source).read_text(encoding="utf-8")
        if not re.search(
            r"\bvoid\s+(?:[A-Za-z_][A-Za-z_0-9]*::\s*)?"
            + re.escape(slot) + r"\s*\(\s*\)\s*(?:;|\{)",
            source_text,
        ):
            raise M23ContractError("claimed concrete test slot missing: " + slot)
    return manifest


def parse_junit(paths: list[Path]) -> dict[str, str]:
    """Select exact CTest suite outcomes; any fail/skip makes NOT_PASS."""
    observations: dict[str, str] = {}
    for path in paths:
        if not path.is_file():
            raise M23ContractError("JUnit file is missing")
        try:
            root = ET.parse(path).getroot()
        except ET.ParseError as exc:
            raise M23ContractError("malformed CTest JUnit") from exc
        suites = [root] if root.tag == "testsuite" else root.findall(
            ".//testsuite"
        )
        if not suites:
            raise M23ContractError("JUnit contains no test suite")
        for suite in suites:
            for testcase in suite.findall("testcase"):
                name = testcase.get("name")
                if not name:
                    raise M23ContractError("JUnit testcase has no name")
                # CTest emits one testcase per CTest *executable* suite.
                state = (
                    "FAIL" if testcase.find("failure") is not None
                        or testcase.find("error") is not None
                    else "SKIP" if testcase.find("skipped") is not None
                    else "EXECUTED_PASS"
                )
                old = observations.get(name)
                if old is not None and old != state:
                    raise M23ContractError("conflicting JUnit verdict: " + name)
                observations[name] = state
    if paths and not observations:
        raise M23ContractError("JUnit contains no executed CTest cases")
    return observations


def build_report(
    manifest: dict, observations: dict[str, str], platform: str
) -> dict:
    cases = []
    for c in manifest["cases"]:
        suite = c.get("ctest")
        if c["static_coverage"] == "gap":
            runtime = "UNPROVEN_GAP"
        elif suite in WINDOWS_ONLY and platform == "linux":
            runtime = "NOT_RUN_PLATFORM_WINDOWS_ONLY"
        else:
            runtime = observations.get(suite, "NOT_EXECUTED")
        cases.append({
            "category": c["category"], "static_coverage": c["static_coverage"],
            "ctest": suite, "runtime": runtime,
            "qualified": False, "rationale": c["rationale"],
        })
    missing = [
        c["category"] for c in cases
        if c["static_coverage"] != "existing"
    ]
    unrun = [
        c["category"] for c in cases
        if c["static_coverage"] == "existing"
        and c["runtime"] not in ("EXECUTED_PASS",
            "NOT_RUN_PLATFORM_WINDOWS_ONLY")
    ]
    return {
        "schema": "rgsml.m23.failure-matrix-receipt/1",
        "status": "RECONCILED_NOT_FOR_RELEASE",
        "baseline_sha": manifest["baseline_sha"],
        "platform": platform,
        "source_contract_valid": True,
        "passed_existing_suites": sorted(
            k for k,v in observations.items() if v == "EXECUTED_PASS"
        ),
        "unresolved_categories": missing,
        "unexecuted_existing_categories": unrun,
        "m23_closed": False,
        "physical_gate_m": False,
        "frozen_m23_i_p_m_passed": False,
        "full_fq_passed": False,
        "product_admission_authorized": False,
        "release_qualified": False,
        "cases": cases,
    }


def run(
    root: Path, matrix: Path, junit: list[Path], platform: str,
    require_existing_executed: bool
) -> dict:
    manifest = read_manifest(root, matrix)
    observations = parse_junit(junit)
    report = build_report(manifest, observations, platform)
    if require_existing_executed and report["unexecuted_existing_categories"]:
        raise M23ContractError(
            "genuine existing CTest suites not executed cleanly: "
            + ",".join(report["unexecuted_existing_categories"])
        )
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--matrix", type=Path,
        default=Path(__file__).with_name("failure_matrix_reconciliation.json"))
    parser.add_argument("--junit", action="append", default=[], type=Path)
    parser.add_argument("--platform", choices=("windows", "linux"), required=True)
    parser.add_argument("--require-existing-executed", action="store_true")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    result = run(
        args.root, args.matrix, args.junit, args.platform,
        args.require_existing_executed
    )
    raw = json.dumps(result, sort_keys=True, indent=2) + "\n"
    if args.out:
        if args.out.exists() or args.out.is_symlink() or not args.out.parent.is_dir():
            raise M23ContractError("M23 report path must be a fresh file")
        args.out.write_text(raw, encoding="utf-8")
    print(raw, end="")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as exc:
        print("M23_NONRELEASE_CONTRACT_FAIL: " + str(exc), file=sys.stderr)
        sys.exit(1)
