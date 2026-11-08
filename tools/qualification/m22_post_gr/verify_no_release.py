#!/usr/bin/env python3
"""M22 output clock, real PCM and frozen Q0 FIR identity: nonrelease gate.

One-writer post-GR work. Full Phase-6 feature registry, Q1-Q6 Final SRC,
M17/M18 Product admission and manual Windows FQ are NOT inferred here.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
BASE = "c096637b01ba31b8ea83055d35bdb16a20e3c487"
ALLOWED = {
    "docs/architecture/PHASE6_M22_POST_GR_UNRESOLVED_GATE_PLAN_20261108.md",
    "render/src/internal/m22_checked_output_clock_candidate.hpp",
    "render/src/internal/m22_checked_output_pcm_candidate.hpp",
    "render/src/internal/m22_final_src_frozen_pin_candidate.hpp",
    "render/src/internal/m22_authentic_q0_conversion_candidate.hpp",
    "tests/unit/render/test_m22_authentic_q0_candidate.cpp",
    "tests/unit/render/test_m22_real_manual_q0_m17_chain_candidate.cpp",
    "render/src/internal/m22_full_chain_witness_candidate.hpp",
    "app/src/internal/m22_product_chain_epoch_capture_candidate.hpp",
    "render/src/internal/m22_verified_full_output_window_candidate.hpp",
    "app/src/internal/m22_verified_master_preview_parity.hpp",
    "tests/unit/app/test_m22_product_chain_epoch_capture_contract.cpp",
    "tests/integration/app/test_project_session.cpp",
    "tests/unit/render/test_m22_checked_output_clock_candidate.cpp",
    "tests/CMakeLists.txt",
    ".github/workflows/m22-post-gr-focused.yml",
    "tools/qualification/m22_post_gr/verify_no_release.py",
}
FROZEN = (
    "app/src/main.cpp",
    "app/src/m21_product_status_view_model.hpp",
    "app/src/internal/m22_product_verified_dispatch.hpp",
    "app/src/internal/m22_product_preview_admission.hpp",
    "render/src/internal/m22_request_plan_candidate.hpp",
    "audio/src/phase11_final_src_candidate.cpp",
    "audio/include/rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp",
    "audio/include/rgsml/audio/internal/phase11_frozen_rgsmsr1_geometry.hpp",
    "ui/qml/Main.qml",
    "ui/qml/TruePeakGrHistoryGraph.qml",
    "tests/ui_smoke/test_source_metadata_panel.cpp",
    "resources/fir/README.md",
)

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)

def fail_if(test, message):
    if test:
        raise RuntimeError(message)

def blob(ref, path):
    return git("show", ref + ":" + path)

def require(text, markers, name):
    for m in markers:
        fail_if(m not in text, name + " missing: " + m)

def main():
    fail_if(subprocess.call(
        ["git", "merge-base", "--is-ancestor", BASE, "HEAD"],
        cwd=ROOT) != 0, "M17 GR #2 double-green ancestor missing")
    changed=set(git("diff", "--name-only", BASE, "HEAD").decode().splitlines())
    fail_if(changed != ALLOWED,
            "Unapproved/MISSING paths: "+repr({
                "extra":sorted(changed-ALLOWED),
                "missing":sorted(ALLOWED-changed)}))
    for p in FROZEN:
        fail_if(blob("HEAD",p)!=blob(BASE,p),
                "Frozen Product/DSP/FIR/GR changed: "+p)
    clock=blob("HEAD",
        "render/src/internal/m22_checked_output_clock_candidate.hpp"
    ).decode()
    require(clock,(
        "authorizes_release() const noexcept",
        "return false;",
        "required_source_true_eos",
        "required_output_true_eos",
        "absolute_dither_first",
        "source_processing_frames>limit",
        "declared_full_output_frames>limit",
        "UNATTESTED_FINAL_SRC",
    ),"M22 clock")
    pcm=blob("HEAD",
        "render/src/internal/m22_checked_output_pcm_candidate.hpp"
    ).decode()
    require(pcm,(
        "AudioBufferView",
        "FrameDomainId::OUTPUT_RATE",
        "NOT_WHOLE_PROGRAMME",
        "WRONG_OUTPUT_RATE",
        "actual.frame_count()",
    ),"Real PCM carrier")
    fir=blob("HEAD",
        "render/src/internal/m22_final_src_frozen_pin_candidate.hpp"
    ).decode()
    require(fir,(
        "phase11_frozen_original_fir_pin(",
        "phase11_frozen_shape(",
        '"RGSMSR1_L"',
        "WRONG_FROZEN_ASSET_NAME",
        "authorizes_release() const noexcept",
    ),"Real frozen original FIR")
    tests=blob("HEAD",
        "tests/unit/render/test_m22_checked_output_clock_candidate.cpp"
    ).decode()
    require(tests,(
        "everyCrossRateRatioAndNoInferredLength",
        "onlyActualFrozenQ0AssetIdentityForAll30Routes",
        "genuineOutputBufferMetadataIsNotAnExternalClaim",
        "equalRateFullMasterAndSubviewAreNotInterchangeable",
        "source_processing_frames=cap+1",
        "declared_full_output_frames=cap+1",
        "PcmClockFailure::INVALID_OUTPUT_DOMAIN",
        "FrozenFirFailure::WRONG_FROZEN_ASSET_NAME",
    ),"M22 real test matrix")
    actualq0=blob("HEAD",
        "render/src/internal/m22_authentic_q0_conversion_candidate.hpp"
    ).decode()
    require(actualq0,(
        "materialize_phase11_scoped_q0_pinned_offline_candidate(",
        "phase11_frozen_original_fir_pin(",
        "FrameDomainId::SOURCE_PROCESSING_RATE",
        "FrameDomainId::OUTPUT_RATE",
        "pre_terminal_pcm",
        "authorizes_release() const noexcept {return false;}",
        "M22_Q0_ACTUAL_FULL_OUTPUT_OR_PIN_MISMATCH",
        "produced.value()->source_rate",
        "produced.value()->output_rate",
    ),"M22 genuine Q0 preterminal conversion")
    actualtest=blob("HEAD",
        "tests/unit/render/test_m22_authentic_q0_candidate.cpp"
    ).decode()
    require(actualtest,(
        "realPinnedQ0BothDirectionsExactReplay",
        "refusalAndIdentityDoNotBypassQ0",
        "allThirtyDirectedOriginalQ0FirsReallyProcessPCM",
        "allSixIdentityRatesAreBitExactWithoutFIR",
        "std::size_t{30}",
        "stopAfterProgress",
        "quiet_NaN()",
        '"RGSMSR1_L160_M147.bin"',
        '"RGSMSR1_L147_M160.bin"',
        "std::bit_cast<std::uint64_t>",
        "FrameDomainId::OUTPUT_RATE",
        "return true;",
    ),"M22 true FIR binary64 test")

    fullchain=blob("HEAD",
        "tests/unit/render/test_m22_real_manual_q0_m17_chain_candidate.cpp"
    ).decode()
    require(fullchain,(
        "render_preview(*request.value(),*registry.value())",
        "render_true_peak_limiter_owned_output_rate_candidate(",
        "materialize_phase11_scoped_q0_pinned_offline_candidate(",
        "allThirtyRealQ0RoutesViaImmutableManualOwnerAndRealM17Bypass",
        "realActiveM17AfterQ0RunsM16AndRetainsManualOwner",
        "allSixNativeIdentityRatesProduceM16CheckedWithoutFIR",
        "observe_real_complete_chain_candidate(",
        "tamperAndStaleManualEpochCannotPublishM17",
        "std::size_t{30}",
        "final_check.has_value()",
        "src_kernel_sha256",
    ),"M22 genuine manual-Q0-M17 whole-chain proof")

    nativewitness=blob("HEAD",
        "render/src/internal/m22_full_chain_witness_candidate.hpp").decode()
    require(nativewitness,(
        "observe_real_complete_chain_candidate(",
        "phase11_frozen_original_fir_pin(",
        "FrameDomainId::SOURCE_PROCESSING_RATE",
        "FrameDomainId::OUTPUT_RATE",
        "FullChainWitnessFailure::INVALID_M17_OR_M16",
        "src_kernel_sha256!=std::string_view{pin->sha256}",
        "authorizes_release() const noexcept { return false; }",
    ),"M22 native full-chain witness")
    productbridge=blob("HEAD",
        "app/src/internal/m22_product_chain_epoch_capture_candidate.hpp").decode()
    require(productbridge,(
        "product.upstream.source.sample_rate_hz()",
        "product.upstream.source.channel_count()",
        "M18ProductCompleteBinding",
        "Binding::can_offer_verified_master(product,fanout)",
        "product.candidates.staged()!=expected",
        "source->reference().same_resource_identity(",
        "observe_real_complete_chain_candidate(",
        "owner->epoch",
        "still_current(",
        "authorizes_release() const noexcept {return false;}",
    ),"M22 exact Product owner epoch bridge")
    checked_window=blob("HEAD",
        "render/src/internal/m22_verified_full_output_window_candidate.hpp"
    ).decode()
    require(checked_window,(
        "observe_verified_full_output_window_candidate(",
        "observe_real_complete_chain_candidate(full,witness_failure)",
        "witnessed->actual_master_frames",
        "full_pcm.subview(absolute_output_window.begin()",
        "FrameDomainId::OUTPUT_RATE",
        "authorizes_release() const noexcept {return false;}",
        "M22_PREVIEW_REQUIRES_REAL_FULL_MANUAL_Q0_M17_M16",
    ),"True full-M17 output-rate crop candidate")
    product_crop=blob("HEAD",
        "app/src/internal/m22_verified_master_preview_parity.hpp"
    ).decode()
    require(product_crop,(
        "Binding",
        "M18ProductCompleteBinding::can_offer_verified_master(",
        "observe_verified_full_output_window_candidate(",
        "witnessed.complete_source_frames!=currentSource.frame_count()",
        "witnessed.manual_source_hz!=currentSource.sample_rate_hz()",
        "witnessed.terminal_chain_revision",
        "witnessed.final_master_hz",
        "exact_output_crop",
    ),"Real Product cross-rate Preview admission")
    fail_if("full.frame_count()!=rendered.programme_frames" in product_crop
        and "The source full-programme count" not in product_crop,
        "Product Preview still assumes source-rate frame count")
    require(fullchain,(
        "observe_verified_full_output_window_candidate(",
        "The SOURCE is 257 frames",
        "No SOURCE-rate window may be mislabeled as output-rate.",
        "Authentic Q0 conversion with M17 bypass",
        "audio::internal::m18_pcm24_block(",
        "audio::internal::m18_float32_block(",
        "wrongLocalDither!=cropPcm24",
        "chunkedPcm24==fullPcm24",
        "allThirtyActiveOriginalQ0RoutesUseGenuineFullM17AndM16",
        "QCOMPARE(completed,std::size_t{30})",
        "fullPcm24.begin()+static_cast<std::ptrdiff_t>(9U*channels*3U)",
        "fullF32.begin()+static_cast<std::ptrdiff_t>(9U*channels*4U)",
    ),"Native active true-M17 absolute crop and M18 encoding regression")
    producttest=blob("HEAD",
        "tests/unit/app/test_m22_product_chain_epoch_capture_contract.cpp").decode()
    require(producttest,(
        "M22ProductChainEpochCaptureCandidate",
        "is_default_constructible_v<Probe::Capture>",
        "Probe::still_current(",
    ),"M22 Product capture compile contract")
    productruntime=blob("HEAD",
        "tests/integration/app/test_project_session.cpp").decode()
    require(productruntime,(
        "M22ProductChainEpochCaptureCandidate",
        "const auto completeChain=FullChainEpoch::capture(",
        "FullChainEpoch::still_current(",
        "refusedOffThread=!forbidden&&!forbiddenEpoch",
        "!FullChainEpoch::capture(",
        "const auto refreshedFullChain=FullChainEpoch::capture(",
        "immutable_chain_witness.actual_master_frames",
        "!completeChain.value()->authorizes_release()",
        "M18 export ticket is live",
        "m22LiveProductCrossRateQ0M17PreviewFullExportAndRevocation()",
        "renderer.set_original_q0_fir_root(originalQ0Root)",
        "M22ProductChainEpochCaptureCandidate::capture(",
        "M22ProductVerifiedDispatch::begin_preview(",
        "M22ProductVerifiedDispatch::begin_full(",
        "QTRY_VERIFY_WITH_TIMEOUT(exporter.snapshot().phase==",
        "reader.value()->get()->info().frame_count().value()",
        "inputAfter.readAll(),originalBytes",
        "!M22ProductChainEpochCaptureCandidate::still_current(",
    ),"M22 actual identity/cross-rate full Product-WAV regression")
    cmake=blob("HEAD","tests/CMakeLists.txt").decode()
    require(cmake,(
        "rgsml_m22_checked_output_clock_candidate_tests",
        "rgsml_audio Qt6::Test",
        "rgsml_m22_authentic_q0_candidate_tests",
        "rgsml_m22_real_manual_q0_m17_chain_candidate_tests",
        "rgsml_m22_product_chain_epoch_capture_tests",
        'RGSML_M22_SOURCE_DIR="${CMAKE_SOURCE_DIR}"',
        "${CMAKE_SOURCE_DIR}/audio/src/internal",
        "RGSML_M22_SOURCE_DIR=",
    ),"M22 Qt test target")
    workflow=blob("HEAD",
        ".github/workflows/m22-post-gr-focused.yml").decode()
    require(workflow,(
        "rgsml_m22_checked_output_clock_candidate_tests",
        "rgsml_m22_authentic_q0_candidate_tests",
        "rgsml_m22_real_manual_q0_m17_chain_candidate_tests",
        "rgsml_m22_product_chain_epoch_capture_tests",
        "tools/qualification/m22_post_gr/verify_no_release.py",
        "staging/m22-post-gr-focused-ci-20261108",
        "tools/qualification/playback_src/test_approved_16_fir_package.py",
        "Unified 16-FIR source and manifest contract (Linux, non-FQ)",
        "Unified 16-FIR source and manifest contract (Windows, non-FQ)",
    ),"M22 focused CI")
    doc=blob("HEAD",
        "docs/architecture/PHASE6_M22_POST_GR_UNRESOLVED_GATE_PLAN_20261108.md"
    ).decode()
    require(doc,(
        "PHASE6_IMPLEMENTATION_AUTHORIZED_BY_THIS_DECISION = NO",
        "No second registry",
        "Phase-6",
        "M22",
    ),"Authority status")
    print("M22_POST_GR_REAL_OUTPUT_CLOCK_AND_FROZEN_Q0_IDENTITY_PREQUAL_PASS")
    print("Release/FIR Q1-Q6/Phase6 Gate G/manual FQ: UNAUTHORIZED")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as e:
        print("M22_POST_GR_GUARD_FAILURE: "+str(e), file=sys.stderr)
        sys.exit(1)
