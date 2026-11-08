# RGS MasterLab — Phase-6 release eligibility / M22 generalized planner after qualified M17 GR HISTORY

**8 November 2026 · isolated architecture and nonrelease preintegration · not a PO release gate · NO MAIN MERGE.**

## Immutable green baseline

- M18–M22 UI/Product preintegration #7: Windows/Linux SUCCESS, SHA `3aca7bdf4d96269d86e6690617642b876dcb3cbc`, CI https://github.com/casaferla/RGS-MasterLab/actions/runs/42492719918.
- Rebased M17 GR HISTORY #2: Windows/Linux SUCCESS, SHA `c096637b01ba31b8ea83055d35bdb16a20e3c487`, CI https://github.com/casaferla/RGS-MasterLab/actions/runs/42497222058, PR #97 Draft.
- Last GR CI #2 artifact contains **four real Qt captures** of a synthetic GR fixture, including whole M17 editor at 25% and 75% (966 × 480). Text glyphs are boxes in the headless renderer, so only geometry/cursor/shape are claimed, **not** real font/physical PO acceptance.
- Branch created for the next independent single-writer lot: `staging/post-gr-phase6-m22-preintegration-no-ci-20261108`, based **exactly** on the GR HISTORY #2 green commit. All predecessors and candidates remain frozen. Do not start new CI until a coherent change set exists.

## Phase-6 authority — important stop-line

Drive frozen **Phase-6 Residual Semantic Closure P01–P06**, ID `1rBiwPOz5lMGDMGuKyFNVzw9d9SIBhS3Znx3aWVr-eE4`, authorizes **semantic decisions**, not product code release or Gate G. Explicit frozen tokens `PHASE6_IMPLEMENTATION_AUTHORIZED_BY_THIS_DECISION = NO`, `GATE_G_EXECUTED_BY_THIS_DECISION = NO`. A later live-repository reconciliation and a Product Owner implementation authorization are separately necessary.

Accepted binding choices to preserve before canonical implementation:
- P01: six target clock banks 44.1/48/88.2/96/176.4/192 kHz with exact bank identity/hash.
- P02: `MEAN_CHANNEL_POWER`, never summed-channel or per-channel substitute.
- P03: `CONTINUOUS_FFT_CELL_PSD_FIRST_MOMENT`.
- P04: `DIRECT_RATIONAL_200HZ_GRID`, current-frame-center events, next-power-of-two FFT, inclusive bin-center frequency membership, raw positive spectral flux, robust Z separately.
- P05: defer undefined macro-dynamics occupancy rather than fabricate a feature.
- P06: `LOUDNESS_GATED_ACTIVE_UNION` and canonical K-weighting scope-state origin.
- Include `rgsml.phase6.residual-semantic-closure/1.0.0` or individual policy IDs in **future official** signed feature dependencies. Do not rewrite historical GOLD persisted identifiers to REFERENCE.

**Current repository reality:** `app/src/m21_product_status_view_model.hpp` has `production_authorized() const noexcept { return false; }` and `M21ProductQtReadinessBridge` returns status/quality only, with all numeric payloads masked. The M19/M20 real worker and M21 Inbox can accept true numerical results, but that is NOT the canonical `FeatureDefinitionRegistry` nor its versioned `FeatureStatus`. No second registry or parallel persisted schema may be introduced. Do not turn `productionAuthorized` true or bind numerical QML data until genuine canonical registry, producer signature, active scope, epoch and release status are materialized and qualified.

## M22 exact implementation gap, not M18 success rewritten

