# L3-M07 — Frozen 27-slot registry projection and M10 bridge (nonrelease)

Authority: L3-M07 P01 (2026-10-06) **ACCEPTED_FROZEN**, and
L3-M07 Product Owner closure **ACCEPTED_FROZEN**, with original
M07 pre-audit §§5–14 + exact incorporated P01. M05/M06 are the
sole authority for source feature math, support, selectors, scales,
deadbands, role/ownership and domain mass.

This writer-only implementation slice provides:
- Compile-time immutable 27 canonical artistic bindings, four domains
  Tonal 6, Dynamics 12, Stereo 6, Loudness 3; T1 1/3 is **exclusive
  fallback** on T1 (not a 28th slot).
- Exact reduced rational masses and an integer 8000-unit compile-time
  proof of 1.0 configured mass per domain; component shape is one,
  thirds or 1/N configured (N fixed **before** eligibility).
- 13 separate safety, diagnostic, UI/deferred records with zero
  artistic mass; there is no extra artistic coverage or 5th Global
  domain.
- Exactly eight Reference Match intent records referencing seven
  canonical DSP typeIDs; Parametric EQ is reused by both Tonal and
  broad Stereo M/S intent, not re-registered as a new DSP.
- Read-only runtime availability of descriptor and factory obtained
  **only from the already canonical DSP ModuleRegistry**; a factory
  does not imply a valid typed payload, a permitted plan, legal
  topology, actual runtime module or source feature authority.
- A strict M07 to M10 adapter accepts 27 order-exact source records
  and the M08 domain-correction mask only when every slot carries
  externally authenticated exact per-feature dependency identity,
  source feature ID/selected fallback, appropriately dimensioned
  component values, native pair support and real M08 optimization
  targets. It rejects missing/incompatible identity, missing targets,
  nonfinite support, wrong dimension or altered canonical ordering.
- Outputs keep every configured slot mass and are always
  **nonpublishing**. The existing M10 frozen support-aware objective
  consumes the result without reweighting unimplemented modules.

This is an **in-memory static projection and adapter**, NOT a
materialized/qualified Phase-9 M07 artifact. In particular the full
source/feature-definition registry, Phase-6 authoritative evidence
producer, canonical JSON wire payload, RegistryDerivationSignature,
RegistryContentChecksum, RegistryRevision artifact lifecycle,
exact dependency checksums, M08 Amount/target controller, M09
availability+seed and topology proof, M11/M12 protected plan
persistence/materialization, M13 verifier and UI are NOT falsely
claimed implemented. The strings identifying the frozen M07
canonicalization contract are metadata, not a checksum generator.

The M07 PO closure's historical “Implementation authorized = NO”
applied to the preproduction freeze sequence at that time. The
current later Product Owner instruction is to execute the L3
implementation rapidly with the frozen semantics unchanged.
No old freeze document was edited or retrospectively reclassified.

Native focused Windows/Linux tests check rational masses, 27 exact
keys, alternate T1, zero-mass safety, factoryless original and
explicit opt-in C01/M03 descriptor snapshots, all 27 resolved
synthetic score inputs and fail-closed mismatches. Synthetic tests
prove plumbing only, not a real valid/qualified 27-feature audio
source or a passing M13 fresh analysis.
