# R1d-P2 I2 — Physical Windows audition findings (PO report, 2026-11-12)

## Scope / provenance
Research-only Windows x64 I2 ZIP; **no Product merge**, no M16/M17 frozen contract modification, no FQ, no AutoMatch authorization.
Hardware findings are **user-reported**; they have NOT been independently reproduced in CI or certified as callback/dropout telemetry.

**File:** "Reati universali (Remastered) (Edit) 44.wav" (approx. 3:19.184), stereo PCM 32-bit, **44,100 Hz**.
**Windows shared-mode output:** speaker default **48,000 Hz / 16-bit**. Available: 44.1/48/96/192 kHz at 16/24-bit; no statement that RGS's actual QAudioSink format is necessarily 24-bit in 24-bit shared mode.

## Observed, reported by PO
1. P2 Ceiling and Drive change sound, bypass nearly instant, active P2 re-render 4–5 s. Playback remains audible throughout the full-track render.
2. **All DSP parameter edits** incur 4–5 s global RENDERING, even when P2 is bypassed. Hence this duration is not attributable to P2 process cost alone.
3. After publishing a new PROCESSED realization during continued playback, M16 LIVE TP stops displaying valid values. Manual PREPARED -> PROCESSED reselection makes it work again.
4. PREPARED -> PROCESSED while PLAYING produces a brief audible discontinuity. Seek while PLAYING also produces a brief pause.
5. Loop Region appears to work. Other files/rates/devices have not yet been physically tested.

## Code-root analysis (source inspection only)
- `MasteringPreviewController::request_preview` sends the **whole PREPARED owner** through `render_preview` and replaces the whole PROCESSED owner after the worker completes. It does not cache preceding DSP stage outputs, even for a change to P2 alone. Active P2 and bypass P2 are not separable from total render wall time yet.
- `AuditionSourceSelector::switch_to_after_authorized_mutation` in the ordinary path calls `stop_and_clear`, then `prepare_realization` and `playOrResume`. The research-only P2 opt-in branch now attempts a same-format *already PLAYING* handoff instead. Requires CI and hardware gate.
- `PlaybackEngine::seek` stops the sink and clears its queue by design (code shared with production), so sample-accurate, truly gapless seek needs a separately proved mechanism.
- `PlaybackEngine::c2_processed_coverage_probe` can remain `UNPROVEN_HANDOFF` after a SRC crossfade (sticky `c2AcceptedContainsUnprovenHandoff_`); `c2_src_retirement_observation` currently refuses that path. This is consistent with M16 stopping until a fresh source session is selected. **Do not disable provenance checks** or claim M16 qualified.

## Research-only changes in current candidate
- P2 Ceiling copper track fills from 0 dBTP to chosen negative ceiling (same convention as M17).
- Opt-in same-format, currently-playing PREPARED/PROCESSED handoff without stop/clear and with atomic failure behavior. Regular Product path unchanged.
- Dedicated test of 44,100 Hz, no stop/clear, and failure atomicity.
- Isolated `bin/P2_I2_RENDER_TIMINGS.tsv` logging of full DSP render wall-time, frames, input rate, module count, and P2 active/bypass. This is NOT a per-module profile or WASAPI callback latency.

## Unresolved gates — do not claim fixed
- M16 LIVE SRC-after-handoff provenance recovery: needs an independently owner-stamped post-crossfade consumed OUTPUT suffix and precise SRC FIR retirement proof; crossfade/GAP cannot be silently upgraded.
- Gapless seek: requires sink prefill/scheduled crossfade and playhead identity consistency; no implementation qualified.
- Full-streaming DSP/AutoMatch performance: require separated P2 stage timings, incremental stage caching, controlled parameter epochs, stress benchmarks and physical dropout/callback evidence.
- Physical A/B verification on the user's actual Windows 10/11 x64 device after the new CI candidate is green.

## Suggested repeat matrix
1. Same 44.1k file, Windows shared output 48k/16-bit — reproduce current behavior.
2. Same 44.1k file, output 44.1k/16-bit — test no SRC path.
3. Optional output 48k/24-bit — isolate shared-mode bit-depth change.
4. Report M16 behavior after manual DSP edits and after research A/B transitions.
5. Preserve WAV and full exact device details; attach timing TSV and if possible physical audio logs.
