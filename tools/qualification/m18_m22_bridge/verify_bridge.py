#!/usr/bin/env python3
"""Read-only provenance guard for isolated M18-M22 + REFERENCE UI review.
Does not authorize Product registration, CI/PO FQ, or any main integration.
"""
from pathlib import Path
import subprocess
import sys
import difflib

ROOT = Path(__file__).resolve().parents[3]
BASE = "5a5b9b7e4b01779832e32662d5c6b66b23087ce9"  # M22 CI#12 SUCCESS
UI = "b7a30ad2fb22a9d718a3e75adbf8d337bbccf6dd"  # REFERENCE CI#6 SUCCESS

IMPORTED = (
    "ui/qml/AnalysisModuleDockHostPreview.qml",
    "ui/qml/AuditionSourceSelector.qml",
    "ui/qml/M19LevelsPanelPreview.qml",
    "ui/qml/M19M21AnalysisWorkbenchPreview.qml",
    "ui/qml/M20TonalStereoPanelPreview.qml",
    "ui/qml/M21MetricsComparisonPreview.qml",
    "ui/qml/M21MetricsDeltaPanelPreview.qml",
    "ui/qml/Main.qml",
    "ui/qml/StudioReadOnlyMeasurement.qml",
    "app/src/audition_source_selector.cpp",
    "app/src/gold_selection_view_model.cpp",
    "app/src/playback_transport_view_model.cpp",
    "platform/windows/src/windows_resource_identity.cpp",
    "app/src/internal/m21_independent_panels_review_candidate.hpp",
    "app/src/internal/m21_qt_snapshot_review_candidate.hpp",
    "app/src/internal/m21_review_epoch_store_candidate.hpp",
    "app/src/internal/m21_ui_readonly_projection_candidate.hpp",
    "tests/ui_smoke/test_source_metadata_panel.cpp",
    "tests/unit/app/test_m21_qml_typed_convergence.cpp",
    "tests/unit/app/test_m21_ui_readonly_projection_candidate.cpp",
    "tools/qualification/m19_m21_gui/CMakeLists.txt",
    "tools/qualification/m19_m21_gui/analysis_workbench_screenshot.cpp",
    "tools/qualification/m21/qt_preview/m21_qml_preview_smoke.cpp",
    "tools/qualification/ui_reference_terminology/check_ui_reference_contract.py",
    "docs/architecture/M19_M21_INDEPENDENT_QT_REVIEW_FRAMES_20261108.md",
    "docs/architecture/M19_M21_INDEPENDENT_ROUTING_QT_REVIEW_20261108.md",
    "docs/architecture/M19_M21_PRIVATE_REVIEW_EPOCH_GATE_20261108.md",
    "docs/architecture/M19_M21_QML_TYPED_CONVERGENCE_ISOLATED_GATE_20261108.md",
    "docs/architecture/M19_M21_TYPED_UI_EVIDENCE_PROJECTION_20261108.md",
    ".github/workflows/m19-m21-analysis-gui-screenshots.yml",
    ".github/workflows/m19-m21-qml-typed-convergence.yml",
    ".github/workflows/m19-m21-typed-ui-projection.yml",
)
ADDED = {
    "tests/CMakeLists.txt",
    "tools/qualification/m18_m22_bridge/verify_bridge.py",
    ".github/workflows/m18-m22-reference-ui-bridge.yml",
    "docs/architecture/M18_M22_REFERENCE_GUI_ISOLATED_BRIDGE_20261108.md",
    "app/src/internal/m21_product_qt_readiness_bridge.hpp",
    "app/src/internal/m21_product_spectrum_evidence.hpp",
    "app/src/internal/m22_product_preview_admission.hpp",
    "app/src/internal/m21_product_analysis_handoff.hpp",
    "app/src/internal/m22_product_verified_dispatch.hpp",
    "app/src/m21_product_status_view_model.hpp",
    "app/src/m21_product_status_view_model.cpp",
    "app/src/main.cpp",
    "app/CMakeLists.txt",
    "tests/integration/app/test_project_session.cpp",
}
PROTECTED = (
    "core/src/processing_state.cpp",
    "project/include/rgsml/project/project_document.hpp",
    "project/src/project_snapshot.cpp",
    "project/src/internal/canonical_json.cpp",
    "render/src/internal/m22_request_plan_candidate.hpp",
    "app/src/internal/m21_product_owner_analysis.hpp",
    "app/src/internal/m21_product_publication_inbox.hpp",
)

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)

