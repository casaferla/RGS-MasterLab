# L3 M06 real native E[t,b] from SAME M20 FFT pass

The original L1 M20 streaming accumulator now exposes an OPTIONAL,
read-only, synchronous source-owned callback for each selected native
FFT frame, absent/null by default. Old L1 analysis consumers execute
exactly their prior PSD sum and no native series allocation. The
AutoMatch-only sink integrates the original mean-channel one-sided
PSD cells within caller-attested fixed canonical bands into E[t,b].
It consumes no second FFT and preserves full original STFT ordinal
labels. An invalid callback aborts M20 instead of silently omitting
a bad candidate frame. Normalized means, L1 output and M20
selected count are regression compared with/without the sink.
It requires explicit Phase6 grid/owner/gate/source stamps. There
is no implied M07/M08 eligibility or M13 fresh-verifier qualification.
