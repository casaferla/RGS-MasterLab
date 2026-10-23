# RGS MasterLab — M17 Realtime Feasibility Gate v1.0

Date: 2026-10-23. Lane A, experimental PR #55.
Classification: PRELIMINARY / NON-AUTHORITATIVE / PRODUCT OWNER DECISION PENDING.

## Gate verdict

PASS: exact isolated selector arithmetic and bitwise frozen M17 PCM on verified cases. The monotone tick cursor consistently improved the 64-frame integrated mean against the original binary-bound selector.

NO GO for claiming this experimental selector is qualified HARD REALTIME at 192 kHz and 64 frames. Every 192 kHz p99 measured on both CI hosts exceeded the strict 333.333 microsecond deadline. A mean speedup is not deadline qualification. This negative claim concerns this tested implementation and does not prove realtime limiting is impossible for other architectures.

KEEP PR #55 OPEN/DRAFT/UNMERGED and PR #54/main and the frozen M17 DSP unchanged. No product replacement, FQ, UI change or automatic offline fallback follows from this gate.

## Evidence and methods

EXTENDED run #696: https://github.com/casaferla/RGS-MasterLab/actions/runs/39941175391 ; commit 8f674f885147ce65d0ce08d7726a31fd9386e25c; Windows and Linux SUCCESS. Use the RGS-M17-IntegratedLazy-Windows-696 and corresponding Linux-696 downloadable workflow artifacts, not cross-run host comparisons.

Each platform contributed 96 CSV rows: 48/192 kHz, mono/stereo, -6/-12 dBTP, FROZEN/LAZY controllers, BINARY/CURSOR locators and three independent repetitions. Each record reports PCM bitwise PASS, detector drained PASS, qualified realtime NO. The table uses the 12 paired LAZY cases per rate and host.

| LAZY 64-frame metric | Windows 48 kHz | Windows 192 kHz | Linux 48 kHz | Linux 192 kHz |
| --- | ---: | ---: | ---: | ---: |
| Median BINARY/CURSOR quantum mean ratio | 1.084x | 1.064x | 1.141x | 1.101x |
| Faster CURSOR quantum mean | 12/12 | 12/12 | 12/12 | 12/12 |
| Median BINARY/CURSOR selector mean ratio | 1.289x | 1.189x | 1.308x | 1.185x |
| CURSOR mean quantum range, microseconds | 301–384 | 402–519 | 1,280–1,471 | 2,005–2,385 |
| CURSOR p99 range, microseconds | 355–462 | 505–696 | 1,568–1,854 | 2,751–3,481 |
| CURSOR p99 exceeding corresponding deadline | 0/12 | 12/12 | 12/12 | 12/12 |
| CURSOR p99 worse than BINARY | 2/12 | 2/12 | 0/12 | 0/12 |
| CURSOR deadline misses in 128 quantums | 0 | 103–117 | 36–52 | 128 |

Budgets: 48 kHz = 1,333.333 microseconds/64 frames; 192 kHz = 333.333 microseconds/64 frames. Windows at 48 kHz had no misses in these limited tests, but that is not a real-device qualification.

## Coverage boundaries

Integrated quantum A/B timings cover only 48 and 192 kHz. Other EXTENDED M17/M16 assertions cover the six frozen rates: 44.1, 48, 88.2, 96, 176.4, 192 kHz, mono/stereo, exact PCM, EOS and checkpoint. The FAST #697 test adds six-rate signed-detector CURSOR vs BINARY exact attack-floor/winner verification, including resetting the cursor after accepting a new batch. This still is not six-rate integrated realtime measurement.

CI hosts are not pinned low-jitter audio workstations; per-tick instrumentation adds overhead. The waveforms are synthetic. CI deadline misses do not mathematically disprove realtime on all systems, but cannot be translated into successful realtime evidence either. All reported p99s above the target prohibit a qualified realtime claim.

## Findings and bounded decision

Cached Hann/log demands improve isolated sparse selection, but do not qualify the 192 kHz quantum. About 80% of demands were exactly reusable across measured adjacent detector batches, yet matcher overhead often offset the work removed. Over 95% of detector future events were already skipped conservatively in the dense 192 kHz cases. The cursor avoids repeated window-boundary binary searches and produced the most repeatable integrated speedup in this sprint, but p99 failed.

STOP the bounded #694–#697 micro-optimisation sprint after successful #697 FAST. Do not automatically initiate #698. Retain evidence for a separate PO-directed REAL_TIME_FIRST architecture search only if supported by a new falsifiable hypothesis. LANE B R1 Protector gate found NO PROTECTOR PASSED; do not silently integrate A/S/O candidates or reopen the frozen C4 bridge.

PO alternative, not authorised here: M17 may become a terminal module requiring explicit OFFLINE preparation/processing before its audio output is available to audition, compare or render. This is distinct from Stage 1 RESTORE/PREP and requires separate lifecycle, progress, cache validity, error/bypass, resource and UI contracts, followed by PO authorisation and qualification.

Final experimental disposition: RESEARCH_GATE_COMPLETE_WITH_NO_REALTIME_QUALIFICATION subject to successful #697 Focused CI. Approval authority remains with the Product Owner.