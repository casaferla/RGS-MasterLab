#!/usr/bin/env python3
"""Synthetic falsification for bounded M17 edge extraction."""
import importlib.util
from pathlib import Path

source=Path(__file__).with_name("summarize_m17_edge_matrix.py")
spec=importlib.util.spec_from_file_location("m17_edge_summary",source)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
SHA="a"*40
sample=(
    "1/2 Test: unrelated_test\n"
    "QINFO: PCM_PRIVATE_NEVER_EXPORT raw-file-location\n"
    "2/2 Test: rgsml_terminal_product_activation_tests\n"
    "QINFO: M17_EDGE_CASE=READY_BYPASS_RETAINS_EXACT_PCM\n"
    "QINFO: M17_EDGE_CASE=REPEATED_AB_NEW_DEVICE_IDS_NO_RERENDER\n"
    "QINFO: RGS M17 render serial=2 queue_ms=2 render_ms=18 result=accepted_for_gate\n"
    "2/2 Test: other_test\n"
    "Test Passed.\n"
)
report=module.collect(sample,SHA)
assert "WINDOWS_PRODUCT_TEST=NOT_CONFIRMED" in report
assert "SCENARIOS_REACHED=2" in report
assert "READY_BYPASS_RETAINS_EXACT_PCM" in report
assert "raw-file-location" not in report
assert "RENDER_SCALAR=" in report
passed=sample.replace("2/2 Test: other_test","Test Passed.\n2/2 Test: other_test")
assert "WINDOWS_PRODUCT_TEST=PASS" in module.collect(passed,SHA)
failed=passed.replace("Test Passed.","FAIL!  : Terminal test line 17")
assert "WINDOWS_PRODUCT_TEST=FAIL" in module.collect(failed,SHA)
assert "WINDOWS_PRODUCT_TEST=NOT_EXECUTED" in module.collect("",SHA)
assert len(module.collect(sample*200,SHA).encode("utf-8"))<=4096
print("M17_EDGE_MATRIX_BOUND_FILTER_SELFTEST_PASS")
