# Experimental SkinPack write ledger (append-only)

## 2026-10-16 — Delegation activation
- **PO approval:** recorded in RGS ContentBox mandate `RGS_SKINPACK_PO_ISOLATED_EXPERIMENTAL_BRANCH_DELEGATION_20261016`.
- **Operational ACK:** `OPERATIVE_ACK=ACCEPTED`, `OWNERSHIP_CONFLICT=NONE`, `SKINPACK_FIRST_EXPERIMENTAL_WRITE=PERMITTED_WITHIN_PO_SCOPE`.
- **Branch:** `experimental/skinpack-r1-r4-unqualified-20261016`.
- **Initial parent commit:** `c30b4149437b6404892973ea543baf8773c0060f`.
- **First checkpoint intent:** register only design contract and the existing R2 exact-slot map, without touching application integration or shared QML controls.
- **Evidence:** R2.5 local Python static validation reported 19/19 PASS; **no native C++/Qt compilation or CI/FQ**.
- **First commit SHA:** referenced by branch history; append in the next checkpoint's ledger (a commit cannot embed its own SHA).
- **Restricted actions:** no merge, no PR, no production CI, no modifications to branches or files outside the PO allow-list.

## 2026-10-16 — R1 native loader reference sources
- **Previous checkpoint commit:** `b26392eab2106587d1bdeeaf5b8daf70719f4f5a`.
- **Parent SHA for this write:** `b26392eab2106587d1bdeeaf5b8daf70719f4f5a`.
- **Scope:** `app/src/skinpack_loader.hpp` and `app/src/skinpack_loader.cpp` (new files only); append ledger.
- **Origin:** offline R1 Integration Kit v0.1; preliminary native ZIP/JSON/PNG loader and `image://` store/provider.
- **Build status:** NOT REGISTERED IN CMAKE; NOT COMPILED; NO Qt GUI TEST; NO CI/FQ.
- **Mandatory gaps:** native ZIP fuzz/local-vs-central consistency, strict schema validation, decoder/size and platform tests, UI regression and integration GO.
- **Resulting commit SHA:** retrievable from Git history; to be cross-referenced at the next checkpoint.

## 2026-10-16 — Native C++ ZIP container preflight checkpoint
- Parent SHA: `1a5060592f8eb6ec36ec8c4a51e7b2d9501eb2bd`.
- Source allow-list: `app/src/skinpack_zip_preflight.hpp`, `app/src/skinpack_zip_preflight.cpp`, `app/src/skinpack_loader.cpp`, `skinpack/tests/native_zip_preflight_cli.cpp`, `skinpack/EXPERIMENTAL_WRITE_LEDGER.md`.
- Preflight is invoked before the minizip-based loader. Reject malformed ZIP metadata, duplicate/case aliases, symlinks, unknown paths, unsupported ZIP features, overlapping/gapped local records, unsafe expansion ratios and lengths.
- Local harness: GCC C++20 `-Wall -Wextra -Werror -pedantic -O2`; 18/18 local tests (valid original Kintsugi R1/R2 and negative/malformed ZIP scenarios). Python test generator is preserved in the deferred laboratory kit and should be versioned alongside the harness at the next checkpoint.
- **Not tested:** Qt6 application linking, QML UI, Windows or Android, CI/FQ, full security fuzz, actual inflate CRC validation against minizip. No production integration.
- Commit SHA: infer from branch history; next checkpoint shall cross-reference.

## 2026-10-16 — Reproducible native ZIP fixture tests
- Prior native-preflight commit: `f568cd17a35352d0db000b44114057b578685b33`.
- Publish `skinpack/tests/test_zip_preflight.py` under allow-list.
- Command: `python -m unittest discover -s skinpack/tests -v` (requires g++/C++20; real fixture optional via `SKINPACK_REAL_FIXTURE`).
- On lab GCC: 18 tests PASS including genuine Kintsugi .rgsskin. Native C++ source blob on branch checked byte-identical to locally compiled source.
- Not a Qt build; loader is not yet registered in app/CMakeLists.txt and UI is unchanged. No CI/FQ, no PR.

