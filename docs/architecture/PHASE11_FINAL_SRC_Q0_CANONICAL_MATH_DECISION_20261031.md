# Phase11 Final SRC — canonical SRC-Q0 mathematical qualification decision
**Date:** 2026-10-31  
**Decision class:** SOURCE-SCOPED MATHEMATICAL SRC-Q0 — **ACCEPTED WITH EXPLICIT MODEL AND PROVENANCE ASSUMPTIONS**.  
**NOT:** product SRC admission; global Q0–Q6 release; physical Windows FQ; M17/M16 TP qualification; M18 approval; permission to merge `main`.

## Decision and authority
The Product Owner directs completion of Q0 qualification and progression toward the fourth M17 work package on 2026-10-31. On this instruction, the operating reviewer accepts the *mathematical evidence* for the frozen 14-kernel Phase11 Final SRC family **within the exact, explicitly documented scope and trust boundary below**. This is a repository-traceable engineering acceptance of existing, independently audited private evidence, NOT a new cryptographic signer, new execution of every private numerical proof, or new private artifact. This decision does not impersonate a signed physical Product Owner acceptance.

**Q0 mathematical component: ACCEPTED (SCOPED). Product admission: BLOCKED.** All 28 previously unqualified SOURCE→MASTER routes remain denied by the production policy. No edit to the existing 14 FIRs, no publication of protected coefficients, no release manifest or M17 master PCM authorization follows from this acceptance alone.

## Exact frozen evidence identities
- Original 14 genuine binary64 RGSMSR1 FIRs, complete private ZIP SHA256: `94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4`.
- Independent Work Phase2 continuous-band/MPFR ZIP SHA256: `dbc5d6ba083aa54274b041e39885cce04333d767192de7c69e4ba476456fadb6`.
- Independent Work Phase3 guarded review ZIP SHA256: `b6dea3257aa2b4202f297094750e59d0f29d4bd871b106bae092c80417c40251`.
- Secondary original 36-vector Q1 archive SHA256 (not itself Q0 proof): `1c48bf8cd078ab9ad75649303ddc3dae370fd1a16acfea94e3804bd355b09dff`.
- Frozen 14 per-kernel checksums and 28 per-band receipts are bound by the three **private original archive/manifest** roots. Do not publish raw taps/PCM/transcripts or substitute re-generated, unpinned byte streams.

## Evidence accepted, without falsifying the executions
1. Exact six-rate inventory 44.1, 48, 88.2, 96, 176.4, 192 kHz: 36 SOURCE→MASTER paths, six identity, 30 nonidentity, 14 distinct reduced rational FIRs, 28 entire pass/stop frequency bands.
2. Phase2 independently audited 175/175 outer SHA records, 219,128 fully classified continuous-frequency cells and 27 directed MPFR repairs; nine large-ratio direct replays at BOTH 256 and 512 bits on separate MPFR 4.2.2 host reproduced 18 newer bands, plus the restricted exact-byte 147/40→147/80 transfer of two bands. The original eight bands were preserved, and three were additionally independently rerun: **do not claim all eight were newly rerun**.
3. Phase3 independent checked member integrity: 289 entries, 288 member digests; 14 original full asset byte/geometry checks; all 28 recorded full RN-even coefficient receipts at 256/512 bit; all 28 exact rational band domains; 72 Q1/Q2 auxiliary frame/crop receipts. Receipt checking is not the same as re-running the underlying producer in this decision.
4. The principal operator's 2026-10-31 read-only audit independently rederived exact-rational FFT forward-error constants, Taylor/order-20/interval-coverage assumptions and the absolute underflow bounds; inspected actual C++ producer, floating guard, independent coefficient constructor and review evidence; environmental MXCSR/SSE test rerun showed prohibited rounding/floating modes rejected. No contradiction was found in the reviewed scope.
5. Stored continuum certificates rely on Taylor/Lagrange-enclosed rational cell union plus directed MPFR repairs, **not sampled FFT bins**. Response envelope: `|H/L-1| <= 1/30000` in passband and `|H/L| <= 1e-7` in stopband, under the actual frozen specification. Proof must be understood with its stated convention of gain normalization.

