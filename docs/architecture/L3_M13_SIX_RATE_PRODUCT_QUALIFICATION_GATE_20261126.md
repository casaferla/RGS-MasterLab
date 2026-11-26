# L3 native Product M12/M13 six-rate gate — 2026-11-26

Status: **controlled native Product lifecycle passed at all six rates on revision `ea15ebe4af814b6ac1f0cfb2757a0c11643600c7`**. This is a development qualification checkpoint, not formal QTP approval or a general multichannel/multidomain music qualification. The macro-lot CI
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

The real ProjectSession test depends on the Windows Product resource
reader/writer path. Linux continues running the native M12/M13 unit and
integration macro suite; it is not a substitute for the Windows
six-rate Product lifecycle.

The dedicated workflow `.github/workflows/l3-six-rate-qualification.yml`
runs six independent Windows jobs on an explicit fast-forward of
`staging/l3-six-rate-qualification-20261126` to a green reviewed revision.
Do not push the Writer head or launch this qualification during an
unvalidated Focused run. Each job selects one exact rate through
`RGSML_L3_E2E_RATE`, requires the matching PASS data tag, and retains
the report and commit/rate provenance even when a job fails. It does
not run on ordinary Writer/Focused updates.

Windows PowerShell, optional local all-rate run:

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

## Recorded qualification run — 2026-11-26

- Exact tested SHA: `ea15ebe4af814b6ac1f0cfb2757a0c11643600c7`.
- [Six-rate Windows run #1](https://github.com/casaferla/RGS-MasterLab/actions/runs/45860788999): completed SUCCESS, all six jobs SUCCESS.
- 44100: job 140050954589; 48000: 140050954344; 88200: 140050954421; 96000: 140050953901; 176400: 140050954248; 192000: 140050954371.
- [Focused #138](https://github.com/casaferla/RGS-MasterLab/actions/runs/45843548670): same tested SHA, Windows/Linux SUCCESS.
- [Focused #139](https://github.com/casaferla/RGS-MasterLab/actions/runs/45860738130): subsequent telemetry-only revision `b47b3b96219ae4dc62be0521b2b15b96c812ac8b`, Windows/Linux SUCCESS. It does not supersede the six-rate evidence at its original SHA.

Retain these exact revision boundaries. The six jobs prove the controlled
Product lifecycle on native SOURCE rates, not a general claim about every
possible DSP topology, arbitrary reference pair, 5-minute runtime or formal
QTP release approval.
