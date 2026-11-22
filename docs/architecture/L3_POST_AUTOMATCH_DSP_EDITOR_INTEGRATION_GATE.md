# Post-L3 Product DSP editors — integration checkpoint (2026-11-22)

**Status:** approved integration queue, NOT Product integration, NOT additional L3 milestone.  
**Single writer:** L3 AutoMatch operational writer owns the later Product integration.  
**Priority:** finish native L3 AutoMatch M12 materialization/reopen and M13 fresh
full-candidate verification and attain a stable first Product candidate. Do not
interrupt the present L3 writer for UI work. Once these conditions are met,
integrate both editors together as one bounded, coherent Product lot, before
calling the resulting Windows application qualified.

## Isolated research authorities (read-only until the integration gate)

| Editor | Research branch | Frozen handoff baseline |
| --- | --- | --- |
| Dynamic EQ C01 | `research/l3-dynamic-eq-editor-isolated-20261122` | `a16e030813548776986953dcd7624b84a519ac8b` |
| Transient Shaper C00 | `research/l3-transient-shaper-editor-isolated-20261122` | `96a1c777247068eb7e4866f573d42e1f93ad4ad9` |

Authoritative, maintained handoff files in their respective research trees:
- `research/l3-dynamic-eq-editor/INTEGRATION_HANDOFF.md`
- `research/l3-transient-shaper-editor/INTEGRATION_HANDOFF.md`

Do not opportunistically merge either research branch into operational L3.
Cherry-pick or adapt the reviewed Product-relevant implementation only at the
checkpoint, resolving shared-QML host changes from the CURRENT Product code.
Do not ship research Demo.qml, fabricated Activity or test UUID generators.

## Trigger to begin integration

1. **M12**: One actual accepted M10/M11 winner is committed with the Product's
   exclusive writer/epoch and preserved MANUAL graph; re-opened REF_LINKED
   project produces the same real native sound, or intentionally remains
   fail-closed if the required owner/factory is missing.
2. **M13**: Fresh full master is re-rendered independently and measured from
   authoritative real PCM (M16 TP, complete frozen feature set); stale
   SOURCE/REFERENCE, wrong owner, or different protected MANUAL rejects.
3. **Product stability**: the minimal L3 AutoMatch candidate selection/adoption
   lifecycle can be exercised without changing the active MANUAL sound on
   rejection; the relevant focused Windows+Linux CI is green.

The editor lot follows this checkpoint. It is NOT a condition for accepting
M12/M13. The final combined app still needs separate Product qualification.

## Non-negotiable Product integration decisions

- Reuse the current common DSP host, docking, multiple floating windows,
  native header, window selection and lifecycle, and 1184x688 minimum-size
  conventions. Do not duplicate the old research window framework.
- Product owns one typed parameter state per module, unique production UUIDs,
  edit permission, snapshot/persistence, Undo/Redo and re-render invalidation.
  Neither editor introduces an independent DSP engine.
- Dynamic EQ: inherit existing Parametric EQ controls and Spectrum coordinates,
  Bell/Low Shelf/High Shelf only, no static Overall; six dynamic values live in
  a separate WAV Export-family nonmodal Qt.Tool companion. Optional Activity
  is a second native true-telemetry view, never an overlay that shrinks EQ.
- Transient Shaper: two copper Attack/Sustain controls, true 0.1 dB numeric
  input and Reset, authentic C00 20 ms applied-gain history only. No Demo.qml.
- Activity in BOTH editors requires accepted immutable PCM provenance,
  matching RealizationId, revision, source frame domain/rate, PROCESSED audible
  playback snapshot, active non-bypassed DSP and complete history. Otherwise
  hide/clear; pause/seek/loop use real transport cursor. Manual edits invalidate
  old PCM and all sidecar history until a new accepted render is published.
- Keep M17 WAV Export as an independent export ceiling/PCM-format action;
  never map it into AutoMatch DSP coordinates or confuse it with optional
  P2 Streaming True Peak Limiter. M16 is the analytical True Peak authority.

## Acceptance/exit

Run the new integrated focused Windows/Linux tests, then distribute a distinct
Windows Product artifact for physical tests: input and Reset/Undo, project
save/reopen, SOURCE/PROCESSED/REFERENCE, Play/Pause/Stop/seek/loop, bypass,
edit/re-render invalidation, authentic telemetry, DPI 100/150%, 1184x688,
multi-window docking and dual-monitor, and channel/sample-rate transitions.
No merge to main, official FQ or physical-audio success may be inferred from
isolated research Qt CI. Release/FQ retains its own approval process.

This file is a coordination decision; it certifies no Product code or CI.
