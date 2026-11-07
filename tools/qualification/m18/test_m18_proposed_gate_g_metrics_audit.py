#!/usr/bin/env python3
"""Adversarial tests of the independent parser; synthetic passing rows are NOT evidence."""
from __future__ import annotations

from audit_m18_proposed_gate_g_metrics import (
    START, END, BOUNDS, COUNTS, expected_cases, audit_transcript,
)


def make_synthetic() -> str:
    rows = [START]
    for family, seed, channel, case in sorted(expected_cases()):
        expected, half_width = BOUNDS[family]
        rows.append(f"M18_STAT,{family},{seed},{channel},{case},"
                    f"{expected:.15g},{expected:.15g},{half_width:.15g},PASS")
    for name, n in COUNTS.items():
        rows.append(f"M18_STAT_COUNT,{name},{n},{n}")
    rows.append("M18_STAT_SUMMARY,total=87,expected=87,worst_ratio=0")
    rows.append(END)
    return "\n".join(rows) + "\n"


def test() -> None:
    source = make_synthetic()
    assert audit_transcript(source)["count"] == 87
    lines = source.splitlines()

    def reject(v: list[str]) -> None:
        try:
            audit_transcript("\n".join(v) + "\n")
        except ValueError:
            return
        raise AssertionError("adversarial metric transcript accepted")

    missing = lines.copy()
    missing.pop(1)
    reject(missing)
    duplicated = lines.copy()
    duplicated[2] = duplicated[1]
    reject(duplicated)
    wrong_seed = lines.copy()
    wrong_seed[1] = wrong_seed[1].replace("0000000000000000", "FFFFFFFFFFFFFFFE")
    reject(wrong_seed)
    falsely_passed = lines.copy()
    p = falsely_passed[1].split(",")
    p[5] = "123"
    falsely_passed[1] = ",".join(p)
    reject(falsely_passed)
    changed_limit = lines.copy()
    p = changed_limit[1].split(",")
    p[7] = "9"
    changed_limit[1] = ",".join(p)
    reject(changed_limit)
    fake_expectation = lines.copy()
    p = fake_expectation[1].split(",")
    p[6] = "0.5"
    fake_expectation[1] = ",".join(p)
    reject(fake_expectation)
    nan_value = lines.copy()
    p = nan_value[1].split(",")
    p[5] = "nan"
    nan_value[1] = ",".join(p)
    reject(nan_value)
    float_inf = lines.copy()
    p = float_inf[1].split(",")
    p[5] = "inf"
    float_inf[1] = ",".join(p)
    reject(float_inf)
    fake_count = lines.copy()
    fake_count[88] = "M18_STAT_COUNT,dither_mean,7,7"
    reject(fake_count)
    fake_summary = lines.copy()
    fake_summary[94] = "M18_STAT_SUMMARY,total=87,expected=87,worst_ratio=0.75"
    reject(fake_summary)
    wrong_marker = lines.copy()
    wrong_marker[-1] = "M18_GATE_G_QUALIFIED"
    reject(wrong_marker)
    extra_line = lines.copy()
    extra_line.insert(1, "warning: evidence dropped")
    reject(extra_line)
    reordered = lines.copy()
    reordered[88:94] = list(reversed(reordered[88:94]))
    reject(reordered)
    malformed = lines.copy()
    malformed[1] = malformed[1].replace(",PASS", ",FAIL")
    reject(malformed)
    wrong_preamble = lines.copy()
    wrong_preamble[0] = wrong_preamble[0].replace("1048576", "8192")
    reject(wrong_preamble)
    print("M18_PROPOSAL_METRICS_AUDITOR_15_ADVERSARIAL_FALSIFICATIONS_PASS")


if __name__ == "__main__":
    test()