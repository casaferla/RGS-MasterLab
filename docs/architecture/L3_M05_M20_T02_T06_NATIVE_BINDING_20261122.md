# L3 M05 T02–T06 from real M20 PSD and OLS to frozen M07 slots

This adapter consumes the already implemented read-only real Product
M20 tonal bundle, without new FFT, resampling or a separate feature
definition. It requires a single explicit commonComparisonBandwidth,
the exact authoritative density floor, independent Phase6 feature
definition/revision for each T02..T06, and three current owner stamps.
T02 and T06 are identity dB/oct slope, T03/T04/T05 are LOG2_POSITIVE
centroid and rolloffs from the actual common canonical M20 PSD.
Missing source/reference coordinates retain their configured mass
as ineligible; missing current Candidate never implies achieved.
T01 profile vector and any legal primary/fallback selection remain
separate, so no automatic 1/3 replacement is invented here.
