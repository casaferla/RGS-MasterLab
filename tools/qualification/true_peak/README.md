# L1-M16 Gate G / B2a: official EBU source preflight

**Status:** preliminary fixture/provenance oracle infrastructure only; **NOT** production Gate G. It must fail when the nine official cases are missing.

Authority: RGS ContentBox M16 independent mathematics Work, Appendix B (case 15–23 original WAV/PCM hashes, exact peak numerator and index), frozen True-Peak M16 contract. The nine *official* EBU files had been assessed by Work in pre-production; production C++ has not yet been compared against them.

Official EBU source: https://tech.ebu.ch/publications/ebu_loudness_test_set ; EBU terms: https://tech.ebu.ch/files/live/sites/tech/files/shared/testmaterial/use%20of%20EBU%20AUDIO%20test%20sequences.pdf . **Internal technical R&D only, © EBU. Do not commit, package, share, or redistribute any test audio.** The RGS Work recorded the original EBU v5 archive SHA-256 in the adjacent manifest. These fingerprints are RGS observed provenance, not EBU-published hashes.

Usage (after authorized local acquisition of original EBU v5 ZIP):

```bash
python tools/qualification/true_peak/verify_ebu_3341_v5.py --archive /private/location/ebu-loudness-test-setv05.zip
# or after private local extraction (original double .wav.wav basename required):
python tools/qualification/true_peak/verify_ebu_3341_v5.py --directory /private/location/ebu-files
```

The script checks the ZIP hash (ZIP mode), individual WAV hashes, stereo 48 kHz signed PCM24 and exact frame counts, frame-major PCM binary64 hashes, indexed L/R exact peak numerator via independent integer 4-phase FIR with 11 EOS zeros, the Work's pre-production dBTP, and the official **asymmetric** [expected−0.4, expected+0.2] dBTP window. Cases 20–23 retain 153604 frames. It fails closed on missing or mismatched data. No synthesized substitute earns official EBU qualification.

**Still required before M16 QUALIFIED:** run the authentic fixtures through the *production C++ True-Peak estimator* at the same exact release SHA/build; compare each L/R/global result against the independent numerator/oracle within the accepted RGS 0.01 dBTP criterion where applicable, while separately checking the published asymmetric EBU interval; complete the six-rate RGS matrix, FMA/FP integrity, coefficient-byte hash, chunk/state/finalization/metadata tests. The script's output always says `production_gate_g: NOT_EXECUTED`. Never use successful standalone Python checks to set M16 qualification metadata.


## M16-B2b: private production C++ comparison bridge (NOT Gate G)

Compile the qualification-only `rgsml_true_peak_b2b_probe` target from the
current exact checkout, and run its **synthetic** `--selftest` through CTest.
No EBU WAV is used in repository CI.

For authorized **private** official EBU v5 assets and the matching local
production build:

```sh
python tools/qualification/true_peak/qualify_ebu_cpp.py \
  --archive /private/ebu-loudness-test-setv05.zip \
  --probe /path/to/rgsml_true_peak_b2b_probe
```

On Windows use the built `.exe` path and quote paths containing spaces.
Or use `--directory /private/unmodified/ebu-wavs` in place of `--archive`.

The supervisor first runs the B2a independent authentic-WAV preflight for
all nine cases, then pipes authenticated frame-major little-endian binary64
PCM to the C++ adapter. The adapter invokes the existing production
`measure_unqualified_true_peak_full_resource`, not Python FIR code.
The supervisor requires **exact** rational PCM24 L/R/global linear peaks,
a separate <=0.01 dBTP RGS comparison for each channel/global, and the
official inclusive asymmetric EBU interval. The C++ adapter itself cannot
produce `QUALIFIED`.

**Restrictions:** internal R&D use only, © EBU. Never add WAV/ZIP, decoded
audio, derivative files or log dumps to Git/CI artifacts. The existing
fixture hashes are RGS observations, not EBU-certified hashes.
The downloaded test set cannot be redistributed.

