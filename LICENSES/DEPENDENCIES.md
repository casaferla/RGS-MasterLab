# Dependency inventory

## Runtime/build dependency

- **Qt 6.8 or later compatible Qt 6 release** — modules currently used: Core,
  Gui, Qml, Quick, and QuickControls2. The Test module is a development-only
  dependency enabled by `RGSML_BUILD_TESTS`. Qt licensing terms depend on the
  selected distribution and deployment model; verify them before distribution.
  The L0-M02 acceptance build was tested with Qt 6.8.3. The future choice
  between applicable LGPL terms and a commercial license remains open.

No other third-party runtime dependencies are introduced by this bootstrap.
