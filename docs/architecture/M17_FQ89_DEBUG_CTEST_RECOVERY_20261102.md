# M17 FQ #89 — Debug CTest install/deploy regression recovery, 2026-11-02

Authority: exact FQ #89 https://github.com/casaferla/RGS-MasterLab/actions/runs/41430414970 on PR #61 `6e5171e0efcbc07b5bace97f3b3975798aea4f57`. Failure is immutable: Windows Debug cold build PASS, Qt UI smoke PASS, Full Debug CTest **123/124** (one failed `install_deploy_smoke`), later Release / Tests-OFF / 16-FIR staged gate NOT RUN. Bounded artifact `RGS-FQ-Debug-Test-Errors-89-6e5171e...` ID 13136850746 reports `tests/cmake/run_install_deploy_smoke.cmake:98 (string): JSON member 'assets 2 private' not found`.

Root: pre-publication installer smoke retained former policy `FINAL_SRC_Q0_PRIVATE_NONRELEASE`, `assets[i].private` and explicit exclusion of Q0 binaries, contradicting PO's now-approved flat 16-file ordinary source bundle. Not a reported M17 DSP mathematics or filter-integrity failure. Focused #966 Windows/Linux PASS on pre-repair SHA but **does not** qualify FQ Debug deploy smoke.

Corrective coherent staging-only macro-lot:
- Replace obsolete inline FIR assertions in `tests/cmake/run_install_deploy_smoke.cmake` with a shared source-driven gate `tests/cmake/verify_installed_fir_assets.cmake`.
- Require actual 16 binary files + one manifest in one `resources/fir` root, 14 `FINAL_SRC_Q0_ORIGINAL` packaged and 2 `PLAYBACK_FROZEN`, complete authorized schema v2, 30 route identities.
- Recompute installed SHA for each binary, verify the two immutable Playback digests and **independently match every 14 Q0 ratio/digest against the frozen C++ header**. Admit only the original two historic DEVICE routes; forbid premature qualification of 28 DEVICE paths; `releaseAuthority` stays false. Fail closed on missing/changed/extra binaries or manifest.
- Add execution of this **same native CMake test** on the genuine checked-in original 14 assets as part of existing `test_approved_16_fir_package.py` Windows and Linux Focused preflight; include tampered-Q0 negative. Prevents future FQ-only accidental contract drift.
- No coefficient, SRC/DSP math, runtime, UI, source ownership, manual FQ, or `main` change.

Before PR promotion verify exact staging-to-PR fast-forward and Single-Writer Epoch, then one CAS update to Draft PR. Obtain new Windows/Linux Focused green on the new SHA. This certifies the shared standalone manifest gate, **NOT** the complete FQ Debug install / Release / physical acceptance. Only Product Owner dispatches a NEW cold Windows FQ on that new exact SHA. Keep M17 and post-M17 CI reset before M18; no main merge.
