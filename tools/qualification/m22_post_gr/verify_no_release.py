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
        "tamperAndStaleManualEpochCannotPublishM17",
        "std::size_t{30}",
        "final_check.has_value()",
        "src_kernel_sha256",
    ),"M22 genuine manual-Q0-M17 whole-chain proof")

    cmake=blob("HEAD","tests/CMakeLists.txt").decode()
    require(cmake,(
        "rgsml_m22_checked_output_clock_candidate_tests",
        "rgsml_audio Qt6::Test",
        "rgsml_m22_authentic_q0_candidate_tests",
        "rgsml_m22_real_manual_q0_m17_chain_candidate_tests",
        'RGSML_M22_SOURCE_DIR="${CMAKE_SOURCE_DIR}"',
    ),"M22 Qt test target")
    workflow=blob("HEAD",
        ".github/workflows/m22-post-gr-focused.yml").decode()
    require(workflow,(
        "rgsml_m22_checked_output_clock_candidate_tests",
        "rgsml_m22_authentic_q0_candidate_tests",
        "rgsml_m22_real_manual_q0_m17_chain_candidate_tests",
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