## 2026-10-16 — R1 app/CMake/QML experimental wiring
- Parent SHA: `000075f89e3664013cfba8eb7ac32d517bf50c7a`. All changes **only on SkinPack branch** in previously authorized minimal integration paths `app/CMakeLists.txt`, `app/src/main.cpp`, `ui/qml/Main.qml` plus this ledger.
- CMake registers SkinPack loader and C++20 preflight and reuses `MINIZIP::minizip`; main.cpp registers context object and image provider; Main.qml adds Import/Original menu, error dialog, and image **behind** the Transport controls without changing controls or DSP.
- Source-first anchor guards: one match per intended edit. Inspected existing Transport geometry/handlers remain byte-identical in the QML source apart from inserted decorations.
- This is an **UNCOMPILED experimental source wiring**. Local toolchain lacks Qt 6 SDK. Native standalone preflight passed 18/18; this does NOT qualify Qt CMake/QML integration, contrast, mouse/focus or Windows/Android GUI.
- No PR, CI, FQ, main or M16 branch write; production integration remains NOT AUTHORIZED.

## 2026-10-16 — Qt probe attempt and archive compression reconciliation
- **Parent:** `7fb6daa7e15897830e45ba1b41268aa3b948e3fe` on exclusive SkinPack branch.
- **Verified blocker:** host `third_party/CMakeLists.txt` sets `MZ_ZLIB OFF`; original Kintsugi R1/R2 use ZIP method 8 DEFLATE. Qt SDK absent in the local container; package-manager network/DNS unavailable. GitHub connector has no workflow_dispatch action; no production workflow invoked.
- **Resolution (experiment only):** preflight now accepts STORE (method 0) only, aligned with minizip-ng build; old Deflate packages remain archived and are rejected by R1. Added STORE repack tool under `tools/skinpack/`; original asset bytes/asset hashes unchanged.
- **Proof:** regenerated R1/R2 STORE fixtures tested locally with v0.1 structural validator and C++ preflight PASS; GCC C++20 compile `-Wall -Wextra -Werror -pedantic -O2` and 18/18 adversarial unit tests PASS using R2 STORE fixture.
- **Qt complete build:** NOT EXECUTED; Windows `tools/skinpack/qt_r1_probe.ps1` script prepared for isolated Release app build and --rgsml-deploy-smoke. No GitHub Actions, PR, merge, CI/FQ performed.
- **Critical follow-up:** Native minizip R1 loader, QML startup, Windows visual/focus controls, pointer semantics, rollback and malicious archive fuzz not yet qualified. User-facing original compressed prototype cannot be installed with current experimental host until normalized.

## 2026-10-16 — Candidate experimental CI workflow, pre-ACK staging
- Parent SHA: `f4017ce415a7a0d87f065bfc1b1fa42f916e8012`.
- PO extension written to MASTER PLAN: https://docs.google.com/document/d/12A4l9xAVCCZfZ3Ui7mksRvqhj3BozdE9uuzQi-yhRWQ/edit
- New files staged under existing allow-list: `tools/skinpack/ci/skinpack-experimental.yml`, `tools/skinpack/ci/README.md`, `tools/skinpack/ci/check_experimental_workflow.py`.
- `.github/workflows/**` remains untouched until **new** ACK from operative M16 writer.
- Trigger candidate: exact branch and one path `skinpack/ci/run-request.txt`, not ordinary pushes. Marker NOT created.
- Qt build NOT RUN, actions NOT STARTED. Candidate static checks only; no runtime qualification.

## 2026-10-16 — First Windows Qt failure, GitHub direct-file repair
- CI run #38957388108: **RED** in experimental `app/src/skinpack_loader.cpp`, MSVC C2061 caused by missing `mz_strm.h` include before `mz_zip_rw.h`; checkout, Qt installation and other targets advanced successfully.
- Precise source-only repair committed via the supported GitHub **update_file** operation: `d90c31a044f57bed99a90f9b48a12742465e72c3`.
- Only `app/src/skinpack_loader.cpp` was changed by this repair; correct header order is `mz.h`, `mz_strm.h`, `mz_zip.h`, `mz_zip_rw.h`.
- Branch and `main` were checked independently. This is not proof of compile success until the next isolated Windows Qt CI completes.
- No need for manual user file editing. No PR, merge, production CI or M16 modification.

