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

## Development gate review — 2026-11-26

**Trigger satisfied for the controlled Product lifecycle; editor integration may begin.**

- Focused #138, exact SHA `ea15ebe4af814b6ac1f0cfb2757a0c11643600c7`, Windows/Linux SUCCESS; six-rate Windows run #1 at the same SHA SUCCESS on all six rates (run 45860788999).
- Focused #139, SHA `b47b3b96219ae4dc62be0521b2b15b96c812ac8b`, Windows/Linux SUCCESS; real native candidate, exclusive adoption, independent M13 numerical evidence, replacement, persistence/reopen, Undo/Redo and stale/rejected owner checks exercised by the Product lifecycle test.
- Formal QTP approval remains unavailable. This is the development trigger described above, not FQ or a general multidomain music qualification. The optional full-track benchmark remains a separate pending performance test.

The next coherent integration lot is owned by the operational Writer. Use the current `DspChainAdapterModel`, `DspModuleAdapter`, `DspEditorHost.qml` and `DspFloatingModuleWindow.qml`. The current adapter list enumerates MANUAL plus analysis/export/monitor services; it does not expose the REF_LINKED Dynamic EQ and Transient Shaper nodes. Start with authenticated exact instance identity and read-only linked parameters from the current Product owner; do not pretend those nodes are MANUAL or add default nodes to existing projects. Adapt the research controls/graphs to that owner, then implement explicit authorized edit/persistence and render invalidation only through Product transactions. Keep Activity hidden until accepted actual DSP sidecars are wired. Recheck the research handoff HEADs before importing any implementation.

## First Product integration lot — native read-only presentation seam (2026-11-26)

The operational writer now resolves the current REF_LINKED editor candidates from the installed M12 Product ticket, exact live PREPARED owner, MANUAL owner/registry, structural revision and full typed bindings. Both supported editors are enumerated in actual linked-chain order. No missing, foreign, duplicate or stale identity is substituted with a MANUAL/default instance. Native C00 Attack/Sustain and C01 band identity, filter/routing, shape and all six dynamic fields have a read-only Qt value projection. Inapplicable Q or shelf slope is absent, not a fabricated active parameter.

The real Product lifecycle test now checks this presentation seam on materialization, reopened project, Undo/Redo and retired PREPARED or changed MANUAL bindings. Manual editing, Activity and independent preview remain unauthorized. This is not yet the QML host registration or visual integration. The next lot should bind the shared DSP chain model and current editor host without adding default graph nodes or pinning a stale PCM owner. Activity and mutation are separate owner-authorized integration gates. The new focused CI must pass before treating these additions as qualified; no musical benchmark or FQ is implied.

## Second integration lot — shared host and editor presentation

The shared model is now explicitly enabled by Product startup. Linked rows use real instance IDs, current owner identity and read-only typed parameters. The research C01 EQ editor/graph and detached six-parameter Dynamics companion are ported, including native C01 base-gain coefficients at the actual Prepared rate. C00's reviewed Attack/Sustain editor is ported with the common header. Both use the existing dock/floating host, without new engine instances or default project nodes. The first visual lot deliberately excludes Activity because authentic accepted PCM sidecars are not wired. Preset actions and all parameter mutation remain locked. Focused CI includes native owner/curve regression and Product compilation; physical appearance, genuine playback history and Windows artifact acceptance remain outstanding.

## Third integration lot — native activity capture and PCM identity

C00 and C01 observation are opt-in at the real engine/module/render boundary; ordinary AutoMatch search and verification remain unobserved. Per-instance histories aggregate 20 ms buckets at all six rates, with bounded allocation and complete coverage checks. Native M12 replay requests observation only for its actual REF_LINKED stage, seals it only after all required stages (including terminal P2 where present) succeed, and the Product preview worker attaches the sealed history to its candidate PCM and exact plan checksum. The Product selector binds the same accepted RealizationId; allocation failure or invalid history suppresses Activity without changing PCM. Tests cover observed/unobserved bitwise parity, complete native history, wrong instance, cancellation, capacity and actual selector publication. The next gate is playback-authoritative resolution (stable NEW audible identity, correct rate/domain/current owner, no bypass, complete buckets) and the reviewed QML Activity surfaces. This lot does not claim physical FQ, musical benchmark or GUI Activity qualification.
