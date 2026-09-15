# GUI-01 — Core Source / Waveform / Audition Shell v1

## Delivery status

- Status: `TECHNICAL_DELIVERY_READY_FOR_PO_VISUAL_GATE`
- Baseline: `88829c376e18541848dc471e279ffc7c3cabe179`
- Baseline tree: `038618cdf6f3210ebbc65c7791920b35247fd245`
- Branch: `task/GUI-01-core-shell`
- M10 / Task 013: `NOT_STARTED / NOT_PREPARED`

## Repository reconciliation

The frozen GUI-01 handoff maps directly to the existing M05/M06 presentation
objects. Source selection/metadata, Source-derived waveform, exact-frame
navigation, single Audition Region, segmented time editors, playback transport,
Gold selection, truthful PREPARED/PROCESSED/GOLD availability, independent cue
domains, and Gold playhead suppression remain owned by their accepted C++
ViewModels. No DSP, playback, render, resource, or public core contract changed.

## Materialization

- Replaced the scrolling prototype with a fixed desktop composition at a frozen
  minimum of 1184 x 688 logical pixels and a 1440 x 900 reference size.
- Added the branded 56 px application header, right-aligned Audition Target
  selector, system-behavior window controls, and the File/Edit/View/Transport/
  Help desktop menu taxonomy with truthful enabled states.
- Materialized the Source/Gold identity strip, elastic Studio Slate waveform,
  compact Precision Navigator, intentional elastic center, right-aligned
  Stop/Play-Pause/Time transport, segmented Region editors/actions, and truthful
  status bar.
- Kept the waveform Source-derived while GOLD is selected and preserved the
  accepted Source-playhead suppression behavior.
- Added one presentation-only continuous zoom seam. QML supplies a normalized
  control position; all mapping to Source frames and anchoring remains in C++.
  The waveform summary, peak pyramid, exact seek, region, playback, and SRC
  contracts are unchanged.
- Selected the Qt Basic control style before application/test construction so
  the frozen visual controls are supported without runtime customization
  warnings.

## Qualification

| Gate | Result |
|---|---|
| Fresh configure | PASS |
| Focused viewport + UI smoke | PASS 2/2 |
| Debug build and complete CTest | PASS 65/65 |
| Release build and complete CTest | PASS 65/65 |
| Dependency/public-header contracts | PASS 27/27 in both complete suites |
| Tests-off Release | PASS; 0 tests |
| Staged Release deploy smoke | PASS; exit 0 |
| Runtime QML/warning smoke | PASS; empty log |
| ToolchainManifest build/stage | PASS; byte-identical |
| Historical untracked file | PASS; untracked, unstaged, byte-identical |

ToolchainManifest SHA-256 for the current configured kit:
`05701BF7E061F6AFFB9B687D4D6D389048B7D2CEC57CC4CFCE5075FF1B22355B`.

## Visual evidence

All images are captured from the real QQuickWindow and real M05/M06 ViewModels.
The loaded Source is a deterministic stereo WAV decoded through the authoritative
WAV/AudioBuffer/waveform path; the Audition Region is committed in Source frames.

- `docs/implementation/evidence/GUI-01/gui01_1440x900_prepared.png`
- `docs/implementation/evidence/GUI-01/gui01_1184x688_prepared.png`
- `docs/implementation/evidence/GUI-01/gui01_1440x900_gold.png`
- `docs/implementation/evidence/GUI-01/gui01_1184x688_unavailable.png`

The evidence shows PREPARED selected, GOLD selected, PROCESSED unavailable, the
empty/unavailable state, both frozen viewport sizes, real stereo waveform lanes,
the frozen region, continuous navigator, transport, segmented editor, and status
bar without stacking or clipping.

## Scope and residuals

No future workspace, fake module, DSP chain, Analyzer, Restore, Reference Match,
AutoMatch, Compare, Render, DNA, project persistence, or new format was added.
Input scope remains `WAV_ONLY`. The automated and visual qualification found no
blocking residual. Product Owner visual acceptance remains the only pending gate.
