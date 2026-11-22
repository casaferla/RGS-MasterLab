# L3 M06 Phase6 P04 authentic positive spectral flux source

A new Product-only P04 detector pass is required because canonical
P04 20ms/5ms STFT differs materially from native M20 85ms window.
The implementation reuses the established native Radix2Fft and
periodic Hann mathematics without sample-rate conversion. Grid
start(j)=RTE(j Fs/200) with direct rational indexing, Nw=RTE(Fs/50),
Nfft next power of two; both complete-window and frequency-bin
membership are exact and inclusive. Phase6 bin centers are
40Hz..min(16000Hz,.45Fs) inclusive (NO spectral-cell overlap).
FFT magnitude power is mean over channels; normalized sqrt
floor1e-12 spectra yield RAW positive flux, with first native frame
NULL for missing predecessor, never a synthetic flux zero.
The detector baseline/MAD/NMS and event descriptor contexts are
the next native source steps. This source alone is NOT a published
transient or ReferenceMatch feature, and status is nonrelease.
