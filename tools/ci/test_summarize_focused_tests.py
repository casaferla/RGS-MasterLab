#!/usr/bin/env python3
"""No-GitHub-log, runner-local self-test for bounded Qt smoke diagnostics."""
from pathlib import Path
import subprocess
import sys
import tempfile

SCRIPT = Path(__file__).with_name("summarize_focused_tests.py")


def run_summary(tmp: Path, log: Path) -> str:
    output = tmp / "failure_summary.txt"
    subprocess.run(
        [sys.executable, str(SCRIPT),
         "--ctest-dir", str(tmp / "no_ctest_yet"),
         "--qt-smoke-log", str(log), "--output", str(output)],
        check=True, capture_output=True, text=True)
    data = output.read_bytes()
    assert len(data) <= 12000, "filtered artifact exceeded the 12 KB ceiling"
    return data.decode("utf-8")


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="rgs-qt-brief-") as folder:
        temp = Path(folder)
        qt = temp / "m12b-smoke.txt"
        qt.write_text(
            "PASS   : UiSmoke::first() tests first control\n"
            + ("PASS   : UiSmoke::early() unrelated success\n" * 55)
            + "FAIL!  : UiSmoke::m17Layout() 'lowerSpacer->height() >= 2' returned FALSE.\n"
            + "   Loc: [test_source_metadata_panel.cpp(908)]\n"
            + "QWARN  : UiSmoke::m17Layout() QML warning diagnostic\n"
            + ("PASS   : UiSmoke::late() unrelated success\n" * 70),
            encoding="utf-8",
        )
        result = run_summary(temp, qt)
        assert "FAIL!" in result, "missing failed QtTest assertion"
        assert "Loc:" in result, "missing QtTest failure location"
        assert "QWARN" in result, "missing QML warning"
        assert "unrelated success" not in result, "unfiltered PASS data leaked"
        assert "Local CTest LastTest.log unavailable." in result
        assert "Direct Qt UI smoke — RUNNER FILTERED" in result
        # Reproduce #714's actual symptom: hundreds of early QWARN/INFO
        # matches hid the one useful FAIL! in the old first-35 filter.
        qt.write_text(
            ("QWARN : UiSmoke::noise() window geometry warning\n" * 140)
            + ("INFO : UiSmoke::noise() QVERIFY(control)\n" * 40)
            + "FAIL!  : UiSmoke::m16m17Parity() Compared values are not the same.\n"
            + "   Actual (a): QRectF(1,2,3,4)\n"
            + "   Expected (b): QRectF(5,6,7,8)\n"
            + "   Loc: [test_source_metadata_panel.cpp(3000)]\n",
            encoding="utf-8",
        )
        late = run_summary(temp, qt)
        assert "FAIL!" in late and "m16m17Parity" in late
        assert "Actual" in late and "Expected" in late and "Loc:" in late
        assert late.count("QWARN") == 0, "Warnings outranked fatal assertion"
        missing = run_summary(temp, temp / "not-present.txt")
        assert "Direct Qt UI smoke local log unavailable." in missing
        assert "FAIL!" not in missing
    print("QT_DIRECT_DIAGNOSTIC_FILTER_SELFTEST_PASS")


if __name__ == "__main__":
    main()
