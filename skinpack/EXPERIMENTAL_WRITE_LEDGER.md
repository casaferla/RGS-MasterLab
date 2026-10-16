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
