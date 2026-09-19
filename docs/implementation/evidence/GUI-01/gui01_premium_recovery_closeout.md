# GUI-01 Premium Recovery — functional blocker and technical closeout

Authority: GUI-01 Functional Blocker Resume and Closeout Execution Delta v1.1.
Branch: `task/GUI-01-core-shell`. Main anchor:
`88829c376e18541848dc471e279ffc7c3cabe179`.
Pre-Premium GUI-01 parent:
`2455b1a78a256af1632101116870da3dcfebd745`.

## 44.1 kHz blocker

The current default output supports stereo Float32 at 48 kHz, but neither
authorized exact stereo format at 44.1 kHz. Thus a 44.1 kHz Source selects the
accepted 44.1→48 kHz playback SRC, while a 48 kHz Source selects exact-rate
48 kHz. The output selection, SRC ratio/frame map, Qt sink format and
worker-thread pump are unchanged from the accepted pre-Premium backend.
The sink drains a bounded byte queue in its callback; a separate 10 ms worker
timer refills it. Its processed time is converted using the negotiated output
rate. No UI event loop participates in refill.

An isolated, ten-second stereo canonical-PCM playback test reproduced the
performance contrast in the original Debug build:

| Path | Original Debug preparation | Pre-fix Release preparation |
| --- | ---: | ---: |
| 44.1→48 kHz SRC, 480,000 output frames | 31.49 s | 4.92 s |
| 48 kHz exact, 480,000 output frames | 0.29 s | not needed |

The 192-tap SRC translation unit was compiled with the normal MSVC Debug
optimization/inlining disabled. It could not refill ten seconds of output
within ten seconds, matching the reported slow/fragmented Debug playback.
The frozen algorithm, coefficient asset, arithmetic order and public APIs
were not changed. `audio/CMakeLists.txt` now applies Release-equivalent
optimization/inlining only to this translation unit in Debug, and disables
incompatible MSVC runtime checks only there. Debug symbols and strict
floating-point settings remain in force; all other translation units retain
their existing configuration. Attempts to optimize the pump or change the
SRC loop were removed because they brought no measured benefit.

The new `tenSecondSourceMaintainsOutputRateDuration` regression covers
44.1→48 and 48 exact: each produces exactly 480,000 output frames and the
correct ten-second Source-frame endpoint. The offline budget is under 12 s
to allow bounded host scheduling overhead; this newly added timing bound
was adjusted from its initially tighter development value, while no
pre-existing assertion, validation, or test was removed or weakened. The
real-device probe below checks wall-clock progress more tightly. Corrected Debug SRC preparation
was approximately 9.2 s for ten seconds of output; 48 exact remained
approximately 0.3 s. The Release case was approximately 4.9 s.

An opt-in real-device probe, `realDeviceTenSecondRateProbe`, runs through
the actual Windows audio adapter using silent ten-second canonical-PCM
Sources. At eight seconds, a valid PLAYING snapshot must be between seven
and nine seconds at both input rates, with no sink error:

| Config | Source rate | Source-frame position after ~8 s | Source time |
| --- | ---: | ---: | ---: |
| Debug | 44,100 | 359,648 | 8.155 s |
| Debug | 48,000 | 387,134 | 8.065 s |
| Release | 44,100 | 356,120 | 8.075 s |
| Release | 48,000 | 387,614 | 8.075 s |

Both configurations passed. The Debug direct-run probe printed Qt's
`No QtMultimedia backends found` warning, but `QAudioSink` and timing
worked; the deployed Release smoke emitted no warnings. Audible quality
remains for the Product Owner's real-executable inspection, not
auto-certified by this silent probe.

## Final automatic qualification

- Current Debug build: PASS. Full CTest: 64/65 on first pass; the sole
  `install_deploy_smoke` failure was an old staged Debug executable held
  open by the host. That instance and its old Release counterpart were
  closed normally. The smoke alone was rerun and PASS: effective 65/65.
- Fresh Premium Recovery Release build: PASS; complete CTest 65/65.
- Dependency/public-header contracts: 27/27 in each configuration.
- After adjusting only the offline test's host-overhead tolerance,
  `rgsml_pcm_playback_tests` was rebuilt/rerun PASS in Debug and Release;
  the other 64 tests were not invalidated.
- Release/tests-off build in `build/windows-msvc-tests-off`: PASS;
  the affected QML/app target was rebuilt after VG-16 and remained PASS.
- Dedicated staged Release:
  `build/windows-msvc/stage/GUI-01-Premium-Recovery-VG16/Release/bin/RGSMasterLab.exe`.
  Embedded Main-QML deploy smoke: exit 0, empty stdout/stderr,
  new GUI-01 QML warning count 0.
- Generated and installed ToolchainManifest SHA-256 both
  `05701BF7E061F6AFFB9B687D4D6D389048B7D2CEC57CC4CFCE5075FF1B22355B`.
  The tracked manifest was not modified.

## VG-16 and final visual/focus evidence

The staged Release exposed VG-16: clicking File set the menu to
`visible=true`, but its popup width was 0 px. The only product patch is in
`ui/qml/Main.qml`: File has an explicit 230 logical-pixel popup width and
uses the in-window Qt Quick popup layer. The existing `Open Gold Reference`
item still opens the existing `goldDialog`, whose accepted selection calls
the M06 `goldSelection.selectGold` routing path. No Gold panel or backend
contract was added. The focused `rgsml_source_metadata_panel_smoke` now
checks the File click, nonzero popup geometry, enabled Gold command and
existing picker object; it passed 1/1 in both Debug and Release. A diagnostic
capture of the open popup showed the Gold command visibly reachable. Only
these UI-smoke tests, tests-off QML/app build and the affected Release
app/deploy smoke were invalidated by this patch; the remaining full-suite and 27 dependency/
public-header contract results above remain applicable.

Product Owner supplied staged-executable N4 (unavailable 1184×688), N1
(PREPARED 1440×900) and N2 (PREPARED 1184×688) and accepted their reuse;
their earlier repository component captures were not altered by VG-16.
Product Owner manually verified N3 (GOLD 1440×900) as PASS. Its accepted
staged-executable PNG is now exactly
`docs/implementation/evidence/GUI-01/gui01_1440x900_gold.png`, 2160×1350
physical pixels at 150% display scaling, SHA-256
`AE38E308233A170164FB84AD6CD040F42A6F03D59E9106EBBC6F6884E8999567`.
The screenshot shows the Source waveform retained, GOLD selected and
playback active, without a Source playhead or invented Gold waveform/cursor.

The earlier C1–C9 component screenshots remain supporting QML UI-smoke
evidence, not substitutes for the PO staged-executable captures.
`gui01_focus_and_state_test.txt` records the exact global and eight-segment
Tab orders and remains valid because focus code was unchanged.

No new visual-polish pass was performed. Residual graphical debt is left
for Product Owner review per the v1.1 sequencing decision. No backend
architecture, SRC policy, DSP, public API or future workspace was changed.
All technical and PO evidence gates required for this bounded closeout are
complete; the final delivery commit and merge are intentionally pending
separate authorization.
