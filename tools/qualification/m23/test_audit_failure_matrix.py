#!/usr/bin/env python3
"""Adversarial tests of the M23 no-release reconciliation and JUnit binder."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest
from xml.etree.ElementTree import Element, SubElement, ElementTree

from audit_failure_matrix import (
    M23ContractError, WINDOWS_ONLY, build_report, parse_junit, read_manifest,
    run, safe_path,
)

ROOT = Path(__file__).resolve().parents[3]
MATRIX = Path(__file__).with_name("failure_matrix_reconciliation.json")


class M23EvidenceSelfTests(unittest.TestCase):
    def test_canonical_repo_contains_all_twenty_concrete_categories(self):
        manifest = read_manifest(ROOT, MATRIX)
        self.assertEqual(len(manifest["cases"]), 20)
        self.assertFalse(manifest["m23_closed"])
        self.assertFalse(manifest["physical_gate_m"])
        self.assertFalse(manifest["release_qualified"])
        counts = {k: sum(c["static_coverage"] == k for c in manifest["cases"])
                  for k in ("existing", "partial", "gap")}
        self.assertEqual(counts, {"existing": 18, "partial": 2, "gap": 0})

    def test_fake_release_and_manual_verdict_never_accepted(self):
        baseline = json.loads(MATRIX.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "tampered.json"
            for key in (
                "m23_closed", "physical_gate_m",
                "product_admission_authorized", "release_qualified",
            ):
                modified = copy.deepcopy(baseline)
                modified[key] = True
                target.write_text(json.dumps(modified), encoding="utf-8")
                with self.subTest(key=key), self.assertRaises(M23ContractError):
                    read_manifest(ROOT, target)

    def test_missing_duplicated_or_reordered_category_fails_closed(self):
        baseline = json.loads(MATRIX.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "tampered.json"
            variants = []
            fewer = copy.deepcopy(baseline)
            fewer["cases"].pop()
            variants.append(fewer)
            swap = copy.deepcopy(baseline)
            swap["cases"][1], swap["cases"][2] = (
                swap["cases"][2], swap["cases"][1]
            )
            variants.append(swap)
            inject = copy.deepcopy(baseline)
            inject["cases"][6]["static_coverage"] = "gap"
            variants.append(inject)
            for i, modified in enumerate(variants):
                target.write_text(json.dumps(modified), encoding="utf-8")
                with self.subTest(variant=i), self.assertRaises(M23ContractError):
                    read_manifest(ROOT, target)

    def test_unsafe_or_nonexistent_source_file_never_accepted(self):
        with self.assertRaises(M23ContractError):
            safe_path(ROOT, "../outside")
        with self.assertRaises(M23ContractError):
            safe_path(ROOT, "/etc/passwd")
        with self.assertRaises(M23ContractError):
            safe_path(ROOT, "tests/not-a-real-m23-source-file.cpp")

    def make_junit(self, target: Path, names: list[str],
                   failed: str | None = None,
                   skipped: str | None = None) -> None:
        root = Element("testsuites")
        suite = SubElement(root, "testsuite", {"name": "ctest"})
        for name in names:
            testcase = SubElement(suite, "testcase", {"name": name})
            if name == failed:
                SubElement(testcase, "failure", {"message": "fault"})
            elif name == skipped:
                SubElement(testcase, "skipped", {"message": "not executed"})
        ElementTree(root).write(target, encoding="utf-8")

    def test_static_source_without_junit_never_claims_exec_pass(self):
        report = run(ROOT, MATRIX, [], "windows", False)
        self.assertFalse(report["m23_closed"])
        self.assertFalse(report["release_qualified"])
        self.assertFalse(report["full_fq_passed"])
        self.assertEqual(len(report["unexecuted_existing_categories"]), 18)
        self.assertEqual(len(report["unresolved_categories"]), 2)
        with self.assertRaises(M23ContractError):
            run(ROOT, MATRIX, [], "windows", True)

    def test_real_ctest_names_but_unresolved_gaps_remain_not_qualified(self):
        manifest = read_manifest(ROOT, MATRIX)
        names = sorted(set(
            c["ctest"] for c in manifest["cases"] if c["ctest"]
        ))
        with tempfile.TemporaryDirectory() as temp:
            junit = Path(temp)/"junit.xml"
            self.make_junit(junit, names)
            evidence = run(ROOT, MATRIX, [junit], "windows", True)
            self.assertEqual(evidence["unexecuted_existing_categories"], [])
            self.assertEqual(len(evidence["unresolved_categories"]), 2)
            self.assertEqual(len(evidence["passed_existing_suites"]),len(names))
            self.assertFalse(evidence["m23_closed"])
            self.assertFalse(evidence["frozen_m23_i_p_m_passed"])
            self.assertFalse(evidence["physical_gate_m"])
            self.assertFalse(evidence["release_qualified"])
            self.assertTrue(all(not c["qualified"] for c in evidence["cases"]))

    def test_linux_may_not_mint_windows_fault_coverage(self):
        manifest = read_manifest(ROOT, MATRIX)
        suite_names = sorted(set(
            c["ctest"] for c in manifest["cases"]
            if c["ctest"] and c["ctest"] not in WINDOWS_ONLY
        ))
        with tempfile.TemporaryDirectory() as temp:
            junit = Path(temp)/"linux-junit.xml"
            self.make_junit(junit, suite_names)
            result = run(ROOT, MATRIX, [junit], "linux", True)
            self.assertTrue(any(
                c["runtime"] == "NOT_RUN_PLATFORM_WINDOWS_ONLY"
                for c in result["cases"]
            ))
            self.assertFalse(result["release_qualified"])

    def test_failed_and_skipped_runtime_never_count_as_executed_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            junit = Path(temp)/"notallpassed.xml"
            names = ["rgsml_project_session_tests",
                     "rgsml_render_preview_tests"]
            self.make_junit(junit,names,failed=names[0],skipped=names[1])
            cases = parse_junit([junit])
            self.assertEqual(cases[names[0]],"FAIL")
            self.assertEqual(cases[names[1]],"SKIP")
            report = build_report(read_manifest(ROOT,MATRIX),cases,"windows")
            self.assertIn("render_window",report["unexecuted_existing_categories"])
            self.assertIn("project_open_atomicity",
                          report["unexecuted_existing_categories"])
            self.assertFalse(report["m23_closed"])

    def test_duplicate_conflicting_suite_verdict_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            first = Path(temp)/"pass.xml"
            second = Path(temp)/"fail.xml"
            self.make_junit(first,["rgsml_project_session_tests"])
            self.make_junit(second,["rgsml_project_session_tests"],
                            failed="rgsml_project_session_tests")
            with self.assertRaises(M23ContractError):
                parse_junit([first,second])

    def test_malformed_or_empty_junit_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            empty = Path(temp)/"empty.xml"
            empty.write_text("<testsuite/>",encoding="utf-8")
            with self.assertRaises(M23ContractError):
                parse_junit([empty])
            empty.write_text("<broken",encoding="utf-8")
            with self.assertRaises(M23ContractError):
                parse_junit([empty])


if __name__ == "__main__":
    unittest.main(verbosity=2)
