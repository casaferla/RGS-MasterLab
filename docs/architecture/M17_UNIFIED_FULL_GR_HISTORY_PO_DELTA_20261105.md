# M17 post-release UX — Single Full-Master GR HISTORY

**PO directive: 2026-11-05.** Status: **isolated staging / no PR qualification / NO MERGE**. This replaces the same-day proposal for a GR LIVE ↔ OFFLINE toggle in issue [#81](https://github.com/casaferla/RGS-MasterLab/issues/81), which is updated in place.

## One visualization only
- The M17 editor has **one** GR HISTORY: the entire currently prepared MASTER's *offline sampled* gain reduction graph.
- Eliminate the historical LIVE GR graph in M17 UI and the proposed future LIVE/OFFLINE switch. Do **not** remove the independent M16 True Peak live meter, consumed-output authentication path, internal witness/owner/epoch guards, offline observation provenance, M17 core/DSP/limiter, FIR SRC, Verification Trim or PCM.
- Overlay same visual playhead as main waveform: yellow `#F2B632`, vertical line and top triangle; use the same authoritative position and FULL-TIMELINE fraction, not a separate timer.
- Build a copper gradient region **under the GR profile** between the first point and current playhead. Increase saturation towards the playhead; do not paint a solid rectangular progress block. The full precomputed curve remains visible beyond the cursor.
- On seek backwards the mask contracts; pause freezes; resume advances; rate conversion and Master Audition use the same duration-normalized timeline. When no authoritative timeline or current MASTER is available, omit the playhead/gradient and never fabricate consumed GR.
- The existing 256 offline buckets report peak of bounded sampled gain frames, not a guaranteed peak of all frames; draw only reported samples and do not label a bucket an exact frame. Use `frameBegin` and `frameEndExclusive` where possible.
- A measured NON-CONFORMING owner may display authentic offline applied gain but must remain explicitly nonconforming; bypass/no owner/invalidated source remains empty/unavailable.
- UI readout must fit min main 1184×688, 42/54px GR lane, 4K+FHD and floating/docked, without tooltip-only overflow.

## Ownership and tests
1. Derive UI position from waveform-aligned canonical SOURCE/Master clock, with no duplicate playback timer. Validate source vs master sample rate, seek, loop, pause/resume/stop, return and absence of device.
2. Clear/invalidate old curves whenever current MASTER ownership is lost or invalidated. Reuse existing M17 source-bound offline capture. Distinguish cursor position (navigation) from Windows consumed-output attestation.
3. Test QML component for empty, bypass, corrupt history, 0/50/100%, seek backwards, snapshot removal, long file, 192→48, direct Preview and separate Master Audition; enforce no LIVE graph/toggle in active UI.
4. Do not modify M17 audio results or event/epoch evidence. Run Windows+Linux focused preproduction CI only after cohesive staged macro-lot and test coverage.

## Branch and integration gate
- Sole no-CI staging: `staging/m17-gr-playhead-preproduction-20261105`.
- Separately promoted CI branch: `experimental/m17-gr-playhead-20261105` and Draft PR (NO MERGE).
- Stage coherent macro-lots; expected-SHA non-force FF to CI branch once; no CI per tiny commit.
- Integration **after** formal post-M17 CI reset/audit closure and M18 integration+qualification, **only with explicit PO approval**; coordinate with subsequent M19–M22 gates. Frozen M17 FQ #97 remains unaffected.
- Current state: documentation only. UI implementation, CI and physical Windows acceptance **not yet claimed**.

## Sources of authority
- [Replaced/updated issue #81](https://github.com/casaferla/RGS-MasterLab/issues/81)
- Current M17 merged code on `main` as of branching: `304f593c89e7079aefa31727055b0880243245e4`
