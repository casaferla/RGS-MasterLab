"""M20 isolated UI contract: does not activate Main / change real UI authoring."""
from pathlib import Path
root=Path(__file__).resolve().parents[3]
qml=(root/"ui/qml/M20BasicTonalStereoPreview.qml").read_text(encoding="utf-8")
main=(root/"ui/qml/Main.qml").read_text(encoding="utf-8")
cmake=(root/"ui/CMakeLists.txt").read_text(encoding="utf-8")
assert "M20BasicTonalStereoPreview" not in main
assert "M20BasicTonalStereoPreview" not in cmake
assert "m20StaticSpectrumCanvas" in qml
assert "m20FractionalBandsCanvas" in qml
assert "m20CorrelationRecentCanvas" in qml
assert "m20NoDataPanel" in qml and "m20DataPanel" in qml
assert "NOT LIVE B3" in qml
assert "ISOLATED PREVIEW" in qml
for forbidden in ("compute_onesided_psd","log10(","gate_complete_400ms",
                  "integrated_lufs","validBandEnergy","FeatureDefinitionRegistry",
                  "rgsml.stereo.width_ratio"):
    assert forbidden not in qml,forbidden
print("M20_QML_PREVIEW_STATIC_NONINTEGRATION_GUARD_PASS_NOT_UI_FQ")
