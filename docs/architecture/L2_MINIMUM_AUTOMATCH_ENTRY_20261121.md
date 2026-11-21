# L2 minimal evidence bridge — AutoMatch L3 entry point (2026-11-21)

**Baseline:** L1 PO-accepted `main` merge `9191bbb9` (PR #100). This does not resolve FQ #102 Debug failures and makes no release qualification claim.

## Only indispensable L2 contract

Read the **live, Product-owner rechecked** `M21ProductPublicationInbox::view_current()` on the Qt owner thread. Convert its PREPARED-versus-REFERENCE or PROCESSED-versus-REFERENCE evidence into a small **value-only typed pair** for future AutoMatch L3.

- Carry original candidate/REFERENCE identity, producer generation and independent source sample rate. Do not force 44.1 and 48 kHz to share frame or timebase.
- Reuse the existing unrounded M21 Gold - Current delta calculation, no new FFT/PSD/K-weighting/LUFS/mid-side/TP computation.
- Only genuine `Quality::present` metric pairs are usable future scalar objectives; absent, stale, invalid or low-confidence data are not silently converted into zeros or accepted for optimization.
- Revalidate identity/generation after each DSP candidate render or REFERENCE replacement. Disable stale feedback before any future L3 decision.
- Start with **Full Track** evidence only; Region comparative semantics/normalization and tonal shape or M20 measured band-energy scoring will be defined in L3 if needed. Do not manufacture canonical common-band proofs or arbitrary weights.
- L2 changes neither production GUI, Main QML, private worker publication, DSP Chain, exporter, final limiter, WAV, `.rgsml`, or frozen math.
- One exact-SHA focused Windows/Linux QtTest. No FQ required for this development bridge.

## L3 immediately afterwards

Use an offline, cancellable optimization driver to propose typed DSP-chain parameter settings, render full-program candidates through the existing actual Product chain, recapture genuine M19–M21 evidence, and compare using this receipt. Scoring weights, search order, objective bounds, cache and stopping policy belong to L3, not to a new L2 architecture layer. Initial L3 must preserve chain identity, original REFERENCE, project persistence, source epochs and M17 final ceiling.
