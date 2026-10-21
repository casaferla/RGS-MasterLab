#!/usr/bin/env python3
"""Bounded failing-CTest assertion extraction ON the CI runner, never raw logs."""
import argparse
from pathlib import Path
import re

PAT = re.compile(
    r"FAIL!|FAILED|Failure|Loc:|Actual|Expected|Test #|"
    r"\*\*\*Failed|SIG[A-Z]+|SegFault|Assertion|error:",
    re.IGNORECASE,
)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ctest-dir", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--qt-smoke-log", type=Path, default=None,
                    help="Runner-local direct QtTest log (never uploaded raw)")
    args = ap.parse_args()
    folder = args.ctest_dir
    failure_file = folder / "LastTestsFailed.log"
    log = folder / "LastTest.log"
    result = [
        "RGS FOCUSED TEST ASSERTIONS — RUNNER FILTERED <= 12 KB",
        "NO whole GitHub job logs or complete CTest logs included.",
    ]
    failed_tests = set()
    if failure_file.is_file():
        items = failure_file.read_text(encoding="utf-8", errors="replace").splitlines()
        result.append("Failed tests: " + "; ".join(s[:120] for s in items[:30]))
        failed_tests = {
            item.split(":", 1)[1].strip()
            for item in items if ":" in item
        }
    else:
        result.append("Failed test list unavailable.")
    if log.is_file():
        rows = log.read_text(encoding="utf-8", errors="replace").splitlines()
        # LastTest.log contains every passing test too. Restrict diagnostics
        # to the actual failed test blocks to avoid exhausting the 12 KB cap
        # on irrelevant QtTest PASS lines.
        test_marker = re.compile(r"^\s*\d+/\d+\s+Test:\s+(\S+)")
        current_test = None
        indexes = set()
        for i, line in enumerate(rows):
            match = test_marker.match(line)
            if match:
                current_test = match.group(1)
            if (not failed_tests or current_test in failed_tests) and PAT.search(line):
                indexes.update(range(max(0, i - 2), min(len(rows), i + 4)))
        # CTest's failed QtTest may have no PAT-matching assertion at all
        # (e.g. an early process abort). Always retain a bounded, *only*
        # failing-test block, not the entire LastTest.log. This uses the
        # local runner copy; external log retrieval is never needed.
        failing_blocks = set()
        if failed_tests:
            boundaries = []
            active_name = None
            active_start = 0
            for i, line in enumerate(rows):
                m = test_marker.match(line)
                if m and m.group(1) != active_name:
                    if active_name is not None:
                        boundaries.append((active_name, active_start, i))
                    active_name = m.group(1)
                    active_start = i
            if active_name is not None:
                boundaries.append((active_name, active_start, len(rows)))
            for name, begin, end in boundaries:
                if name in failed_tests:
                    # At most the first 55 lines per explicitly failed test.
                    failing_blocks.update(range(begin, min(end, begin + 55)))
        for i in sorted(indexes | failing_blocks)[:110]:
            result.append(f"L{i + 1}: {rows[i][:190]}")
        if not indexes and not failing_blocks:
            result.append("No assertion pattern or failed-test block found.")
    else:
        result.append("Local CTest LastTest.log unavailable.")
    # Qt UI smoke runs DIRECTLY before CTest. If it fails, no CTest
    # LastTest.log exists. Extract only relevant assertions ON RUNNER.
    if args.qt_smoke_log is not None:
        result.append("Direct Qt UI smoke — RUNNER FILTERED")
        if args.qt_smoke_log.is_file():
            rows = args.qt_smoke_log.read_text(
                encoding="utf-8", errors="replace").splitlines()
            qt_pattern = re.compile(
                r"FAIL!|QFATAL|QWARN|Loc:|Actual|Expected|"
                r"ASSERT|Exception|Fatal error|Unable to assign|"
                r"ReferenceError|TypeError|QQmlComponent: Component is not ready",
                re.IGNORECASE)
            indexes = set()
            for i, line in enumerate(rows):
                if qt_pattern.search(line):
                    indexes.update(range(i, min(i + 3, len(rows))))
            shown = 0
            for i in sorted(indexes):
                # Context helps locate a failure, but never copy PASS spam.
                if rows[i].lstrip().startswith("PASS"):
                    continue
                result.append(f"QT{i + 1}: {rows[i][:190]}")
                shown += 1
                if shown >= 35:
                    break
            if not indexes:
                result.append("No QtTest failure-pattern lines found.")
        else:
            result.append("Direct Qt UI smoke local log unavailable.")
    # Enforce the 12 KB artifact cap on bytes, not Unicode code points.
    out = ("\n".join(result) + "\n").encode("utf-8")[:12000].decode(
        "utf-8", errors="ignore")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(out, encoding="utf-8")
    print("RUNNER_FILTERED_CTEST_BYTES", len(out.encode("utf-8")))


if __name__ == "__main__":
    main()