## 2026-10-16 — Native R1 package import/rollback/reset functional smoke candidate
- Last green Windows build/smoke CI: `38960457938`, `7ebadc31d863f350f93df78041e73f39aa434312`, SUCCESS (all steps). This did **not** load an archive.
- New diagnostics added only in existing allow-list: `tools/skinpack/generate_r1_smoke_fixture.py` (commit `0d39e66981fef01445df0dae081f861532187ae3`), `app/src/main.cpp` (commits `dbc23397b30de42feb4712bc86d0e44fc96c52e3`, `71924e79701ca19ba04887b92fbdb3a5781a19e7`), and `tools/skinpack/qt_r1_probe.ps1` (`0726baca3dc60f14cff7b860e130c1314e6ea13d`).
- Fixture format: STORE-only synthetic v0.1 archive, deterministic PNG 4x4, declarative manifest and SHA-256, deliberate EOCD corruption for rejection path. No outside QML or shaders.
- Qt diagnostic flag tests actual native minizip unpack, JSON validation, PNG decode and QML transport texture activation, corrupt ZIP rejection without active-skin replacement, and reset to RGS Original.
- Laboratory proof: generated fixture ZIP passed Python `ZipFile.testzip()`; native C++20 zip preflight CLI (GCC warnings-as-errors) ACCEPTED the valid synthetic fixture and REJECTED corrupted EOCD. Full Qt import/rollback runtime not yet run at the time of this note.
- No production M16, `main` (`46e917bd2adc8ff13e83c0802895afda7daf6b2a`), audio math, third_party, CI workflow, shared QML components, PR, FQ or merge changed. Work remains preliminary/unqualified.
- Next run must execute the new diagnostic; even success does NOT qualify pixels, accessibility, realistic Kintsugi asset rendering, security fuzz or audio.

## 2026-10-16 — R1 native rejection/rollback coverage extension
- CI #38963165042 **SUCCESS**, exact source commit `0fa939eb0d158b4229bc7f4d7a634af76df41599`. Native Windows Qt loader import, PNG decode/QML activation, corrupt EOCD reject and Original reset PASS.
- Additional seven deterministic hostile package fixture variants added under previously allowed `tools/skinpack/generate_r1_smoke_fixture.py`, commit `c71ec4013fd40553428fe19e55e31ba369af1284`: wrong manifest asset SHA, disallowed semanticColors override, unsupported hostContract, executable code trust flag, correctly hashed invalid PNG, ZIP DEFLATE, unsafe parent directory path.
- Existing `app/src/main.cpp` diagnostic extended (commit `687153b156d3b9258d2fb7d84435bea0e4bfbe15`) to reject all seven without mutating the accepted skin, decoded texture, revision, name or QML visibility; reset then must restore Original. No normal UI handler or DSP change.
- Locally compiled C++20 **STORE-only** ZIP preflight: negative fixtures with valid ZIP envelopes accepted for subsequent schema/hash/PNG checks; Deflate and unsafe-path fixtures rejected at preflight. This is not Qt native evidence for the extended cases; Windows CI still required.
- Retain scope as experimental. No PR/merge/FQ, no M16/main/workflow edits, main stays at `46e917bd2adc8ff13e83c0802895afda7daf6b2a`.
- Source changes grouped before next CI; no run active when ledger recorded.

## 2026-10-16 — CI #38967059687 GREEN / minimum-window SkinPack invariant follow-up
- Verified GitHub CI `38967059687`, head `07d56665603e887f01c873fcf57a64ef9a4ef222`, conclusion SUCCESS. Log explicitly confirms `SKINPACK_R1_SEVEN_NEGATIVE_ARCHIVE_CASES_PASS` plus `SKINPACK_R1_NATIVE_IMPORT_ROLLBACK_RESET_SMOKE_PASS`.
- Next consolidated proof (not yet run): `app/src/main.cpp` diagnostic adds Qt Quick item coordinate+size snapshot for `controlStrip`, `controlStripContent`, `rgsSkinTransportTexture`, `stopButton`, `playPauseButton`, `transportTimeModule`, `waveformZoomOutButton`. It enters the canonical 1184x688 minimum window before capture and checks unchanged geometry after valid skin import, after all seven rejections, and after Original reset.
- Commits: `740bf2d10778a9e7671870ba61bb8e5f54d442c9`, `22f4e6cf84f49158eb0a2a14445c5f63ef8c12e7`. No QML component edits and no production logic changes; diagnostic CLI path only.
- This tests bounding rectangles, not actual painted pixels, focus order, hit-testing, DPI or audio behavior. Real Kintsugi STORE pack has passed local archive/manifest checks but full realistic-art import is still unqualified.
- Remains exclusively on experimental branch. No PR, FQ, merge, main/M16 or workflow edits.

