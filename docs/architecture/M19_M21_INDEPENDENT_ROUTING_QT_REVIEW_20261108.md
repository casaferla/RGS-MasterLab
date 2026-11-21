# M19–M21 — independent panel source routing (review-only)

Date: 8 November 2026. Product Owner chose **three independent M19/M20/M21 analysis panels**, with consistent DSP visual controls, M19 large Input Gain typography, and **20–40 meaningful commits per isolated macro-lot, one focused CI per lot**.

## Baseline and dependency

PR #92 backend/test-only typed projection was green on Linux + Windows CI #13, run https://github.com/casaferla/RGS-MasterLab/actions/runs/42464164006 at commit `0807fe7d7ab4908d00ce05761aa4282c99581ded`. PR #91 remains the **separate** QML appearance prototype, not linked to this branch. Nothing is merged into main or the milestone branches.

## New scope

- `IndependentPanelRouting` in `app/src/internal/m21_independent_panels_review_candidate.hpp` provides distinct `Column` selections for M19, M20 and M21. Each panel can independently choose PREPARED, GOLD or PROCESSED in the private **review** layer. No routing selection changes the other two panels, nor overwrites the underlying audio results.
- The existing `ReviewEpochStore` remains the sole writer for typed owner evidence. Its `review_revision` and the new routing-specific `routing_revision` are independent, monotonic signals.
- An additional `capture_test_panels(store,routing,ExplicitTestFixture)` overload reads **one coherent typed comparison** and creates three separately tagged, least-privilege Qt maps. M19 gets only read-only levels + quality, M20 only stereo metrics + quality, and M21 only comparative rows, deltas and per-column lifecycle statuses. Changing M19 selection cannot change M20 source selection.
- `review_frame_current(store,routing,frame)` refuses frames made before *either* owner revision or routing revision changes, rejects a tampered selection/identity, and never grants canonical Product permission.
- Routing is not an alternative way to authorize missing M16 True Peak or generate M20 spectrum. The TP number stays UNAVAILABLE. No PSD/tonal curve is inferred from RMS or width. GOLD comparisons preserve independent scope and do not assume the same sample timeline.
- Old `capture_test_panels(store,fixture)` is retained for compatibility with the previously qualified tests, which still use the single selected column. New API overload is only for three independent panels; it remains test-only and *does not* activate UI components or Main.qml.

## Tests and guardrails

New QtTest cases exercise independent routing, idempotent and invalid selector changes, routing revision validity, the three panels simultaneously selecting different sources, selective source/GOLD/PROCESSED invalidation, independent quality states, unqualified TP, unreadable/invalid owner claims, no-data states, cross-panel field isolation and stale/queued frame rejection. A separate real AudioBuffer case measures a 48-kHz mono sine via genuine M19 and M20 analyzer types, passes the typed result through M21's review-owner gates and Qt map conversion, and checks that changing M19 to an absent GOLD source **cannot** hide the valid M20 PREPARED data.

This remains preproduction design review. External receipt booleans are **not canonical owner proofs**; productionAuthorized stays false. No physical Windows Product UI testing, no canonical M19/M20/M21 ViewModel wiring, no cross-branch GUI PR #91 integration, no main merge, no DSP/FIR/render modification. M18 physical tests A01–A15, M19–M22 integration and qualification, 4K+FHD Windows acceptance and final Full Qualification remain required.

## Focused acceptance

The next dual-OS CI must build `rgsml_m21_ui_readonly_projection_candidate_tests` including all independent routing tests, and the existing real M21 Product-port regression. No one-off CI in the middle of the macro-lot and **no merge without explicit PO authorization**.
