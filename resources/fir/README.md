# RGS MasterLab — immutable FIR coefficient sources

This folder contains **14 exact original Final SRC FIR binaries**, each
version-controlled directly as an ordinary build resource. Their frozen
SHA-256 pins remain defined in `phase11_frozen_rgsmsr1_pins.hpp`.
The two existing Playback FIRs are exported bit-for-bit from their
frozen C++ coefficient inputs.

**One build, one destination:** CMake verifies every original SHA-256,
then packages all **16 binary files** alongside a unified
`fir_manifest.json` at `<installed-exe>/resources/fir`.
No separate ZIP download, manual installation or end-user Python step.

The original, independently authenticated 11,455,283-byte archive is
preserved on RGS ContentBox as immutable evidence, but is **not**
a build-time requirement. Its SHA-256 is
`94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4`.
The historical NONRELEASE manifest inside that archive is **not**
installed or treated as a release authorization.

FQ fails closed if any required source binary is missing or altered.
Permission to distribute the coefficients does not itself certify
all DEVICE SRC routes, Q1–Q6, or physical listening acceptance.
