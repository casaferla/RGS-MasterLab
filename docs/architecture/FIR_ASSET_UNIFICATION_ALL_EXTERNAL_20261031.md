# RGS MasterLab — PO decision: all SRC FIR assets external in installed product

**Product Owner decision: 2026-10-31.** **Status: APPROVED FOR DEFERRED IMPLEMENTATION; NOT IMPLEMENTED, NOT CI-QUALIFIED.**
**Priority:** protect current M17 Focused #942 double-green and physical Windows FQ. Do not reopen M17 DSP or delay M18 with an installer refactor. Schedule this as **one coherent packaging/loader macro-lot** before normal public distribution, with only one Focused Windows+Linux CI per substantive completed candidate, plus recovery CI only if red. No standalone documentation CI.

## Uniform architecture — 16 external, install-managed FIR assets

- **All 16 SRC FIR assets external to executable code:** two existing frozen playback 44.1→48 and 48→44.1 kHz FIRs (currently constexpr arrays in `audio/src/playback_sample_rate_adapter.cpp` sourced from `internal/playback_src_kernel_44100_48000.inc` and `internal/playback_src_kernel_48000_44100.inc`), plus the **14 source-pinned Final SRC FIR** binaries kept privately in RGS ContentBox pending release authorization.
- **One installed resources root** and authenticated, versioned manifest (e.g. `resources/fir/playback/` and `resources/fir/final_src/`). Shared **distribution, manifest, error and verification** conventions; separate product functions, numerical contracts and release gates. Do not invent one DSP SRC engine or silently equate Final SRC with playback DEVICE SRC.
- Distribute these assets alongside `RGSMasterLab.exe` through the Windows installer or complete portable ZIP; users must **not** have to download from Drive, run Python, configure environment variables, modify Windows system directories, whitelist antivirus, or have internet access.
- **No silent unsafe fallback:** fail closed on missing, wrong-version, corrupt, stale or substituted coefficients. Manifest/digests must be independently pinned by trusted signed product code/package (not merely self-reported by the loose asset files). Windows installation and update provide ordinary ACLs, code/installer signing and normal antivirus/SmartScreen review. No runtime remote code or auto-fetch.
- For the two legacy FIRs, extract the exact original `uint64_t` binary64 coefficient bits into portable binary files and verify **bit-for-bit kernel identity and current frozen SHA-256/oracles**, including frame mapping, ratio, channel layout, boundary reflection, output and device listening. Never regenerate, change tap order or DSP policy. Resource loading and immutable lifetime must be established **before** any audio callback; no disk I/O, allocations or hash verification in the realtime callback.
- For the 14 Final SRC FIRs, retain the exact original Q0 SHA-pinned `RGSMSR1` files and existing full-pack/per-ratio fail-closed checks, not the separate Phase2/Phase3 receipts or regenerated approximations. Keep them private and **outside PUBLIC GitHub/CI/release artifacts until Product Owner authorizes distribution**; the current explicit local `RGSML_FINAL_SRC_Q0_PRIVATE_DIR` workflow remains mandatory for FQ before then.
- Windows default installed location under `Program Files/RGS MasterLab/resources/fir`, or application-relative resources in a portable distribution. No `System32` or Windows driver/service. Shared loader/platform-neutral code must not break future Android adaptation.

## One coherent implementation macro-lot — avoid CI proliferation

1. Model exact 16-entry asset inventory/version + immutable read-only loader with fail-closed checks and trusted digest pins; retain two distinct numerical paths and frozen filter math.
2. Migrate the existing two constexpr playback arrays into external binary assets **without numerical changes**, and unify the Final SRC pack's installed resource discovery. Preserve optional private FQ override until release authorization; never expose private taps to public CI.
3. Update CMake/Windows install/deployment ZIP + local run-from-build fixtures + portable-path tests, together with relevant documentation and a tiny distributable test fixture where safe.
4. Add one consolidated native regression suite: legacy bitwise playback equivalence and corruption/missing/tamper rejection; Q0 original 14 full SHA/geometry, 36 SOURCE→MASTER + OWNER/M17/M16 unchanged when source-pinned assets are authorized; offline independent MASTER→DEVICE monitoring (experimental playback caveats preserved); packaging relocation, no network and app-startup integrity check.
5. Promote this **one complete code+tests+packaging candidate** from no-CI staging to Draft PR with exact SHA CAS, then one Windows+Linux Focused CI. If a job fails, diagnose bounded failure-only evidence, fix coherently, requalify in one recovery CI. Recheck final Windows installer/portable artifact and physical smoke separately under PO FQ.

## Frozen quantities and qualification boundaries

- Original 14 `RGSMSR1` files: **1,430,028** float64 coefficient words, **11,440,952 bytes** (10.91 MiB including 52-byte headers per file), derived from current immutable public manifest *without publishing private coefficients*.
- Two existing built-in playback FIR arrays: **30,721 + 28,225** 64-bit words; original static coefficients approx. 0.45 MiB. Together 16 raw FIR data approx. 11.92 MB decimal, excluding manifest/container overhead.
- **Current product**: 14 original binaries are locally provisioned as private FQ fixtures; no release authorization is implied by SRC-Q0 scoped mathematical acceptance. Existing 44.1↔48 conversion remains inside binary and production behavior MUST NOT change until external migration is Focused/FQ-qualified.
- **Latest verified Focused on M17 candidate**: [#942](https://github.com/casaferla/RGS-MasterLab/actions/runs/41196958089) Windows PASS + Linux PASS at Draft PR #61 `f446baf91a1b4bd8abed7bd3df63f6060be1fc9b`. This *document-only staging decision* is **not** tested by #942. No FQ manually dispatched here, no main merge, no M18 yet.

**Continuity:** GitHub operational checkpoint first; mirror this decision and #942 exact SHA/result in RGS ContentBox linked handoff under the dual-archive contract.
