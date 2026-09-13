# L1-M06 — Gold Reference Picker and Truthful Audition Routing

## Outcome

Technical delivery is complete on `task/L1-M06-gold-audition-routing` from
baseline `87605710b66d7dc8342f3e96db809d371fbb9e9f`. Automated qualification is
green after the authorized playback-SRC input forward delta. The original
bundled Product Owner gate passed except for the now-corrected 44.1 kHz Source
finding. The minimal Product Owner retest has now passed; L1-M06 is ready for
review. The branch is not merged.

## Materialized behavior

- `AuditionSourceSelector` owns PREPARED/PROCESSED realizations, target state,
  the shared Source-derived cue, independent Gold cue, routing, fallback, and
  fail-closed status.
- `GoldSelectionViewModel` uses an independent local WAV picker, probes through
  the canonical decoder path, commits only after validation, and preserves a
  prior valid Gold when a candidate fails.
- Windows same-file validation combines canonical resource identity with
  read-only volume serial/file-index identity; exact files and hardlinks are
  rejected.
- The application owns Render Preview PCM. The Windows backend borrows its
  immutable `AudioBufferView`, and the selector synchronously stops/clears the
  backend before realization replacement or destruction.
- WAV-backed and PCM-backed sources enter one private PlaybackEngine input
  abstraction and share the existing playback SRC, output, encoder, queue,
  scheduler, and QAudioSink. The private SRC frame input now accepts either a
  WavReader-backed source or a borrowed immutable AudioBufferView-backed source.
- PREPARED/PROCESSED use absolute Source-frame ranges and reject cues outside a
  partial realization. A Source loop is applied only when fully representable.
  GOLD never receives the Source loop, and the Source waveform playhead is
  suppressed during GOLD.

## Frozen boundaries

The public `IAudioPlaybackService`, `ResourceReference`, resource byte ports,
canonical audio buffers, DSP/chain/render contracts, waveform and Audition
Region semantics, writer/export path, and SRC coefficients/policy are
unchanged. No second engine, temporary WAV, Gold analysis, Reference Match,
level match, persistence, codec expansion, or new dependency was introduced.

## Automated evidence

| Gate | Result |
|---|---|
| Fresh configure | PASS; 65 tests |
| Debug complete CTest | PASS; 65/65 |
| Release clean build and complete CTest | PASS; 65/65 |
| Focused selector/identity/PCM/UI tests | PASS; 4/4 |
| Dependency/public-header contracts | PASS; 27/27 |
| Existing WAV playback and SRC regressions | PASS |
| Tests-off Release | PASS; 0 tests |
| Staged install/deploy smoke | PASS; exit 0 |
| ToolchainManifest build/stage | PASS; byte-identical, `49D7AF5974E7F39FCAE5A44193EAF913D4CCF7282746CA4860D29CF706223E11` |
| Historical root file | PASS; untracked, unstaged, byte-identical |

Focused tests cover the deterministic eight-case WAV/PCM 44.1/48 kHz
exact/paired matrix, with byte-identical downstream output for both backing
types, plus availability and unavailable rejection, shared and
independent cues, fallback, failed Gold preservation, partial range rejection,
loop representability, repeated WAV/PCM replacement, borrowed-input clear,
hardlink identity, active UI target state, and Source-playhead truth.

## Resolved findings

During implementation, the application test composition lacked its direct
render dependency, the QML target lacked explicit QuickDialogs2 composition,
and the private source owner required an out-of-line constructor for a complete
type at destruction. Each was corrected within the allow-list and regressed.
One transient Qt module-load observation was reproduced with the configured
QML import path and resolved without product or machine changes.

### Material Product Owner finding — resolved in the forward delta

The Product Owner observed that
`listening_stereo_44100_pcm16.wav` played as GOLD but PREPARED/Source playback
remained unavailable, while the 48 kHz Source played. The first divergence was
the Windows output candidate: WAV-backed playback enabled the authorized
paired-rate candidate, whereas PCM-backed PREPARED playback disabled it. The
existing SRC also accepted only WavReader input, so changing that boolean alone
would have dereferenced no valid decoder for PCM.

The authorized targeted forward delta generalized only the private input
boundary of that same SRC. Both backing types now provide bounded canonical
frames through one source-agnostic interface; the PCM implementation maps the
SRC-local range into its borrowed immutable Source view. PREPARED output
selection now applies the same exact-first, authorized paired-rate policy as
GOLD. SRC coefficients, convolution mathematics, rate mapping, output policy,
public playback API, canonical buffers, Gold behavior, and WAV-only scope are
unchanged. No second SRC, PCM copy, or WAV bridge was introduced.

## Minimal Product Owner retest — PASS

Use the staged executable:

`C:\Dev\RGS-MasterLab\build\windows-msvc\stage\L1-M06\Release\bin\RGSMasterLab.exe`

The Product Owner loaded
`tests/audio_golden/playback_src/listening_stereo_44100_pcm16.wav` as Source,
confirmed Play available, started playback, and heard correct audio without
evident clicks or dropouts. The 44.1 kHz Source finding is resolved. All other
bundled Product Owner checks had already passed.

## Integrity and residuals

The historical root file remains untracked and unstaged at SHA-256
`3669527CB6DD75E59254C6C73070647466E7C54F04321D17B011F743C15D0FFF`.
No automated or Product Owner gate blocker remains. Integration is not merged.
