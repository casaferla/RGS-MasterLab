# L3 M13 explicit fresh native verification probe

A guarded M13 pre-verifier can accept only an actual native
REF_LINKED segment and currently attested materialized M12 receipt,
plus fresh current PREPARED PCM. It performs one NEW RenderPreview
and a new real M19/M20 analysis, separately from all M10 candidate
renders, with verificationEvaluations = 1 in a distinct counter.
It never accepts the M10 candidate audio as verifier output.

This deliberately returns a *fresh native analysis probe*, not
a completed verifier report: correct combined fixed/replaceable chain
with protected MANUAL nodes, one terminal true peak limiter,
full 27 original features, M11 final protected safety, exact
expected/actual M12 receipt, current owner, QTP tolerance profile
qualification, and final report are mandatory subsequent gates.
No publish/export/VERIFIED state is granted by this function.
