# M18 — Final WAV OUTPUT Preference v1 (staging contract)
Date: 2026-11-07

## Scope and release decision

This is an implemented **staging proposal** for the stable Project
OUTPUT schema. It does not grant permission to publish, merge to main,
or label M18 complete. The Product Owner must accept the field names
and the physical Windows gate before integration. Earlier `m18Draft*`
preproduction keys remain unsupported unknown future semantics and are
**not** silently promoted.

## Exact canonical JSON

Stored inside the existing `ProjectDocument.output` object:

| Key | Type | Meaning |
|---|---|---|
| `finalMasterWavEncoding` | string, `"PCM24_TPDF"` or `"IEEE_FLOAT32"` | User's next final WAV encoding preference; no render/delivery proof |
| `finalMasterPcm24TpdfSeed` | JSON unsigned uint64 integer | REQUIRED for `PCM24_TPDF`, FORBIDDEN for `IEEE_FLOAT32` |

The pre-existing `targetMasterOutputRateHz` remains independently
authoritative for the six validated MASTER rates. Other unrelated
OUTPUT keys must be retained byte-semantic-preserving by the opaque
project codec; unknown fields still trigger conservative degraded
semantics. The exact seed, including values above 2^53 and UINT64_MAX,
never passes through a QML JavaScript Number or a double.

A legacy output `{}` or `{"targetMasterOutputRateHz":48000}` has
**no** explicit WAV preference; GUI defaults to PCM24/TPDF until the
user selects one. A new explicit PCM24 choice obtains the v1 default
seed `0x5247534D31380001`, while an explicitly persisted PCM24
seed from a valid project is retained unchanged. Selecting Float32
removes the seed completely; selecting PCM24 after Float32 chooses the
v1 default. The seed is a deterministic dithering input, **not** a
random cryptographic secret, and no noise shaping is introduced.

## Product sequencing

1. Project Open validates rate + format/seed before touching SOURCE,
   playback, chain or terminal state. Invalid/partial fields reject
   the Open atomically; valid fields become live only after successful
   Project commit.
2. The ComboBox binds to the live native Project preference and updates
   on opening another project; its selection does not claim that the
   immutable M17 Master has changed, nor grant a worker ticket.
3. The actual Product Export action reads the selected format and
   exact native seed on the Qt owner thread at the instant of explicit
   user consent. A spawned Qt-free worker captures a value-copy.
4. SaveAs retains independent unknown fields and writes the native
   choice only if one was explicitly selected/read from an existing
   Project. Legacy output remains untouched otherwise.
5. Export completion is independent of these preferences: only actual
   Windows readback, decoded exact samples, M16 delivered True Peak,
   Ceiling check, epoch revalidation and SHA-256 yield VERIFIED DELIVERED.
6. Standard M18 rules remain: no original SOURCE overwrite; no
   concurrent worker; no main merge without PO GO; one global Full
   Qualification after integrating M18–M22.

## Tests (not yet re-run since staging changes)

- `rgsml_m18_master_wav_preference_tests`: legacy/strict/64-bit/unknown
  preservation and Float32 seed exclusion
- `rgsml_project_session_tests`:
  `m18CanonicalWavPreferenceSurvivesSaveAsAndProjectOpen`
- Existing M18 verified Product PCM24/F32 transaction and full Windows
  sample-rate/channel/fault matrices

**STATUS: SOURCE WRITTEN ON STAGING — FOCUSED CI AND PO SIGNOFF PENDING.**
