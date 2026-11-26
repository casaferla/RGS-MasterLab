# L3 integrated Windows physical acceptance — NON-FQ

This checklist is for an isolated CI artifact, never a release approval. Keep the last accepted FQ installation, original WAVs and project files untouched. Extract the entire archive into a new writable folder; run bin/RGSMasterLab.exe. Record the CI SHA, Windows version, CPU, RAM, audio device and device rate. The normal package does not enable the optional P2 research marker; if required, use the existing explicit P2 opt-in switch --rgsml-p2-i2-preview and record that choice separately.

## A. Startup and real candidate

- Verify Source/Reference loading, PREPARED and original listening. Select an appropriate source/reference pair requiring correction.
- Request AutoMatch, record first admissible candidate and completed search times separately, candidate costs and diagnostic failure/cache-hit counts; do not equate an admissible candidate with a completed winner.
- Adopt the measured winner through the real Product action. Record materialization and independent verification status verbatim. Do not claim formal QTP approval if unavailable.
- Inspect actual REF_LINKED Dynamic EQ and Transient Shaper rows only when those modules are genuinely present in the selected plan. A winner without either module is not evidence of that editor.

## B. Editor and Activity checks

- Compare docked and floating geometry at 1184 x 688 and larger; DPI 100% and 150%, one and two screens where available. Check no overlapping controls, fixed header placement, and readable numeric fields.
- Dynamic EQ: same graph/spectrum positions as Parametric EQ; read-only native band values, six dynamics values, separate Dynamics and Activity windows. Compare Activity band labels/colors with the actual enabled bands.
- Transient Shaper: Attack/Sustain native values and integrated Activity History. Manual changes and preset application must remain locked in REF_LINKED mode.
- Play actual PROCESSED audio, then Pause, seek forward/back, enable Loop and observe wrap, Stop, switch PREPARED and REFERENCE, return PROCESSED. Activity must never show foreign or future PCM; unavailable/transition are legitimate gaps. Check frozen Pause and no Activity in independent audition.
- Close/reopen the editor, dock/undock, change Source/Reference, save a new project, reopen and compare native parameter identities. Exercise Undo/Redo where available. Record a screenshot or video of each failure.
- Repeat using all six supported source rates (44.1, 48, 88.2, 96, 176.4, 192 kHz); CI-controlled six-rate evidence is not physical device evidence.

## C. Output separation

- For a complete accepted candidate, check Render Master / Listen to Master and WAV export separately from AutoMatch. M17 Export TP Limiter remains terminal, not an AutoMatch control. Use new output names, never overwrite original inputs.
- Record output rate, sample format, ceiling and measured TP, including an optional P2 opt-in run if applicable.

## D. Full-track musical performance

Use the optional `benchmark` directory in the standalone package (same-rate stereo WAV pair; source 240–360 seconds). From the extracted folder, run `powershell -File .\benchmark\run_l3_local_music_benchmark.ps1 -SourceWav 'C:\Music\source.wav' -ReferenceWav 'C:\Music\reference.wav' -SampleRate 48000 -ReportDirectory '.\local-benchmark-reports'`. Change the paths and rate to the actual files. The package includes the matching native test executable and the performance-evidence notes; this is not a physical-device timing certification. Keep audio local. Report wall time, first admissible latency, full search, independent M13 duration, physical evaluation ledger and numerical acceptance. A failed/no-correction result is not an accepted performance measurement. The 12-second controlled fixture does not predict music performance.

## Result

Mark each actual observation PASS / FAIL / NOT RUN with evidence; preserve any errors and exact artifact SHA. Neither a successful cold smoke nor a visually correct screenshot qualifies physical playback or formal FQ.