def blob(ref, path):
    return git("show", ref + ":" + path)

def check(ok, msg):
    if not ok:
        raise RuntimeError(msg)

def main():
    check(subprocess.call(["git", "merge-base", "--is-ancestor", BASE, "HEAD"],
                          cwd=ROOT) == 0, "M22 frozen base is not an ancestor")
    paths = set(git("diff", "--name-only", BASE, "HEAD").decode().splitlines())
    permitted = set(IMPORTED) | ADDED
    check(paths <= permitted, "Unapproved paths in bridge: " + repr(sorted(paths - permitted)))
    check(paths == permitted, "Bridge scope incomplete: " + repr(sorted(permitted - paths)))
    for path in IMPORTED:
        check(blob("HEAD", path) == blob(UI, path),
              "Imported blob diverged from CI#6: " + path)
    for path in PROTECTED:
        check(blob("HEAD", path) == blob(BASE, path),
              "Protected canonical M22/Product/state changed: " + path)

    # The real M22 ProductSession and M21 worker tests may only gain
    # assertions; never drop/alter historical Product gates to go green.
    test_path = "tests/integration/app/test_project_session.cpp"
    baseline = blob(BASE, test_path).decode().splitlines(keepends=True)
    current = blob("HEAD", test_path).decode().splitlines(keepends=True)
    changes = difflib.SequenceMatcher(a=baseline, b=current, autojunk=False)
    check(all(tag in ("equal", "insert") for tag,*_ in changes.get_opcodes()),
          "Existing M22 ProductSession assertions deleted or altered")
    joined = "".join(current)
    for marker in (
        '#include "internal/m21_product_qt_readiness_bridge.hpp"',
        "using QtReadiness=app::internal::M21ProductQtReadinessBridge;",
        "privatePanels.current_product_owner",
        "MISSING",
        "panel.value(\"levels\").toMap().isEmpty()",
        "QtReadiness::still_current(reroutedPanels",
    ):
        check(marker in joined, "M21 real-owner Qt regression absent: "+marker)
    bridge = blob("HEAD",
        "app/src/internal/m21_product_qt_readiness_bridge.hpp").decode()
    for marker in ("M21ProductPublicationInbox",
                   "view_current(", "current_product_owner",
                   "product_ui_authorized{false}",
                   "masked_unpublished_envelope",
                   "routing.is_current_route(",
                   "privateQualityOnly",
                   "m21_private::ui_candidate::quality_label(",
                   "metric==3",
                   "s.lifecycle==Lifecycle::current",
                   "M18ProductExportIdentity owner_identity",
                   "now.owner_identity==expected.owner_identity",
                   "now.identities==expected.identities"):
        check(marker in bridge, "Private UI owner guard missing: "+marker)

    spectrum=blob("HEAD",
        "app/src/internal/m21_product_spectrum_evidence.hpp").decode()
    for required in (
        "M21ProductQtReadinessBridge",
        "view_current(",
        "average_spectrum",
        "record.resource_identity!=proof.identity.resource",
        "record.generation_token!=proof.identity.generation",
        "Readiness::still_current(",
        "product_ui_authorized{false}",
        "QThread::currentThread()!=selector.thread()",
    ):
        check(required in spectrum, "M20 true PSD proof guard missing: "+required)
    for required in (
        '#include "internal/m21_product_spectrum_evidence.hpp"',
        "const auto provenSpectrum=SpectrumProof::capture(",
        "referenceSpectrum=app::internal::M21ProductSpectrumEvidence::",
        "!SpectrumProof::still_current(provenSpectrum",
    ):
        check(required in joined, "M20 real PSD ProductSession test absent: "+required)

    preview = blob("HEAD",
        "app/src/internal/m22_product_preview_admission.hpp").decode()
    for required in (
        "M18ProductCompleteBinding",
        "m22_verified_master_preview_crop(",
        "product.candidates.staged()!=capture.full_master",
        "now->identity!=capture.owner.identity",
        "QThread::currentThread()!=product.session.thread()",
        "absoluteOutputWindow.end()",
        "return true;",
        "expected.value()->data()!=retained.value()->data()",
        "QThread::currentThread()!=product.session.thread()",
    ):
        check(required in preview, "M22 real Product preview admission guard missing: "+required)
    for required in (
        '#include "internal/m22_product_preview_admission.hpp"',
        "using PreviewAdmission=app::internal::M22ProductPreviewAdmission;",
        "const auto admittedMasterPreview=PreviewAdmission::capture(",
        "PreviewAdmission::still_current(",
        "!PreviewAdmission::capture(",
        "std::thread privatePreviewWorker(",
        "QCOMPARE(crop.value()->data(),whole.value()->data()+731)",
        "PreviewAdmission::still_current(",
    ):
        check(required in joined, "M22 real Product preview acceptance test absent: "+required)

    delivery=blob("HEAD",
        "app/src/internal/m22_product_verified_dispatch.hpp").decode()
    for required in ("M18PrivateProductExportAction",
                     "M22ProductPreviewAdmission",
                     "Binding::can_offer_verified_master(product,fanout)",
                     "exporter.can_begin_verified_export()",
                     "return exporter.begin(",
                     "return Preview::capture("):
        check(required in delivery, "M22 verified M18/M17 dispatcher absent: "+required)
    for required in (
        '#include "internal/m22_product_verified_dispatch.hpp"',
        "using M22Dispatch=app::internal::M22ProductVerifiedDispatch;",
        "const auto dispatched=M22Dispatch::begin_full(",
        "M22Dispatch::begin_preview(",
        "action.snapshot().verified_encoded_sha256.size()",
        "std::bit_cast<std::uint64_t>(actualWav)",
        "kMaxDitherQuantizerError",
    ):
        check(required in joined, "M22 genuine full WAV/preview regression absent: "+required)

    # Reuse the real Product M18 Qt instance; the native M21 ViewModel
    # is strictly a status presenter until authoritative Phase-6 gates.
    handoff=blob("HEAD",
        "app/src/internal/m21_product_analysis_handoff.hpp").decode()
    for required in (
        "M21ProductOwnerAnalysis",
        "M21ProductPublicationInbox",
        "capture_on_owner(",
        "measure_on_worker(",
        "publish_on_owner(",
        "Owner::capture_gold(",
        "Owner::measure_gold_worker(",
        "Owner::measure_render_worker(",
        "inbox.accept_render(",
        "inbox.accept_gold(",
        "captured_epoch>0",
    ):
        check(required in handoff, "M21 genuine Product worker handoff missing: "+required)
    for required in (
        '#include "internal/m21_product_analysis_handoff.hpp"',
        "goldFromThread.emplace(Handoff::measure_on_worker(",
        "handoffRenderInbox",
        "Handoff::publish_on_owner(",
        "wrongColumnJob.column=",
        "wrongGeneration.captured_epoch",
    ):
        check(required in joined, "M21 real worker handoff ProductSession test missing: "+required)

    vmh=blob("HEAD","app/src/m21_product_status_view_model.hpp").decode()
    vmc=blob("HEAD","app/src/m21_product_status_view_model.cpp").decode()
    for required in ("Q_PROPERTY(QVariantMap m19", "Q_PROPERTY(QVariantMap m20",
                     "Q_PROPERTY(QVariantMap m21",
                     "productionAuthorized READ production_authorized CONSTANT",
                     "M21ProductQtReadinessBridge",
                     "Q_INVOKABLE bool selectPanelSource("):
        check(required in vmh, "M21 native Qt status-only ViewModel contract missing: "+required)
    for required in ("Bridge::capture(", "owner_thread()", "emit changed()",
                     'QStringLiteral("REFERENCE")'):
        check(required in vmc, "M21 native Qt owner refresh or public label missing: "+required)
    for required in ("app::M21ProductStatusViewModel",
                     "liveNativeStatus", "goldNativeStatus",
                     "goldNativeStatus.refresh();",
                     "liveNativeStatus.refresh();"):
        check(required in joined, "M21 authentic native Qt ProductSession test missing: "+required)

    # Real app bootstrap/CMake may only gain the M21 Presenter.
    for apppath,needles in (
        ("app/src/main.cpp", (
            '#include "m21_product_status_view_model.hpp"',
            "M21ProductPublicationInbox m21ProductInbox;",
            "M21ProductStatusViewModel m21ProductStatusViewModel{",
            'QStringLiteral("m21ProductStatusViewModel")',
            "&rgsml::app::M21ProductStatusViewModel::refresh",
        )),
        ("app/CMakeLists.txt", (
            "src/m21_product_status_view_model.cpp",
            "src/m21_product_status_view_model.hpp",
        )),
    ):
        original_lines=blob(BASE,apppath).decode().splitlines(keepends=True)
        present_lines=blob("HEAD",apppath).decode().splitlines(keepends=True)
        edits=difflib.SequenceMatcher(
            a=original_lines,b=present_lines,autojunk=False)
        check(all(tag in ("equal","insert") for tag,*_ in edits.get_opcodes()),
              "Original Product bootstrap/CMake altered outside additive Qt seam: "+apppath)
        for needle in needles:
            check(needle in "".join(present_lines),
                  "M21 native Product binding absent: "+apppath+" / "+needle)

    cmake = blob("HEAD", "tests/CMakeLists.txt")
    original = blob(BASE, "tests/CMakeLists.txt")
    begin = b"# L1-M19/M20/M21 preproduction read-only typed UI envelope."
    end = b"# L1-M20 read-only real AudioBuffer spectral/stereo analysis."
    check(cmake.count(begin) == 1 and cmake.count(end) == 1,
          "Isolated M19-21 CMake block missing or duplicated")
    start = cmake.index(begin)
    finish = cmake.index(end, start)
    check(cmake[:start] + cmake[finish:] == original,
          "M22 CMake changed outside isolated UI test registration")
    mainqml = blob("HEAD", "ui/qml/Main.qml").decode()
    for required in ('text: "Open Reference"', 'text: "Clear Reference"',
                     'text: "Reference / AutoMatch"', '"NO REFERENCE LOADED"',
                     'goldSelection.selectGold(selectedFile)', '"GOLD_AUTOMATCH"'):
        check(required in mainqml, "Public UI/internal compatibility failed: " + required)
    audition = blob("HEAD", "ui/qml/AuditionSourceSelector.qml").decode()
    check('text: "REFERENCE"' in audition and
          'auditionSelector.activeTarget === "GOLD"' in audition,
          "Audition public REFERENCE/internal GOLD mismatch")
    check(b'QML_FILES' in blob("HEAD", "ui/CMakeLists.txt"),
          "Product UI registration file unexpectedly absent")
    check(blob("HEAD", "ui/CMakeLists.txt") == blob(BASE, "ui/CMakeLists.txt"),
          "Production QML registration altered")
    print("RGS_M18_M22_REFERENCE_UI_ISOLATED_IMPORT_PROVENANCE_PASS")
    print("Not Product active; no FQ, no PO physical acceptance, no main merge")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print("BRIDGE_PROVENANCE_FAILURE:", error, file=sys.stderr)
        sys.exit(1)
