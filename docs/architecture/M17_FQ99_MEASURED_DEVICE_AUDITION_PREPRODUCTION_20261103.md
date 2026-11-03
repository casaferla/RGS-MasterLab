# FQ99 — NON-CONFORMING M17 device audition (preproduction, NOT ACTIVE)

Date: 2026-11-03. Branch: `staging/m17-fq99-measured-device-audition-preproduction-20261103`. Parent: FQ98 (not to be promoted without CI).

## Blocker
A SOURCE at 48 kHz prepared as a MASTER at 88.2 kHz may produce a genuinely M16-measured NON-CONFORMING candidate. Windows configured at 48 kHz cannot directly install the 88.2-kHz terminal PCM. Current FQ96 correctly suppresses impossible `Preview Anyway` rather than mutating the source device; it does NOT yet provide cross-rate nonconforming audition.

## Isolated FQ99 macro-lot 1 (authenticity boundary, no UI)
- `app/src/terminal_measured_device_audition_ticket.hpp` is a SEPARATE type from `StagedCandidate` / verified monitor stage. It holds only an explicit nonconforming stage pointer, independently materialized DEVICE PCM, and the original manual realization ID.
- Admission requires explicit user request, the SAME current staged measured pointer after offline conversion, M16-derived positive finite miss, complete zero-origin OUTPUT_RATE PCM, applied-gain frame count, exact master+device duration/rate/layout, and nonempty endpoint token.
- A typed admission alone is NOT a playback ticket. It cannot create M17 VERIFIED, M16 LIVE, GR HISTORY, new SOURCE/PROCESSED published owner, export or Windows sink; physical DEVICE identity must still be rechecked immediately before installation.
- Native negative test uses synthetic stages clearly marked as structural fixtures, not real M16 evidence. Tests cover consent, stale pointer, empty endpoint, short PCM and actual converted 192->48 PCM binding.
- Never return the original MASTER PCM itself as device PCM when the rates differ.

## Still required for real FQ99 closure
1. Capture only on QObject owner thread, with SOURCE epoch, selected Project MASTER rate, true measured stage identity and USER-EXPLICIT opt-in.
2. Reuse the existing Windows six-rate, memory, cancellation and pinned-FIR preflight, but keep a *distinct unqualified* controller/transport and no M16 LIVE path. Protect a full-duration conversion from stale source/loop/pending state and worker exceptions.
3. On explicit command, atomically prove original MANUAL sink identity, prepare independent DEVICE PCM, recheck device ID/Hz/format before actual AUDIO installation, and prove new anonymous sink owner. Do not silently stop MANUAL if DEVICE/PCM cannot be admitted.
4. Play/Pause/Stop/Seek/Loop must be in MASTER coordinates, mapped from DEVICE PCM only, and not confuse SOURCE or any true-peak chronology.
5. Return to Source must preflight original MANUAL PCM on a fresh physical endpoint, conditionally retire exact unqualified sink, restore cue/loop/state, retain result on recoverable failure, and fail closed on alien sink. Proven native fake cases and Windows physical evidence required.
6. Repeat a genuine nonconforming 48->88.2 rendered output on physical 48k Windows device, and 48->48 identity. Verify exact M16 measured peak and requested Ceiling remain attached to original MASTER, not recomputed after playback SRC or rounded to false equality.
7. FQ99 UI button remains disabled for cross-rate until all above gates pass; never convert a broken UI block into bogus success by borrowing verified status.

Release rule: an FQ99 data model PASS is NOT an M17 audio gate PASS. PR #61 and main must not merge merely on this new type.
