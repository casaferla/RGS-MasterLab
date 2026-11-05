# M17 — Windows FQ #90 cold PASS: verified standalone 16-FIR distribution (2026-11-02)

## Exact immutable qualification
- Repository: `casaferla/RGS-MasterLab`, PR #61 Draft and unmerged.
- PR branch: `experimental/m17-monitoring-pcm-isolation-20261024`.
- FQ source SHA: `5e1604c484913dd08bcba6a08401e180930b4b8e`.
- Focused #967 prior Windows+Linux PASS: https://github.com/casaferla/RGS-MasterLab/actions/runs/41437614078 .
- Manually dispatched Windows cold Full Qualification **#90**: https://github.com/casaferla/RGS-MasterLab/actions/runs/41440702796 ; `completed/success`.
- Canonical VS Debug full build/CTest/Qt smoke PASS; Ninja Release full build/CTest/Qt smoke PASS; parity PASS; isolated Tests-OFF PASS; staged install PASS; verify installed 16 authentic flat FIR binaries PASS; isolated deployed executable smoke PASS; artifact upload PASS.
- FQ #89 red and its obsolete `assets[i].private` deploy smoke failure remain historical, addressed in #967/FQ #90; do not rewrite the earlier FQ outcome.

## Independent actual artifact validation
- GitHub Actions artifact ID `13144651316`: `RGS-MasterLab-Windows-FQ-90-5e1604c484913dd08bcba6a08401e180930b4b8e`; 90,245,631 bytes; reported and independently computed ZIP SHA256 `8bc2acce9c9625fac6001076e4fc9c6c71b99599ae0982afbb312b2db5da5851`.
- Downloaded artifact ZIP was fully tested with Python zipfile; **1407 entries, zero corrupt**, `bin/RGSMasterLab.exe` exists.
- Exactly 16 FIR binaries at ONE `bin/resources/fir`: 14 genuine `RGSMSR1_L*_M*.bin` plus 2 historical `playback_src_kernel_*.bin`; one `fir_manifest.json`; no manual original-14 installation step.
- Manifest schema `rgsml.external-src-fir-assets/2`, `complete16FilePackage=true`, `publicOriginalFirDistributionAuthorized=true`, `releaseAuthority=false`, 16 assets and 30 listed SOURCE/DEVICE nonidentity route identities.
- Independently SHA-256 checked every file against the known frozen C++ original-14 pins and both historic Playback pins: **16/16 EXACT MATCH** (also 16/16 equal manifest). No coefficient regeneration or byte differences.
- No new PR commit, merge or downstream milestone acceptance implied by this document. Any staging-only docs checkpoint is NOT CI/FQ qualified.

## Remaining physical Windows PO gate — M17 not yet accepted
1. Unzip entire artifact on Windows, start `bin/RGSMasterLab.exe`; **no FIR ZIP, Drive download, manual installer, Python or network**.
2. On a known 44.1kHz stereo SOURCE, test SOURCE ordinary playback and M16 LIVE, then M17 Prepare 44.1→48k MASTER and Preview/Listen to Master, full track time/TP and ceiling. Verify SOURCE unchanged and SOURCE↔MASTER handoff with shared Region controls, no separate Master Loop window.
3. Probe historic slow/blocked paths 44.1→88.2k and 192→48k, actual per-rate ETA/Prepare duration, temporary GUI freeze and owner-correct return, including transient hiss/gap. Test physical DEVICE 48k as used previously.
4. Confirm static COMPLETE PRE-LIMITER TP remains correctly source-bound during Preview Prepared without fabricating MASTER LIVE/post-limiter readings; check genuine M17 post-limiter compliance/verification trim if available. Test full-length -12.0dBTP and -3.9dBTP historic ceiling cases as appropriate.
5. Check on actual Windows 150% DPI/dual monitors: one shared visible Region control, LED glow, Parametric EQ Overall ON by default, module UI, Windows icon, no UI overflow. Record remaining failures rather than claiming passed.

DO NOT confuse automatic FQ with physical Device/M17 acoustic acceptance or full 28 pending DEVICE SRC route qualification. No main merge. Only after M17 physical acceptance, PO-approved CI structural reset/audit, then M18 Dither. Maintain Single-Writer Epoch, large coherent batches and one PR CAS per finished batch.
