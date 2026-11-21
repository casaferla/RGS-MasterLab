# L3-M09 P02 — Spectral D10/D11/D12 and native threshold seeds (nonrelease)

Authority: P02 ACCEPTED_FROZEN + M09 closure (7 October 2026).
Caller supplies authenticated exact native M05/M06, M07, M08 and M11
dependency/selection evidence. This in-memory analytical helper checks
three or more contiguous eligible selected canonical grid cells with
exact edges, coherent nondeadband signed D10, agreeing D11, and D12
compatibility. D10 drives cluster priority sum |delta|×pairedSupport,
geometric center exp(sum(w ln fc)/sum w), and signed spread delta
sum(w×delta)/sum w. Raw and ±2 dB-bounded seed are recorded separately.
The source-aligned complete cluster edges are retained, but the
future legal Q/filter/shelf branch remains conditional.

Native E[t,b] energy rows are consumed only with exact common frame
and unit authority. A frame participates iff every cluster member
is present. Exact positive total energies produce 10log10(sum E) and
Type7 P75; missing rows are excluded, never imputed. No replacement
DSP detector equivalence or extra artistic feature ID is inferred.

No actual Product source, waveform rendering, solver, verifier, or
REF_LINKED materialization is integrated by this helper.
