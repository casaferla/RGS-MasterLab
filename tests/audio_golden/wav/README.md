# L1-M01 WAV golden vectors

These tiny byte vectors were hand-authored for RGS MasterLab and are released
under the repository license. They contain no recorded or generated music and
were not downloaded from an external corpus.

- kRiffPcm16Mono: canonical RIFF header plus three PCM16 codes.
- kRf64F64NegativeZero: canonical RF64/ds64 header plus one IEEE binary64
  negative-zero sample.
- expected_bits.hpp: independent binary64 oracles derived from the frozen
  conversion formulas and IEEE-754 encodings, not from production decoder
  output.

These are milestone conformance fixtures only. They do not represent a Phase 10
catalog, qualification lockfile, or release-qualified corpus.
