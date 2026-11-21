# R1d-P2 / Streaming TP Limiter Live — I0 (research-only)
Date: 2026-11-12. Status: **PO GO for isolated development**, **NO product merge / activation GO**.

## Authority and provenance
- [Official I0 mandate on RGS ContentBox](https://docs.google.com/document/d/1XBlRR0XwKM7suZjc7rnXwZFJMclpvhg4tfc3yYJo3Vg/edit).
- [R1d technical ledger](https://docs.google.com/document/d/1eaHB5FnSPkKOGZZVzJlRFb1r5V2uxTeIZnpMtBB0na0/edit), entries 68–84.
- P2 original ZIP `RGS_R1d_P2_Gate_B_Evidence_2026-11-11.zip`, SHA256 `4859c2aa7a601f189e101e24538575867301431e19436d38739a841af83a1f39`. Provenance in `code/stream.hpp`, `code/music_p2.hpp`, `results/*`.
- Portable physical gate ZIP SHA256 `b422646c32168ac1418a0f09d8d8fb3891284439d08416e397eb2a4770324a56`.
- Base main commit `304f593c89e7079aefa31727055b0880243245e4`. Other L1 stages and PR #98/#99 are **not** sources for writes from this branch.

## Architecture boundary
1. **Three independent entities must not be conflated:**
   - `TruePeakLiveCoordinator`: existing M16 live *meter* only.
   - `rgsml.dsp.true-peak-limiter`: M17 terminal after Final SRC; **leave unchanged**.
   - Proposed `rgsml.dsp.streaming-true-peak-limiter`: separate P2 DSP candidate, prior to Master Audition; ID is provisional/non-frozen.
2. Adapt P2 research code to `IModule` (`runtime_requirements`, `prepare`, `process`, `finalize`, `reset`, `runtime_checkpoint`, `restore_runtime_checkpoint`) as an isolated target **before** touching `ModuleRegistry`, `MasteringChainState`, serialisation or QML.
3. Report latency and FIFO carefully: P2 64-frame internal processing quantum is **not** the device callback deadline. Controller predictive look-ahead plus safety delay must preserve full PCM identity/length after compensated EOS.
4. Real-time path: no dynamic allocations, locks, Qt, filesystem or logging after `prepare`; fixed-size state, deterministic failure and no retroactive changes to emitted frames. Enforce strict FP / approved backend. Support six project Fs and mono/stereo per the pinned prototype.
5. Distinguish in-chain ceiling from final master ceiling. M17 remains the terminal protection after Final SRC irrespective of P2 presence or later DSP changes.
6. Keep eta=2% as initial research baseline, not a new frozen product default. Parameter/UX/persistence contracts are deferred to a reviewed step.

## Evidence-aware minimum gates
- **I0:** isolated port/adapter, no Product registry; source provenance + interface mapping + compilation evidence.
- **I1:** actual M16 output TP, INTERIOR identity and backend consistency, EOS/epoch/seek/bypass/reset/checkpoint; samples and seam joins; deterministic error on invalid inputs. Fail-closed on any hard invariant regression.
- **I2:** after Single-Writer release, integrate gradually into manual DSP chain/UI on a distinct, approved staging branch; do **not** alter M17 or existing M16 meter. No automatic main merge, PR CI or FQ.
- **I3:** focused Windows real callback measurement against the established native 512-frame WASAPI period, without inventing 64-frame device qualification or universal hard-WCET.
- **I4:** measurable time budget and deterministic repeated renders for AutoMatch with original Q0 FIR, 27 canonical L3 features, M10 limits, and M13 Fresh Verify. Never place terminal M17 in the full solver's repeated render loop.

## Managed Windows PC / no PowerShell
The PO can execute portable Windows x64 EXEs on the organisation-managed PC but cannot install software and does not want to use PowerShell there. A native double-click runner with WAV picker, limited profiles, blind A/B and reports is a **future deliverable only when an actual gate needs it**. It must require no elevation/installer, respect AppLocker/WDAC/AV and save to user-owned locations; policy bypass is prohibited. Physical audio qualification still depends on device capability and local policy.

## Current state
- Isolated governance branch opened only. **No P2 implementation, no build, no new tests, no CI/FQ, no Product wiring** are claimed from this ADR.
- Next implementer step: inspect the pinned P2 source and draft the concrete C++ adapter API and latency contract, then implement in this research branch with repeatable tests.
- Explicit PO authorization still required for production integration, merge or freeze.
