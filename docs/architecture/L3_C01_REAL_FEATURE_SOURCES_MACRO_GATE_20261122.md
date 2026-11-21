# L3 C01 Macro Lot #26 — Product M05/M06 Real Feature Sources

Baseline #25 focused GREEN both platforms: 0babfe1091a43fd30e145242f2a96fbf3c5a7844.
Writer accumulated twenty consecutive commits plus this single
focused CI registration, without per-commit CI.

Real analysis substance:
- D03/D04 real PCM native complete 400ms/Grid100 RMS and same-window
  crest with C*N power and digital floor 1e-12, efficient 100ms
  block ring and Type7 selectors; missing in denominator.
- D02 EBU LRA native M19 Short-term gating and DISCRETE nearest
  percentiles; ≥60s active and ≥100 retained with separately
  attested file-based 1.5s trailing policy.
- D01 real M19 native centered Type7 ST shape and one-time MIN support.
- Existing true M19 scalar reuse D05,D06,L01,L02,L03.
- Existing true M20 Stereo S01/S02 reuse (mono endpoint, native
  Fisher-Z quantiles, separate safety) and Tonal T01..T06
  (primary 1/12 configured grid and common PSD/band OLS).
- Product native feature bundle consumes one actual rendered PCM
  plus its already-measured true M19/M20, not a second FFT/render.
- Exact M07 27-slot complete-or-partial generation and collected
  native fragments: 17 original slot IDs have source-level adapters;
  D07–D12 and S03–S06 remain explicitly unimplemented native sources.
  Slot count is not measured readiness or actual AutoMatch success.

Twelve new focused Qt native suites plus existing Product/DSP/CI
regressions are run on Windows MSVC and Linux with warnings-as-errors.
This gate tests the new source/read-only glue only. It does not certify
a complete Phase6 FeatureDefinitionRegistry, M11 provider source,
M12 REF_LINKED, M13 verifier or full AutoMatch audio output.
No merge into main; no release or final qualification label.
