# RF-1 — Post-L1-M03 evidence checkpoint

## Checkpoint status

- Date: `2026-09-10`
- Baseline: `d6b265f96a2d50440fb0631914c6b372878aa4e9`
- Branch: `task/L1-M03-windows-wav-playback`
- Scope: actual implementation evidence through L1-M03, including the
  R12-01 device limitation and the authorized playback-rate recovery.
- Decision: `KEEP_BASELINE`
- Frozen L1-M03 estimate: `2 / 3 / 5 PDE`, unchanged.
- Recovery envelope: `2 / 3 / 5 PDE`, recorded separately and not converted
  into a milestone or a retroactive estimate.

This checkpoint is prospective implementation evidence. It does not rewrite
the Phase-12 roadmap, milestone sequencing, scope, DoD, or any frozen estimate.
Wall-clock/agent hours and PDE are distinct measures; no automatic 1:1
conversion is made.

## FROZEN PHASE-12 BASELINE — IMMUTABLE

The following values are transcribed from the consolidated Task 007 packet.
The packet supplies cumulative Most Likely PDE and calendar ranges for these
endpoints; it does not supply separate cumulative Optimistic/Conservative PDE
columns, so none are reconstructed or interpolated.

| Endpoint Fase 12 | PDE Most Likely frozen | Calendario owner + agent intensivo frozen |
|---|---:|---:|
| First Real Remaster | 76 | 11–16 settimane |
| Artist Alpha Windows | 119 | 17–25 settimane |
| Alpha + Automatic Reference Match | 182 | 26–37 settimane |
| Feature-Complete Windows | 537 | 72–96 settimane |
| Feature-Complete Windows + Android | 662 | 89–116 settimane |
| Qualified Release Candidate | 772 | 106–136 settimane |

## Task 001–007 evidence

`0` owner/review hours means that no measured value was supplied; it is not an
estimate. `N/A` means the accepted repository contains no versioned telemetry
for that field. Agent/wall-clock duration across interrupted sessions was not
captured reliably and is therefore not fabricated.

| Task | Frozen O/ML/C PDE | Owner h | Review h | Codex iterations | Configure/build/test cycles | Defects | Regressions | Blocked h |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 001 / L0-M01 | 2 / 3 / 5 | N/A | N/A | N/A | N/A | N/A | N/A | N/A |
| 002 / L0-M02 | 1 / 2 / 3 | 0 | 0 | 4 | 7 | 3 | 0 | 0 |
| 003 / L0-M03 | 2 / 3 / 5 | 0 | 0 | 5 | 9 | 2 | 0 | 0 |
| 004 / L0-M04 | 1 / 2 / 3 | 0 | 0 | 5 | 13 | 3 | 0 | 0 |
| 005 / L1-M01 | 2 / 3 / 5 | 0 | 0 | 8 | 20 | 2 | 0 | 0 |
| 006 / L1-M02 | 2 / 3 / 5 | 0 | 0 | 10 | 16 | 4 | 0 | 0 |
| 007 / L1-M03 | 2 / 3 / 5 | 0 | 0 | 9 | 18 | 1 | 0 | 0 |

Task 007 counts nine discrete implementation/resume cycles and eighteen
observable configure/build/test command groups, including development,
generator/oracle, regression, tests-off, deploy, and post-listening-finding
revalidation. Blocked duration was not instrumented, so `blockedHours` remains
zero rather than encoding an invented elapsed time.

## L1-M03 evidence and velocity

### Pre-finding implementation

- The accepted Task 004 `IAudioPlaybackService`, Task 005 `WavReader` and
  canonical `AudioBuffer`, and Task 006 `WindowsResourceReader`/Source
  transaction were reused.
- A bounded private Windows `QAudioSink` adapter, deterministic control plane,
  PCM bridge, minimal transport, tests, deploy, and initial device probe were
  implemented before the rate limitation.
- The first audible playback was reported as immediate; no instrumented
  sub-second duration is available.

### R12-01 trigger

- The sanitized probe enumerated three outputs.
- On the probed default, 44.1 kHz stereo Float32 and PCM16 exact support were
  false; 48 kHz stereo Float32 was true and PCM16 was false.
- Therefore 44.1 kHz could not satisfy the former exact-rate-only requirement,
  activating `R12-01 — TRIGGERED / PLATFORM_LIMITATION CONFIRMED`.
