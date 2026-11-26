# L3 native Product M12/M13 six-rate gate — 2026-11-26

Status: **implemented test entry, not yet qualified**. The macro-lot CI
retains the ordinary 48 kHz full native Qt lifecycle row. Before
declaring the L3 Product checkpoint stable across all SOURCE processing
rates, run the full lifecycle with `RGSML_L3_ALL_RATE_E2E=1` on the
supported build and record the per-row results and exact commit.

The six rate rows are 44.1, 48, 88.2, 96, 176.4 and 192 kHz.
Each generates separate actual PCM24 source/reference WAV resources at
its own rate, with the same 12 s duration and exact gain ratio.
The existing native search/adoption/replacement, Undo/Redo, stale and
Amount-zero lifecycle assertions stay in force; M13 must independently
render the physical full master and measure M16/M19/M20 at that rate.
The fixture is a controlled mono gain case, not an artistic multi-domain
benchmark or proof of every DSP at every rate.

Linux, from a configured and built tree:

```sh
RGSML_L3_ALL_RATE_E2E=1 ctest --test-dir build-l3 --output-on-failure -V -R '^rgsml_project_session_tests$'
```

Windows PowerShell:

```powershell
$env:RGSML_L3_ALL_RATE_E2E = '1'
ctest --test-dir build-l3 -C Release --output-on-failure -V -R '^rgsml_project_session_tests$'
Remove-Item Env:RGSML_L3_ALL_RATE_E2E
```

For diagnosis use QtTest's function:data-tag selector with the existing
test executable and the same environment. No sample-rate substitution,
no M17 in the candidate chain, no artificial verification evidence.
A failure is a qualification blocker, not permission to omit a rate.
This gate does not confer formal QTP approval or physical Windows FQ.
Capture actual elapsed time and the source rate from the native timing
log; the 12 s fixture does not establish the target 5 min track runtime.