## 2026-10-16 — CI #38969317665 GREEN / real-artifact static inspection
- Windows Qt R1 run `38969317665`, exact commit `62c775dec8b0a8a7b9c141605361cef0c49f2097`: GitHub COMPLETED/SUCCESS. Job log confirms `SKINPACK_R1_CANONICAL_TRANSPORT_GEOMETRY_PASS`, `SKINPACK_R1_SEVEN_NEGATIVE_ARCHIVE_CASES_PASS`, and `SKINPACK_R1_NATIVE_IMPORT_ROLLBACK_RESET_SMOKE_PASS`.
- This proves unchanged canonical Qt Quick transport-item bounds at **1184 × 688** before/after the synthetic test import, seven rejected archives, and reset, not user-visible pixel fidelity, focus/keyboard/hit testing, different DPI, or actual Kintsugi import.
- Local original-artifact check of `Kintsugi_Prestige_R1_STORE_Experimental.rgsskin`: ZIP method STORE, 5 entries, 1,554,781 bytes; archive SHA-256 `761c602972ecc09c83007745f52616677b542bae4f1db263b9a88516548db6c0`. The reference structural validator and local GCC C++20 ZIP preflight accept it; manifest index verifies 76,864-byte transport PNG SHA-256 `b047ad76b34aaa75a57e092e65743ad8d9eb265b96ae7e1896bc0ab0c9fed1c5`. Profiles: surface, controls, effects; default surface.
- Local `Kintsugi_Prestige_R2_STORE_Experimental.rgsskin`: ZIP method STORE, 5 entries, 1,554,760 bytes; archive SHA-256 `7b300476f12abc3c43dd3bcfc4784ddce7484a4fc5b13ba76134466457a60b30`. Structural validation and local native ZIP preflight PASS; identical marble PNG asset hash.
- **Critical distinction:** Neither actual Kintsugi .rgsskin file is included in GitHub's branch checkout/Windows CI test. Their *full Qt import and rendered pixels* remain NOT TESTED. Do not label the prototype as qualified/installable in production. Binary artifacts should not be copied into this Git branch or fetched from an unpinned external URL casually.
- Next gate: controlled Windows native import of real Kintsugi R1 STORE archive + visual/contrast/window interaction review, with reproducible artifact delivery and no production integration. No additional CI is necessary merely to repeat synthetic results.
- Production branch, M16 writer, frozen math and CI/FQ authority remain entirely separate.

## 2026-10-17 — Kintsugi Prestige R1 Art Family v0.3 prepared OFFLINE; native Qt still pending
- User authorized testing the enlarged artistic texture family. Artwork variants derived from the high-resolution RGS Kintsugi white marble/gold reference; original v0.1 and v0.2 packs remain unchanged.
- New experimental family assembled in isolated work container as `Kintsugi_Prestige_R1_ArtFamily_v0.3_STORE_UNQUALIFIED.rgsskin`: 9,394,629 bytes; SHA-256 `61186747376723442adaa18c3ba4d81e1939eed0475b565c49c48374bcd4b55a`, ZIP STORE, eight entries, verified integrity of all indexed PNGs.
- `transport-marble.png`: **2172 x 96**, SHA-256 `7c751a6f803ba05758dbe9260b7d9689b3ba1d489637caa06d4eaa24692b0b22`. The width allows a full 1184/1440 window Transport strip without repetition; pixel L/R seam equality exact.
- `panel-marble-soft.png`: **1254 x 1254**, SHA-256 `553bef35f29e1a0fabc5db9c0bf32eaf08a381f6e27caed32661faf38f8f5d6d`. L/R and T/B edges pixel identical.
- `accent-marble-01.png`: **1254 x 1254**, SHA-256 `8df0bffffb4a680c0da84ba0b22273611cbf0ab98b5062cca312118833d2f8f9`. L/R and T/B edges pixel identical.
- `accent-marble-02.png`: **1254 x 1254**, SHA-256 `102893286138eb9548ac7b8ae63b77ee77653cdc1aac3437a7f7cced5c1e6c45`. L/R and T/B edges pixel identical.
- Deterministic Python/Pillow seam cross-fade applied to **new copies**; unmodified generated source PNGs retained locally. R1 active `surface.transport.background` points to new transport texture; extra panel/accent materials are bundled and integrity indexed but **NOT WIRED** to other Qt panels. R2 controls/effects remain preliminary and unaltered.
- Local reference `validate_skinpack_v0_1.py`: `STRUCTURAL PASS` (8 entries; manifest/schema/assets). Compiled standalone GCC C++20 native ZIP preflight against this exact package: `ACCEPT 8 entries`. Reproduced SHA-256 above. **No Windows Qt native import of this specific new binary, no actual Kintsugi pixel screenshot, no DPI/contrast/keyboard qualification**.
- Current GitHub connector writes UTF-8 source/document files but cannot directly transmit the locally built .rgsskin/PNG binary into this branch; avoid pretending CI has tested these bytes. No new CI requested. Next meaningful gate: authorized binary transfer into isolated Windows test environment (by supported method), SHA-256 check, real Qt import, screenshot/contrast review and rollback at canonical window; only then consider visual GO.
- This ledger-only write does not trigger experimental CI; `main`, M16 single-writer, frozen DSP, workflow and all production areas untouched. No PR, merge or FQ.

