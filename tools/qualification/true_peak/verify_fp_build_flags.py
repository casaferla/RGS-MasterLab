#!/usr/bin/env python3
"""M16-B2f: fail-closed, source-scoped effective CMake FP build attestation.

Windows: inspect the generated MSBuild .vcxproj ClCompile entries;
Linux Ninja: inspect actual compile_commands.json per translation unit.
This proves configuration/command intent, NOT instruction-level disassembly,
runtime FP control state, or complete Gate G certification.
No source audio is read or written.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import sys
import xml.etree.ElementTree as ET

SOURCES = (
    "true_peak_itu4x_channel_stream.cpp",
    "true_peak_full_resource.cpp",
)


class AttestationError(Exception):
    pass


def normalized_source(path: str) -> str:
    return path.replace("\\", "/").lower()


def source_match(path: str, filename: str) -> bool:
    text = normalized_source(path)
    return text.endswith("/src/" + filename) or text == ("src/" + filename)


def windows(project_path: Path) -> list[dict]:
    if not project_path.is_file():
        raise AttestationError(f"Missing generated MSBuild project: {project_path}")
    tree = ET.parse(project_path)
    units = []
    for element in tree.iter():
        if element.tag.rsplit("}", 1)[-1] != "ClCompile":
            continue
        source = element.attrib.get("Include", "")
        if source and any(source_match(source, f) for f in SOURCES):
            # Source-level metadata, NOT merely unrelated/global target flags.
            children = {}
            for child in element.iter():
                key = child.tag.rsplit("}", 1)[-1]
                if key in ("AdditionalOptions", "FloatingPointModel"):
                    children.setdefault(key, []).append(child.text or "")
            units.append((source, children))
    if len(units) != len(SOURCES):
        raise AttestationError("Expected exactly two scoped True-Peak ClCompile entries")
    result = []
    for filename in SOURCES:
        matches = [(src, props) for src, props in units
                   if source_match(src, filename)]
        if len(matches) != 1:
            raise AttestationError(f"{filename}: missing/duplicated compiler entry")
        src, props = matches[0]
        options = " ".join(props.get("AdditionalOptions", []))
        model = " ".join(props.get("FloatingPointModel", []))
        explicit = bool(re.search(r"(?i)(?<!\w)/fp:strict(?![\w-])", options))
        mapped = bool(re.search(r"(?i)\bstrict\b", model))
        if not (explicit or mapped):
            raise AttestationError(f"{filename}: MSVC strict FP option not present")
        if re.search(r"(?i)/fp:(?:fast|contract)\b", options):
            raise AttestationError(f"{filename}: contradictory MSVC FP option")
        result.append({"source": filename, "policy": "/fp:strict",
                       "effective_project_file_sha256":
                           hashlib.sha256(project_path.read_bytes()).hexdigest()})
    return result


def linux(commands_path: Path) -> list[dict]:
    if not commands_path.is_file():
        raise AttestationError(f"Missing generated compile database: {commands_path}")
    records = json.loads(commands_path.read_text(encoding="utf-8"))
    result = []
    for filename in SOURCES:
        found = [rec for rec in records
                 if source_match(rec.get("file", ""), filename)]
        if len(found) != 1:
            raise AttestationError(f"{filename}: missing/duplicate compile command")
        item = found[0]
        args = item.get("arguments")
        if args is None:
            args = shlex.split(item.get("command", ""))
        if "-fno-fast-math" not in args or "-ffp-contract=off" not in args:
            raise AttestationError(f"{filename}: strict GNU/Clang flags not effective")
        prohibited = ("-ffast-math", "-Ofast", "-fassociative-math",
                      "-funsafe-math-optimizations", "-ffinite-math-only",
                      "-fno-signed-zeros", "-ffp-contract=fast", "-ffp-contract=on")
        if any(arg in prohibited for arg in args):
            raise AttestationError(f"{filename}: contradictory fast-math option")
        effective = " ".join(args)
        result.append({
            "source": filename,
            "policy": "-fno-fast-math -ffp-contract=off",
            "effective_compile_command_sha256":
                hashlib.sha256(effective.encode("utf-8")).hexdigest()
        })
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=("windows", "linux"))
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        if not re.fullmatch(r"[0-9a-f]{40}", args.source_sha):
            raise AttestationError("Expected exact source SHA40")
        if not re.fullmatch(r"[0-9]+", args.run_id):
            raise AttestationError("Missing numeric workflow run ID")
        if args.platform == "windows":
            checks = windows(args.build_dir / "analysis" / "rgsml_analysis.vcxproj")
        else:
            checks = linux(args.build_dir / "compile_commands.json")
        if len(checks) != 2:
            raise AttestationError("Incomplete True-Peak source-specific check")
        report = {
            "status": "B2F_FP_BUILD_CONFIGURATION_ATTESTATION_PASS",
            "production_gate_g": "NOT_COMPLETE",
            "scope": "TWO_PRODUCTION_TRUE_PEAK_CPP_OBJECTS_ONLY",
            "platform": args.platform,
            "source_commit_sha": args.source_sha,
            "workflow_run_id": args.run_id,
            "checked_sources": checks,
            "runtime_fma_disassembly": "NOT_EXECUTED",
            "full_strict_fp_environment": "NOT_CERTIFIED"
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n",
                               encoding="utf-8")
    except (AttestationError, OSError, ValueError, TypeError,
            ET.ParseError, json.JSONDecodeError) as exc:
        print(json.dumps({"status": "FAIL_CLOSED", "reason": str(exc),
                          "production_gate_g": "NOT_COMPLETE"}), file=sys.stderr)
        return 1
    print("B2F_FP_BUILD_CONFIGURATION_ATTESTATION_PASS", args.platform,
          "2_OF_2__GATE_G_OPEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