**Still OPEN:** no official ZIP was available to this chat's execution
environment; the nine-case C++ comparison must be run on an actual
matching build and authentic fixture set. Build/run SHA provenance,
six-rate RGS matrices, independent ideal/oracle qualification,
strict-FP/FMA, chunk/finalize, coefficient-hash, metadata/status tests
and complete Gate G remain separate. `B2B_OFFICIAL_48KHZ_CPP_COMPARISON_PASS`
is a narrow sub-result, **never** an overall Gate G PASS.


## M16-B2c: six-rate Hertz-defined synthetic C++ evidence (NOT Gate G)

The standalone, stdlib-only `qualify_six_rate_cpp.py` adds the missing
physical-rate dimension. In private local mode it pins the **CI #417 exact-head**
C++ production-linked probe and its immutable byte SHA; fresh focused CI
uses an independently pinned checkout HEAD/run ID plus actual packaged EXE
SHA cross-check, and executes six rates:

`44100, 48000, 88200, 96000, 176400, 192000 Hz`.

At every rate it synthesizes six stereo fixtures in memory: a 1000-Hz
integer-defined bipolar square signal; a 997-Hz pulse train; sign-inverted
and channel-swapped metamorphic pairs; a finite ±2/+3 dyadic boundary
case; and exact digital silence. Hertz waveforms contain **different PCM
sequences at different rates**, rather than relabeling one 48-kHz vector.
The synthetic signals are RGS fixtures, **not EBU certification** or
high-resolution sine/ideal continuous-waveform qualification.

The independent direct signed integer FIR reference has the *separately
transcribed* published lag-major coefficients, an explicit 11-frame EOS
and exact dyadic rational denominator. It checks the real C++ L/R/global
linear peaks by exact equality, each defined dBTP against the frozen RGS
0.01-dB QTP, null dB fields for silence, rate/layout/length metadata,
plus sign- and channel-swap invariance. The standalone bridge preserves
the original C++ estimator and does not modify production code.

Prerequisite: retain private CI #417 artifact
`RGS-M16-B2b-CPP-Probe-4152f1ba6bfc2c8037a3397631cd4f19f5e471c2`
and its `B2b_build_provenance.json`; do not substitute a rebuilt EXE.

```sh
python tools/qualification/true_peak/qualify_six_rate_cpp.py --selftest
python tools/qualification/true_peak/qualify_six_rate_cpp.py \
  --probe /private/rgsml_true_peak_b2b_probe.exe \
  --provenance /private/B2b_build_provenance.json
```

Output is numerical JSON only and always retains
`production_gate_g: NOT_COMPLETE`. This constitutes a *supplemental*
six-rate normalized discrete-math / Hertz-defined synthetic check if and
only if actually executed. It must never be reported as six official
EBU tests, an independent ≥32x ideal reconstruction, a complete six-rate
release-metadata qualification, or an overall Gate G pass.

Remaining mandatory Gate G components must be reconciled against the
M16 mathematical authority and actual CI evidence before any
QUALIFIED label, Full Qualification, or milestone freeze.

## M16-B2d: extreme FP and fail-closed production probe (NOT Gate G)

The new stdlib-only `qualify_extreme_fp_cpp.py` checks the UNCHANGED
production True-Peak C++ estimator through the same exact-SHA authenticated
qualification-only probe. All synthetic binary64 PCM is generated in RAM,
streamed over an anonymous pipe; no audio files, EBU assets or decoded
derivatives are saved or redistributed.

There are 27 isolated stereo cases: +/- smallest subnormal and minimum
normal (4); +/-DBL_MAX finite lone impulses (2); a 12-frame adversarial
accumulation overflow (1, categorical failure); NaN, +Inf and -Inf in
either channel at first/middle/last (18, categorical failure);
one extra byte after expected frames (1); one unsupported 32-kHz rate (1).

