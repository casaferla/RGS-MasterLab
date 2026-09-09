# RGS MasterLab

Bootstrap repository for the Windows shell of RGS MasterLab. The current code
contains a Qt-free C++20 core, a platform-neutral audio library for canonical
PCM buffers and bounded WAV decode, a minimal Qt 6/QML UI, the Windows
executable, and headless CTest contracts. It does not contain DSP, playback,
or concrete resource-adapter functionality.

## Prerequisites

- Windows 10 or 11 x64
- Visual Studio 2022 or Build Tools 2022 with the MSVC x64 C++ toolchain
- CMake 3.25 or newer
- Qt 6.8 or newer for MSVC 2022 64-bit, including Core, Gui, Qml, Quick,
  QuickControls2, and Test

Set `RGSML_QT_ROOT` locally to the prefix of the selected Qt kit. Do not commit
its machine-specific value:

```powershell
$env:RGSML_QT_ROOT = '<Qt-kit-prefix>'
```

Alternatively, provide the standard CMake inputs `CMAKE_PREFIX_PATH` or
`Qt6_DIR` locally.

## Configure, build, and test

From a Visual Studio 2022 developer environment:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug-clean
ctest --preset windows-msvc-debug --output-on-failure
cmake --build --preset windows-msvc-release-clean
ctest --preset windows-msvc-release --output-on-failure
```

The original incremental `windows-msvc-debug` and `windows-msvc-release`
build presets remain available. All repository acceptance presets enable
target-scoped warnings-as-errors for first-party RGSML code.

Install and validate the staged Release deployment with:

```powershell
cmake --install build/windows-msvc --config Release --prefix build/windows-msvc/stage/L0-M02/Release
& build/windows-msvc/stage/L0-M02/Release/bin/RGSMasterLab.exe --rgsml-deploy-smoke
```

The generated toolchain record is written to
`build/windows-msvc/generated/rgsml/ToolchainManifest.json` and installed to
`share/RGSMasterLab/ToolchainManifest.json` below the selected prefix.

The Debug executable is generated as
`build/windows-msvc/Debug/RGSMasterLab.exe`.
