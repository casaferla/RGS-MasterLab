"""Mandatory M20 private U/G readiness honesty preflight, not official U or G."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[3]
manifest=json.loads((root/"tools/qualification/m20/m20_u_g_gate_readiness_candidate.json").read_text(encoding="utf-8"))
assert manifest["private_preproduction_only"] is True
assert manifest["official_U_qualified"] is False
assert manifest["official_G_qualified"] is False
deps={r["id"]:r for r in manifest["required_before_official_U_and_G"]}
assert len(deps)==len(manifest["required_before_official_U_and_G"])==12
assert all(r["state"].startswith("BLOCKED_") for r in deps.values())
assert "M19_RETAINED_IDENTITIES" in deps and "VALID_BAND_LOW_SUPPORT" in deps
assert "PHASE6_REGISTRY" in deps and "DISPLAY_COMPOSITION_PO" in deps
assert "M17_PHYSICAL_PO" in deps and "M18_INTEGRATED_QUALIFIED" in deps
assert "M19_INTEGRATED_QUALIFIED" in deps and "FINAL_PO_MERGE" in deps
scope=(root/"analysis/src/internal/m20_m19_phase6_gate_port.hpp").read_text()
assert "producer_disclosure_missing" in scope
assert "M19RetainedDisclosureCandidate" in scope
assert "M19GatePortState::ready" in scope
cpp=(root/"analysis/src/internal/m20_bounded_spectrum_candidate.hpp").read_text()
assert "Radix2Fft" in cpp and "HannWindow::create" in cpp
assert "compute_onesided_psd" in cpp
assert "std::span<const std::uint8_t> mask_" in cpp
assert "frame*geometry_.hop" in cpp
assert "frames_.empty()" not in cpp
qml=(root/"ui/qml/M20BasicTonalStereoPreview.qml").read_text()
main=(root/"ui/qml/Main.qml").read_text()
module=(root/"ui/CMakeLists.txt").read_text()
assert "M20BasicTonalStereoPreview" not in main+module
assert "ISOLATED PREVIEW" in qml
print("M20_CANDIDATE_U_G_READINESS_FAIL_CLOSED_PREFLIGHT_PASS_NOT_QUALIFIED")