## 2026-10-17 — Exact v0.3 artwork upload verified / native Windows art smoke staged
- Product Owner manually uploaded `skinpack/Kintsugi_Prestige_R1_ArtFamily_v0.3_STORE_UNQUALIFIED.rgsskin` to isolated SkinPack branch, upload HEAD `e6c041078e118abed1f55604e14a8b5fd8b91bea`. Exact GitHub contents metadata: 9,394,629 bytes, Git blob SHA-1 `af5233e946256c95094c4555ccc65972f25930f2`, independently equal to local Git blob digest. Local file SHA-256 `61186747376723442adaa18c3ba4d81e1939eed0475b565c49c48374bcd4b55a`. Only the exact SHA-pinned binary must be tested; no external URL/art regeneration.
- Added experimental CLI `--rgsml-skinpack-art-smoke=` in existing `app/src/main.cpp` at commit `ca27fc87edc14d7985c5b5f4e7a3fa01b35cb7a3`. It tests actual Kintsugi native Qt import, 2172x96 texture/image provider, QML Image Ready, captures application screenshots at 1184x688 and 1440x900, and verifies reset to Original. Normal application UI/DSP paths remain unchanged.
- Created experimental log image evidence encoder `tools/skinpack/ci/emit_art_screenshot.py`, commit `0201dd5483b786e5dcae840ffc68a0ade9e65eb5`. It emits chunked PNG bytes and SHA from disposable screenshots to the job log for review, without modifying shared workflow or uploading user files.
- Updated `tools/skinpack/qt_r1_probe.ps1`, commit `e7eae4096b0ac483db88a1f03dba52b731a3d3c0`: exact size+SHA-256 check prior to native art smoke, unchanged synthetic negative+reset suite, both screen captures must exist. No GitHub workflow modification. This is a test candidate, **not yet a successful Windows run or visual qualification**.
- No changes to `main` (`46e917bd2adc8ff13e83c0802895afda7daf6b2a`), production M16 branches, protected math/DSP, CI/FQ productive jobs, PR or merge; SkinPack remains UNQUALIFIED.

## 2026-10-17 — CI #38976015244 RED / diagnosis: Qt GUI-process synchronization & Hyper-V display
- Exact GitHub run `38976015244` on `1b4c8b5b6c55ff2214c7bed03adfc5a31a6ad11a`: COMPLETED/FAILURE. Build, Qt deploy, reference synthetic suite, and real Kintsugi v0.3 SHA-256 verification PASS.
- Native stdout confirms `SKINPACK_R1_REAL_ART_NATIVE_IMPORT_1184_PASS` (real archive accepted, 2172 x 96 texture decoded, QML image READY, 1184 x 688 screenshot saved). Asynchronous PowerShell check erroneously started before GUI subprocess had completed; screenshot falsely reported missing. GUI stdout arrived later.
- Hyper-V monitor rejected `QQuickWindow::resize(1440,900)`: Qt diagnostic reports it instead constrained the client to `1184 x 749`; thus original art-smoke 1440 target failed on runner capability, not Kintsugi pixels.
- Repairs restricted to experimental test code: `app/src/main.cpp` commit `0762fa9e5537a16d04ab24e51c3c0851518c788f` clamps expanded viewport to available desktop and prints ACTUAL dimensions; `tools/skinpack/qt_r1_probe.ps1` commit `c14dd4b09f510cb824c95f6f9dd9c6011c39b180` waits for GUI exe completion via `Start-Process -Wait -PassThru` for deploy, synthetic and art tests; `tools/skinpack/ci/emit_art_screenshot.py` commit `82b911809e110bd42dc9d5638eb47e142d16705c` properly labels second screenshot `expanded` instead of falsely declaring 1440.
- This does not expand SkinPack v0.3 artwork itself or touch product UI logic, M16, DSP, math, `main`, workflow or PR. The physical 1440 x 900 screen capture and all color/contrast/artistic judgements remain **PENDING** until evidence reviewed.
- Next isolated CI is required to verify corrected test synchronization and native screenshots; do not claim GREEN before result.

