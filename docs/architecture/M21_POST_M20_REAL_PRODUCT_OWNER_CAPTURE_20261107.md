# M19–M21: authenticated real PREPARED/PROCESSED Product owner capture
Date: 2026-11-07
Status: staging implementation, **CI/official gate pending**.

## Purpose
Prior M19–M21 integration proved numerical APIs on actual AudioBuffer
fixtures but still let caller-supplied owner strings stand in for Product
resource identity. This change implements the first owner-side bridge:
`app/src/internal/m21_product_owner_analysis.hpp`.

On ONE current Qt owner thread, it captures the actual immutable
`AuditionSourceSelector::prepared_realization_snapshot()` and
`processed_realization_snapshot()` plus the accepted real
`processed_realization_id()`, SOURCE reference, and authoritative
`M18OwnerMutationFanout::OwnerSnapshot` (epoch, SOURCE/Project/MASTER
generations, chain revision, master-rate revision).

It accepts the capture ONLY when the complete M18 SOURCE/Project/M17
Single-Writer guards are already bound. The Product rate and manual chain
must agree with the captured authoritative epoch. The source must have
read permission. An absent processed result remains explicitly absent
rather than being filled with RAW, PREPARED or a GUI value.

`measure_current` analyzes the actual retained render result using
M19 public full-resource level and M20 public real PCM scope adapters,
then converts their typed statuses through M21's existing Gold−Current
port. It rechecks SOURCE, pointer identity, processed realization ID,
all Product generations/epoch and current chain/rate both before and
after the synchronous read-only analysis. No QObject crosses a thread
boundary and there is no event-loop pump. An M16 numerical TP remains
SUPPORTED_UNQUALIFIED, never release-authorized by this capture.

The test `rgsml_project_session_tests` now exercises the **real Product**
M18 fixture, M17 verified owner, two real PCM analyses, a parameter-only
Gain edit with unchanged structural chain revision, a first SOURCE with
no processed realization, and a same-rate SOURCE replacement. Stale
snapshots must refuse publication. Independent GOLD can only come from
its own authenticated read-only WAV owner; `Column::gold` is denied
until that independent decoder/binding exists.

## Limitations to close
- No genuine GOLD `SourceResource` decoder / immutable PCM owner yet.
- No asynchronous worker scheduling, revision-tagged completion queue,
  retained-block Phase-6 status/registry, M20 qualified band support,
  public GUI result publisher or independent inter-column Gold scopes.
- Synchronous analysis is *only* a test/owner-thread integration seam;
  do not call it directly for a long real programme from the Qt GUI.
  Full programme analysis must run off-thread after an immutable capture
  and be revalidated on the Product owner thread before publication.
- Does not change M18 export, M17 DSP, true peak FIR, QtQuick UI or main.
- M18 physical A01–A15 and PO approval remain deferred. No FQ or
  merge of PR #84/#86/#87/#88/#89 follows from these tests.

Use the single scoped M22 post-M21 focused CI Windows ProductSession
for compilation/regression; Linux tests only portable M19–M21.
