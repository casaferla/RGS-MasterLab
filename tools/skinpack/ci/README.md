# RGS SkinPack Experimental Qt CI — candidate only

Status: **PO-approved conditionally, pending M16 operative ACK for exact path exception**.
Authority: RGS ContentBox / MASTER PLAN
https://docs.google.com/document/d/12A4l9xAVCCZfZ3Ui7mksRvqhj3BozdE9uuzQi-yhRWQ/edit

Repository `casaferla/RGS-MasterLab` | branch `experimental/skinpack-r1-r4-unqualified-20261016`.

## File policy
- Candidate source: `tools/skinpack/ci/skinpack-experimental.yml` (ALREADY allowed).
- Executable GitHub Actions path, **AFTER NEW OPERATIVE ACK ONLY**:
  `.github/workflows/skinpack-experimental.yml`.
- File-segnale: `skinpack/ci/run-request.txt`.
- Do NOT create/touch the run-request in the same commit as workflow activation.
- No `workflow_dispatch`: GitHub requires that workflow to exist on the
  default branch, which would violate the isolation agreement.
- `on.push` is restricted by exact branch AND the single changed path;
  changes to code or workflow alone do not run CI.
- Activation and each actual run require distinct authorizations and ledger entries.

## Job
Windows 2022/MSVC, Qt 6.8.3 with Qt Multimedia, `rgsml_app` CMake
build, staged deploy and `--rgsml-deploy-smoke`, tests OFF,
warnings-as-errors ON. `contents: read`, `persist-credentials: false`,
no cloud dependencies in the shipped product, no workflow secrets, no merge,
PR, Linux full matrix, release or deployment.
A PASS is a **build smoke**, never Qt GUI/pixel qualification, archive
security sign-off, Windows device audio test or Full Qualification.

## Next mandatory gates
1. Receive `OPERATIVE_ACK_EXTENSION=ACCEPTED` for the exact new workflow path.
2. Compare current branch SHA and existing workflows; validate triggers/ACL.
3. Install workflow on the branch in one auditable commit, **without** touching
   `skinpack/ci/run-request.txt`; verify that no Actions run was started.
4. Receive explicit PO request for *one* CI; then create/update the marker.
5. Inspect logs, preserve result in the append-only ledger and RGS ContentBox.
