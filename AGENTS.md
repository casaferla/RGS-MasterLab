# AGENTS.md — Normative Guidelines

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