## 2026-10-17 — CI #38977048137 GREEN / actual Qt screenshot visual assessment
- Isolated Qt 6.8.3 Windows run #38977048137: COMPLETED/SUCCESS, commit `c236e3109205e93a037b469de5f841b3feb7772a`. GitHub logs read (job #117228907011) and screenshots decoded for human image inspection.
- Logs PASS: seven rejected .rgsskin variants, canonical transport geometry, native import/reset synthetic suite; the exact uploaded real v0.3 pack passes SHA-256; `SKINPACK_R1_REAL_ART_NATIVE_IMPORT_1184_PASS` and `SKINPACK_R1_REAL_ART_NATIVE_IMPORT_AND_RESET_PASS`.
- Two genuine `QQuickWindow::grabWindow` PNG captures exist in logs. `1184`: 191106 bytes, SHA-256 `94ec03fa93fcecc53baced050dc49eb8d90a83dfb6f4cf189f875e580416d311`. `expanded`: **identical 191106 bytes and SHA-256**. Log explicitly confirms `SKINPACK_R1_REAL_ART_EXPANDED_CAPTURE_ACTUAL=1184x688; 1440x900 PHYSICAL_REVIEW_PENDING`; thus only one effective geometry was tested (not two). Both images were visually inspected from encoded runner evidence.
- Visual evidence: real cream/white marble and gold veins render in the Transport band without an obvious tiling seam across the visible width (single 2172px source wider than strip). Main time display remains readable on dark housing; dark RGS MasterLab body unaffected; no observed layout migration in screenshot. At this no-WAV/no-playback snapshot, transport and waveform zoom controls are intentionally disabled and appear low-emphasis, so enabled-state visual contrast, hit targets and accessibility remain **UNVERIFIED**.
- IMPORTANT SCOPE: CI success proves native art load and screenshot capture, not full artistic qualification. Visual review provisional for the **static default transport surface only**, at 1184x688 and single runner DPI. 1440x900 physical viewport, scaling/DPI, enabled/focus/hover contrast, real WAV playback state, and visual review of panel/accent extension remain HOLD; no production integration.
- Only source-of-truth ledger text changed after CI. `main` remains `46e917bd2adc8ff13e83c0802895afda7daf6b2a`, M16 unaffected; no new CI started, no workflow changes, no PR/merge/FQ.

