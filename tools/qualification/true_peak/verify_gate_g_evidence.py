#!/usr/bin/env python3
"""M16 B2i: fail-closed provenance and qualification-evidence reconciler.

This is an OFFLINE qualification-guard test, not the release registry/DTO.
A PASS means that a preliminary evidence matrix is coherent and remains
UNQUALIFIED. It cannot qualify a rate or close Gate G automatically.
No EBU PCM, audio files, private reports or network access.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import re
import subprocess
import sys

RATES = (44100, 48000, 88200, 96000, 176400, 192000)
B2F_CI421_SHA = "843d1da9caade489e92fde73c4a70f7e58afe291"
B2G_CI422_SHA = "0684483c2912641356539e4bb1c7d8c8992e183f"
B2H_CI423_SHA = "f8a751bdd65014d155b64bc52d54a982cce3ffa4"
B2F_EXE_SHA = "eda73e8b5aa40b58f52477a3554631c0e4bd1e44c6f3d2dd9c5a2b0d57b198e7"
B2H_EXE_SHA = "fc251fb2e379f4888d8a109be1c12d18cc836ffec7fd973d7c08be9600f13c53"
TAP_SHA = "D1DF3373D3C5A82A3E4CFE3576BDDF97E153F3281FF9287554E024FA6CB0D316"
SCHEMA = "rgsml.m16.b2i.audit-evidence-matrix/0.3"
ORIGINAL_BLOB_IDS = {
    "analysis/src/internal/true_peak_full_resource.hpp": "c45a25736090964d94e3122a0813212cc04a30ae",
    "analysis/src/internal/true_peak_itu4x_coefficients.hpp": "fca4dde9f64dfa5fd59c3f36f89d3bd6753b146e",
    "analysis/src/internal/true_peak_itu4x_channel_stream.hpp": "57305d199a0f60477bdb35824a3536c1ff2eea17",
    "analysis/src/true_peak_full_resource.cpp": "ceb2195c0e1be21e2c80a1b53c29dbb26c7a7881",
    "analysis/src/true_peak_itu4x_channel_stream.cpp": "f6e02fdc2a5fede3fe5542062a815442b012a802",
    "analysis/CMakeLists.txt": "dfca065ab8b1fb7488b044d3b81bc5d5dda78a97",
}


class EvidenceError(ValueError):
    pass


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise EvidenceError(reason)


def validate_matrix(matrix: dict) -> dict:
    """Check conservation of authentic evidence; NEVER issue QUALIFIED."""
    require(matrix.get("schema") == SCHEMA, "Unknown evidence matrix schema")
    require(matrix.get("status") == "PRELIMINARY_NON_AUTHORITATIVE__CI423_RECONCILED",
            "Release authority must remain preliminary")
    require(matrix.get("checked_source_sha") == B2H_CI423_SHA,
            "Evidence source SHA cannot be silently inherited")
    require(matrix.get("acceptance") == "GATE_G_OPEN__NO_PUBLIC_QUALIFIED_LABEL",
            "Gate G incorrectly closed")
    require(matrix.get("scope") == "B2I_MATH_EVIDENCE_RECONCILIATION_NOT_PRODUCT_RELEASE",
            "Unsupported product qualification claim")
    require(matrix.get("release_guard") ==
            "ALL_6_SUPPORTED_UNQUALIFIED__GATE_G_OPEN__NO_AUTO_FQ_MERGE_M17",
            "Release guard changed")
    frozen = matrix.get("frozen_identity", {})
    for key, expected in (
        ("algorithmVersion", "1.0.0"),
        ("truePeakPolicyId", "rgsml.analysis.true-peak.itu4x/1.0.0"),
        ("resultSchemaId", "rgsml.analysis.true-peak.result/1.0.0"),
        ("filterAssetSha256", TAP_SHA),
        ("interpolator", "ITU-R_BS.1770-5_ANNEX_2_BINARY64_4X"),
    ):
        require(frozen.get(key) == expected, f"Frozen {key} mismatch")
    require(matrix.get("production_blob_lock") == ORIGINAL_BLOB_IDS,
            "Declared production code scope changed")

    ev = matrix.get("evidence", {})
    older = ev.get("ci_421", {})
    gain = ev.get("ci_422", {})
    current = ev.get("ci_423", {})
    ebu = ev.get("official_ebu_15_23", {})
    screened = ev.get("b2h_high_resolution_six_hertz_rates", {})
    require(older.get("source_sha") == B2F_CI421_SHA and
            older.get("run_id") == "38310143556" and
            older.get("linux_ctest_passed") == 44 and
            older.get("windows_ctest_passed") == 47 and
            older.get("strict_fp_build_flag_checks_per_runner") == 2,
            "Original strict-FP source evidence missing")
    require(gain.get("source_sha") == B2G_CI422_SHA and
            gain.get("run_id") == "38407061252" and
            gain.get("status") == "PASS" and
            gain.get("metamorphic_combinations") == 60,
            "B2g gain-law CI evidence incomplete")
    require(current.get("source_sha") == B2H_CI423_SHA and
            current.get("run_id") == "38411791580" and
            current.get("windows_probe_exe_sha256") == B2H_EXE_SHA and
            current.get("artifact_id") == "11823258952" and
            current.get("linux_ctest_passed") == 44 and
            current.get("windows_ctest_passed") == 47 and
            current.get("b2c_synthetic_hertz_cases") == 36 and
            current.get("b2d_extreme_fp_cases") == 27 and
            current.get("b2e_five_case_diagnostic") == 5 and
            current.get("b2h_six_real_hertz_rates") == 6 and
            current.get("strict_fp_source_audit_each_platform") == 2 and
            current.get("ideal_infinite_sinc_tail_certified") is False and
            current.get("status") == "PASS_WITH_LIMITED_SCOPE",
            "Current CI423 test/probe scope cannot be established")
    require(ebu.get("sample_rate_hz") == 48000 and
            ebu.get("source_sha") == B2F_CI421_SHA and
            ebu.get("executed_windows_probe_sha256") == B2F_EXE_SHA and
            ebu.get("same_binary_as_ci_423") is False and
            ebu.get("cases_passed") == 9 and
            ebu.get("predicates_passed") == 27 and
            ebu.get("report_sha256") ==
            "d9e9de4b6f0f7ff263b726b4e843826f52dc49994d55128447c438f2bcdcbd80" and
            ebu.get("evidence_scope") ==
            "AUTHENTIC_LOCAL_CI421_BUILD_ONLY_NOT_REEXECUTED_CI423",
            "Official EBU evidence incorrectly transferred to a later build")
    require(screened.get("test_state") == "PASS_CI423_BOUNDED_SCREEN_ONLY" and
            screened.get("global_ideal_certificate") == "NOT_EXECUTED" and
            screened.get("release_status") == "NOT_QUALIFIED" and
            screened.get("sample_rates_hz") == list(RATES),
            "Ideal screen misrepresented as global/release certificate")
    require(ev.get("metamorphic_scalar_gain", {}).get("status") ==
            "PASS_CI422_AND_CI423_CTEST", "Gain law status is stale")
    rows = matrix.get("rows")
    require(isinstance(rows, list) and len(rows) == 6,
            "Exact six-rate evidence table required")
    require([x.get("sample_rate_hz") for x in rows] == list(RATES),
            "Per-rate rows must be ordered unique approved Fs")
    for row in rows:
        hz = row["sample_rate_hz"]
        require(row.get("supported_by_frozen_policy") is True and
                row.get("gate_g_per_rate") == "OPEN" and
                row.get("product_qualification_status") == "SUPPORTED_UNQUALIFIED",
                f"{hz}: unsupported status promotion")
        for key, accepted in (
            ("signed_scalar_gain_metamorphism", "PASS_CI422_REPEATED_CI423"),
            ("b2h_high_resolution_ideal_left_hz_tone",
             "PASS_CI423_FINITE_DIAGNOSTIC_ONLY"),
            ("b2h_independent_stereo_discrete_itu_oracle",
             "PASS_CI423_VS_FROZEN_RGS_0_01_DBTP"),
            ("high_resolution_ideal_reconstruction",
             "BOUNDED_SCREEN_CI423_NOT_GLOBAL_CERTIFICATE"),
            ("qualification_provenance_and_product_status",
             "NOT_IMPLEMENTED_PUBLIC_RESULT"),
            ("production_code_blob_identity",
             "PASS_LOCKED_CI421_THROUGH_CI423"),
        ):
            require(row.get(key) == accepted,
                    f"{hz}: {key} not independently reconciled")
        if hz == 48000:
            require(row.get("official_ebu_authenticated_15_23_at_this_rate")
                    == "PASS" and
                    row.get("exact_binary_release_authority") ==
                    "CI421_EBU_PASS__CI423_BINARY_NOT_EBU_REEXECUTED__NO_QUALIFIED",
                    "48k official EBU binary binding invalid")
        else:
            require(row.get("official_ebu_authenticated_15_23_at_this_rate")
                    == "NOT_APPLICABLE_OFFICIAL_48KHZ_ONLY" and
                    row.get("exact_binary_release_authority") ==
                    "NO_OFFICIAL_EBU_AT_OTHER_RATE__MATH_SCREEN_ONLY__NO_QUALIFIED",
                    f"{hz}: invented official EBU validation")
    require("PUBLIC_TRUEPEAKMEASUREMENT_DTO_STATUS_AND_ALGORITHM_POLICY_SCHEMA_FILTER_IDENTITY_B3_SEPARATE"
            in matrix.get("pending_mandatory", []),
            "Required separate B3 status binding forgotten")
    return {"rates": list(RATES), "qualified_rates": 0,
            "b2h_six_rate_ideal_screen": "BOUNDED_CI423_PASS",
            "global_ideal_certificate": "NOT_EXECUTED",
            "authentic_ebu": "ONLY_CI421_BUILD_48KHZ",
            "gate_g": "OPEN"}


def git_capture(repo: Path, *args: str) -> str:
    cp = subprocess.run(["git", *args], cwd=repo, capture_output=True,
                        text=True, check=False, timeout=20)
    require(cp.returncode == 0, f"Git command failed: {args[0]}")
    return cp.stdout.strip()


def check_live_checkout(repo: Path, head: str) -> dict:
    require(bool(re.fullmatch(r"[0-9a-f]{40}", head)), "Invalid HEAD SHA")
    require(git_capture(repo, "rev-parse", "HEAD") == head,
            "Runner is not checked out at the expected exact source HEAD")
    verified = []
    for path, sha in ORIGINAL_BLOB_IDS.items():
        require((repo / path).is_file(), f"Missing production file {path}")
        actual = git_capture(repo, "hash-object", "--", path)
        require(actual == sha, f"Protected production file changed: {path}")
        verified.append(path)
    return {"exact_checkout_sha": head,
            "frozen_production_file_blobs_verified": len(verified),
            "scope": "SOURCE_INTEGRITY_ONLY_NOT_EXE_BINARY_EQUIVALENCE"}


def mutation_selftest(matrix: dict) -> int:
    cases = (
        ("prematureQualified", ("rows", 0, "product_qualification_status"), "QUALIFIED"),
        ("missingSixthRate", ("rows", 5, "sample_rate_hz"), 48000),
        ("falseMainGate", ("acceptance",), "GATE_G_PASS"),
        ("filterHash", ("frozen_identity", "filterAssetSha256"), "0" * 64),
        ("futureEbu", ("evidence", "official_ebu_15_23", "source_sha"), B2H_CI423_SHA),
        ("pretendSameExe", ("evidence", "official_ebu_15_23", "same_binary_as_ci_423"), True),
        ("b2hPretendCertified", ("evidence", "b2h_high_resolution_six_hertz_rates", "global_ideal_certificate"), "COMPLETE"),
        ("dropB2h", ("rows", 1, "b2h_high_resolution_ideal_left_hz_tone"), "TEST_STAGED_AWAITING_CI"),
        ("fakeEbuOn192k", ("rows", 5, "official_ebu_authenticated_15_23_at_this_rate"), "PASS"),
        ("badCI423Exe", ("evidence", "ci_423", "windows_probe_exe_sha256"), B2F_EXE_SHA),
        ("dropStatusOwner", ("pending_mandatory",), []),
        ("wrongCodeBlob", ("production_blob_lock", "analysis/CMakeLists.txt"), "0" * 40),
    )
    for name, path, wrong in cases:
        altered = copy.deepcopy(matrix)
        node = altered
        for k in path[:-1]:
            node = node[k]
        node[path[-1]] = wrong
        try:
            validate_matrix(altered)
        except EvidenceError:
            continue
        raise EvidenceError(f"Mutation incorrectly accepted: {name}")
    return len(cases)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--matrix", type=Path, required=True)
    ap.add_argument("--expected-head", required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    try:
        raw = args.matrix.read_text(encoding="utf-8")
        matrix = json.loads(raw)
        status = validate_matrix(matrix)
        mutated = mutation_selftest(matrix)
        repo = Path(__file__).resolve().parents[3]
        live = check_live_checkout(repo, args.expected_head)
        result = {
            "status": "B2I_EVIDENCE_GUARD_PASS_NOT_RELEASE_QUALIFIED",
            "matrix_schema": SCHEMA,
            "mutation_falsification_cases": mutated,
            "production_gate_g": "NOT_COMPLETE",
            "product_public_status": "SUPPORTED_UNQUALIFIED",
            "evidence": status,
            "live_source": live
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n",
                               encoding="utf-8")
    except (EvidenceError, ValueError, TypeError, OSError,
            subprocess.TimeoutExpired, json.JSONDecodeError) as exc:
        print(json.dumps({"status": "FAIL_CLOSED",
                          "reason": str(exc), "production_gate_g": "NOT_COMPLETE"}),
              file=sys.stderr)
        return 1
    print("B2I_EVIDENCE_GUARD_PASS_6_RATES_12_NEGATIVE_FALSIFICATIONS__GATE_G_OPEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
