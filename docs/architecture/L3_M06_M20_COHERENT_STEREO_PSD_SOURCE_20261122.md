# L3 M06 coherent L/R cross-PSD tap on canonical M20 FFT

Original M20 selected native STFT frames are optionally exposed to
a separate stereo-only source observer. The observer receives REAL
L/R PSD with the existing one-sided Fs*sumW2 norm and a signed cross
PSD real(FFT_L*conj(FFT_R)), doubled for nonedge bins. A temporary
left FFT half-spectrum is allocated ONLY for the opt-in stereo
callback; the usual L1 M20 branch has no new allocation and no
change to mean-channel-PSD output. No downmix, squared-coherence
substitution, re-render, or second FFT is permitted.
Tests compare stereo negative/positive phase correlation signs,
individual PSD inequality guards and exact original M20 output.
This is the data precondition for original S03/S04/S05/S06; applying
their frozen gates and Phase6 source policy remains separate.
