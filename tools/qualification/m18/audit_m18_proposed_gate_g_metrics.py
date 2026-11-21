#!/usr/bin/env python3
"""Independent exact-shape audit of M18's 87-statistic PROPOSED Gate G transcript.

NO claim of formal mathematical/product approval. Parses every metric, not
merely the candidate executable's summary/pass marker; rejects altered limits,
missing/duplicate cases, fabricated counts, nonfinite observations, and FAIL.
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

SEEDS = ("0000000000000000", "0123456789ABCDEF", "FFFFFFFFFFFFFFFF")
CHANNELS = (0, 1)
LAGS = (1, 7, 31, 1024)
BOUNDS = {
    "dither_mean": (0.0, 0.00247713140466),
    "dither_second": (1.0 / 6.0, 0.00120285030056),
    "lag_normalized": (0.0, 0.00611085463606),
    "stereo_normalized": (0.0, 0.00611085463606),
    "error_mean": (0.0, 0.00303719895126),
    "error_second": (0.25, 0.00263880970848),
}
COUNTS = {
    "dither_mean": 6,
    "dither_second": 6,
    "lag_normalized": 24,
    "stereo_normalized": 3,
    "error_mean": 24,
    "error_second": 24,
}
START = "M18_GATE_G_PROPOSAL_BEGIN N=1048576 seeds=3 channels=2 primary=87"
END = "M18_GATE_G_PROPOSAL_PASS_NOT_FORMAL_GATE_G"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def expected_cases() -> set[tuple[str, str, int, int]]:
    items: set[tuple[str, str, int, int]] = set()
    for seed in SEEDS:
        for ch in CHANNELS:
            items.add(("dither_mean", seed, ch, 0))
            items.add(("dither_second", seed, ch, 0))
            for lag in LAGS:
                items.add(("lag_normalized", seed, ch, lag))
            for case in range(4):
                items.add(("error_mean", seed, ch, case))
                items.add(("error_second", seed, ch, case))
        items.add(("stereo_normalized", seed, 0, 0))
    require(len(items) == 87, "internal expected-case shape invalid")
    return items


def close_to(observed: float, expected: float, *, abs_tol: float = 2e-13) -> bool:
    return math.isfinite(observed) and math.isclose(
        observed, expected, rel_tol=0.0, abs_tol=abs_tol
    )


def audit_transcript(text: str) -> dict[str, float | int]:
    lines = text.splitlines()
    require(len(lines) == 96, f"expected exactly 96 evidence lines, got {len(lines)}")
    require(lines[0] == START, "incorrect full-proposal test preamble")
    require(lines[-1] == END, "missing full-proposal success marker")
    observed_cases: set[tuple[str, str, int, int]] = set()
    ratios = []
    for line in lines[1:88]:
        cells = line.split(",")
        require(len(cells) == 9 and cells[0] == "M18_STAT", "malformed metric row")
        _, family, seed, channel, case, value_str, expectation, width_str, passed = cells
        require(family in BOUNDS, "unknown metric family")
        require(seed in SEEDS, "unapproved seed in metric")
        try:
            ch, case_id = int(channel), int(case)
            observed, want, width = map(float, (value_str, expectation, width_str))
        except (ValueError, OverflowError) as exc:
            raise ValueError("non-numeric metric") from exc
        require(channel == str(ch) and case == str(case_id), "noncanonical metric address")
        key = (family, seed, ch, case_id)
        require(key in expected_cases(), f"unexpected metric coordinate {key}")
        require(key not in observed_cases, "duplicate metric coordinate")
        observed_cases.add(key)
        frozen_expectation, frozen_width = BOUNDS[family]
        require(close_to(want, frozen_expectation), "metric expectation changed")
        require(close_to(width, frozen_width, abs_tol=1e-16), "metric confidence bound changed")
        require(math.isfinite(observed), "nonfinite metric")
        ratio = abs(observed - frozen_expectation) / frozen_width
        require(math.isfinite(ratio) and ratio <= 1.0, "metric outside proposed bounds")
        require(passed == "PASS", "metric not marked PASS")
        ratios.append(ratio)
    require(observed_cases == expected_cases(), "some primary cases missing")
    for index, (family, count) in enumerate(COUNTS.items(), start=88):
        require(lines[index] == f"M18_STAT_COUNT,{family},{count},{count}",
                f"mismatched family count {family}")
    summary = lines[94]
    prefix = "M18_STAT_SUMMARY,total=87,expected=87,worst_ratio="
    require(summary.startswith(prefix), "mismatched summary count")
    try:
        reported = float(summary[len(prefix):])
    except ValueError as exc:
        raise ValueError("unparseable worst ratio") from exc
    calculated = max(ratios)
    # 12 significant figures in native emitted summary; metric rows have 15.
    # The two independent render roundings permit a few 1e-11 in ratio.
    require(math.isfinite(reported) and abs(reported - calculated) < 1e-10,
            "worst ratio not substantiated by 87 metric rows")
    return {"count": 87, "worst_ratio": calculated}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    result = audit_transcript(args.path.read_text(encoding="utf-8-sig"))
    print(f"M18_INDEPENDENT_87_PROPOSED_METRICS_AUDIT_PASS_NOT_FORMAL_GATE_G,"
          f"count={result['count']},worst_ratio={result['worst_ratio']:.12g}")


if __name__ == "__main__":
    main()