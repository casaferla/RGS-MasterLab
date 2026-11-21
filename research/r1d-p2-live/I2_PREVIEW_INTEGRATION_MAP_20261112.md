# Streaming True Peak Limiter — minimum path to an **audible** DSP Chain preview

**Research-to-product handoff only; approved UI title and icon, no productive changes here.**

## Why the GUI isn't visible yet
- `dsp::ProcessingChain` enforces order/identity and is *metadata*, not a PCM runner. I1.7 confirms the isolated creation and dispatch, **not** the Product audio route.
- `app/src/mastering_chain_state.cpp`: Product default/restore authority accepts fixed Gain, EQ, Compressor, Stereo/M-S; no P2 instance ID, parameter authority, or execution binding exists yet.
- `app/src/dsp_chain_adapter_model.cpp`: only explicitly known type IDs create QML adapter rows; P2 has no branch. M16 and M17 rows are separately appended; adding a cosmetic item would be misleading.
- `ui/qml/DspChainSelector.qml` uses fixed `moduleIconKind(editorContentKey)` and icons from `StudioIcon.qml`; its icon/title mapping must add P2 without reusing M17 shield. The research `StreamingTruePeakLimiterIconResearch.qml` is a concept only; no Qt rendering performed.
- `ui/qml/DspEditorHost.qml` has per-module editor selection; no P2 controls/view model exist in Product. Use existing font/tooltip/glow, 1184x688 minimum and 16px pictogram, text **Streaming True Peak Limiter**, Copper `#C4774A`.
- `dsp/src/module_registry.cpp` Product factory package v1 is frozen; isolated registry tests must not be mistaken for a release. New type ID candidate `rgsml.dsp.streaming-true-peak-limiter` is nonfrozen and must not collide with M17 type ID `rgsml.dsp.true-peak-limiter` or M16 Live Meter.

## Minimum manual preview: order of work
1. Ask coordinating L1 epoch authority for isolated Product corridor, without editing existing PRs or frozen workflows. Branch from an approved consolidated baseline after L1 changes. **No merge yet.**
2. Define user-editable P2 parameters and sonic identity/versioning and decide explicit bypass/latency semantics. Keep M17 terminal unchanged; new P2 is last **MANUAL** audio processor, before M16 presentation-only Meter. Limit startup to research profile η2, ceiling -1 dBTP; **do not freeze** these as Product defaults without review.
3. Wire a genuine P2 module factory to the preview's registry and `MasteringChainState`, its `execution_bindings()`, and the preview renderer; ensure the presenter binds to *processed P2 PCM*, not input/old M17. Add gap/crossfade or safe mute on seeks/loops/parameter/bypass commits under host quiescence, do not claim click-free transitions without measurement.
4. Add QML adapter and panel (Ceiling, Drive, Bypass, reduction telemetry only if real and trustworthy), waveform-under-ceiling icon and approved dimensions/glow/tooltip; prevent UI-only false active indication.
5. Cross-compile or run MSVC/native Windows x64 test, actual Qt startup at min size, 48/192k sample processing and M16 true-peak verification. Distribute isolated preview build to PO only after green preview gate; Windows portable no-PowerShell runner is a separate physical-gate convenience deliverable.

## AutoMatch sequencing
Do **not** block first audible manual-preview GUI on AutoMatch. I1.8 confirmed bit-exact full re-render on repeated candidate after unrelated settings, with a brand new `IModule` per evaluation, and no checkpoint. Canonical AutoMatch cost and optimizer decisions are later. Do not run full M17 in the repeated solver loop; M17 remains final output-rate after Final SRC.

## Promotion gate
Separate explicit PO authorization is still required for Product merge or release. Main, M17, L1 active branches/PRs and CI/FQ remain untouched in I1.8.
