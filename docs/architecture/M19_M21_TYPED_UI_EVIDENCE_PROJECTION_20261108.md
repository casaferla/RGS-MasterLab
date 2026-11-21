# M19-M21 typed UI evidence projection — isolated integration preproduction

Date: 2026-11-08. Based on independent backend M21 Draft PR #88 at SHA 1175e7588ca9273625e7c07225e2e3d22453d2e4. The separate GUI Draft PR #91, Windows Qt6 CI #12 at SHA dded4e1c789703e10c90fb7f97df7ab242879e65, completed SUCCESS. No merge.

Source of measurement values is real typed M19 BasicLevelAnalysisResult, real typed M20 TonalStereoAnalysisResult, existing M21 import_real_product_metrics and M21 Gold-minus-Current compose. New m21_ui_readonly_projection_candidate.hpp is a preproduction C++ readonly envelope. It is not registered with Main.qml or any QML context.

An external receipt must contain exact semantic column, generation, realization identity, frame begin/count, sample rate, scope and a frame-domain-checked-at-source assertion. These caller booleans are claims rather than canonical authorization, so production_authorized remains permanently false. Without an exact receipt, each column and dependent M21 delta is masked.

Missing/STALE/INVALID/NOT APPLICABLE/INCOMPATIBLE/LOW SUPPORT remain explicit quality states, with no false zero. One stale PREPARED does not stale independent GOLD. There is no implied time alignment. Malformed CURRENT numeric storage is INVALID. M16 True Peak and TP deltas are withheld even if an upstream mock supplies a value, until genuine canonical M16 publication authority exists.

QtTest exercises a real 48-kHz mono M19/M20 AudioBuffer sine measurement, exact owner match/mismatch, stale source with unaffected independent GOLD, missing receipt, unqualified TP, malformed scalar and candidate-only release status. A focused Windows/Linux CI builds this new QtTest and existing real M21 port regression. It is NOT Full Qualification or Windows physical GUI acceptance.

Next integration seam: adapt candidate evidence to the three independent GUI panels only after canonical PREPARED/GOLD/PROCESSED Product owners and their true epoch/generation/identity gates exist. PR #91 remains separate. M18 A01–A15, real M19–M22 Product lifetime tests, 4K+FHD physical Windows tests and final Full Qualification remain pending.
