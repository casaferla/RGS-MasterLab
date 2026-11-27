# Product closure decisions — 2026-11-27

Product owner approved retiring the standalone Master Audition selector row and enabling permanent, genuinely editable Dynamic EQ and Transient Shaper, including fine tuning after AutoMatch. These are distinct work packages under the single-writer epoch.

## A. Master Audition
- Shipping Product does not enumerate MASTER_MONITOR in the visual chain. Legacy isolated diagnostics retain their editor until regression qualification.
- Keep TerminalMonitorViewModel, native private MASTER/DEVICE ownership, immutable verified MASTER, RAM/SRC admission, exact device return/recovery, global transport/region and WAV Export semantics intact.
- Exceptional no-DEVICE offline render formerly exposed by MonitorEditor must remain accessible from Render Master, using the existing explicit native authorization. Cancellation must dispatch to the actual owning transaction, never select an owner by a shared status string.
- Test regular render/listen/return, different rates, memory caution, offline DEVICE-refused Source, cancellation/recovery, project rebuild and selection fallback. Remove legacy QML only after verified parity.

## B. Manual DSPs — implementation pending, not a claim of qualification
Current Product MasteringChainState has no native MANUAL parameter storage or execution bindings for C01 Dynamic EQ or C00 Transient Shaper. The existing linked editors only read a current immutable M12 ticket and deliberately reject mutation. Do not simply flip manualEditable or inject synthetic DSP rows.

1. Establish registry/typed MANUAL topology and execution bindings with stable IDs; neutral/bypassed defaults for new sessions and a backwards-compatible project migration policy. Proposed manual order: Gain, Parametric EQ, Dynamic EQ, Transient Shaper, Compressor, Stereo Imager, optional P2. Confirm registry constraints and native render order in tests rather than overriding them. No duplicate processing of linked and manual corrections.
2. Wire native typed ViewModels, draft/commit/cancel, bypass/reset/history and global status validation. Preserve single admitted mutation and preview rebuild only on accepted commit.
3. Add project/preset serialization and restoration of typed parameters, instance IDs, bypass and history contracts. Test old project restoration without silent audio changes.
4. Define the transaction for explicit fine tuning of an adopted REF_LINKED result: preserve the original accepted receipt for history; any changed candidate must have a new authoritative identity and invalidate numerical and terminal verification; avoid implicit duplication, silent mutation of linked tickets, or falsely retaining verification. Specify how subsequent search, suspend, resume, replace and undo interact with manual edits before releasing the control.
5. Test exact parameter/PCM replay, neutral defaults, Undo/Redo, REF_LINKED transitions, M17/M18 invalidation, project round trip and all six supported sample rates (44.1/48/88.2/96/176.4/192 kHz). Test at minimum and larger window geometry. No manual availability advertised until genuine native commit/render path passes.

## Remaining Product UI checks
Active status register, simultaneous diagnostic persistence, hover behavior, true maximized screenshots, and removal of the user-facing independent AutoMatch M16 safety ceiling remain separate verification/implementation items. Keep the independent internal safety policy separate from solver-owned P2 and M17 Export TP Limiter.

## Fine-tuning transfer preflight — implementation update

The transfer draft stages the target MANUAL module with the exact C01/C00 typed payload and disables only the corresponding REF_LINKED instance. Other REF_LINKED nodes, their order and all typed payloads remain unchanged. The original immutable ticket is retained. The draft rejects stale ownership, ambiguous multiple active sources, a missing or active destination, invalid typed settings, and a remaining conflict. Both structural revisions advance. This is a candidate only; it cannot publish Project state, authorize M13 verification or issue an M12 receipt.

The final Product transaction must produce a distinct derived-plan identity, keep the original exact accepted snapshot for Undo, and publish both sides under one exclusive epoch. Reusing M10/M11 evidence for the altered execution order is forbidden. A fresh native render and independent checks are required; a successful transfer does not promise PCM equivalence. No shipping button is enabled until this ownership/persistence/verification path is implemented and exercised.
