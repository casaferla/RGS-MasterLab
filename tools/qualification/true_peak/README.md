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
