# L3-M10 P04 — Frozen numeric top-eight domain consumer

Consumes the completed canonical M09 legal domain stream and its fully
bound, current domain-restricted PreRenderTopologyProxy per candidate.
If **any** otherwise-legal member is unbound, the whole numeric
ranking remains UNREADY rather than ranking only an available subset.
The M09 enumeration remains unchanged, and original ordinals survive.
If ready, select the first eight in the frozen proxy tie order, without
reserving a NONE slot. The ledger assigns
DOMAIN_CONSUMER_CAP_EXCLUDED to omitted valid descriptions. A winning
currently-unavailable DSP remains selected but conditional; no
availability-driven backfill of candidate nine and no objective
reweighting. Selection doesn't materialize DSP or issue a render.

Source: L3-M10 P04 ACCEPTED_FROZEN (7 Oct 2026), also M09 PO closure.
This is a pure read-only selection helper. M10 combined top32, M11
no-correction evidence, and REF_LINKED/verification remain separate.
