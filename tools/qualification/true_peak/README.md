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