- Genuine current Product path `M22ProductVerifiedDispatch` already reuses the *same* M17 fully verified binary64 Master, true epoch and M18 user-consented transactional PCM24/Float32 export with SHA/ACK. This is an actual working subset.
- `render/src/internal/m22_request_plan_candidate.hpp` is deliberately **PRIVATE metadata-only preflight**. `supported_clock` lists six rates; `validate()` explicitly rejects `processing_rate_hz != target_rate_hz`. This is NOT the target generalized Final SRC workflow.
- `render/src/internal/m22_canonical_chain_port_candidate.hpp` maps authentic Master MANUAL registry and typed bindings but still expects **external** frozen parameter fingerprints, active latency and factory proof; it never instantiates the real DSP executor.
- A generalized M22 planner must bind **real Q0 FIR kernel selection** for each supported target conversion, checked output length/timebase, exact source vs output frame mapping, immutable module factory bindings and latency, global-M17 terminal verification through true EOS, preview from full checked Master (never region-only limiter), and absolute Philox dither frame addressing. No helper may invent FIR identities, inferred output sample counts or M16 TP witness.
- Next safe code lot: build a *typed, fail-closed, read-only* provenance adapter and independent adversarial tests for rate/timebase, preview absolute window/EOS, stale source/chain epoch and source-vs-output frame domains; then bind to authoritative Final SRC assets and real Product execution without replacing checked M17/M18 backend. Do not mark the generalized scheduler complete from a metadata-only plan.

## Final release gates

- M21 Gate S technical status/epoch/numeric evidence, M21 Gate M physical Windows functionality. M19/M20 respective authority gates. Phase-6 canonical registry and Gate G release are NOT equivalent to CI passes.
- Physical PO M18 A01–A15, Windows 4K+FHD panels, playback/crossfade, full-integrated FQ deferred only until PO has PC; all code-side work continues meanwhile.
- PR #96, PR #97 Draft; no unapproved merge on `main`. Next CI should include M17 GR, public REFERENCE, M18–M22 Product tests and dedicated new Phase-6/M22 qualification in one focused gate.

## PO clarification — unified 16 FIR versus M22 final rendering 14 Q0

The product has **ONE approved 16-file FIR asset catalog** in `resources/fir` with one `fir_manifest.json`, one trusted SHA-256 inventory, one install path and uniform missing/tamper/no-fallback behavior. Its **14 unique frozen original-Q0 coefficient sets** mathematically cover all **30 directed SOURCE→MASTER** nonidentity six-rate Final SRC conversions; six identities use no FIR. The additional **two legacy playback kernels** retain different frozen coefficient bits, and are used only for the 44.1↔48 kHz `SOURCE/MASTER→DEVICE` playback routes. The other 28 DEVICE nonidentity routes use frozen original-Q0. A shared catalog does NOT mean that these distinct bit-exact coefficients are freely interchangeable.

Therefore, `m22_final_src_frozen_pin_candidate.hpp` correctly binds ONLY the 14 original Q0 FIR **identities for final Master conversion**. It MUST NOT substitute the 44.1↔48 historical playback coefficients for checked final-Master Q0, nor add a second catalog. **However**, the overall application CI/release gate must separately audit the COMPLETE unified 16-file package and all 30 DEVICE route/fallback semantics. The existing trusted `tools/qualification/playback_src/test_approved_16_fir_package.py` has now been added to BOTH OS in the next `m22-post-gr-focused.yml` candidate and is required by `tools/qualification/m22_post_gr/verify_no_release.py`. Both test classes are complementary, not exceptions to one unified FIR policy.

This additional gate was staged **after** the dual-green M22 CI #3 on `ac2d0d30`; it is NOT retroactively certified by #3. No changes to 16 coefficient files, SHA pins, M17 DSP, Main or canonical Product; no main merge. The next focused CI will qualify these checks as a coherent batch, while true M22 generalized Product execution, Phase-6 feature registry/Gate G and physical Windows FQ remain unqualified.

## Authentically executed Q0 source conversion — 2026-11-08 macro-lot

The earlier isolated M22 planner merely matched all thirty rate ratios to fourteen Q0 *names* and frozen SHA/geometry pins. This new preterminal bridge advances from metadata to **real immutable SOURCE PCM → genuine Phase-11 Q0 strict-scalar conversion → complete OUTPUT_RATE PCM**. It reuses the existing `materialize_phase11_scoped_q0_pinned_offline_candidate` without replacing a single DSP operation. No other SRC algorithm, FIR tap, signed SHA pin, manifest or M18 output code is altered.

