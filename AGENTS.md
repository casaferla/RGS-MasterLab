# AGENTS.md — Normative Guidelines

**2026-10-31 PO OVERRIDE — PERMANENT LARGE COHERENT MACRO-LOTS:** Prefer large functional work packages containing all related code, QML, native regressions and documents on the no-CI staging branch. Intermediate checkpoint commits are allowed. PROMOTE ONCE by expected-SHA fast-forward to Draft PR, run ONE Windows+Linux Focused CI per substantial completed macro-lot (not per fix, file or tiny feature). While that immutable PR CI runs, continue authorized next-lot work on staging and poll job/step states within the active chat; do not demand repeated routine "Vai". On red, use bounded diagnostics, fix all related failures on staging and promote a single subsequent candidate after the prior run is terminal. On green, continue the next authorized macro-lot. Preserve single-writer epoch, exact SHA CAS, genuine SRC maths/provenance, SOURCE/MASTER/DEVICE guards, one Prepare, no private FIR assets on public GitHub, and full Windows/Linux CI. Never claim a staging SHA is covered by prior green CI. PO alone authorizes physical FQ and main merge. The current four workstreams are (1) MAIN Ascolta Master transport, (2) six-rate Prepare time/RAM, (3) canonical Phase3 Final SRC Q0 + applicable genuine 36-pair integration, (4) M17 Windows physical FQ readiness. #939 Windows+Linux PASS qualifies the first two workstreams for Focused CI, NOT physical FQ. Defer unimplemented Android/Reference Match gates; after PO-approved M17 proceed immediately to M18 Dither.

**2026-10-31 PRODUCT OWNER SCOPE DECISION — M17 -> M18 DITHER, NO FUTURE PIPELINE GATES:** M17/Final SRC certification requires mathematically/audibly qualified SOURCE→MASTER conversions and all tests actually applicable to the current M17 Windows product and implemented shared foundations. Do NOT block closing M17 on Android execution, Reference Match, or other Final Pipeline components that do not yet exist; qualify each at its later implementation milestone. Q0 canonical scrutiny, applicable real SRC functional/invariance/performance/regression gates, six-rate Prepare and Windows PO FQ still apply—never misrepresent a partial M17 gate as global Q0–Q6 release PASS. Once M17 is approved by PO, the very next milestone is **M18 Dither**. Do not begin M18 prematurely or bring unrelated future features forward.

**2026-10-27 PO RATE-TIME REPRIORITIZATION:** Include real per-host calibration, truthful pre-Prepare ETA ranges across six real master rates, learning aggregated stage timings from actual Prepare, and strict MASTER/DEVICE rate separation within existing P0 M17+Phase11 contract. Earlier 23 Oct PO had deferred priority after observing in-progress ETA, but current 27 Oct PO revives this goal. Read `docs/architecture/M17_MASTER_AUDITION_SINGLE_PREPARE_GATE.md` for evidence and exact mandatory acceptance; never invent supported 192 SRC, ETA or hardware DAC requirement.


**ACTIVE P0 SUPERSESSION (2026-10-27):** Before any M18 work or PO physical audio Gate M/L, implement and qualify [ONE M17 Prepare → Master Audition owner and master-rate independence](docs/architecture/M17_MASTER_AUDITION_SINGLE_PREPARE_GATE.md). FQ #83 is green but does **NOT** qualify this missing integration. Preserve original RGS ContentBox authority; never treat independent `Prepare MASTER` or device-rate-chosen terminal M17 as completed product behavior. Read latest `ACTIVE_OPERATOR_STATE.md` and `OPERATIONAL_HANDOFF_CONTRACT.md`. Do not modify frozen DSP to bypass this requirement.


## Product & Target Platforms
- **Product**: RGS MasterLab — Reference Guided Sound — Gold Reference Mastering Environment.
- **Primary Platform**: Windows 10/11 x64.
- **Secondary Platform**: Android on shared foundations.
- **Excluded Platforms**: Apple/iOS/macOS excluded.
- **Technology Stack**: C++20 + Qt 6/QML + CMake.

## Architecture & Layering Rules
- **Platform Neutrality**: Shared production layers (`core/`, `audio/`, `dsp/`, `render/`, `project/`) must remain strictly platform-neutral.
- **Platform Layer Isolation**: `platform/windows/` owns Windows-specific APIs. Future Android platform code belongs exclusively under `platform/android/`.
- **Android Compatibility**: Blocking for all shared layers; no Windows-only shortcuts below `platform/windows/`.
- **QML Scope Boundary**: QML is for presentation, layout, and event forwarding only. No DSP, SourceFrame, routing, persistence, or business math in QML.
- **Single Engine & Registry**: One DSP Engine + one Module Registry. Restore and Master are workflow stages, not separate engines.
- **Reference Match Role**: Reference Match is controller/analyzer/planning, not a second DSP engine.
- **Zero Parallel Architectures**: Never invent parallel parameter-binding, checkpoint, renderer, analysis, writer/export, persistence, or diagnostics architectures.
- **No Cloud/Network Dependency**: No cloud or network runtime dependencies in the product.

## Data Contracts & Immutability
- **Read-Only Originals**: Source/Gold originals are strictly read-only and non-destructive; never overwrite them automatically.
- **Canonical Processing States**: `RAW` / `PREPARED` / `PROCESSED` / `GOLD`. Never rename `PROCESSED` to `MASTERED`.
- **Strict Contract Enforcement**: No hidden clamp or default fallback where a frozen contract requires rejection.
- **Preserve Unknown Project Content**: Preserve unknown project content under frozen degraded/opaque rules.

## Mandatory Chat / Agent Handoff Boot — PO FROZEN 2026-10-25
**Read before ANY GitHub write or new operational session:**
1. [Operational handoff / single-writer / CI batching contract](docs/architecture/OPERATIONAL_HANDOFF_CONTRACT.md).
2. [Current staging, PR, CI, FQ and pending defects](docs/architecture/ACTIVE_OPERATOR_STATE.md).
3. Live GitHub staging/PR SHA, branch ancestry, failed-step summaries and Product Owner decisions.

**Non-negotiable:** Stage *all* intermediate changes on the named `staging/*-no-ci-*` branch, with NO PR attached. Batch coherent code, tests and fixes; promote **one complete candidate** to the PR branch with a fast-forward SHA lease. The PR triggers one Focused CI per promotion. On red, stage a correction and promote a new tested candidate; on green, continue the next already-authorized work item in the current chat. NEVER push one partially done file after another to the PR. NEVER fetch complete GitHub job logs. Product Owner alone launches Full Qualification. No main merge without PO Gate. Include exact clickable CI link in every operational update. All handoffs must transmit these two canonical files and explicitly require reading them before any action.

## Development & Process Norms
- **Task Allow-Lists**: Follow exact task allow-lists strictly.
- **Conflict Resolution**: On authority/live-baseline conflict, stop and report; do not redesign.
- **Testing Strategy**: Prefer focused tests during iteration; run full matrices only at milestone checkpoints.
- **Branch & Pull Request Strategy**: Never push or merge directly to `main`. All Jules work must remain in a draft PR until review.
- **Milestone Discipline**: Do not start later milestones unless explicitly authorized.