The independent expected binary64 result vectors come from the accepted M16
independent mathematics preaudit, section G. Accepted cases check exact
left/global linear peak, right-channel silence, defined dBTP within 0.01,
and truthful unqualified status. Rejected cases require nonzero return,
no misleading JSON values, and explicit failure. The script itself fails
closed on an unexpected build/run hash, corrupted input, unexpected output,
or an environment that flushes subnormals contrary to the frozen policy.

```sh
python tools/qualification/true_peak/qualify_extreme_fp_cpp.py --selftest
python tools/qualification/true_peak/qualify_extreme_fp_cpp.py \
  --probe /private/rgsml_true_peak_b2b_probe.exe \
  --provenance /private/B2b_build_provenance.json
```

Private local mode pins to the known green CI #418 build. Fresh CI accepts
an explicit exact source SHA/run ID, rehashes the actual packaged EXE and
stores only numerical results in B2d_extreme_fp_result.json.

A CI failure on a subnormal/overflow/NaN fixture is a qualification finding,
NOT permission to weaken frozen mathematics or silently alter runtime FP
policy. This B2d lane is not complete FMA/strict-FP qualification, not a
high-resolution continuous ideal reconstruction, and not full Gate G. No
Full Qualification, merge, freeze, or M17 is authorized by B2d alone.

## M16-B2e: 80-digit independent ideal-reconstruction diagnostic (NOT Gate G)

The qualify_ideal_screen_cpp.py harness requires the pinned mpmath==1.3.0
high-precision mathematical dependency IN CI ONLY, never in production DSP.
Five short exact-dyadic source vectors are independently sinc-interpolated
at 80 decimal digits using locally refined 32x / 64x / 128x finite-grid
screens, and a 120-digit point-evaluation consistency check.

Five source vectors (unit impulse, adjacent positive, adjacent signed,
separated signed, four alternating) reproduce estimated ideal-peak values
from M16 independent Work section E. The harness separately compares real
production C++ 4x FIR results with exact rational ITU integer numerators.
It reports the diagnostic ITU minus ideal dB differences.
The frozen RGS 0.01-dBTP QTP is against the discrete ITU FIR reference,
NOT a 0.01-dB guarantee against the continuous mathematical ideal.

These are convergence/high-precision point-check SCREENS ONLY:
finite search interval [-16,N-1+16], no certified infinite sinc-tail bound,
no outward-rounded adaptive interval cover and no globally certified ideal
0.0001-dB interval. The 64x/128x diagnostic 0.0001-dB convergence threshold
is a NEW PROPOSAL in the preaudit, not a frozen release acceptance threshold.
NEVER mislabel as completed ideal certificate or full Gate G.

Run privately against the preserved #419 exact-head probe EXE and provenance:

    python -m pip install mpmath==1.3.0
    python tools/qualification/true_peak/qualify_ideal_screen_cpp.py --selftest
    python tools/qualification/true_peak/qualify_ideal_screen_cpp.py --probe PATH_TO_EXE --provenance PATH_TO_PROVENANCE_JSON

CI pins its actual checkout SHA, run ID and produced EXE hash and saves
only numeric screen results. No EBU assets or PCM derivatives, no Full
Qualification, merge, QUALIFIED product status, freeze, or M17 authorization.

## M16-B2f: explicit source-scoped strict floating-point build policy

Compilation options apply ONLY to two True-Peak production objects:
src/true_peak_itu4x_channel_stream.cpp and src/true_peak_full_resource.cpp.
No modification to frozen 48-tap bank, fourfold FIR, EOS, PCM, UI,
public schema, or other spectrum/FFT DSP translation units.

MSVC: /fp:strict. GCC/Clang: -fno-fast-math -ffp-contract=off.
Unrecognized C++ compilers fail CMake before creating an unaudited build.
The CI uses verify_fp_build_flags.py to check effective per-source
generated build settings: MSBuild ClCompile source metadata on Windows
and Ninja compile_commands.json on Linux. Failure is categorical.
Attestation output includes source SHA, run ID and only two checked
source names and compiler flags. Windows numeric proof is packaged
with the private exact-head C++ probe; Linux proof is a separate
seven-day artifact. No EBU assets, PCM, or source audio exported.