- New `render/src/internal/m22_authentic_q0_conversion_candidate.hpp` refuses incomplete/nonzero-origin SOURCE, unknown rate pairs, absent/wrong/tampered original assets and FIR-on-identity. It passes bytes to the *existing* SHA-authenticating Q0 parser, and validates the actual result's source/target clocks, full frame count, channel layout, `OUTPUT_RATE` timebase and trust-pinned kernel SHA. Every returned result is **pre-terminal**: `authorizes_release()=false`, NOT checked M17, NOT Export.
- New Qt test `tests/unit/render/test_m22_authentic_q0_candidate.cpp`: executes all **thirty actual directed Q0 convolution paths** on genuine `resources/fir/RGSMSR1_L*_M*.bin` (not synthetic taps), checks actual per-conversion output sample count and finite PCM, repeats 44.1↔48 kHz binary64 exact-bit waveforms, zero-copy output-rate crop pointer equality, all six same-rate identity clocks in mono/stereo with bit-exact sample copies and no FIR, rejects swapped/tampered FIR, nonzero SOURCE origin, wrong frame domain and nonfinite source samples, and proves cancellation does not yield a partial published output. This is a new independent **real source signal** test, not a Product FQ or a Master/DEVICE audition substitution.
- New target `rgsml_m22_authentic_q0_candidate_tests` builds on both Windows/Linux with frozen source assets; the CI now invokes it alongside `rgsml_m22_checked_output_clock_candidate_tests` and `test_approved_16_fir_package.py` for the **single unified 16-asset manifest**. The 14 original-Q0 execution gate and complete 16-FIR catalog/integrity gate remain complementary. Existing M17 full-history, public REFERENCE and actual M18 ProductSession regressions remain unchanged.

**Critical distinction:** authentic Q0 *source conversion* is before the manual DSP/M17 terminal. Its output alone is not a final Master, so the independent Product M17 true-peak check, immutable owner epoch, true EOS/dither/WAV path, canonical Phase-6 registry and Q1–Q6 qualification must still be completed; do not misleadingly name it a released final Master. This staging lot is non-FQ and no main merge is authorized. Before final release, cross-rate M22 must bind this conversion to real M17, the complete 16-asset Q1–Q6 evidence, and genuine Product Source/Reference states.


## CI #5 qualified; next real Manual/Q0/M17/M16 integration rehearsal (nonrelease)

