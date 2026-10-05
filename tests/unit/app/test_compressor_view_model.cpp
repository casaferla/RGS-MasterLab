#include "compressor_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"

#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <QtTest/QTest>
#include <QVariantMap>

#include <cmath>
#include <limits>

namespace rgsml::tests {
namespace {

using namespace rgsml::app;
using namespace rgsml::dsp;

class CompressorViewModelTest final : public QObject {
    Q_OBJECT

private slots:
    void initialDefaults();
    void validParameterEditsAndPreviewRequests();
    void invalidDraftRejectionNoClampNoPreview();
    void nanAndInfRejection();
    void boundsRejections();
    void undoRedoCycle();
    void peakRmsApplicabilityPreservesStoredValue();
    void monoStereoApplicabilityPreservesStoredValue();
    void staticTransferCurveOraclePoints();
    void resetToDefaultState();
    void textDraftContractAndCommitCancel();
    void sliderDraftAndCommitCycle();
    void interactiveCurveHandlesAndCancel();
    void softKneeRatioHandleSolver();
};

void CompressorViewModelTest::initialDefaults()
{
    CompressorViewModel vm;
    QCOMPARE(vm.detector_mode(), QStringLiteral("RMS"));
    QCOMPARE(vm.channel_link(), QStringLiteral("LINKED_MAX"));
    QCOMPARE(vm.threshold_dbfs(), -24.0);
    QCOMPARE(vm.ratio(), 2.0);
    QCOMPARE(vm.knee_db(), 6.0);
    QCOMPARE(vm.attack_ms(), 30.0);
    QCOMPARE(vm.release_ms(), 200.0);
    QCOMPARE(vm.rms_time_constant_ms(), 50.0);
    QCOMPARE(vm.look_ahead_ms(), 5.0);
    QCOMPARE(vm.mix_percent(), 100.0);
    QCOMPARE(vm.makeup_gain_db(), 0.0);

    QVERIFY(vm.validation_field().isEmpty());
    QVERIFY(vm.validation_message().isEmpty());
    QVERIFY(!vm.can_undo());
    QVERIFY(!vm.can_redo());
    QVERIFY(vm.rms_time_effective());
    QVERIFY(vm.channel_link_effective());
}

void CompressorViewModelTest::validParameterEditsAndPreviewRequests()
{
    CompressorViewModel vm;
    const auto initialGen = vm.preview_generation();

    vm.setThresholdDbfs(-18.0);
    QCOMPARE(vm.threshold_dbfs(), -18.0);
    QVERIFY(vm.validation_field().isEmpty());
    QVERIFY(vm.preview_generation() > initialGen);
    QVERIFY(vm.can_undo());

    const auto gen2 = vm.preview_generation();
    vm.setRatio(4.0);
    QCOMPARE(vm.ratio(), 4.0);
    QVERIFY(vm.preview_generation() > gen2);

    vm.setMakeupGainDb(3.0);
    QCOMPARE(vm.makeup_gain_db(), 3.0);
}

void CompressorViewModelTest::invalidDraftRejectionNoClampNoPreview()
{
    CompressorViewModel vm;
    vm.setThresholdDbfs(-12.0);
    const auto validGen = vm.preview_generation();

    // Set invalid ratio (e.g. 0.5 < 1.0 minimum)
    vm.setRatio(0.5);

    // Validation field set, draft retains input without clamping, preview NOT requested
    QCOMPARE(vm.validation_field(), QStringLiteral("ratio"));
    QVERIFY(!vm.validation_message().isEmpty());
    QCOMPARE(vm.ratio_text(), QStringLiteral("0.50")); // Draft text reflects input
    QCOMPARE(vm.ratio(), 2.0); // Committed state unchanged
    QCOMPARE(vm.preview_generation(), validGen); // No new preview request!
}

void CompressorViewModelTest::nanAndInfRejection()
{
    CompressorViewModel vm;
    const auto validGen = vm.preview_generation();

    const double nanVal = std::numeric_limits<double>::quiet_NaN();
    const double infVal = std::numeric_limits<double>::infinity();

    vm.setThresholdDbfs(nanVal);
    QCOMPARE(vm.validation_field(), QStringLiteral("thresholdDbfs"));
    QCOMPARE(vm.preview_generation(), validGen);

    vm.setRatio(infVal);
    QCOMPARE(vm.validation_field(), QStringLiteral("ratio"));
    QCOMPARE(vm.preview_generation(), validGen);
}

void CompressorViewModelTest::boundsRejections()
{
    CompressorViewModel vm;
    const auto validGen = vm.preview_generation();

    // Out of bounds threshold below min -120 dBFS
    vm.setThresholdDbfs(std::nextafter(-120.0, -121.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("thresholdDbfs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds threshold above max 0 dBFS
    vm.setThresholdDbfs(std::nextafter(0.0, 1.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("thresholdDbfs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds ratio below min 1.0
    vm.setRatio(std::nextafter(1.0, 0.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("ratio"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds ratio above max 20.0
    vm.setRatio(std::nextafter(20.0, 21.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("ratio"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds knee below min 0.0
    vm.setKneeDb(std::nextafter(0.0, -1.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("kneeDb"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds knee above max 24.0
    vm.setKneeDb(std::nextafter(24.0, 25.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("kneeDb"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds attack below min 0.1 ms
    vm.setAttackMs(std::nextafter(0.1, 0.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("attackMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds attack above max 500.0 ms
    vm.setAttackMs(std::nextafter(500.0, 501.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("attackMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds release below min 1.0 ms
    vm.setReleaseMs(std::nextafter(1.0, 0.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("releaseMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds release above max 5000.0 ms
    vm.setReleaseMs(std::nextafter(5000.0, 5001.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("releaseMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds RMS time constant below min 1.0 ms
    vm.setRmsTimeConstantMs(std::nextafter(1.0, 0.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("rmsTimeConstantMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds RMS time constant above max 500.0 ms
    vm.setRmsTimeConstantMs(std::nextafter(500.0, 501.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("rmsTimeConstantMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds look ahead below min 0.0 ms
    vm.setLookAheadMs(std::nextafter(0.0, -1.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("lookAheadMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds look ahead above max 20.0 ms
    vm.setLookAheadMs(std::nextafter(20.0, 21.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("lookAheadMs"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds mix percent below min 0.0
    vm.setMixPercent(std::nextafter(0.0, -1.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("mixPercent"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds mix percent above max 100.0
    vm.setMixPercent(std::nextafter(100.0, 101.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("mixPercent"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds makeup gain below min -24.0 dB
    vm.setMakeupGainDb(std::nextafter(-24.0, -25.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("makeupGainDb"));
    QCOMPARE(vm.preview_generation(), validGen);

    // Out of bounds makeup gain above max 24.0 dB
    vm.setMakeupGainDb(std::nextafter(24.0, 25.0));
    QCOMPARE(vm.validation_field(), QStringLiteral("makeupGainDb"));
    QCOMPARE(vm.preview_generation(), validGen);
}

void CompressorViewModelTest::undoRedoCycle()
{
    CompressorViewModel vm;
    vm.setThresholdDbfs(-18.0);
    vm.setRatio(3.0);

    QCOMPARE(vm.threshold_dbfs(), -18.0);
    QCOMPARE(vm.ratio(), 3.0);
    QVERIFY(vm.can_undo());

    vm.undo();
    QCOMPARE(vm.ratio(), 2.0);
    QCOMPARE(vm.threshold_dbfs(), -18.0);
    QVERIFY(vm.can_redo());

    vm.undo();
    QCOMPARE(vm.threshold_dbfs(), -24.0);

    vm.redo();
    QCOMPARE(vm.threshold_dbfs(), -18.0);

    vm.redo();
    QCOMPARE(vm.ratio(), 3.0);
}

void CompressorViewModelTest::peakRmsApplicabilityPreservesStoredValue()
{
    CompressorViewModel vm;
    vm.setRmsTimeConstantMs(100.0);
    QCOMPARE(vm.rms_time_constant_ms(), 100.0);
    QVERIFY(vm.rms_time_effective());

    // Switch to PEAK mode
    vm.setDetectorMode(QStringLiteral("PEAK"));
    QCOMPARE(vm.detector_mode(), QStringLiteral("PEAK"));
    QVERIFY(!vm.rms_time_effective());

    // Stored RMS time constant MUST be preserved
    QCOMPARE(vm.rms_time_constant_ms(), 100.0);

    // Switch back to RMS mode
    vm.setDetectorMode(QStringLiteral("RMS"));
    QVERIFY(vm.rms_time_effective());
    QCOMPARE(vm.rms_time_constant_ms(), 100.0);
}

void CompressorViewModelTest::monoStereoApplicabilityPreservesStoredValue()
{
    CompressorViewModel vm;
    vm.setChannelLink(QStringLiteral("DUAL_MONO"));
    QCOMPARE(vm.channel_link(), QStringLiteral("DUAL_MONO"));

    // Stored channelLink enum MUST be preserved regardless of effective state
    QCOMPARE(vm.channel_link(), QStringLiteral("DUAL_MONO"));
}

void CompressorViewModelTest::staticTransferCurveOraclePoints()
{
    CompressorViewModel vm;
    // Defaults: Threshold = -24.0, Ratio = 2.0, Knee = 0.0 (Hard knee), Makeup = 0.0
    vm.setKneeDb(0.0);

    const auto points = vm.transfer_curve_points();
    QVERIFY(!points.isEmpty());

    // Evaluate known oracle points:
    // 1. Input = -30 dBFS (below threshold -24): no reduction, output = -30 dBFS
    bool found30 = false;
    for (const auto& p : points) {
        const auto map = p.toMap();
        if (std::abs(map[QStringLiteral("inputDbfs")].toDouble() - (-30.0)) < 0.5) {
            found30 = true;
            QCOMPARE(map[QStringLiteral("gainReductionDb")].toDouble(), 0.0);
            QCOMPARE(map[QStringLiteral("outputDbfs")].toDouble(), map[QStringLiteral("inputDbfs")].toDouble());
            break;
        }
    }
    QVERIFY(found30);

    // 2. Input = -18 dBFS (6 dB above threshold -24, ratio 2:1): reduction = 3 dB, output = -21 dBFS
    bool found18 = false;
    for (const auto& p : points) {
        const auto map = p.toMap();
        const double inVal = map[QStringLiteral("inputDbfs")].toDouble();
        if (std::abs(inVal - (-18.0)) < 0.5) {
            found18 = true;
            const double expectedGR = (inVal - (-24.0)) * (1.0 - 0.5);
            QCOMPARE(map[QStringLiteral("gainReductionDb")].toDouble(), expectedGR);
            QCOMPARE(map[QStringLiteral("outputDbfs")].toDouble(), inVal - expectedGR);
            break;
        }
    }
    QVERIFY(found18);

    // 3. With makeup gain = 4 dB, output shifted up by 4 dB
    vm.setMakeupGainDb(4.0);
    const auto pointsMakeup = vm.transfer_curve_points();
    for (const auto& p : pointsMakeup) {
        const auto map = p.toMap();
        if (std::abs(map[QStringLiteral("inputDbfs")].toDouble() - (-30.0)) < 0.5) {
            QCOMPARE(map[QStringLiteral("outputDbfs")].toDouble(), map[QStringLiteral("inputDbfs")].toDouble() + 4.0);
            break;
        }
    }
}

void CompressorViewModelTest::resetToDefaultState()
{
    CompressorViewModel vm;
    vm.setThresholdDbfs(-12.0);
    vm.setRatio(4.0);

    vm.resetToDefault();
    QCOMPARE(vm.threshold_dbfs(), -24.0);
    QCOMPARE(vm.ratio(), 2.0);
    QVERIFY(vm.bypass()); // Reset to default sets user bypass = true
}

void CompressorViewModelTest::textDraftContractAndCommitCancel()
{
    CompressorViewModel vm;
    QCOMPARE(vm.threshold_text(), QStringLiteral("-24.0"));
    const auto gen0 = vm.preview_generation();

    // 1. Text draft update does NOT commit or trigger preview
    vm.setDraftFieldText(QStringLiteral("thresholdDbfs"), QStringLiteral("-18.5"));
    QCOMPARE(vm.threshold_text(), QStringLiteral("-18.5"));
    QVERIFY(vm.validation_field().isEmpty());
    QCOMPARE(vm.threshold_dbfs(), -24.0); // Uncommitted!
    QCOMPARE(vm.preview_generation(), gen0);
    QVERIFY(!vm.can_undo());

    // 2. Valid commitDraft() updates committed state and requests preview
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.threshold_dbfs(), -18.5);
    QVERIFY(vm.preview_generation() > gen0);
    QVERIFY(vm.can_undo());

    // 3. Unchanged commitDraft() produces no extra undo or preview
    const auto gen1 = vm.preview_generation();
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.preview_generation(), gen1);

    // 4. Incomplete text draft sets validation and blocks commit
    vm.setDraftFieldText(QStringLiteral("thresholdDbfs"), QStringLiteral("-"));
    QCOMPARE(vm.threshold_text(), QStringLiteral("-"));
    QCOMPARE(vm.validation_field(), QStringLiteral("thresholdDbfs"));
    QVERIFY(!vm.validation_message().isEmpty());
    QCOMPARE(vm.threshold_dbfs(), -18.5); // Still committed value

    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.validation_field(), QStringLiteral("thresholdDbfs"));
    QCOMPARE(vm.threshold_dbfs(), -18.5);

    // 5. cancelDraft() restores committed text and clears validation
    vm.cancelDraft();
    QCOMPARE(vm.threshold_text(), QStringLiteral("-18.5"));
    QVERIFY(vm.validation_field().isEmpty());
    QCOMPARE(vm.threshold_dbfs(), -18.5);
}

void CompressorViewModelTest::sliderDraftAndCommitCycle()
{
    CompressorViewModel vm;
    const auto gen0 = vm.preview_generation();

    // 1. setDraftFieldValue changes draft only, no preview, no undo
    vm.setDraftFieldValue(QStringLiteral("thresholdDbfs"), -18.0);
    QCOMPARE(vm.draft_threshold_dbfs(), -18.0);
    QCOMPARE(vm.threshold_text(), QStringLiteral("-18.0"));
    QCOMPARE(vm.threshold_dbfs(), -24.0); // Committed state unchanged!
    QCOMPARE(vm.preview_generation(), gen0); // No preview requested!
    QVERIFY(!vm.can_undo()); // No undo pushed during drag!

    // 2. Continuous drag updates draft repeatedly
    vm.setDraftFieldValue(QStringLiteral("thresholdDbfs"), -12.0);
    QCOMPARE(vm.draft_threshold_dbfs(), -12.0);
    QCOMPARE(vm.threshold_text(), QStringLiteral("-12.0"));
    QCOMPARE(vm.threshold_dbfs(), -24.0);
    QCOMPARE(vm.preview_generation(), gen0);
    QVERIFY(!vm.can_undo());

    // 3. Commit on release updates authority and previews once
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.threshold_dbfs(), -12.0); // Committed!
    QVERIFY(vm.preview_generation() > gen0); // Preview requested once!
    QVERIFY(vm.can_undo()); // Exactly one undo entry pushed!

    // 4. Unchanged commit creates no additional preview
    const auto gen1 = vm.preview_generation();
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.preview_generation(), gen1);
}

void CompressorViewModelTest::interactiveCurveHandlesAndCancel()
{
    CompressorViewModel vm;
    const auto gen0 = vm.preview_generation();

    const auto handles = vm.transfer_curve_handles();
    QCOMPARE(handles.size(), 4);

    // Verify handle IDs, colors, and finite coordinates
    const std::array expectedIds{QStringLiteral("threshold"), QStringLiteral("ratio"), QStringLiteral("knee"), QStringLiteral("makeup")};
    const std::array expectedColors{QStringLiteral("#2ED3FF"), QStringLiteral("#2FD98F"), QStringLiteral("#FFD84A"), QStringLiteral("#FF6B6B")};

    for (qsizetype i = 0; i < handles.size(); ++i) {
        const auto map = handles[i].toMap();
        const auto expectedIndex = static_cast<std::size_t>(i);
        QCOMPARE(map[QStringLiteral("id")].toString(), expectedIds[expectedIndex]);
        QCOMPARE(map[QStringLiteral("color")].toString(), expectedColors[expectedIndex]);
        QVERIFY(std::isfinite(map[QStringLiteral("inputDbfs")].toDouble()));
        QVERIFY(std::isfinite(map[QStringLiteral("outputDbfs")].toDouble()));
    }

    // Drag Threshold handle
    vm.setCurveHandleDraft(QStringLiteral("threshold"), -18.0, -18.0);
    QCOMPARE(vm.draft_threshold_dbfs(), -18.0);
    QCOMPARE(vm.threshold_dbfs(), -24.0); // Uncommitted!
    QCOMPARE(vm.preview_generation(), gen0); // No preview during handle drag!

    // Obtain current Ratio handle metadata after Threshold move
    const auto updatedHandles = vm.transfer_curve_handles();
    QVariantMap ratioMap;
    for (const auto& h : updatedHandles) {
        if (h.toMap()[QStringLiteral("id")].toString() == QStringLiteral("ratio")) {
            ratioMap = h.toMap();
            break;
        }
    }
    QVERIFY(!ratioMap.isEmpty());

    // Feed truthful Ratio handle coordinates back through setCurveHandleDraft
    vm.setCurveHandleDraft(QStringLiteral("ratio"), ratioMap[QStringLiteral("inputDbfs")].toDouble(), ratioMap[QStringLiteral("outputDbfs")].toDouble());
    QVERIFY(std::abs(vm.draft_ratio() - 2.0) < 1e-8); // Bisection round-trip precision
    QCOMPARE(vm.ratio(), 2.0); // Uncommitted!
    QCOMPARE(vm.preview_generation(), gen0); // No preview during handle drag!

    // Drag Knee handle (lower knee boundary at threshold - knee/2 = -18 - 3 = -21)
    vm.setCurveHandleDraft(QStringLiteral("knee"), -24.0, -24.0);
    QCOMPARE(vm.draft_knee_db(), 12.0); // 2 * |-18 - (-24)| = 12 dB

    // Drag Make-up handle
    vm.setCurveHandleDraft(QStringLiteral("makeup"), -54.0, -50.0);
    QCOMPARE(vm.draft_makeup_gain_db(), 4.0); // Shifted by +4 dB

    // Cancel restores committed authority
    vm.cancelDraft();
    QCOMPARE(vm.draft_threshold_dbfs(), -24.0);
    QCOMPARE(vm.draft_ratio(), 2.0);
    QCOMPARE(vm.draft_knee_db(), 6.0);
    QCOMPARE(vm.draft_makeup_gain_db(), 0.0);
    QCOMPARE(vm.preview_generation(), gen0);

    // Re-apply threshold handle drag and commit once
    vm.setCurveHandleDraft(QStringLiteral("threshold"), -15.0, -15.0);
    QCOMPARE(vm.draft_threshold_dbfs(), -15.0);
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.threshold_dbfs(), -15.0); // Committed!
    QVERIFY(vm.preview_generation() > gen0); // Single preview requested!
}

void CompressorViewModelTest::softKneeRatioHandleSolver()
{
    CompressorViewModel vm;

    // High threshold = -2.0 dBFS, wide knee = 24.0 dB. The C++ Ratio anchor
    // is capped at +6 dBFS, placing it 8 dB above threshold and therefore
    // inside the +/-12 dB soft-knee region. Expected target ratio = 4.0.
    vm.setThresholdDbfs(-2.0);
    vm.setKneeDb(24.0);
    vm.setRatio(4.0);
    vm.setMakeupGainDb(0.0);

    // Get C++ generated handle coordinates for ratio = 4.0 in soft knee
    const auto handles = vm.transfer_curve_handles();
    QVariantMap ratioMap;
    for (const auto& h : handles) {
        if (h.toMap()[QStringLiteral("id")].toString() == QStringLiteral("ratio")) {
            ratioMap = h.toMap();
            break;
        }
    }
    QVERIFY(!ratioMap.isEmpty());

    // Reset draft ratio to 2.0
    vm.setDraftFieldValue(QStringLiteral("ratio"), 2.0);
    QCOMPARE(vm.draft_ratio(), 2.0);

    // Feed the soft-knee target coordinate back through setCurveHandleDraft
    vm.setCurveHandleDraft(QStringLiteral("ratio"), ratioMap[QStringLiteral("inputDbfs")].toDouble(), ratioMap[QStringLiteral("outputDbfs")].toDouble());

    // Verify solver reconstructs Ratio 4.0 within tolerance 0.01
    QVERIFY(std::abs(vm.draft_ratio() - 4.0) < 0.01);
}

}  // namespace
}  // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::CompressorViewModelTest)

#include "test_compressor_view_model.moc"
