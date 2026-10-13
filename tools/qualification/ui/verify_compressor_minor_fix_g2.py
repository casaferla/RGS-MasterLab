#!/usr/bin/env python3
"""Minor Fix G2 static wiring check; NOT Windows physical Gate-M acceptance."""
from pathlib import Path
import sys


def main() -> None:
    root = Path(__file__).resolve().parents[3]
    view = (root / "app/src/compressor_view_model.cpp").read_text(encoding="utf-8")
    app = (root / "app/src/main.cpp").read_text(encoding="utf-8")
    resolver = (root / "render/src/audible_compressor_telemetry_resolver.cpp").read_text(encoding="utf-8")
    qml = (root / "ui/qml/CompressorEditor.qml").read_text(encoding="utf-8")

    if "compressorViewModel.resetTelemetryForNewSource();" not in app:
        raise ValueError("new-source commit must discard foreign GR observations")
    if "if (!projectSession.is_committing_project_open())" not in app:
        raise ValueError("do not conflate project DSP settings with GR reset")
    if "void CompressorViewModel::resetTelemetryForNewSource()" not in view:
        raise ValueError("separate source observation reset missing")
    if "resolver_.reset();" not in view:
        raise ValueError("reset must clear old sidecar identities and history")
    if "ModuleExecutionDisposition::BYPASS_IDENTITY" not in app:
        raise ValueError("published accepted bypass signature check missing")
    if "processed_realization_id()" not in app or "processed_realization_snapshot()" not in app:
        raise ValueError("bypass must bind published render to accepted identity")
    if "resolver_.register_sidecar(std::move(bypassEvidence), true)" not in view:
        raise ValueError("bypass evidence not registered by accepted identity")
    if "history_.num_lanes() != lanes.size()" not in resolver:
        raise ValueError("2-to-1 or 1-to-2 audible-lane correction missing")
    if "status_ != AudibleTelemetryStatus::BYPASS" not in resolver:
        raise ValueError("PAUSED must not brighten bypass GR observations")
    if "status_ != AudibleTelemetryStatus::UNAVAILABLE" not in resolver:
        raise ValueError("PAUSED must not rebrand unavailable evidence as valid")
    if "onNumLanesChanged: requestPaint()" not in qml:
        raise ValueError("topology change must invalidate GR canvas")
    if "if (!isUnavailable && buckets" not in qml:
        raise ValueError("unknown provenance must suppress old GR drawing")
    if "l === 0 ? \"L\" : \"R\"" not in qml:
        raise ValueError("dual-mono lane labels changed unexpectedly")
    if "onLiveStateChanged: requestPaint()" not in qml:
        raise ValueError("bypass/pause state change must repaint")
    print("MINOR_FIX_G2_PROVENANCE_STATIC_GUARD_PASS__PHYSICAL_GATE_M_OPEN")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as exc:
        print("MINOR_FIX_G2_PROVENANCE_STATIC_GUARD_FAIL: " + str(exc),
              file=sys.stderr)
        sys.exit(1)