- No device identifier, provider locator, user path, or private audio identity
  is versioned.

### Recovery effort

- The authoritative delta allowed only exact playback-rate pairs
  `44100<->48000`, with exact-rate bypass remaining first priority.
- One MPFR/gmpy2 generator materialization, one deterministic `--check`, and
  one independent response-oracle run produced and verified the two frozen
  coefficient assets. The external offline environment is not a production
  dependency and its machine-specific location is not recorded.
- The first successful 44.1→48 playback was again reported as immediate; no
  instrumented elapsed duration is available.
- Listening attempts: one primary-output attempt exposed a reproducible
  GUI-interaction dropout, one complete primary-output retest passed after the
  scheduler correction, and one complete secondary-output retest passed.
- The corrected product defect was main-event-loop starvation of the refill
  timer during native resize/minimize/maximize. Timer, decoder, sink, and pump
  ownership now reside on a dedicated Qt worker event loop; the real-time
  callback still only drains the bounded queue.

### Toolchain, quality, and gate evidence

- Local accepted toolchain: MSVC x64, Qt 6.8.3 Multimedia, CMake, Windows SDK,
  and Git; no dependency was downloaded or installed by the agent.
- Both kernels reached Ziv-stable IEEE binary64 rounding at 512-bit precision.
- Independent response oracle: passband ripple `0.000012857 dB` for both
  directions; stopband attenuation `121.631422 dB` for 44.1→48 and
  `121.632201 dB` for 48→44.1.
- Final inventory: 34 enabled CTests; Debug 34/34 and Release 34/34 PASS.
- Tests-off configure/build passed with zero test artifacts.
- Staged deploy smoke and normal close both returned exit code 0.
- ToolchainManifest build/install identity passed at SHA-256
  `49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11`.
- Primary headphones and secondary display output both passed 44.1→48,
  48-kHz exact playback and the required transport/EOF/window-interaction
  checks. No remaining reproducible dropout or backend issue was reported.

## R12-01 status

```text
R12-01
LIMITATION_CONFIRMED: exact 44.1 kHz unavailable on acceptance outputs
MITIGATION: playback-only 44100<->48000 deterministic SRC
POST_DELTA_RESULT: PASS on primary and secondary Windows outputs
STATUS: MITIGATED_FOR_WINDOWS_L1-M03_44100_48000
RESIDUAL_RISK: OPEN
```

The frozen probability/impact `M/H` is unchanged. Residual risk remains for
rates other than 44.1/48 kHz, untested devices and drivers, driver-side
processing, Android, latency/bit-transparency claims, and Q0–Q6 qualification.

## RF-1 CURRENT FORWARD FORECAST

| Forecast scope | Optimistic PDE | Most Likely PDE | Conservative PDE | Calendar range | Confidence | Driver |
|---|---:|---:|---:|---|---|---|
| L1-M03 frozen task estimate | 2 | 3 | 5 | Completed at checkpoint; no frozen task calendar supplied | High on completion evidence | All I+S+M+L gates passed |
| L1-M03 recovery envelope | 2 | 3 | 5 | No separate frozen calendar supplied | Medium | Two-rate/two-device recovery succeeded; broader matrix remains unqualified |
| Phase-12 cumulative endpoints | NOT_SUPPLIED | unchanged frozen ML values above | NOT_SUPPLIED | unchanged frozen ranges above | Medium | Do not interpolate absent cumulative O/C values; hardware diversity remains the main driver |

Decision: `KEEP_BASELINE`. The evidence closes L1-M03 and mitigates the
observed two-rate Windows limitation without establishing a defensible reason
to alter the frozen Phase-12 cumulative forecast. The decision applies only
prospectively; no past estimate is rewritten.

## Assumptions, evidence gaps, and next checkpoint

- Owner effort, review hours, cross-session wall-clock agent time, and exact
  blocker duration were not supplied or reliably instrumented.
- The gate is playback sanity, not device-class, latency, DAC
  bit-transparency, Android, or Q0–Q6 qualification.
- Only 44.1/48 kHz mono/stereo and the two acceptance outputs are covered by
  this mitigation.
- No forward work is authorized here. The next forecast checkpoint remains
  the one established by the authoritative roadmap; Task 008 is not prepared
  or started by this checkpoint.
