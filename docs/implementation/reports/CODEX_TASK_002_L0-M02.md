# CODEX TASK 002 / L0-M02 implementation report

## Delivery

- Baseline: `e032441a9df2e70a94b83c2a20d121f89baf5f97`
- Branch: `task/L0-M02-build-policy-toolchain`
- Delivery commit subject: `CODEX TASK 002 / L0-M02`
- State: READY_FOR_REVIEW; NOT_MERGED_TO_MAIN; TASK_003_NOT_STARTED

## Scope and changes

This milestone adds clean-first Debug/Release acceptance presets,
target-scoped warnings-as-errors, deterministic toolchain manifest generation,
Qt-supported staged deployment, and CMake contract/deployment tests. Existing
runtime target names and the Qt-free core boundary are unchanged. No audio,
DSP, project, platform-interface, or Android code is introduced.

All changes are within the Task 002 allow-list. No allow-list deviations or
warning suppressions were needed.

## Acceptance evidence

The final acceptance used the repository presets with warnings-as-errors ON:

```text
cmake --preset windows-msvc --fresh                         PASS
cmake --build --preset windows-msvc-debug-clean             PASS
ctest --preset windows-msvc-debug --output-on-failure       5/5 PASS
cmake --build --preset windows-msvc-release-clean           PASS
ctest --preset windows-msvc-release --output-on-failure     5/5 PASS
cmake --install ... --config Release --prefix .../Release   PASS
staged RGSMasterLab.exe --rgsml-deploy-smoke                PASS (exit 0)
```

A separate configure with `RGSML_BUILD_TESTS=OFF` succeeded while
`CMAKE_DISABLE_FIND_PACKAGE_Qt6Test=TRUE` was supplied and reported unused,
which proves the Qt Test component was not requested.

## Toolchain manifest

Build path: `generated/rgsml/ToolchainManifest.json`. Installed path:
`share/RGSMasterLab/ToolchainManifest.json`. The generated and installed files
were byte-identical. Two equivalent fresh configure trees produced the same
SHA-256:

```text
49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11
```

The normalized manifest content was:

```json
{
  "artifactId": "rgsml.toolchain_manifest",
  "schemaVersion": 1,
  "productName": "RGS MasterLab",
  "target": {
    "systemName": "Windows",
    "architecture": "x64",
    "windowsSdkVersion": "10.0.26100.0"
  },
  "buildSystem": {
    "cmakeVersion": "3.31.6-msvc6",
    "generator": "Visual Studio 17 2022",
    "generatorPlatform": "x64"
  },
  "compiler": {
    "id": "MSVC",
    "version": "19.44.35228.0",
    "toolset": "v143"
  },
  "qt": {
    "version": "6.8.3",
    "minimumVersion": "6.8"
  },
  "sourceControl": {
    "gitVersion": "2.53.0.windows.3"
  },
  "language": {
    "cxxStandard": 20,
    "extensions": false
  }
}
```

The CMake distribution suffix `-msvc6` is retained because it is part of the
selected tool's detected `CMAKE_VERSION`; no path or host identity is present.
Visual Studio Build Tools was 17.14.39 (installation version 17.14.37614.0).

## Warnings and deployment evidence

The warnings fixture verified `/W4`, `/permissive-`, and `/WX` with the option
ON; with it OFF, `/W4` and `/permissive-` remained while `/WX` was absent.
Directory-global flags and an unconfigured target remained untouched. Generated
Visual Studio projects recorded `Level4`, `ConformanceMode=true`, and
`TreatWarningAsError=true` for all four RGSML targets. Qt headers remained
external and no policy was applied to imported targets. Debug and Release
builds emitted zero first-party warnings; no suppression was introduced.

The Release staging tree contained `bin/RGSMasterLab.exe`, the Qt runtime,
platform and QML plugins, and the installed manifest. The automated and explicit
staged smoke both loaded the embedded Rgsml.Ui root plus deployed Qt QML imports
and exited 0 without QML errors or warnings.

The Qt deployment tool reported that optional `dxcompiler.dll`/`dxil.dll` were
not present and that `VCINSTALLDIR` was not exported in the calling shell. These
were tool diagnostics rather than first-party or runtime warnings: deployment
completed, required artifacts were present, and both staged smoke runs passed.

## Files changed

- Build/policy: `CMakeLists.txt`, `CMakePresets.json`, `.gitignore`,
  `cmake/RgsmlOptions.cmake`, `cmake/RgsmlWarnings.cmake`,
  `cmake/RgsmlToolchainManifest.cmake`,
  `cmake/ToolchainManifest.json.in`, `cmake/RgsmlInstallDeploy.cmake`.
- Targets/hook: `core/CMakeLists.txt`, `ui/CMakeLists.txt`,
  `app/CMakeLists.txt`, `app/src/main.cpp`, `tests/CMakeLists.txt`.
- Tests: the six allow-listed files below `tests/cmake/`.
- Documentation: `README.md`, `docs/architecture/README.md`,
  `LICENSES/DEPENDENCIES.md`, this report, task log, and telemetry record.

No files outside the allow-list were changed.

## Findings and telemetry

The known process-local `PATH`/`Path` case collision recurred and caused
MSBuild error MSB6001. Removing only the duplicate uppercase key in child build
processes restored the build while preserving the effective search path. No
machine/user environment, registry, preset, or global configuration changed.

Three implementation defects were found and corrected: the Debug platform plugin
assertion now accounts for Qt's debug suffix and Qt's supported deploy-tool
option now scans the QML source directory so the deployed import closure is
complete. The generated install script also normalizes a caller-supplied
relative staging prefix because Qt's deploy support requires an absolute
`qt.conf` path. No correction changed product semantics. There were no
regressions and no blocker time.

Telemetry: 4 Codex iterations, 7 configure/build/test cycles, 3 defects found,
0 regressions, and status READY_FOR_REVIEW. `actualOwnerHours` and
`reviewHours` remain zero because the product owner did not provide them.

## Worktree attribution

- PREEXISTING_WORKTREE_CHANGES: `RGS_MasterLab_CODEX_TASK_001-2.md` remains
  untracked, unstaged, and unmodified.
- TASK_002_CHANGES: only the allow-listed files reported by the final task
  branch diff.

`actualOwnerHours` and `reviewHours` remain zero because the product owner did
not provide those values.
