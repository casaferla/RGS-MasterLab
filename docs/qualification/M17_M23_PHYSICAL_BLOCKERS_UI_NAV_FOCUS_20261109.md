# M17–M23 Windows PO physical gate — repair macro-lot A (nonrelease)

Date: 9 November 2026. **Writer only; no product certification.**

## Verified baseline and hard boundaries

Official cold Windows FQ #99 PASS at
`5e4871d5849e4121b0612c2142d1e051d6c9a3d2` on Draft PR #99. Physical
Windows testing found **B-01 M18 verified Master Export permanently disabled
after actual verified M17 Prepare**, and **B-02 missing M19/M20/M21 Product
panels**. M17 offline GR HISTORY and audio transport GUI have positive
observations but the cumulative physical gate M17–M23 is not closed.

The original ZIP is FROZEN. Work only on
`staging/m23-failure-matrix-writer-no-ci-20261108`; promote via exact CAS
to `staging/m23-failure-matrix-focused-ci-20261108` for one focused CI.
Never merge to main without PO approval.

## PO UI decision: order and Master Audition retention

The ONE existing DSP/navigation selector must present:

1. INPUT GAIN, PARAMETRIC EQ, COMPRESSOR, STEREO/M-S;
2. M16 True Peak Meter;
3. M17 True Peak Limiter;
4. **M19 Basic Level Analyzer** (observer, no sonic DSP);
5. **M20 Basic Tonal & Stereo** (observer, no sonic DSP);
6. **M21 Metrics / Delta** (observer, no sonic DSP);
7. **Master Audition** last, **temporarily retained**.

No ProcessingChain/ModuleRegistry or `.rgsml` serialisation change.
Master Audition still owns DEVICE/offline-no-device monitoring affordances.
Do not delete until essential functions have been migrated and qualified.

## Status of this macro-lot — architecture scaffold, NOT M19–21 signoff

This branch adds three stable read-only adapters and an `AnalysisStatusEditor`
to the real Main dock/float host. Product `m21ProductStatusViewModel`
provides owner-aware status and **three independent source routes**.
Until authenticated per-cell M19–M21 numerical Product authorities and Phase-6
gates are actually installed, the editor must say data UNAVAILABLE: **no
pretend spectrum, LUFS, Width, RMS or Reference delta**. The previews in
`ui/qml/M19*M21*Preview.qml` are deliberately fixture-only and may NEVER be
rendered in production with `fixtureMode:true`.

Read-only source selection does not imply a published result. The full
M19/M20/M21 features and true spectra remain an **OPEN blocker** on this
writer until wired with canonical Product receipts and physically qualified.
A green CI for this step is merely a safe GUI integration checkpoint.

## M18 B-01: diagnostic first, no safety bypass

The actual disabled menu is bound to
`M18ProductExportViewModel::can_export()`,
`M18PrivateProductExportAction::can_begin_verified_export()`, and
`M18ProductCompleteBinding::can_offer_verified_master()`.
It checks a current Source/Project/chain/terminal/ceiling epoch, M16
final_check and exact stage ownership. A visible M17 READY does not automatically
satisfy this. **Do not force canExport=true or create synthetic epochs.**

The branch adds a read-only `exportReadinessReason` with fixed sanitized
messages identifying the first unsuccessful guard and exposes it in the
bottom status row while an actual Source exists and Export is disabled.
This does NOT change the existing canonical `can_offer` implementation.
The next physical tester will report the exact reason on a new packaged
artifact; the responsible owner mismatch then gets a minimal safe fix
with regression. The physical A03/A04 export remains BLOCKED until real WAV
production, 64-char SHA receipt, post-decoded True Peak and independent
readback pass.

Other UI defect fixed on writer: **Ctrl+O** now has a real guarded
`StandardKey.Open` Shortcut rather than only File menu painted text.

## Focused validation and next work

The focused CI builds the **actual app**, the native adapter tests
(which assert M17→M19→M20→M21→Master Audition and no audio mutation)
and Qt/QML smoke on Windows and Linux, besides existing 20-category M23
nonrelease matrix and prior FQ #98 render boundary regression. Inspect
bounded failure evidence only. If red, fix on writer and rerun focused.
If green, continue with authentic M19/20/21 data publication and the B-01
Export Product owner repair, then one official cold Full FQ on new integrated
exact SHA and physical retest of affected gates. A CI green is NOT a PO
physical pass or permission to merge main.
