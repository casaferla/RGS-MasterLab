# AGENTS.md — Normative Guidelines

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

## Development & Process Norms
- **Task Allow-Lists**: Follow exact task allow-lists strictly.
- **Conflict Resolution**: On authority/live-baseline conflict, stop and report; do not redesign.
- **Testing Strategy**: Prefer focused tests during iteration; run full matrices only at milestone checkpoints.
- **Branch & Pull Request Strategy**: Never push or merge directly to `main`. All Jules work must remain in a draft PR until review.
- **Milestone Discipline**: Do not start later milestones unless explicitly authorized.
