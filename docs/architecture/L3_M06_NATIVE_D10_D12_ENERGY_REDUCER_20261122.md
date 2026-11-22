# L3 M06 D10/D11/D12 native STFT energy reducer

Consumes REAL canonical per-active-frame E[t,b] rows from a later
Phase6 FFT owner, not the time-averaged M20 PSD (which cannot recover
temporal distributions). The caller attests Phase6 periodic Hann
RTE(fs*4096/48000)/NFFT next pow2/hop RTE(N/4), full common grid and
active frame population. Broadband B is recomputed from all exact
common comparison bands; missing cells never shrink it. E >=
max(1e-12,1e-12 B), B>=1e-12; A=10log10 E, G=10log10 B,
R=A-G. D10=P90(R)-P10(R) Type7, D11=P90(A)-P10(A), D12=
atanh(Pearson(chronological A,chronological G)) with endpoint epsilon1e-6.
Each band requires 2s active, 20 valid frames and support >=0.50
valid/candidate active. Constant-series variance below 1e-12 is
UNDEFINED, not spurious rho=0. No M10/Frozen Product legitimacy from
this pure reducer; actual STFT frame producer remains outstanding.