Focused M22 [CI #5](https://github.com/casaferla/RGS-MasterLab/actions/runs/42504003700) at exact SHA `672ff223254ee39a6fb25ca8b40ebbba68863526` completed **SUCCESS on both Windows and Linux** after the one-line C++ test harness correction. This qualifies the preceding real 30-direction Q0 source-PCM candidate, six same-rate identities, cancellation/tamper rejection and the complete **unified 16-FIR manifest** inspection; it does not certify Phase-6/Q1–Q6 or release.

A new single-writer staging-only test now exercises the EXISTING production DSP classes end-to-end, rather than introducing a competing renderer: authentic zero-origin SOURCE -> actual MASTER/MANUAL Gain RenderRequest/RenderResult -> one of the **14 original Q0 coefficient identities in the 16-FIR shared catalog** -> the existing `render_true_peak_limiter_owned_output_rate_candidate` -> real typed terminal M17; on active cross-rate paths, REAL M16 verification. It tests all 30 directed nonidentity target-rate conversions with genuine FIR bytes and frozen SHA, plus both 44.1<->48 active-stereo M17/M16 cases, immutable manual owner, full-frame count/OUTPUT_RATE mapping, zero-origin and fail-closed swapped/tampered FIR and stale manual revision. Bypass is bit-compared against a separate invocation of the same original frozen Q0 implementation. Original coefficients, the two specialized device playback kernels, original DSP sources, M17 algorithm, Product owner, public GUI, persisted GOLD and main remain untouched.

This new native test target `rgsml_m22_real_manual_q0_m17_chain_candidate_tests` is included in the focused Windows+Linux build/CTest and exact-scope nonrelease provenance guard. This scope proves only the isolated real pipeline, **NOT** the canonical immutable Product M22 scheduler/publication, release authority, signed canonical Phase-6 FeatureDefinitionRegistry/Gate G, Q1–Q6 qualification, physical Windows PO approval or final FQ. The 16 FIR remain **one** package with differing frozen coefficient selection for Final MASTER versus two special DEVICE playback cases. The focused next CI result is not known at document preparation time.


## Post-CI #6 — M22 Product full-chain witness + exact SOURCE epoch, macro-lot 1

CI M22 #6 (https://github.com/casaferla/RGS-MasterLab/actions/runs/42505382303) completed **SUCCESS on Windows and Linux** at exact SHA `f0ac4b5a54767dfc9a275daef173b3da32748536`. This confirms the prior 30 actual original-Q0 Final-SRC routes from a real MANUAL Gain render through M17 chain-owned bypass, plus both 44.1↔48-kHz genuine active M17/M16 checks, and the shared 16-FIR manifest. It does not itself authorize M22 Product production.

The next **single-writer macro-lot** adds a _read-only, fail-closed_ native witness and an owner-thread M18 Product capture, without replacing any renderer, SRC, M17 limiter, M16 final check, M18 exporter, file serializer, phase-six authority, or GUI control.

- `render/src/internal/m22_full_chain_witness_candidate.hpp` observes genuine retained SOURCE-domain full MANUAL RenderResult, its immutable revision and complete source length; actual terminal output-rate PCM domain, zero origin, actual frame length and channel layout; actual active M17 gain witness count; genuine final M16 check; and the strictly frozen original Q0 SHA for nonidentity routes. Same-rate paths must use no FIR. No output length is invented from rational metadata. `authorizes_release()=false` is unconditional.
- `app/src/internal/m22_product_chain_epoch_capture_candidate.hpp` is a separate **actual Product owner-thread adapter** over existing `M18ProductCompleteBinding::can_offer_verified_master`, `TerminalCandidateGate::staged()`, M17 accepted full stage and `M18OwnerMutationFanout::current_owner_snapshot()`. It preserves the exact readable physical SOURCE ResourceReference, current native Product epoch, the exact immutable M17 stage and real checked-chain witness. `still_current()` rechecks exact same physical SOURCE identity, owner generation, current stage pointer, source length, rate, chain/terminal revisions, and witness. It is NOT exposed to QML/worker and does not mint Product consent, WAV, DEVICE audition or separate registry.
- The genuine Qt native render integration test now covers **all six same-rate active M17/M16 identities without FIR**, both active stereo original-Q0 cross-rate paths, all thirty nonidentity paths with M17 chain-owned bypass (correctly rejected for Product release), and adverse mutations: missing manual owner, stale manual revision, shortened source, forged output clock, forged FIR SHA, absent/unsupported M16 check, spoofed bypass, dropped M17 gain history, unknown terminal revision, false SRC/no-SRC classification, and recovery after rejected probes.
- Dedicated `rgsml_m22_product_chain_epoch_capture_tests` compiles the actual Product M18 types/Qt interfaces on Windows and Linux but **does not counterfeit a live M18 Product installation**. Native physical Product activation / generalized cross-rate export and full Q1–Q6 remain future gates. The CI preserves the existing M17 GR HISTORY/REFERENCE/M18–M22 tests and complete unified **16-FIR** integrity gate.

**Hard release boundary:** untouched `main`; PR #96, #97, #98 remain Draft; current candidate has no signed FeatureDefinitionRegistry/Gate G, Product generalized M22 scheduler, official Q1–Q6 or user Windows physical FQ. A green focused CI proves only the stated candidate tests; the Product owner capture is not itself the full release pipeline. One writer, staging first, one focused CI per cohesive lot, no merge without Product Owner GO.


## Post-CI #9 — close real M22 cross-rate Preview source/output frame-count mismatch (nonrelease)

Focused [M22 CI #9](https://github.com/casaferla/RGS-MasterLab/actions/runs/42511421884), SHA `fee953111256fae4edf8034131bea47908174a92`, **COMPLETED SUCCESS on Windows and Linux**. PR #98 remains Draft. This qualifies the preceding native M22 full-chain witness + Product contract compilation, **not** a full generalized Product release, M22 Q1–Q6 or physical Windows listening.

Real defect discovered in the existing previously green Product Preview seam `app/src/internal/m22_verified_master_preview_parity.hpp`: the condition `full.frame_count()!=rendered.programme_frames` wrongly treated OUTPUT_RATE full M17 PCM length as equal to SOURCE_RATE whole-manual owner frame count. It is true for an identity-rate render, but false for legitimate original-Q0 nonidentity rate conversion. Do not substitute an estimated rational frame count to make this condition pass.

**Surgical correction in this single writer**:
- New `render/src/internal/m22_verified_full_output_window_candidate.hpp`: nonrelease value + zero-copy output-rate crop of the **real retained full manual/Q0/M17/M16** source candidate. Calls `observe_real_complete_chain_candidate`, insists on active checked full M17+M16, authentic SHA-pinned original Q0 when cross-rate, an output-relative absolute window, true full OUTPUT_RATE PCM frame count, zero-origin EOS and valid output clock. A bypassed M17, fake pin, incomplete manual owner, missing final M16 or invalid region fails closed. It does not publish a Master, export, quantize, rerun M17, initialize state or authorize release.
- Existing `m22_verified_master_preview_crop` still requires genuine `M18ProductCompleteBinding::can_offer_verified_master` and exact staged current M17 owner, same user chain, MASTER rate, physical SOURCE sample rate/layout/length, terminal revision, before delegating to the native checked output window. Existing `M22ProductPreviewAdmission::Capture` keeps the shared owner alive and checks epoch before queued use. It no longer conflates SOURCE frame count with OUTPUT frame count.
- The original Windows/Linux native M22 test extends both actual 44.1↔48 active stereo Q0+M17 paths (binary64 bit-identical zero-copy subview, output clock/absolute window, bad-window refusal), all six same-rate active M17 identities, and fail-closed M17 bypass on all 30 nonidentity Q0 routes.
- The existing focused CI and strict provenance guard are extended for this explicit narrow fix; no new CMake target, no second SRC, no change to 16 FIR assets. M21 Product numerical authority remains OFF; original Phase-6 P01–P06 semantic decision is not Product implementation approval.

**Open:** validate this follow-up in a new Windows+Linux focused CI after the complete writer macro-lot. Live Product cross-rate capture/queued preview, generalized immutable M22 execution scheduler with true EOS/state replay, complete Q1–Q6 and PCM24 absolute frame dither full-crop verification, Phase-6 FeatureDefinitionRegistry/Gate G, PO A01–A15 and first real mastering/FQ are not certified by this private change. Never infer completion from green static/Qt focused tests.


### Real post-M17 PCM24/Float32 absolute-frame crop evidence (same follow-up)

The same native test now sends **actual checked M17 OUTPUT_RATE PCM** (not only detached synthetic M18 fixture) to the previously accepted M18 terminal codec:
- both real nonidentity 44.1→48 and 48→44.1 full programmes, stereo, real original-Q0 asset SHA and full M17 final M16 check;
- convert FULL and exact 9..41 OUTPUT_RATE crop to real PCM24 TPDF using the same seed, and verify byte-exact crop of FULL; poison test `wrongLocalDither!=cropPcm24` rejects preview-local zero reseeding;
- encode same full M17 PCM24 in 17-frame chunks, exact byte equality with full single-block M18 encode;
- compare actual IEEE_FLOAT32 bytes of full vs exact preview crop; existing six same-rate binary64 M17/PREVIEW tests remain and bypassed M17/Q0 never receives Preview admission.

This is the **real manual-DSP/Q0/M17→M18 representation seam** rather than claiming M18 digital codec is reimplemented. It is still nonrelease and not a full WAV readback of all 36 route combinations, generalized scheduler, signed Phase-6 Registry or PO physical approval.


### 30/30 cross-rate active M17/M16 native check added

Beyond the previously green 30 bypassed routes and two active stereo 44.1↔48 cases, `allThirtyActiveOriginalQ0RoutesUseGenuineFullM17AndM16` now exercises all **30 actual original Q0 conversions** through the real immutable SOURCE-domain Gain/Manual owner, real SRC asset, active M17 terminal and original complete M16 final-check. Every route verifies actual OUTPUT_RATE frame count, rate, retained original SHA identity and nonrelease zero-copy Preview. This is **test source staged**, not qualified or asserted as a success until its own Windows+Linux CI passes. It does NOT replace Phase-11 Q1–Q6, physical Windows qualification, actual Product GUI/user master or safe WAV postwrite checks.
