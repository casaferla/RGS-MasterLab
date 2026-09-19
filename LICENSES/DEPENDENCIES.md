# Dependency inventory

## Runtime/build dependency

- **Qt 6.8 or later compatible Qt 6 release** — modules currently used: Core,
  Gui, Qml, Quick, and QuickControls2. The Test module is a development-only
  dependency enabled by `RGSML_BUILD_TESTS`. Qt licensing terms depend on the
  selected distribution and deployment model; verify them before distribution.
  The L0-M02 acceptance build was tested with Qt 6.8.3. The future choice
  between applicable LGPL terms and a commercial license remains open.

No other third-party runtime dependencies are introduced by this bootstrap.

## L1-M10 foundation vendor pins

- **nlohmann/json 3.12.0**, tag `v3.12.0`, commit
  `55f93686c01528224f448c19128836e7df245f72`, MIT. A private,
  SYSTEM-header target uses the pinned single include. No JSON type is exposed
  in public RGSML headers.
- **minizip-ng 4.2.2**, tag/commit
  `7b2387161c542fa9f427352dcdef76097d0d692b`, zlib-style license.
  The vendored static build disables compression and encryption backends;
  STORE is qualified independently. Neither vendor fetches at configure/build.
