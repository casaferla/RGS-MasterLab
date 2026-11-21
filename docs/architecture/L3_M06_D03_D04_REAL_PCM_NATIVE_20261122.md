# L3-M06 actual PCM D03/D04 native series — 22 November 2026

Sources: Phase6 canonical reconstruction C10/C11/E02; M06 frozen
closure and August preaudit D03/D04. This implementation reads the
actual full-track source at processing frame domain. Native grids are
anchored to absolute frame 0: complete rectangular 400-ms windows
every 100 ms, C*N (per-channel, per-frame) mean-square power,
windowed-only digitalPowerFloor 1e-12, dBFS 10log10(power);
below-floor and nonfinite windows remain NULL but included in
complete-native denominator. Same-window sample peak (ungated)
minus RMS yields fixed-window D04 crest, not event crest D08.

Shared four-block 100ms ring avoids repeated 400ms reads for
overlapping windows; no extra Product render is needed.
D03 Type7 P95-P10 and D04 Type7 P10/P50/P90 selectors use ONLY
valid members, while native support uses valid/complete separately
and is not multiplied twice. Actual reference-grade status, ≥10s
active duration, Phase6 source content checksum, fixed-chain owner,
M07 binding revision, M08 M10 and M13 are NOT asserted by this
measurement function. No new full-scope RMS digital floor.