## 2026-10-17 — R1 genuine-source enabled controls + 3 art-family panel previews (batched candidate; Qt qualification PENDING)
- After exact real v0.3 archive native import, screenshot and reset **CI #38977048137 PASS**, user authorized progression to enabled transport appearance and selective optional marbles (no more redundant screenshot-export-only runs).
- Changes remain on the isolated experimental branch and within initially delegated SkinPack/QML presentation allow-list. NO existing shared control component modified; `main`, M16 writer, math/DSP and production workflows untouched.
- `app/src/skinpack_loader.hpp` commit `115f63e957b0d68adc8ac5cb9a5cc75ed399dd3e` adds optional panel/accent swatch slots; `app/src/skinpack_loader.cpp` commit `68b79742e238214efec6dbb830d73f137ab7eecb` atomically validates/decodes additional PNGs only if their materials are in the approved artifact. Optional swatches are absent for the earlier synthetic/legacy packs. Reset clears all decoded images. Provider accepts only local named swatches and no executable pack content.
- `ui/qml/Main.qml` commit `cbdc7cd3e7d9ccccef7e1e149d2675ad0dfc8754`: opt-in diagnostic-only `skinPackLabVariant` default=0. Variant1 previews Panel Soft behind Audition Region controls at opacity 0.13; variant2/3 preview Accent01/02 behind source waveform content at opacity 0.10. No layout geometry change, no shared control or semantic-color change. The normal user-facing app stays identical until a future separately authorized integration.
- `tools/skinpack/ci/generate_visual_source.py` commit `589e0c982ff91fca7f85969b48790e45f11010ca` generates standalone 48kHz stereo PCM16 two-second WAV locally for CI. `app/src/main.cpp` commits `f3253f8b6335af9d10d5ae6c25b64da1fdd8ce03` and `a8a7079c7797723d52cfcc432628596ec131e4ec` test real `sourceSelection.selectSource` and `playbackTransport.can_play()` before capturing genuinely enabled Play/Stop; NO mock-enabled override and NO playback. Captures 4 opt-in states: enabled, panel, accent01, accent02 after default 1184 real-art baseline; checks optional image provider READY, proper teardown to RGS Original.
- `tools/skinpack/qt_r1_probe.ps1` commit `fca36202b1b45324006ed60dc9165475ee29ae9f` runs generator, passes WAV in environment, waits for GUI, reads 5 PNGs including baseline. `tools/skinpack/ci/emit_art_screenshot.py` commit `40c1f31ab8271932d9a36c0df92f8f6e051020fd` labels proof distinctly. A real Windows Qt build+runtime check remains necessary before claiming success, especially source-ready on Hyper-V runner.
- Existing severe user constraints unchanged: pixel/focus/hover/DPI/1440x900 qualification HOLD; accent previews are NOT a user-facing installed R2/R3 skin; present context is preliminary/unqualified R1 visual laboratory only.
- HEAD at staging `40c1f31ab8271932d9a36c0df92f8f6e051020fd`, main `46e917bd2adc8ff13e83c0802895afda7daf6b2a`. Next: issue exactly one isolated CI on marker under PO standing delegation, then inspect build and actual screenshots and reconcile artistic readability. No PR/merge/FQ.

## 2026-10-17 — CI #38979939725 RED / headless no-Play readiness is NOT an artistic rendering failure
- GitHub CI8 run `38979939725`, SHA `c1676401e697fa144a2acb4105178e74c3a3dfcd`, COMPLETED/FAILURE. Windows Qt 6.8.3 app compile/deploy, prior synthetic ZIP rejection (7 cases), canonical transport geometry and native import/reset PASS; SHA-pinned genuine v0.3 package and initial 1184 Qt capture PASS.
- Generator ran and produced a legitimate local PCM16 stereo 48kHz two-second WAV. `sourceSelection.selectSource()` was successful (no sourceSelection.errorMessage); test failed at stronger prerequisite `playbackTransport.can_play()` with `SKINPACK_R1_ART_LAB_FAIL: source not playback-ready:` and empty source error. Source reader acceptance is not synonymous with PREPARED readiness or a usable output device. Root dependency is confirmed by `SourceSelectionViewModel` / `AuditionSourceSelector` / `PlaybackTransportViewModel` chain; exact underlying unavailable reason in CI8 cannot be established from its limited message and must not be invented.
- Corrective batch (experimental diagnostics only): `app/src/main.cpp` `0acc1de3f20c127630093a53b21f26b63bff42b8` accepts a genuine WAV only if `has_source()` is true, prints real audition, output and playback state, asserts actual QML Play/Stop `enabled` match true ViewModel properties instead of mock-enabling controls. If `can_play()` is false, it emits an explicit `SKINPACK_R1_ART_ENABLED_TRANSPORT_HOLD_NO_READY_OUTPUT` and proceeds to capture Panel Soft/Accent01/Accent02 with genuine source metadata, without claiming an enabled-state PASS. Screenshots labelled `source` rather than `enabled`.
- `tools/skinpack/qt_r1_probe.ps1` commit `57b4de8c1a93fe4c2496c7ffc56c425247cbca1f`, `tools/skinpack/ci/emit_art_screenshot.py` `383aa75576f158630801b2d1da93a88f85c0b05c`, align capture filenames to honest `source` designation. Existing 1184, panel, accent01 and accent02 capture cases retained. No functional change to regular source selection or player.
- New batch not validated on Windows until next isolated run. Enabled PLAY/STOP artistic qualification stays HOLD if GitHub runner cannot prepare true audio output; contrast/focus, DPI and 1440 physical size remain HOLD. No fake PASS and no DSP/math/production change, PR/merge/FQ or modification to CI workflow.
- Append-only ledger status: CI8=RED_TEST_PRECONDITION; ART_REAL_IMPORT=PASS_FROM_PRIOR; PLAY_ENABLED=HOLD_UNDER_INVESTIGATION; PANEL_SWATCH_QT_RUN=PENDING.
