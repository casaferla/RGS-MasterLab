# L3-M09 P01 — Frozen weighted atom-fit arithmetic (nonrelease)

Source: M09 P01 + PO closure ACCEPTED_FROZEN (7 Oct 2026).
Calls to the actual canonical DSP response evaluator remain external:
the caller must supply the EXACT active response at bounded g0 for
all frozen-support-weighted comparison components, not g0 × h(+1 dB).
First seed projection remains unweighted dot(r,h)/dot(h,h).
Tonal limit ±3 dB, Stereo SIDE ±1.5 dB, with raw and limited seed
recorded separately. Selection compares whole-population weighted
squared-dB errors, requires positive ≥1% relative improvement with
zero-old-error stop, and uses LOW_SHELF/BELL/HIGH_SHELF, ascending
frequency, ascending Q/slope for exact error ties. No missing
response component may be discarded to inflate the fit.

This does not create EQ band IDs, enumerate the actual canonical
dictionary, apply Q/frequency legality, replace the native EQ
response evaluator, publish a plan or claim predicted proficiency.
