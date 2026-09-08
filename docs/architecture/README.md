# Architecture bootstrap

RGS MasterLab uses a shared C++20 core with platform-specific application shells.
The initial dependency direction is deliberately one-way:

```text
rgsml_app -> rgsml_ui -> Qt Quick / QML
          -> rgsml_core (Qt-free)
```

`rgsml_core` must remain independent of Qt and operating-system UI APIs. QML is
limited to presentation and interaction; business logic belongs in C++. Audio,
DSP, project persistence, rendering, and Android packaging are outside this
bootstrap.

Dependencies are resolved locally by CMake. Ordinary configure operations must
not download packages or embed machine-specific installation paths.

Compiler warnings are configured only through `rgsml_apply_project_warnings`
on first-party targets. Dependency targets never inherit warnings-as-errors.
The generated `ToolchainManifest.json` describes only stable, detected
toolchain values; it excludes paths, timestamps, source-control state, and host
identity. Windows staging uses Qt's generated QML deployment script and remains
inside the ignored build tree.

## Core foundation primitives

L0-M03 places error/result, UUID/typed-ID, checked integer, exact rational,
frame/time, and processing-state primitives in `rgsml::core`. Their public
headers are self-contained and their implementation uses only the C++20
standard library; Qt, Win32, I/O, persistence, and platform services remain
outside `rgsml_core`.

Valid value objects preserve their invariants at construction: rational
denominators are positive and reduced, sample rates are positive, frame counts
are non-negative, typed IDs are non-nil, and frame ranges are half-open with
`end >= begin`. Expected validation and arithmetic failures are represented by
`Result`/`Status` with stable categorical error tokens. Frame-to-time mapping is
exact rational arithmetic; time-to-frame rounding and FrameMap semantics are
intentionally deferred.