These checks prove explicit compiler configuration; they are NOT
instruction-level FMA disassembly or runtime FP environment attestation,
and cannot by themselves declare Gate G fully qualified.
All pre-existing B2c 36/36, B2d 27/27, B2e 5/5 and CTest regressions
must pass on the newly compiled candidate. RED means investigate, never
change fixed FIR numerators or weaken the mathematical reference.

The private original official EBU nine WAVs were last run against an
older SHA. AFTER green B2f CI + reverse audit, obtain the NEW exact
Windows probe and rerun the authentic B2b 9-of-9 suite on the user's
Windows PC, retaining numeric output only.

Gate G/L/M remain OPEN. No Full Qualification, merge, accepted-frozen
claim, release QUALIFIED metadata, or M17 dependency is inferred.

## M16-B2g: bounded per-rate Gate-G reconciliation — PRELIMINARY

The companion b2g_gate_g_coverage_matrix.json is an evidence-by-predicate
RECONCILIATION table, NOT an authoritative release matrix or a product
TruePeakMeasurement DTO. Six rows deliberately remain
SUPPORTED_UNQUALIFIED / gate_g_per_rate OPEN; a successful focused CI,
a list of supported sample rates or a historical WAV oracle does not
automatically qualify a product algorithm/filter/build/rate combination.

Positive bounded evidence at the prior exact CI #421 source includes:
four phase-major frozen coefficient/hash checks, first/last+11-tail,
the complete resource, canonical 4096 boundary chunk cases, independent
exact signed-integer ITU reference, per-channel stereo, signed peaks
above unity, silence/DC, nonfinite/FP overflow and distinct six-rate
physical Hertz stimuli. Official EBU version 5 assets 15..23 were
executed and independently checked for the 48-kHz Windows strict-FP
build only. These are NOT six official EBU certifications.

B2g adds TruePeakFullResourceTest::binaryExactPowerOfTwoGainLawSixRates:
six real supported Fs × mono/stereo × five signed dyadic scaling gains
= 60 metamorphic full-resource comparisons. Exact peak equality follows
for the bounded normal binary64 test inputs because powers-of-two
scaling preserves the exact dyadic FIR products and accumulated sums;
dBTP difference is checked within 1e-12 dB against 20*log10(|gain|).
Other arbitrary finite gains can round differently; exact bitwise
homogeneity is NOT claimed or imposed on the product.

Pending after CI: complete per-rate evidence audit and independent
high-resolution-oracle applicability/coverage, public algorithm version,
policy/filter identity, product qualification evidence provenance,
globalPeakChannel tie/all-silent resolution (escalate if no accepted
rule), and truthful qualification status transitions. The five-case
B2e 80-digit ideal sinc finite-interval screen remains diagnostic,
NOT a global sinc-tail enclosure. A new 0.0001-dB *global* requirement
is a preaudit NEW PROPOSAL, not a frozen Gate G threshold.

B2g changes only this README, the qualification matrix and a Qt C++
test body. Production True-Peak C++ sources and strict-FP compiler flags
stay byte-for-byte unchanged, so the already accepted B2f Windows
official EBU qualification retains its bounded algorithm/evidence
identity; any future change to estimator/flags/asset requires new
exact-build provenance plus authentic private EBU requalification.

No Gate G completion, QUALIFIED release labeling, final FQ, M16 merge,
Gate L/M closure or M17 transition is implied.

## M16-B2h: extend independent high-resolution coverage to SIX physical rates

The independent, stdlib+mpmath(1.3.0) CI-only script
`qualify_six_rate_ideal_cpp.py` uses 80/120-digit sin/cos synthesis
at 12345/6789 Hz, rounding the actual 24-frame stereo PCM once into
binary64. At each of the six frozen Fs, the source interleaved PCM hash
is distinct; high-precision generator stability must hold byte-for-byte.

