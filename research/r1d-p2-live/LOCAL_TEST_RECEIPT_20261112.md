# R1d-P2 I0 local research execution receipt — 2026-11-12

- Source: exact pinned P2 headers from the archives named in README; generated files are only under `research/r1d-p2-live/`.
- Runtime: local Linux x86_64 container, GNU g++ and CMake. No physical Windows device connected to this execution.
- Configure/build: scalar Release with strict FP and `RESEARCH_P2_CACHE/RECOVERY/EASED_TAIL`; secondary Release with `RESEARCH_P2_PHASE_SSE` additionally.
- `ctest --test-dir build --output-on-failure`: 1/1 PASS.
- `ctest --test-dir build-sse --output-on-failure`: 1/1 PASS.
- Full stdout comparison (`cmp`): PASS, including generated PCM FNV64 hashes, for each rate/channel/interior/stress/short-case.
- Sample M16 native measured TP (linear): 48 kHz stressed stereo 0.880279; 192 kHz stressed stereo 0.877929; both below ceiling 0.8912509381337456.
- Explicit `operator new` witness reports zero heap allocations in `process` plus `finish` after prepare for 32 callbacks of 512 frames at 192 kHz.
- Reference PCM comparison uses the original research `PredictiveMusic` and `Stream` directly in a separately batched offline loop; after stripping initial latency, programme PCM is byte-identical across input partitions.
- This is **not** a qualification of 512-frame Windows WASAPI or the product M17 limiter, not a 64-frame physical claim, not a full I1 gate and not authorization to merge.
