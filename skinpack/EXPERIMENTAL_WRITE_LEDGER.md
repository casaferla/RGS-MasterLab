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