The script combines TWO SEPARATE independent reference problems:

* NORMATIVE discrete ITU 4x frozen peak against exact signed integer
  lag numerators applied to the actual rounded dyadic PCM at 100
  digits. Both channels and global C++ peak/dBTP must satisfy the
  pre-existing RGS 0.01-dBTP QTP against this DISCRETE oracle.
* DIAGNOSTIC mathematical IDEAL sinc reconstruction of the LEFT input
  tone at 80 decimal digits, refined on 64x/128x finite grids and
  point checked at 120 digits. The prior B2e finite search window
  and diagnostic 0.0001-dB CONVERGENCE screen are reused unchanged.
  We report ITU-minus-ideal dB as OBSERVATION, not an RGS error.

This broadens high-resolution sinc diagnostics from a handful of
48k labelled vectors to real frequency-dependent source PCM for
44.1/48/88.2/96/176.4/192 kHz. It is NOT an infinite-tail global
maximum proof, not six EBU official 48-kHz cases at other rates,
not a certificate that RGS 4x is within 0.01 dB of the ideal,
and not the final per-rate production Gate G evidence registry.
All rate rows remain SUPPORTED_UNQUALIFIED with Gate G OPEN.

Execution is only in the Windows CI job, against the actual freshly
built and provenance-pinned C++ probe, with numeric report
B2h_six_rate_ideal_result.json. Synthetic PCM stays in RAM and is
piped privately; only its SHA-256 fingerprint is retained.
No production true-peak mathematics, filter taps, strict-FP options,
API, GUI, release status or authenticated EBU audio is changed.

No FQ, merge, M16 freeze, Gate L/M acceptance or M17 authorization
may follow automatically from a B2h SCREEN PASS.

## M16-B2i: CI423 readback + fail-closed status/provenance guard

The updated b2g_gate_g_coverage_matrix.json (schema 0.3) replaces
stale B2g/B2h TEST_STAGED_AWAITING_CI rows with the independently
audited CI422 and CI423 bounded PASS evidence. Each of the six
frozen sample rates remains SUPPORTED_UNQUALIFIED / Gate G OPEN.

Crucial distinction: the authentic EBU v5 WAVs 15–23 were executed
on the CI421 Windows probe (9/9, 27/27 checks, real user PC verified).
The CI423 probe EXE SHA is DIFFERENT; CI423 rebuilt unmodified
production True-Peak sources and strict-FP policy but does not
inherit a fictional authentic EBU run at that executable identity.
Thus a final release authority cannot simply copy that 48-kHz
qualification into the CI423 binary. The other five rates have
RGS synthetic/high-resolution evidence, not official EBU WAV runs.

verify_gate_g_evidence.py reads the matrix on Linux AND Windows.
It enforces six immutable production Git blob IDs, official case
source/executable identity, CI421/422/423 evidence scope, six
distinct ordered rate rows, truthful OPEN/SUPPORTED_UNQUALIFIED
status and distinct ITU 0.01dB vs ideal-sinc diagnostic semantics.
The script performs twelve independent negative tampering/falsification
fixtures (false QUALIFIED, wrong rate, missing evidence, false EBU
scope, filter/compile hash mutation, fabricated sinc certificate).
It checks the ACTUAL CI checkout HEAD and content-addressed blobs.
GitHub Windows CI packages numeric-only JSON with its private probe;
Linux prints an equivalent evidence-guard PASS to its CI log.

A B2i test PASS is an audit of evidence GAPS as well as positives.
It is NOT a production release registry, an ITU ideal-peak global
certificate, a public TruePeakMeasurement implementation or a
final authoritative Gate G PASS. The frozen public result identity,
status wiring, globalPeakChannel tie/all-silent semantics and final
exact-build 48-kHz EBU requalification remain distinct closeout tasks.

The protected measurement DSP, coefficient bank, source-local strict
FP options and Qt UI are unchanged. No merge/FQ/freeze/M17 action
is authorized by this test.
