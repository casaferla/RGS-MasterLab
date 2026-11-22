# L3 AutoMatch — M16 Analyzer vs M17 WAV Export separation (2026-11-22)

Product Owner clarification, authoritative for current implementation:
- M16 is a read-only True Peak analyzer. AutoMatch MUST measure actual
  candidate and newly rendered REF_LINKED chain output with this native
  M16, within the separately accepted AutoMatch safety policy.
- M17 is Export TP Limiter/WAV delivery, independently controlled by
  the user's chosen WAV ceiling and output encoding PCM24 or FLOAT32.
  It is **not** an AutoMatch DSP node, and must not be inserted,
  configured, activated, or held hostage by AutoMatch.
- P2 Streaming True Peak Limiter is a user-managed, optional DSP with
  its own independent Ceiling/Drive, distinct from M17 export settings.
  AutoMatch MUST NOT take over this module either.
- M12 writes only the accepted REF_LINKED Match DSP nodes; native
  MANUAL, Input Gain, P2, project reference, Output Export/M17 and
  all other settings remain outside the reference-match controller.
- M13 independently rerenders actual MANUAL + REF_LINKED using the
  Product DSP registry then calls analysis::measure_true_peak_full_resource
  on the complete post-Match PCM. M16 is not the Export limiter and
  never applies an implicit gain or ceiling trim.
- Real M16 numeric result is not automatically a formally QUALIFIED
  M16 Gate-G release certificate; this change does not assert M13
  verified or enable WAV export.

Correction to earlier focused gate #32:
The preflight had incorrectly required an active M17 terminal and
equalized its ceiling with AutoMatch; those checks have been removed
from AutoMatch M12/M13. An optional read-only M17 export diagnostic
remains independent, and its presence/absence is never M12/M13
admission authority.

Separate issue: project metadata uses existing QJsonDocument + the
project::OpaqueJsonValue canonical JSON path. nlohmann/json.hpp was
not a repository dependency and caused Linux gate #32 compilation
failure; it was eliminated without adding a new package.
