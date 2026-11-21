# RGS MasterLab — P2 component cost attribution, 2026-11-14

Status: **RESEARCH/FOCUSED CI CANDIDATE, NOT PHYSICAL PERFORMANCE QUALIFIED**.

## Verified prior baseline

Product L1+P2 cache LRU CI #43626588409: Windows/Linux **7/7 SUCCESS**,
SHA `168dfba63bf51f89d35f5ed8dfcd82bf534f12e1`.
Execution-layer CI #43628616803: Windows/Linux **7/7 SUCCESS**,
SHA `ddbc5b62333e534668b2a7ca95bd051164c29e35`.

The execution-layer profiler compares the *same* programme against a
bit-for-bit identical original P2 output and delayed EOS tail. Measured
quick 2s **quiet** runner results (NOT PO Windows timings):
- Windows 2022 P2 focused job at 44.1 kHz: direct 43.04 ms,
  genuine IModule 44.34 ms; 48 kHz: direct 47.57 ms, module 48.96 ms.
- Ubuntu P2 focused job at 44.1 kHz: direct 56.87 ms,
  module 57.00 ms; 48 kHz: direct 61.41 ms, module 61.37 ms.
- Windows GUI smoke at 44.1 kHz: direct 51.50 ms,
  module 52.67 ms; 48 kHz: direct 62.66 ms, module 64.79 ms.
Do not compare runners with the PO machine or infer sustained performance.
**Hypothesis:** bridge/epoch/IModule overhead is small compared with the
unmodified limiter itself on this short case, though longer and hot cases
still require measurement. These are sequential timing measurements, not
calibrated simultaneous paired statistics.

## New component profiling gate

`r1d_p2_component_perf_probe` compares the **original pinned**
`PredictiveMusic` controller and `Stream` M16 safety stage using a
block-level chronometer around each original stage. Independently,
the **unchanged** `CallbackEngine` consumes the exact same input and is
the per-sample binary64 oracle for **all** output frames and EOS tail.
The safety stage is fed exactly N post-controller programme frames;
no hidden M16 bridge zeros are published as audio. 44.1 and 48 kHz,
quiet/normal/hot Drive6, 300s physical mode; 2s quiet CI smoke.

`Run_P2_Component_Cost.cmd` writes
`bin/P2_COMPONENT_COST_5MIN.tsv` in the research bundle. The TSV
reports callback kernel time, predictive controller time, safety time,
EOS cost, number of exact compared samples, independent verified
frames/checks/sweeps. It does not touch the Product renderer, M16, M17,
M18, Final SRC, QML or `main`. No musical or True Peak performance
claims without hardware measurements.

**A failed oracle is a STOP**: study the discrepancy, do not accept an
invalid performance delta as evidence. The entire component pipeline
remains unchanged; this gate only adds instrumentation inside its own
standalone research executable and does not recompile callback probes
with timing counters.

## Next technical choice

After green CI and **physical** CSV collection, compare controller vs
safety time and the already measured full 5-minute costs. If safety
dominates, investigate a separate, opt-in experimental optimization
with strict floating-point order and M16 post-gain check on identical
PCM, stress-adversarial cases and fallback. If controller dominates,
profile its per-frame FIR+envelope/hold behavior. No speculative SIMD
or replacement of the certified True Peak safety proof. L1 M24 closeout
and minimum L2→L3 bridge remain the overall programme priority.

Research-only candidate. No FQ/release/merge authority implied.
