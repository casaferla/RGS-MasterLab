#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[3]
qml=(root/"ui/qml/M21MetricsComparisonPreview.qml").read_text()
for forbidden in ("Math.log","Math.pow","parseFloat(","levelMatch","targetProximity",
                  "matchAmount","referenceMatch","reAnalyze"):
    assert forbidden not in qml,forbidden
assert 'property var presentation' in qml and 'modelData.preparedDelta' in qml
assert 'no time alignment' in qml and 'NOT' not in qml[:0]
for p in ("ui/qml/Main.qml","ui/CMakeLists.txt","app/CMakeLists.txt"):
    assert "M21MetricsComparisonPreview" not in (root/p).read_text(),p
print("M21_QML_NONACTIVATION_PASS_NOT_GATE_M")
