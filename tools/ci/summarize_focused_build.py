#!/usr/bin/env python3
"""Emit a small, deterministic local CI diagnostic without downloading job logs."""
from __future__ import annotations
import argparse
import re
from pathlib import Path

LIMIT_INPUT = 16 * 1024 * 1024
LIMIT_OUTPUT = 20 * 1024
PATTERN = re.compile(
    r"(?:\berror\b\s*(?::|\(|[A-Z]?\d{4})|fatal error|\bC\d{4}\b|"
    r"\bLNK\d{4}\b|\bMSB\d{4}\b|undefined reference|unresolved external|"
    r"\bFAILED:|ninja: build stopped|No such file)", re.I
)
CONTEXT = re.compile(r"^\s*(?:\^|\||note:|from )", re.I)

def summarize(path: Path, platform: str) -> str:
    lines = [
        "RGS_FOCUSED_BUILD_DIAGNOSTIC_V1",
        f"platform={platform}",
        "authority=CI_RUN_ARTIFACT_ONLY",
        "full_job_log_download=FORBIDDEN",
    ]
    if not path.is_file():
        return "\n".join(lines + [
            "build_log=NOT_CREATED",
            "check=Inspect failed GitHub job/step metadata; no full log downloads.",
            "",
        ])
    raw = path.open("rb").read(LIMIT_INPUT + 1)
    lines.append(f"input_truncated={len(raw) > LIMIT_INPUT}")
    rows = raw[:LIMIT_INPUT].decode("utf-8", errors="replace").splitlines()
    hits = [i for i, row in enumerate(rows) if PATTERN.search(row)]
    indices: set[int] = set()
    for index in hits[:65]:
        indices.add(index)
        for j in range(index + 1, min(index + 3, len(rows))):
            if CONTEXT.search(rows[j]):
                indices.add(j)
    if not hits:
        indices.update(range(max(0, len(rows) - 15), len(rows)))
    lines.extend((
        f"scanned_lines={len(rows)}",
        f"matched_lines={len(hits)}",
        "---- filtered compiler messages ----",
    ))
    for i in sorted(indices)[:95]:
        lines.append(f"L{i+1}: {rows[i][:310]}")
    if len(hits) > 65:
        lines.append(f"suppressed_matches={len(hits)-65}")
    return ("\n".join(lines) + "\n").encode("utf-8")[:LIMIT_OUTPUT].decode("utf-8", errors="ignore")

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--platform", choices=["linux", "windows"], required=True)
    args = parser.parse_args()
    report = summarize(args.input, args.platform)
    args.output.write_text(report, encoding="utf-8")
    print(f"RGS_FOCUSED_COMPACT_REPORT_WRITTEN={args.output.name} bytes={len(report.encode('utf-8'))}")

if __name__ == "__main__":
    main()
