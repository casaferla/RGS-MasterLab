# L3 real native search performance evidence — 2026-11-26

Status: implementation on Writer, awaiting focused CI. The measurement
is diagnostic only and does not alter the frozen M09 selection, M10
coordinate search, 800 render/analyze attempt budget, M11 safety, M12
adoption or M13 independent verification.

The native Product search records monotonic elapsed milliseconds from
the start of independent Prepared/Reference native analysis until the
first freshly measured, hard-gated candidate carrying a valid native
prediction. Cache hits do not establish this first observation. It is
an internal admissible candidate, **not** an early GUI offer or a
promise that M12/M13 can already adopt it. The ordinary GUI still
offers the reconstructed winner after the completed search.

Track separately the original-pair, Fixed, seed and final search times,
real attempted/completed/failed evaluations, and exact completed-cache
hits. The real Product lifecycle test reconciles attempted evaluations
with the M10 batch ledger and logs these measurements at the selected
source rate. Do not include the separate independent M13 render in the
M10 attempt count.

The existing 12-second controlled mono gain fixture is not a
representative 5-minute multi-domain music benchmark. The Product
performance target (first admissible candidate within a few minutes,
final bounded search within a practical duration) remains to be
measured on representative full-track stereo audio and actual Windows
hardware. No elapsed time may substitute for quality or safety gates.
