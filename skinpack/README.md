# RGS SkinPack — isolated experimental branch

**Status:** EXPERIMENTAL / UNQUALIFIED / NOT FOR PRODUCTION.

This directory is governed by the PO mandate approved on 2026-10-16 and by the independent **OPERATIVE_ACK=ACCEPTED** issued by the M16 chat.

- Repository: `casaferla/RGS-MasterLab`.
- Exact branch: `experimental/skinpack-r1-r4-unqualified-20261016`.
- Frozen starting snapshot: `c30b4149437b6404892973ea543baf8773c0060f` (reference FQ65; NOT an integration baseline).
- Production Single-Writer Epoch remains exclusively with the M16 operational chat.
- No edits to M16 branches, `main`, DSP/math, `.github/workflows/`, or governance rules.
- No PR, merge, production CI/FQ, or release without further PO authority.
- Only declared allow-list paths are writable; shared UI controls need additional reconciliation.
- Fully offline application, unchanged geometries/interaction/measurement semantics, reversible RGS Original theme.

## Workstream

R1: `.rgsskin` parser and Surface Renderer — prototype files prepared offline, native build not qualified.
R2: decorative Controls — prototype prepared offline; existing shared controls not yet authorized for editing on this branch.
R2.5: universal visual roles and R2 exact-slot bridge — machine-readable **design draft** in `skinpack/contracts/`.
R3: non-executable decorative effects and presets — not yet committed.
R4: library and selection — scope requires new explicit technical definition before implementation.

An unregistered UI role falls back to **RGS Original**; role/color/geometry semantics are host-owned. A future host implementation must validate input safely, avoid loading external QML/JS/shaders, and commit a new skin only after all assets are verified.

## Authoritative documents (RGS ContentBox / MASTER PLAN)

- PO experimental-branch mandate: https://docs.google.com/document/d/1cgIkeck07DvGJ0EraKy5n0YLKIVRXBpEeGnJgz-350A/edit
- R1 deferred handoff: https://docs.google.com/document/d/1pwRHMD4eSL3jhuYW9087SiOork7G0GxdHlR8viFIXrE/edit
- R2.5 Universal Skin Contract: https://docs.google.com/document/d/1iCLLt4BGBnL_zvBFN1Ygq4SUUJIWR97CcIJAolHR_vE/edit
- R2.5 technical kit: https://drive.google.com/file/d/1pEUQS516aYkJMn0Wp8aO2QWy1VQX2u_E/view

GitHub material is an **experimental mirror**: it does not supplant the Drive authority.
