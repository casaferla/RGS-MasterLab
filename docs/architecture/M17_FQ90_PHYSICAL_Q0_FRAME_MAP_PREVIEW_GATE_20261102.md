# FQ #90 Windows physical gate — M17 Preview Prepared blocked on original Q0 FIR (2026-11-02)

## Direct Product Owner finding on genuine Windows ZIP
- Candidate: manually launched FQ #90 cold artifact on PR #61 SHA `5e1604c484913dd08bcba6a08401e180930b4b8e`; FQ automated SUCCESS https://github.com/casaferla/RGS-MasterLab/actions/runs/41440702796 and prior Focused #967 Windows/Linux PASS https://github.com/casaferla/RGS-MasterLab/actions/runs/41437614078. **Physical M17 gate BLOCKED**.
- SOURCE: user's ~3m19.184 stereo 44.1 kHz PCM 32-bit programme, selected MASTER 48 kHz; genuine original Q0 package of 16 SHA-verified FIRs. Prepare took ~110 seconds. After clicking Preview Prepared, GUI displayed `ERROR · BYPASS`, red LED, and no usable Listen to Master. Bottom global status: `True Peak Limiter: M17 accepted render install failed: Terminal PCM frame length or FIR kernel differs from accepted provenance.`
- Also Prepare button shrinks when replaced by Cancel during render, shifting Rate & Time left (observed actual Windows). Neither issue was covered by FQ90's automated ZIP install gate.
- Owner has started a separate native 48→48 identity Prepare test; result not yet reported. Do not assume its outcome.

## Proven and source-located root
- `app/src/terminal_playback_frame_map.cpp`, `TerminalPlaybackFrameMap::create` and `create_measured` did `provenance.src_kernel_sha256 != PlaybackSampleRateAdapter::kernel_sha256()` and refused `M17_FRAME_MAP_KERNEL_OR_LENGTH_STALE`.
- The M17 whole-resource renderer uses authenticated original **Phase11 Q0 FIR** for SOURCE→MASTER via `materialize_phase11_scoped_q0_pinned_offline_candidate`, carrying its frozen original SHA, while `PlaybackSampleRateAdapter` is a different legacy/provisional DEVICE coefficient family (different legitimate SHA). Both retain the same exact rational half-open-ceil SOURCE→OUTPUT *coordinate mapping* and output-frame-count math. Comparing Q0 original SHA to Playback coefficient SHA is an invalid requirement and blocks preview after costly successful render. It does NOT establish a corrupt original FIR nor prove the PCM output is fully audible/qualified.
- Never bypass any owner, digest, frame-domain, full PCM length, invalid interval or M17/M16 verification checks.

## Coherent staging-only corrective batch (NOT CI/FQ qualified)
- `app/src/internal/terminal_src_frame_map_provenance.hpp`: pure contract binds source/target rates, original/input and actual/output frame counts to the exact coordinate adapter; accepts either the legacy Playback SHA from that adapter OR the distinct immutable source-authenticated original-Q0 SHA from `phase11_frozen_rgsmsr1_pins.hpp` at the exact reduced L/M ratio. Arbitrary/mismatched SHA and frame counts remain rejected.
- `app/src/terminal_playback_frame_map.cpp`: use same contract in verified and measured-nonconforming install map creation; keep all other accepted ownership and independent M17 gate checks unchanged.
- `tests/unit/app/test_terminal_product_activation.cpp`: explicit regression including Q0 vs legacy 44.1→48, 48→44.1 and 192→48; wrong digest, wrong source/output counts and exact frame-boundary mapping checks.
- `ui/qml/DspEditorHost.qml`: Cancel minimum width now equals Prepare (86px), same RowLayout slot; `tests/ui_smoke/test_source_metadata_panel.cpp` asserts both identical implicit/minimum width.
- No changes to binary FIRs, DSP/MPFR math, render filter output, Q0 pins, source mutability, main branch, or manual FQ.

## Next gates
- This is **STAGING-ONLY UNQUALIFIED** until the entire batch is promoted in ONE CAS FF to PR #61 after live writer/branch/CI checks and a new Windows/Linux Focused CI passes; then PO-controlled Windows cold FQ on the new immutable SHA; then actual physical 44.1→48 Preview Prepared, Listen to Master, source return and 48→48 if available. Preserve residual 110-second Prepare performance/ETA issue as separate unclosed defect. Do not mark M17 accepted or enter CI reset/M18 without complete physical gate.
