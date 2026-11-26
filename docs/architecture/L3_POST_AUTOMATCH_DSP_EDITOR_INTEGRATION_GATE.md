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

## Fourth integration lot — playback projection and reviewed Activity surfaces

Product startup explicitly binds the linked adapters to the real audition selector and playback transport. Activity requires exact current accepted PCM owner, plan checksum, native linked revision, active node, stable NEW audible RealizationId, source-rate PCM domain, and completed history. Paused playback retains the current window; unavailable, stopped, foreign/transition and independent audition suppress it. Separate transport identity notifications prevent unnecessary EQ curve recomputation or shared chain row rebuilding, including handoff changes at an unchanged position. The reviewed C00 scope and C01 companion are restored; C01 colors and numbers follow native band UUIDs. Qt map regression exercises native values, pause and invalid evidence. This lot remains unqualified until Focused CI passes; visual fit and actual WASAPI listening must still be checked on the Windows artifact. No manual editing, FQ, release or musical benchmark is implied.

## Updated integration qualification — 2026-11-26

- Focused #144, SHA `15874fe1ff35644eb6dc7f43fc3d4d7dc94bbc11`: SUCCESS, Windows/Linux macro gate including Product Activity integration.
- Six-rate Product qualification #2, SHA `6a3ab745f8490e0f3a99ed92bf21dc83558fd6f8`: [SUCCESS](https://github.com/casaferla/RGS-MasterLab/actions/runs/45893828867). All six Windows matrix jobs (44100, 48000, 88200, 96000, 176400, 192000 Hz) completed successfully, including native Product search lifecycle and the corresponding linked M12 editor/Activity lifecycle row. Evidence retention steps passed in all jobs.
- The standalone Windows artifact #1 uses the same tested SHA. At this checkpoint its build is still in progress; no artifact or cold-smoke success is claimed. [Run](https://github.com/casaferla/RGS-MasterLab/actions/runs/45893820238).

Keep the qualified candidate frozen while packaging and physical testing proceed. Manual REF_LINKED editing remains locked; physical playback, visual fit, representative full-track performance, formal QTP/FQ and release remain separate open gates. No main merge or extra DSP work is required to document this result.

### Windows artifact #1 startup regression

The [artifact run](https://github.com/casaferla/RGS-MasterLab/actions/runs/45893820238) compiled Release and installed the full standalone Product (including the FIR integrity check), but failed strict Cold Smoke with four Qt Basic Dialog implicitWidth binding-loop warnings at DspPresetHeaderActions.qml:105. The Product smoke warning guard correctly returned 1, so no executable artifact was published. This is a shared GUI sizing failure, not a failed native DSP test. Fix `fc2e6f8880c092e8886608a9d743d8a6b01dc250` assigns independent dialog and label implicit widths for both preset dialogs; no DSP, solver or persistence semantics changed. The strict warning gate remains intact; successful cold startup and actual package publication must be demonstrated by the retry.

Artifact #2 also built and installed successfully, but Cold Smoke failed during QML component instantiation: Qt 6.8.3 reports that Label.implicitWidth is read-only (line 122). This invalidates the preceding sizing fix, not the numerical qualification. Commit `aa9964a084c000635a6094e96c9292cf1bacbcb5` moves independent implicit sizing into a plain Item wrapper and leaves the wrapped Label's natural implicit dimensions untouched. Both preset dialogs retain their confirmation/error semantics. The retry must pass the unchanged strict startup gate; no artifact success is presumed.

### Windows integrated artifact #3 — successful delivery, 2026-11-26

[Run #3](https://github.com/casaferla/RGS-MasterLab/actions/runs/45898422852), exact SHA `249b3e39d38d6bd7e71873ed83fcb670d5b73ae7`, completed SUCCESS. Release build, standalone installation and FIR check, strict Cold Smoke, local benchmark executable controlled-fixture smoke, and package publication all passed. [Download artifact 15154793507](https://github.com/casaferla/RGS-MasterLab/actions/runs/45898422852/artifacts/15154793507), 94,696,794 bytes, ZIP SHA256 `f9e70fa3ff45071bd567ace97f0f44870f6121b3b6b52233c2b6f134a039fdac`, expiry 2026-12-10. This is NON-FQ, and the candidate is now frozen for the physical acceptance checklist. The earlier six-rate matrix #2 remains the numerical evidence; the intervening changes were shared QML preset sizing, workflow diagnostics, and documentation. No physical playback or full-track musical result has yet been measured. Main remains unchanged.
