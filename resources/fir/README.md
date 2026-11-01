# RGS MasterLab — Frozen original SRC FIR source for the standard FQ package

The Product Owner has explicitly authorized PUBLIC DISTRIBUTION of the 14
original RGSMSR1 FIR binaries and the 2 historic Playback FIR binaries together.

This source folder is for the ONE-TIME exact immutable original-Q0 ZIP import:

**Required file name:** `phase11_original_14_q0.zip`

**Required SHA-256:** `94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4`

**Original source:** RGS ContentBox, Google Drive ID
`1pbmZqDNefH-F_RiNZEdP9z_BC3seUGsh`.
It contains the fourteen `RGSMSR1_L*_M*.bin` original bytes plus a
historical source-candidate manifest, which the installer MUST NOT copy.

The exact 11,455,283-byte source ZIP must be present in the repository
before any official `RGSML_PROVENANCE_TYPE=FQ` build. The CMake build
authenticates the archive and all 14 SHA pins, then installs **only the
original binary files**, together with the 2 bit-identical Playback FIR
files and one unified `fir_manifest.json`, all at one level under
`<installed-exe>/resources/fir`.

**Do not regenerate, approximate or modify the ZIP.** A missing or tampered
source makes FQ FAIL. Upload/import once to this folder, then every subsequent
FQ is automatically self-contained with all 16 FIRs. No end-user Python,
secondary download or separate installation.

Note: permission to distribute original coefficients does NOT imply that
all SRC DEVICE routes, Q1-Q6 or physical listening gates have passed. Those
independent correctness gates remain mandatory.
