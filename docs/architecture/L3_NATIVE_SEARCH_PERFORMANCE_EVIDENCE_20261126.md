# L3 real native search performance evidence — 2026-11-26

Status: baseline telemetry passed Windows/Linux Focused #139; optional full-track benchmark on Writer, awaiting its own CI. The measurement
is diagnostic only and does not alter the frozen M09 selection, M10
coordinate search, 800 render/analyze attempt budget, M11 safety, M12
adoption or M13 independent verification.

The native Product search records monotonic elapsed milliseconds from
the start of independent Prepared/Reference native analysis until the
first freshly measured, hard-gated candidate carrying a valid native
prediction. Cache hits do not establish this first observation. It is
an internal admissible candidate, **not** an early GUI offer or a
promise that M12/M13 can already adopt it. The ordinary GUI still
offers the reconstructed winner after the completed search.

Track separately the original-pair, Fixed, seed and final search times,
real attempted/completed/failed evaluations, and exact completed-cache
hits. The real Product lifecycle test reconciles attempted evaluations
with the M10 batch ledger and logs these measurements at the selected
source rate. Do not include the separate independent M13 render in the
M10 attempt count.

The existing 12-second controlled mono gain fixture is not a
representative 5-minute multi-domain music benchmark. The Product
performance target (first admissible candidate within a few minutes,
final bounded search within a practical duration) remains to be
measured on representative full-track stereo audio and actual Windows
hardware. No elapsed time may substitute for quality or safety gates.

## Review and audit boundary

The completed candidate review now exposes the first admissible latency
separately from total search duration, with physical successes, failures
and exact cache hits. The timing line is suppressed when the worker's
physical-attempt ledger does not reconcile with the M10 refinement batch
or timestamps are missing/inconsistent. This is a display-only diagnostic;
it does not block a separately valid candidate or authorize its adoption.

The last physical attempt includes failed renders and never advances on a
cache hit. Preserved evaluator preflight rejections and budget refusals
are counted separately; these are not a count of every skipped direction
(the refiner can reject out-of-bounds or provably noncompetitive directions
before the preserved evaluator is entered). Unit tests use a deterministic
clock; the real Gain and owned-P2 tests reconcile native trial counts, and
the Windows Product lifecycle checks the review evidence after replacement.

## Optional full-track Windows benchmark

After the focused gate is green, run the existing native Product test
executable locally with an explicit pair of separately owned stereo WAVs.
No music files are committed or uploaded. The first version requires the
same native rate for both files, a SOURCE lasting 240–360 seconds, a
nonempty REFERENCE and an explicit rate from the six supported rates.
Choose a pair genuinely requiring correction across the artistic domains.
All four domains are requested at full Amount. The test retains the
real original-pair analysis, M10 measured winner, M11 safety, M12
adoption and independent M13 checks; it does not repeat the replacement
and Undo/Redo searches in this optional performance lane.

PowerShell example (use your own absolute paths and actual sample rate):

```powershell
$env:RGSML_L3_E2E_RATE = '48000'
$env:RGSML_L3_BENCH_SOURCE_WAV = 'C:\\Music\\source.wav'
$env:RGSML_L3_BENCH_REFERENCE_WAV = 'C:\\Music\\reference.wav'
$env:QTEST_FUNCTION_TIMEOUT = '4200000'
$exe = Get-ChildItem build-l3 -Filter rgsml_project_session_tests.exe -Recurse
if (@($exe).Count -ne 1) { throw 'Expected one built test executable' }
& $exe.FullName nativeMeasuredWinnerUsesRealQtM12AndM13 -o -,txt
Remove-Item Env:RGSML_L3_BENCH_SOURCE_WAV,Env:RGSML_L3_BENCH_REFERENCE_WAV,Env:RGSML_L3_E2E_RATE,Env:QTEST_FUNCTION_TIMEOUT
```

Run only after building the actual Windows Product test in Release and
configuring its Qt DLL path. The test's search wait ceiling is 45 minutes
and its independent M13 wait ceiling is 15 minutes, not target times or
solver termination controls. A failed or no-correction result is not
an accepted performance measurement for a candidate search. Record the
actual first admissible time, full search time, physical trial ledger,
separate M13 elapsed time, hardware, rate and duration. No 5-minute
music benchmark has yet been run by this change; numerical and formal
qualification remain separate.

## Actual baseline telemetry — Focused #139

Exact tested SHA: `b47b3b96219ae4dc62be0521b2b15b96c812ac8b`.
[Windows job 140050777225](https://github.com/casaferla/RGS-MasterLab/actions/runs/45860738130/job/140050777225), controlled 12-second mono 48 kHz fixture:

| Observation | Actual log value |
|---|---:|
| Original pair | 564 ms |
| Fixed | 271 ms |
| Seeds | 0 ms (integer millisecond resolution) |
| Search | 3695 ms |
| First admissible | 1106 ms |
| Attempted/completed | 12/12 |
| Failed/cache hits | 0/0 |
| Evaluator rejected/budget refused | 0/0 |
| M13 required/configured components | 491/751 |

M13 reported only `VERIFICATION_PROFILE_UNAVAILABLE` at the formal
approval reference; numerical evidence passed. The 12 s fixture does
not establish a runtime scaling law or performance of representative
stereo music. The separate native owned-P2 fixture measured 12 attempts,
6 owned-P2 attempts and 12 exact fixed-prefix uses.
