# L4 M01 C00 — explicit DC subtraction (nonrelease)

Baseline: d5818c9b080d61d2acace212b0b1a772cb56daec, L3 Windows #32 and six-rate #3 green. Single writer moves to L4; preserve the L3 branches and artifacts.

Authority: PO DC D1–D7 (2026-10-12), PO roadmap (2026-11-28). Existing registry descriptor rgsml.dsp.dc-offset is RESTORE_PREP/REPAIR only, with no implementation version/factory. This slice implements the offline native PCM kernel only, not a registered product module or a project schema. No new normative ID or QTP issuance.

- Caller supplies complete source-rate PCM, beginning at absolute frame zero, one explicitly authorized finite stored offset per native channel in [-0.25, +0.25] FS, and an absolute half-open scope within source bounds. It must prove complete resource identity upstream: a view by itself cannot certify completeness.
- Reject nonfinite input anywhere in the supplied source, including outside scope or in a different channel; no partial output publication.
- Subtract the supplied constant in scope (strict binary64), copy outside scope bit-for-bit. No clipping, normalization, high-pass, smoothing, adaptive action or inferred offset. No resampling, latency, pre/post-context or state.
- New owning output; RAW remains untouched. Empty scope is identity. No automatic action from diagnostic mean, no O2 expert centering, no regional proposal.
- Local tests use exact dyadic expectations, signed-zero exterior, mono/stereo, all six rates, invalid input and scope. They are implementation checks, not formal QTP or physical qualification.
- Next: recover the existing parameter/descriptor contracts and scope ownership before IModule, serialization, Restore workspace or PREPARED materialization. Dehum and Declip holds unchanged.
