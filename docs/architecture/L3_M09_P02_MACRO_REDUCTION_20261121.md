# L3-M09 P02 — Macro and Type7 analytical seed slice (nonrelease)

Authority: M09 P02 + M09 Product Owner closure ACCEPTED_FROZEN
(7 October 2026). The caller MUST authenticate exact Fixed output and
M11 protection. D01 three frozen components use oriented [-1,+1,+1],
but constitute ONE distinct Macro group; D02–D06 are positive. Only
requested, unprotected, current, non-deadband, support-positive
components participate. Any nondeadband expansion blocks the
conservative compressor seed. At least two distinct coherent Macro
groups are needed. RequiredMovement is normalized support-weighted
positive reduction; no arbitrary [0,1] cap or Amount × DSP parameter
conversion. All original feature objective masses remain M05/M06-owned.
The native 400ms/Grid100 Fixed RMS series Type7 P75 – knee/2 is the
only admitted threshold carrier; missing original series means no seed.

This patch is deliberately pure/in-memory, without product data,
Compressor parameter payload, real GR constraints, M11 fixed-chain
measurement, M10 render/score, M13 verification, persistence, or
publication. P02 micro/spectral reducer work remains outstanding.
