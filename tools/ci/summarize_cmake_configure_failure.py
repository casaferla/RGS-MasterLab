#!/usr/bin/env python3
"""Produce only bounded configuration FAILURES, never upload entire CI logs."""
from __future__ import annotations

import argparse
from pathlib import Path
import re

PATTERN = re.compile(
    r"CMake Error|EXTERNAL_FIR|Error:|Traceback|ModuleNotFoundError|"
    r"No such file|not found|Unknown CMake|Cannot find|"
    r"SyntaxError|FAILED|FATAL_ERROR|invalid|"
    r"refused|SHA256_MISMATCH",
    re.IGNORECASE,
)


def summarize(source: Path) -> str:
    header = "RGSML_BOUNDED_CMAKE_CONFIGURE_ERROR_V1\n"
    if not source.is_file():
        return header + "CONFIGURATION_LOG_NOT_CREATED\n"
    lines = source.read_text(encoding="utf-8", errors="replace").splitlines()
    selected = set()
    for i, line in enumerate(lines):
        if PATTERN.search(line):
            for j in range(max(0, i - 1), min(len(lines), i + 3)):
                selected.add(j)
        if len(selected) >= 60:
            break
    if not selected:
        selected.update(range(max(0, len(lines) - 18), len(lines)))
    safe = [f"{i + 1}: {lines[i][:220]}" for i in sorted(selected)[:60]]
    body = "\n".join(safe)[:5900]
    return header + f"total_lines={len(lines)}\n" + body + "\n"


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args()
    args.output.write_text(summarize(args.input), encoding="utf-8")


if __name__ == "__main__":
    main()
