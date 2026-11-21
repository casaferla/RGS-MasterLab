# RGS MasterLab — P2 incremental PCM prefix cache, first Product candidate

Date: 2026-11-14. State: **L1 nonrelease staging, CI pending**, not M24/FQ/physical qualification. This document does not authorize main merge, a default-on P2 or a user timing SLA.

## Why this specific first step

P2 has five genuine MANUAL processors. Adjusting Ceiling/Drive/Margin or toggling P2 bypass previously reran all four upstream DSP stages plus P2. The independent R1d-P2 research cache proved bit-for-bit PCM equivalence in representative scenarios and exposed the danger of naive post-trim stage substitution. This Product implementation uses the **canonical full-chain RenderPreview as a cold oracle** and admits a cached prefix only when every binary64 PCM sample and the full result's signatures, revision, window/domain and sidecar state agree.

The cache lives **only on the MasteringPreviewController worker thread**, never in the Windows callback, UI thread or project serializer. It is now a multi-entry, bounded, worker-local LRU of immutable shared-pointer prefixes; the independent PREPARED RenderResult pointer is the source-content authority. Changing SOURCE produces a different immutable owner; raw pointer reuse is impossible while the cache retains the previous shared_ptr. No mutable PCM buffer is accepted as a cache key.

## Exact first-phase eligibility

- The complete full SOURCE_PROCESSING_RATE window and exactly ordered Gain → Parametric EQ → Compressor → Stereo/M-S → Streaming TP Limiter, with five matching immutable typed bindings.
- Compressor and Stereo/M-S must both be genuinely inactive/bypassed; otherwise **full fresh render** is mandatory. This intentionally excludes upstream lookahead, M14 telemetry and M15 stage capture/correlation/density. They must never be silently omitted.
- Immutable prepared owner, opt-in registry owner, first four ModuleInstance states, first four typed bindings and fixed processing block size must all compare equal on cache hits. The fifth P2's typed settings and bypass MAY differ; they are rendered fresh as the suffix.
- At most **256 MiB total cached prefix PCM** across a multi-entry LRU; old entries are evicted under memory pressure. The PREPARED source and already published/audible results have their own pre-existing lifetime memory and are not counted as cache PCM. Larger sources, unknown topology, invalid metadata and rejected suffix fall back to canonical full rendering.
- A cold prefix is admitted **only after** rendering full original, rendering four DSP stages, rendering the P2 suffix, comparing every binary64 sample bit, and comparing full signatures, revision, domain, window, M14 and M15 sidecars. A mismatch means no cache admission. This cold probe costs more than the ordinary render; it is *not* a throughput win on the first candidate.
- The Product renderer's new suffix-only function refuses partial requests, absent fifth type/identity, mismatched upstream signatures, any M14/M15 sidecars or unexpected output traits. No producer sidecar is invented.
- Cache lookup is invalidated by changes to upstream typed parameters, bypass/state/identity or block size. Multiple earlier prefixes may coexist within budget so an A/B Gain or EQ revert can produce an LRU hit. A different immutable PREPARED SOURCE or registry clears the prior owner's complete LRU immediately, including on ineligible requests.

## Tests / evidence

The P2 MasteringPreviewController suite includes a genuine worker-level cold oracle, changed P2 settings and bypass cache hits, retained Gain A/B replay with two prefixes, SOURCE-owner invalidation, 44.1/48/96-kHz waveforms and fail-closed M15 active topology. Each accepted candidate is also independently fresh-rendered by the test and compared sample-bit by sample-bit, including signatures and sidecars. The research timing TSV has appended `cache_hit=0/1`, `cache_entries`, `cache_pcm_bytes` and `cache_evictions` fields, so PO can distinguish full rebuild from suffix reuse without trusting impressions of speed. These checks need a new exact-SHA Windows/Linux integrated focused run.

**Not yet qualified:** prefix reuse with active Compressor or Stereo/M-S, any upstream timebase/latency-bearing DSP, broader dynamic per-module suffix caching, M16 consumed-output owner proof, 44.1→48 physical WASAPI continuity, real memory pressure, project reopen under a live sink, and any AutoMatch solver. None may be inferred from this staging candidate.

## Remaining work after this candidate

1. Finish focused CI and inspect bounded failures. Never claim a green before GitHub completion.
2. Independently qualify telemetry/latency-aware stage checkpoints, memory pressure and eviction, stale-job race and exact caching for changing earlier DSP nodes.
3. Measure cold/hot wall-clock on physical Windows, sample rates 44.1/48/96 kHz, representative five-minute audio, and M16 status. Preserve M17 final limiter and M18 WAV delivery proof.
4. Reuse the verified bounded engine for the **minimum L2 interface** feeding L3 AutoMatch's repeated candidate evaluations, not a speculative rewrite of the DSP chain.

No FQ, release or M24 physical approval is claimed by this design note.
