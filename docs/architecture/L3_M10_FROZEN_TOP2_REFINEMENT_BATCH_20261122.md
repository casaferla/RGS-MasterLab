# L3-M10 — Measured top2 to frozen coordinate descent (nonrelease)

A shared bounded numerical controller now joins the existing M10
P04/P05 measured seed shortlist with the canonical CoordinateRefiner:
four fixed normalized step sweeps, same-current minus/plus order,
exact active coordinate inventory, global solver attempted budget800.
It does not rebase references or rank with pre-render proxies.
Failed attempted render/analysis consumes budget, exact full cache hit
does not. The second seed shares the first's remaining budget. A
cancellation cannot claim completed refinement; exhausted budget
may retain the best fully measured candidate.

The callback boundary is NOT a fake Product DSP evaluator: the later
caller must supply fresh typed legal render/analysis with exact
M05/M06 support-mass objective and M11 hard constraints. This pure
numeric orchestrator cannot materialize REF_LINKED, alter Input Gain,
publish audio, report verified proximity or replace M13 verification.
