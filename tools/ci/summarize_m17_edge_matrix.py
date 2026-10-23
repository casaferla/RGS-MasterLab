#!/usr/bin/env python3
"""Extract M17 edge reachability from ONE local CTest block, never a raw log."""
import argparse
from pathlib import Path
import re

TARGET = "rgsml_terminal_product_activation_tests"
MARKER = re.compile(r"M17_EDGE_CASE=([A-Z0-9_]{3,75})")
TEST_START = re.compile(r"^\s*\d+/\d+\s+Test:\s+(\S+)")
TIME = re.compile(r"RGS M17 render serial=\d+.*?\bresult=\S+")
FAIL = re.compile(r"FAIL!|\bTest Failed\b|\*\*\*Failed",re.IGNORECASE)


def collect(log: str, sha: str) -> str:
    rows=log.splitlines()
    start=None
    end=None
    for i,line in enumerate(rows):
        m=TEST_START.search(line)
        if m and m.group(1)==TARGET:
            start=i
            continue
        if m and start is not None:
            end=i
            break
    block=rows[start:end] if start is not None else []
    names=list(dict.fromkeys(m.group(1) for line in block
                             for m in MARKER.finditer(line)))
    failed=any(FAIL.search(line) for line in block)
    passed=bool(block) and not failed and any(
        "Test Passed." in line for line in block)
    status="PASS" if passed else "FAIL" if failed else "NOT_CONFIRMED"
    if not block:
        status="NOT_EXECUTED"
    result=[
        "RGS M17 ADVERSARIAL MATRIX — RUNNER FILTERED <= 4 KiB",
        "NO PCM, SOURCE PATH, FULL CTEST, OR GITHUB JOB LOG",
        "SHA="+sha,
        "WINDOWS_PRODUCT_TEST="+status,
        "SCENARIOS_REACHED="+str(len(names)),
    ]
    result.extend("REACHED="+name for name in names[:48])
    timings=[m.group(0)[:180] for line in block
             for m in TIME.finditer(line)]
    result.extend("RENDER_SCALAR="+t for t in timings[-4:])
    fail_rows=[line[:180] for line in block if FAIL.search(line)]
    result.extend("FAILURE="+v for v in fail_rows[:3])
    output="\n".join(result)+"\n"
    return output.encode("utf-8")[:4096].decode("utf-8",errors="ignore")


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--ctest-log",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--sha",required=True)
    args=parser.parse_args()
    if not re.fullmatch(r"[0-9a-fA-F]{40}",args.sha):
        raise SystemExit("M17_EDGE_MATRIX_SHA_INVALID")
    contents=args.ctest_log.read_text(encoding="utf-8",errors="replace") \
        if args.ctest_log.is_file() else ""
    report=collect(contents,args.sha)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(report,encoding="utf-8")
    print("M17_EDGE_MATRIX_FILTERED_BYTES",len(report.encode("utf-8")))


if __name__=="__main__":
    main()
