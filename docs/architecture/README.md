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