## Mathematical trust boundary — part of the acceptance, not optional caveats
This SRC-Q0 conclusion depends on source/asset SHA identity, certified exact rational geometry, authentic directed MPFR/GMP interval implementation, correctly rounded binary64 nearest-even, the reviewed x86-64/Linux guarded SSE/MXCSR setup (no FTZ/DAZ, fast-math or contraction), gradual underflow, and faithful compiler/runtime implementation. It is not a machine-checked proof of the compiler or a broad software/hardware certification. Original first-phase eight-band rerun coverage is as stated; this decision did not regenerate all 14 FIRs anew nor re-execute all 219,128 cells. The independent original Phase2/Phase3 auditable archives and prior replays supply that evidence. Do not transpose the distinct older third-order 44-checkpoint schema into the Phase2/Phase3 proof schema: its 0/28 status does not erase the separate accepted bounded proof.

## Distinct gate outcomes (do not combine)
- **Q0 SRC mathematics: ACCEPTED — SCOPED to the pinned 14 assets and 28 continuous bands under the assumptions above.**
- Product Native Q1/Q2/alias: the original 2026-10-31 private **NONRELEASE** Qt CI #939-built ELF ran genuine 36/36 conversions, native 15/15 alias and five-stage 36/72/15 checks PASS, but this remains a previously documented subset/rehearsal, not release admission of the exact current final M17 product.
- [Focused CI #940](https://github.com/casaferla/RGS-MasterLab/actions/runs/41153372210): Windows and Linux SUCCESS at immutable **PR SHA `46541cde7f2ce6f94ce66f1ee7ff09a5ebaf2cae`**. It did NOT test this staging-only document or sign Q0.
- 28 newer product routes: **BLOCKED** until version-locked genuine FIR manifest, current native C++/Qt master-rate pipeline with 36 cases, M17/M16 at target, master/device independence, full-resource memory/time/cancel/stale, and all implemented-product relevant qualification PASS. The two existing 44.1↔48 and identity candidates retain their preexisting narrow authority; no new support is silently invented.
- M17 Windows full physical FQ and merge of `main`: **OPEN, Product Owner authorization required**. M18 Dither starts only after M17 approval. Nonimplemented Android/Reference Match are deferred to their own later milestones, not current M17 blockers.

## Sequencing decision
Close only the **mathematical Q0 subgate** here. Prepare workstream 4 (M17 UI/readiness/physical Windows FQ preparation) while finishing workstream 3's genuinely qualified 28-route **product admission** under one coherent subsequent code+test CAS/Windows+Linux Focused CI. Do not call the full third workstream closed until the admission gate is actually complete. Any changed coefficient, digest, FIR algorithm, rounding mode, or theorem assumption invalidates this scoped acceptance and requires new review.

## Sources
- [Principal operator formal SRC-Q0 review](PHASE11_FINAL_SRC_Q0_MATH_REVIEW_MAIN_OPERATOR_20261031.md)
- [Phase2 28-band independent NONRELEASE replay audit](PHASE11_FINAL_SRC_PHASE2_28OF28_NONRELEASE_AUDIT_20261030.md)
- [Current Final SRC gate/worklist](PHASE11_FINAL_SRC_Q0_WORKLIST.md)
- [Frozen rate/Prepare contract (private)](https://docs.google.com/document/d/13fIk06CgiPLWcaW3KAZPCUoNFN5ffh6NHlVXK1sfA_8/edit)
- [Dual-archive sync contract](PROJECT_ARCHIVE_SYNC_CONTRACT.md)

**This is an engineering acceptance record, not a detached, cryptographically signed Q0 certificate and not a product release.**
