# L3-M09 P04 — Exact candidate de-duplication, writer slice

Source authority: **M09 P04 ACCEPTED_FROZEN** and M09 Product Owner closure
(7 October 2026), read from RGS ContentBox. No historic document is changed.

This bounded code adds only the **in-memory** exact generation boundary:
- Existing M07/M08 canonical serializer MUST provide complete unsigned byte
  semantic topology key, seed content, dependency tuple, and exact input
  snapshot. Localized UI names, rounded-Hz aliases, typeId-only grouping and
  current availability never generate keys in this layer.
- Exact triple (semantic tuple, seed exact content, exact dependency tuple)
  de-duplicates and **appends provenance in arrival order**.
- Same complete semantic key with different exact seed within the same
  immutable generation reports `candidate_key_seed_collision`, POISONING
  the entire generation. No first-wins, invented tie or partial emission.
- Legitimate discrete alternatives (SIDE EQ, LR modes/cutoffs, roles) are
  distinct only if that distinguishing semantic identity is already in
  the supplied key. Changed snapshots start a new generation.
- Phase-9 candidate/seed revision, canonical JSON, checksum generation,
  actual abstract topology enumeration, proxy score, P01–P03 seeds,
  M10 measured shortlist, module factory availability, M11 fixed-chain
  protection, M12 REF_LINKED and M13 verification are NOT implied.
- The consumer receives descriptions, never product publish/export rights.

Native QtTest checks de-duplication, provenance ordering, collision poison,
same-DSP distinct roles, distinct LR variants, changed dependencies and
stale snapshots. This is policy plumbing, not M09 end-to-end completion.
