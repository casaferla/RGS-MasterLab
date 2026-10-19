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
    args = ap.parse_args()
    folder = args.ctest_dir
    failure_file = folder / "LastTestsFailed.log"
    log = folder / "LastTest.log"
    result = [
        "RGS FOCUSED TEST ASSERTIONS — RUNNER FILTERED <= 12 KB",
        "NO whole GitHub job logs or complete CTest logs included.",
    ]
    if failure_file.is_file():
        items = failure_file.read_text(encoding="utf-8", errors="replace").splitlines()
        result.append("Failed tests: " + "; ".join(s[:120] for s in items[:30]))
    else:
        result.append("Failed test list unavailable.")
    if log.is_file():
        rows = log.read_text(encoding="utf-8", errors="replace").splitlines()
        indexes = set()
        for i, line in enumerate(rows):
            if PAT.search(line):
                indexes.update(range(max(0, i - 1), min(len(rows), i + 3)))
        for i in sorted(indexes)[:95]:
            result.append(f"L{i + 1}: {rows[i][:190]}")
        if not indexes:
            result.append("No assertion pattern found in local CTest test log.")
    else:
        result.append("Local CTest LastTest.log unavailable.")
    out = ("\n".join(result) + "\n")[:12000]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(out, encoding="utf-8")
    print("RUNNER_FILTERED_CTEST_BYTES", len(out.encode("utf-8")))


if __name__ == "__main__":
    main()
