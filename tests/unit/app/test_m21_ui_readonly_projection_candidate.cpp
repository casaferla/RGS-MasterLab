// L1 M19–M21 standalone typed read-only projection, preproduction only.
// Uses the real M19/M20 Product-port DTO; no fake Phase-6 Product publisher,
// no QML Main integration, and no permission to report release-ready data.
#include "internal/m21_ui_readonly_projection_candidate.hpp"
#include "internal/m21_qt_snapshot_review_candidate.hpp"
#include "internal/m21_review_epoch_store_candidate.hpp"
#include "internal/m21_real_product_metrics_port.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <limits>
#include <optional>
#include <string>

namespace rgsml::tests {
class M21UiReadonlyProjectionCandidateTest final : public QObject {
    Q_OBJECT
private slots:
    void missingReceiptsHideAllNumbers();
    void matchedCandidateExposesOnlyAllowedMetricsAndNeverRelease();
    void changedSourceDoesNotInvalidateIndependentGold();
    void realM19M20DataCanBeProjectedOnlyAsPreproduction();
    void qmlEnvelopeHonorsIndividualColumnQualityAndRejectsRelease();
    void newReviewEpochStoreStartsMasked();
    void reviewStoreProcessedReanalysisDoesNotMaskPreparedDelta();
    void reviewStoreFreshGoldUpdatesBothDeltas();
    void reviewStoreGoldReanalysisStalesOnlyGold();
    void reviewStoreUnselectedColumnNeverLeaksIntoPreview();
    void reviewStoreInvalidSelectionKeepsLastPanel();
    void reviewStoreRejectsUnsupportedSourceRate();
    void reviewStoreRejectsMalformedUpstreamCell();
    void reviewStoreUncheckedFrameDomainIsRejected();
    void reviewStoreInvalidOwnerClaimCannotMakeDataCurrent();
    void reviewStoreRejectsWrongSourceScopeWithoutInstalling();
    void reviewStoreRefusesWrongOwnerWithoutInstalling();
    void reviewStoreRejectsZeroAndBackwardGenerations();
    void reviewStoreRejectsPublishWithoutBeginningEpoch();
    void reviewStoreSelectionDoesNotMutateOtherPanels();
    void reviewStoreProcessedKeepsOwnGenerationDuringSourceChange();
    void reviewStoreIndependentGoldSurvivesPreparedReanalysis();
    void reviewStoreDuplicateCompletionCannotReplaceFirstCurrent();
    void reviewStoreFreshReplacementWinsAfterStaleCompletion();
    void reviewStoreLateCompletionCannotRestoreOldPrepared();
    void reviewStoreNewGenerationStalesOldPrepared();
    void reviewStorePublishesOnlyAfterExactExpectedGeneration();
    void displayZeroSuppressesSignedRoundedZero();
    void qtRowsPreservePerMetricUnitsAndQualityNames();
    void unclaimedGoldColumnIsNeverRenderedInQtRows();
    void processedColumnQtProjectionSelectsProcessedCorrectly();
    void testFixtureRejectsArtificialProductionAuthorization();
    void decimalDisplayUsesStableDotAndDeltaSign();
    void allSixSupportedRatesUseExactMatchingOnly();
    void matchingRegionReceiptKeepsRegionScope();
    void invalidLifecycleMasksEveryReadout();
    void infiniteSampleNeverBecomesReadout();
    void negativeGoldMinusCurrentDeltaRetainsSign();
    void actualZeroIsNumericNotUnavailable();
    void nanMetricRemainsInvalidNotZero();
    void invalidPresentCellWithoutScalarNeverRenders();
    void monoNotApplicableWidthHasNoNumeric();
    void lowConfidenceMetricKeepsExplicitQuality();
    void staleGoldMasksBothDependentDeltaColumns();
    void missingGoldMasksPreparedDeltaOnly();
    void swappedSnapshotColumnIsInvalidNotCurrent();
    void missingLifecycleNeverMasqueradesAsZero();
    void staleLifecycleMasksAllAbsoluteNumbers();
    void rejectAbsoluteFrameRangeOverflow();
    void rejectZeroLengthScope();
    void rejectUnsupportedMatchedRate();
    void rejectEmptyExactRealizationIdentity();
    void rejectZeroGenerationEvenWithMatchingReceipt();
    void rejectUncheckedFrameDomainClaim();
    void rejectUnconfirmedCanonicalOwnerClaim();
    void rejectReceiptWithDifferentSampleRate();
    void rejectReceiptWithDifferentFrameCount();
    void rejectReceiptWithDifferentFirstFrame();
    void rejectReceiptWithDifferentScopeKind();
    void rejectReceiptWithChangedRealization();
    void rejectReceiptWithChangedGeneration();
    void rejectReceiptWithWrongSemanticColumn();
};
using namespace app::m21_private;
using namespace app::m21_private::ui_candidate;
using Receipts=std::array<std::optional<ExpectedOwnerReceipt>,3>;
using Slots=std::array<std::optional<Snapshot>,3>;

static Snapshot example(Column column,std::uint64_t generation,
                        const std::string& id,double rms) {
    Snapshot s;
    s.column=column;
    s.generation_token=generation;
    s.resource_identity=id;
    s.analysis_scope_label="Full Track";
    s.absolute_scope_begin=12;
    s.scope_frames=19200;
    s.sample_rate_hz=48000;
    s.lifecycle=Lifecycle::current;
    s.metrics[1]={rms,Quality::present,{}};
    // Even if a mock incorrectly injects a numeric TP, the candidate
    // UI projection must suppress M16 unqualified public status.
    s.metrics[3]={-1.0,Quality::present,{}};
    return s;
}
static ExpectedOwnerReceipt receipt(const Snapshot& s) {
    return {s.column,s.generation_token,s.resource_identity,
            s.analysis_scope_label,s.absolute_scope_begin,
            s.scope_frames,s.sample_rate_hz,true,true};
}
void M21UiReadonlyProjectionCandidateTest::missingReceiptsHideAllNumbers() {
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"prepared-7",-18.0);
    snapshots[1]=example(Column::gold,3,"gold-3",-13.0);
    const auto view=project_readonly(compose(snapshots),{});
    QVERIFY(!view.production_authorized);
    QVERIFY(!view.columns[0].candidate_matched);
    QVERIFY(!view.columns[1].candidate_matched);
    QVERIFY(view.columns[0].state=="MISSING");
    QVERIFY(view.columns[0].scope_text=="SCOPE UNAVAILABLE");
    for(const auto* col:{&view.columns[0],&view.columns[1]}) {
        for(const auto& cell:col->values) {
            QVERIFY(!cell.numeric);
            QVERIFY(cell.value_text=="UNAVAILABLE");
            QVERIFY(cell.quality_text=="MISSING");
        }
    }
    QVERIFY(view.prepared_to_gold[1].value_text=="UNAVAILABLE");
    QVERIFY(!view.prepared_to_gold[1].numeric);
}
void M21UiReadonlyProjectionCandidateTest::
matchedCandidateExposesOnlyAllowedMetricsAndNeverRelease() {
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"prepared-7",-18.0);
    snapshots[1]=example(Column::gold,3,"independent-gold-3",-13.0);
    Receipts owner{};
    owner[0]=receipt(*snapshots[0]);
    owner[1]=receipt(*snapshots[1]);
    const auto view=project_readonly(compose(snapshots),owner);
    QVERIFY(!view.production_authorized);
    QVERIFY(view.gold_independent_scope);
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(view.columns[1].candidate_matched);
    QVERIFY(view.columns[0].values[1].numeric);
    QVERIFY(view.columns[0].values[1].value_text=="-18.00");
    QVERIFY(view.columns[0].values[1].unit_text=="dBFS");
    QVERIFY(view.prepared_to_gold[1].numeric);
    QVERIFY(view.prepared_to_gold[1].value_text=="+5.00");
    QVERIFY(!view.columns[0].values[3].numeric);
    QVERIFY(view.columns[0].values[3].quality_text=="M16 UNQUALIFIED");
    QVERIFY(!view.prepared_to_gold[3].numeric);
    QVERIFY(!view.columns[2].candidate_matched);

    // A claimed owner token is not a canonical owner; mismatch must
    // suppress the whole selected column and its Gold-minus-Current delta.
    owner[0]->exact_realization_id="wrong-owner";
    const auto mismatch=project_readonly(compose(snapshots),owner);
    QVERIFY(mismatch.columns[0].state=="STALE");
    QVERIFY(!mismatch.columns[0].candidate_matched);
    QVERIFY(!mismatch.columns[0].values[1].numeric);
    QVERIFY(mismatch.columns[0].values[1].value_text=="STALE");
    QVERIFY(mismatch.columns[0].values[1].quality_text=="STALE");
    QVERIFY(mismatch.columns[1].candidate_matched);
    QVERIFY(!mismatch.prepared_to_gold[1].numeric);
    QVERIFY(mismatch.prepared_to_gold[1].value_text=="STALE");

    owner[0]=receipt(*snapshots[0]);
    owner[0]->frame_domain_checked_at_source=false;
    const auto unchecked=project_readonly(compose(snapshots),owner);
    QVERIFY(!unchecked.columns[0].candidate_matched);
    QVERIFY(!unchecked.prepared_to_gold[1].numeric);

    // Invalid storage cannot show a numeric or masquerade as CURRENT.
    owner[0]=receipt(*snapshots[0]);
    snapshots[0]->metrics[1]={std::nullopt,Quality::present,{}};
    const auto poison=project_readonly(compose(snapshots),owner);
    QVERIFY(!poison.columns[0].values[1].numeric);
    QVERIFY(poison.columns[0].values[1].quality_text=="INVALID");
}
void M21UiReadonlyProjectionCandidateTest::
changedSourceDoesNotInvalidateIndependentGold() {
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"prepared-7",-18.0);
    snapshots[1]=example(Column::gold,3,"gold-3",-13.0);
    snapshots[2]=example(Column::processed,2,"processed-2",-14.0);
    Receipts owner{};
    for(std::size_t i=0;i<3;++i)owner[i]=receipt(*snapshots[i]);
    const auto before=project_readonly(compose(snapshots),owner);
    QVERIFY(before.prepared_to_gold[1].numeric);
    QVERIFY(before.processed_to_gold[1].numeric);
    snapshots[0]->lifecycle=Lifecycle::stale;
    const auto after=project_readonly(compose(snapshots),owner);
    QVERIFY(after.columns[0].state=="STALE");
    QVERIFY(!after.columns[0].candidate_matched);
    QVERIFY(!after.prepared_to_gold[1].numeric);
    QVERIFY(after.prepared_to_gold[1].value_text=="STALE");
    QVERIFY(after.columns[1].candidate_matched);
    QVERIFY(after.columns[2].candidate_matched);
    QVERIFY(after.processed_to_gold[1].numeric);
    QVERIFY(after.processed_to_gold[1].value_text=="+1.00");
}
void M21UiReadonlyProjectionCandidateTest::
realM19M20DataCanBeProjectedOnlyAsPreproduction() {
    const auto rate=core::SampleRate::create(48000);
    QVERIFY(rate);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::MONO_C);
    const auto count=core::FrameCount::create(19200);
    QVERIFY(fmt&&count);
    auto pcm=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{12},*count.value());
    QVERIFY(pcm);
    auto samples=pcm.value()->mutable_view().channel(0);
    QVERIFY(samples);
    for(std::size_t i=0;i<samples.value()->size();++i) {
        (*samples.value())[i]=0.25*std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
    }
    const auto level=analysis::measure_basic_level_full_resource(*pcm.value());
    const auto stereo=analysis::measure_tonal_stereo_scope(
        pcm.value()->view(),core::FrameIndex{12},*count.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(level&&stereo);
    const auto imported=import_real_product_metrics(*level.value(),
        *stereo.value(),
        {Column::prepared,11,"real-m19m20-owner-claim","Full Track",true});
    QVERIFY(imported.snapshot);
    QVERIFY(imported.problem==RealProductPortProblem::none);
    Slots snapshots{};
    snapshots[0]=*imported.snapshot;
    Receipts refs{};
    refs[0]=receipt(*snapshots[0]);
    const auto view=project_readonly(compose(snapshots),refs);
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(view.columns[0].values[1].numeric); // Actual M19 RMS
    QVERIFY(!view.columns[0].values[3].numeric); // M16 TP not qualified
    QVERIFY(!view.columns[0].values[5].numeric); // Mono width N/A
    QVERIFY(view.columns[0].values[5].quality_text=="NOT APPLICABLE");
    QVERIFY(!view.production_authorized);        // Still private candidate
    QVERIFY(!view.prepared_to_gold[1].numeric);    // No Gold owner

    // This UI envelope is derived from ACTUAL M19/M20 AudioBuffer
    // measurements, never from a hand-written level value. It may be
    // displayed only as an explicitly marked non-Product test fixture.
    using namespace app::m21_private::ui_candidate::qt_review;
    const auto realReview=candidate_test_fixture_envelope(
        view,Column::prepared,ExplicitTestFixture{0x4D323147U});
    QVERIFY(realReview.value("fixtureMode").toBool());
    QVERIFY(realReview.value("ready").toBool());
    QVERIFY(!realReview.value("productionAuthorized").toBool());
    const auto fields=realReview.value("levels").toMap();
    bool numberOk=false;
    const double displayRms=fields.value("rms").toString().toDouble(&numberOk);
    QVERIFY(numberOk);
    QVERIFY(std::isfinite(displayRms));
    QVERIFY(std::abs(displayRms-*imported.snapshot->metrics[1].unrounded)
            <=0.0051); // decimal UI rounds ONLY at display time
    QCOMPARE(fields.value("truePeak").toString(),
             QStringLiteral("UNAVAILABLE"));
    QCOMPARE(realReview.value("stereo").toMap().value("width").toString(),
             QStringLiteral("NOT APPLICABLE"));
    QCOMPARE(realReview.value("spectrumStatus").toString(),
             QStringLiteral("UNAVAILABLE")); // no fabricated static PSD plot
    QCOMPARE(realReview.value("tonalProfileStatus").toString(),
             QStringLiteral("UNAVAILABLE")); // no invented dB shape
    // Even for this real buffer, the ordinary unpublished envelope is
    // masked and cannot become a production result via test receipt claims.
    QVERIFY(!masked_unpublished_envelope().value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::
qmlEnvelopeHonorsIndividualColumnQualityAndRejectsRelease()
{
    using namespace app::m21_private::ui_candidate::qt_review;
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"prepared-7",-18.0);
    snapshots[1]=example(Column::gold,3,"gold-independent",-13.0);
    Receipts matched{};
    matched[0]=receipt(*snapshots[0]);
    matched[1]=receipt(*snapshots[1]);
    const auto typed=project_readonly(compose(snapshots),matched);
    QVERIFY(!typed.production_authorized);
    // The ordinary (eventual Product-facing) conversion refuses numeric
    // publication despite apparently matched preproduction receipts.
    const auto unpublished=masked_unpublished_envelope();
    QVERIFY(!unpublished.value("ready").toBool());
    QVERIFY(!unpublished.value("fixtureMode").toBool());
    QVERIFY(!unpublished.value("productionAuthorized").toBool());
    QVERIFY(unpublished.value("levels").toMap().isEmpty());
    QVERIFY(unpublished.value("metrics").toList().isEmpty());
    // Wrong marker does not permit even preproduction display.
    const auto denied=candidate_test_fixture_envelope(
        typed,Column::prepared,ExplicitTestFixture{0U});
    QVERIFY(!denied.value("ready").toBool());
    QVERIFY(!denied.value("fixtureMode").toBool());
    QVERIFY(denied.value("levels").toMap().isEmpty());
    // This explicit TEST FIXTURE alone maps typed M19/M21 evidence to QML
    // strings with an unmistakable fixtureMode watermark.
    const auto review=candidate_test_fixture_envelope(
        typed,Column::prepared,ExplicitTestFixture{0x4D323147U});
    QVERIFY(review.value("fixtureMode").toBool());
    QVERIFY(review.value("ready").toBool());
    QVERIFY(!review.value("productionAuthorized").toBool());
    QCOMPARE(review.value("analysisStatus").toString(),QStringLiteral("CURRENT"));
    QCOMPARE(review.value("scopeLabel").toString(),QStringLiteral("Full Track"));
    const auto levels=review.value("levels").toMap();
    QCOMPARE(levels.value("rms").toString(),QStringLiteral("-18.00"));
    QCOMPARE(levels.value("truePeak").toString(),QStringLiteral("UNAVAILABLE"));
    const auto rows=review.value("metrics").toList();
    QCOMPARE(rows.size(),9);
    const auto rms=rows.at(1).toMap();
    QCOMPARE(rms.value("prepared").toString(),QStringLiteral("-18.00"));
    QCOMPARE(rms.value("gold").toString(),QStringLiteral("-13.00"));
    QCOMPARE(rms.value("processed").toString(),QStringLiteral("UNAVAILABLE"));
    QCOMPARE(rms.value("preparedDelta").toString(),QStringLiteral("+5.00"));
    QCOMPARE(rms.value("preparedQuality").toString(),QStringLiteral("CURRENT"));
    QCOMPARE(rms.value("processedQuality").toString(),QStringLiteral("MISSING"));
    QVERIFY(review.value("spectrum").toList().isEmpty());
    QVERIFY(review.value("bands").toList().isEmpty());
    QCOMPARE(review.value("spectrumStatus").toString(),
             QStringLiteral("UNAVAILABLE"));
    // Replacing the source marks PREPARED stale, but independent GOLD stays
    // eligible in a separate nonproduction preview selection.
    snapshots[0]->lifecycle=Lifecycle::stale;
    const auto changed=project_readonly(compose(snapshots),matched);
    const auto stale=candidate_test_fixture_envelope(
        changed,Column::prepared,ExplicitTestFixture{0x4D323147U});
    QVERIFY(!stale.value("ready").toBool());
    QCOMPARE(stale.value("analysisStatus").toString(),QStringLiteral("STALE"));
    QVERIFY(stale.value("levels").toMap().isEmpty());
    const auto gold=candidate_test_fixture_envelope(
        changed,Column::gold,ExplicitTestFixture{0x4D323147U});
    QVERIFY(gold.value("ready").toBool());
    QCOMPARE(gold.value("levels").toMap().value("rms").toString(),
             QStringLiteral("-13.00"));
    QVERIFY(!gold.value("productionAuthorized").toBool());
    // Invalid semantic column index is always refused.
    const auto invalid=candidate_test_fixture_envelope(
        typed,static_cast<Column>(255),ExplicitTestFixture{0x4D323147U});
    QVERIFY(!invalid.value("ready").toBool());
    QVERIFY(!invalid.value("fixtureMode").toBool());
}

void M21UiReadonlyProjectionCandidateTest::rejectReceiptWithWrongSemanticColumn()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p", -18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->column=Column::gold;
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].state=="STALE");
    QVERIFY(!r.columns[0].values[1].numeric);
    QVERIFY(!r.production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::rejectReceiptWithChangedGeneration()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->generation_token=8;
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].state=="STALE");
    QVERIFY(r.columns[0].values[1].value_text=="STALE");
}

void M21UiReadonlyProjectionCandidateTest::rejectReceiptWithChangedRealization()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"original",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->exact_realization_id="replacement";
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(!r.columns[0].values[1].numeric);
    QVERIFY(r.columns[0].scope_text=="SCOPE UNAVAILABLE");
}

void M21UiReadonlyProjectionCandidateTest::rejectReceiptWithDifferentScopeKind()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->analysis_scope_label="Region";
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(r.columns[0].state=="STALE");
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(!r.columns[0].values[1].numeric);
}

void M21UiReadonlyProjectionCandidateTest::rejectReceiptWithDifferentFirstFrame()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->absolute_scope_begin+=1;
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].state=="STALE");
    QVERIFY(!r.prepared_to_gold[1].numeric);
}

void M21UiReadonlyProjectionCandidateTest::rejectReceiptWithDifferentFrameCount()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->scope_frames-=1;
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].values[1].quality_text=="STALE");
}

void M21UiReadonlyProjectionCandidateTest::rejectReceiptWithDifferentSampleRate()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->sample_rate_hz=44100;
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].scope_text=="SCOPE UNAVAILABLE");
}

void M21UiReadonlyProjectionCandidateTest::rejectUnconfirmedCanonicalOwnerClaim()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->canonical_owner_confirmed=false;
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(!r.columns[0].values[1].numeric);
    QVERIFY(!r.production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::rejectUncheckedFrameDomainClaim()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[0]->frame_domain_checked_at_source=false;
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].state=="STALE");
    QVERIFY(!r.production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::rejectZeroGenerationEvenWithMatchingReceipt()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,0,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].values[1].quality_text=="STALE");
}

void M21UiReadonlyProjectionCandidateTest::rejectEmptyExactRealizationIdentity()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(!r.columns[0].values[1].numeric);
}

void M21UiReadonlyProjectionCandidateTest::rejectUnsupportedMatchedRate()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->sample_rate_hz=32000;
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].scope_text=="SCOPE UNAVAILABLE");
}

void M21UiReadonlyProjectionCandidateTest::rejectZeroLengthScope()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->scope_frames=0;
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(!r.columns[0].values[1].numeric);
}

void M21UiReadonlyProjectionCandidateTest::rejectAbsoluteFrameRangeOverflow()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->absolute_scope_begin=std::uint64_t(-1)-9;
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].state=="STALE");
    QVERIFY(!r.columns[0].values[1].numeric);
}

void M21UiReadonlyProjectionCandidateTest::staleLifecycleMasksAllAbsoluteNumbers()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->lifecycle=Lifecycle::stale;
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(r.columns[0].state=="STALE");
    QVERIFY(!r.columns[0].candidate_matched);
    for(const auto& cell:r.columns[0].values) {
        QVERIFY(cell.value_text=="STALE");
        QVERIFY(cell.quality_text=="STALE");
        QVERIFY(!cell.numeric);
    }
}

void M21UiReadonlyProjectionCandidateTest::missingLifecycleNeverMasqueradesAsZero()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->lifecycle=Lifecycle::missing;
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(r.columns[0].state=="MISSING");
    for(const auto& cell:r.columns[0].values) {
        QVERIFY(cell.value_text=="UNAVAILABLE");
        QVERIFY(cell.quality_text=="MISSING");
        QVERIFY(!cell.numeric);
    }
}

void M21UiReadonlyProjectionCandidateTest::swappedSnapshotColumnIsInvalidNotCurrent()
{
    Slots snapshots{}; snapshots[0]=example(Column::gold,7,"g",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(r.columns[0].state=="INVALID");
    QVERIFY(!r.columns[0].candidate_matched);
    QVERIFY(!r.columns[0].values[1].numeric);
    QVERIFY(!r.production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::missingGoldMasksPreparedDeltaOnly()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(!view.columns[1].candidate_matched);
    QVERIFY(!view.prepared_to_gold[1].numeric);
    QVERIFY(view.prepared_to_gold[1].value_text=="UNAVAILABLE");
    QVERIFY(view.prepared_to_gold[1].quality_text=="MISSING");
}

void M21UiReadonlyProjectionCandidateTest::staleGoldMasksBothDependentDeltaColumns()
{
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[1]=example(Column::gold,3,"g",-13.0);
    snapshots[2]=example(Column::processed,9,"x",-14.0);
    Receipts owners{};
    for(std::size_t i=0;i<3;++i)owners[i]=receipt(*snapshots[i]);
    snapshots[1]->lifecycle=Lifecycle::stale;
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(view.columns[2].candidate_matched);
    QVERIFY(!view.columns[1].candidate_matched);
    QVERIFY(!view.prepared_to_gold[1].numeric);
    QVERIFY(view.prepared_to_gold[1].value_text=="STALE");
    QVERIFY(view.prepared_to_gold[1].quality_text=="STALE");
    QVERIFY(!view.processed_to_gold[1].numeric);
    QVERIFY(view.processed_to_gold[1].quality_text=="STALE");
}

void M21UiReadonlyProjectionCandidateTest::lowConfidenceMetricKeepsExplicitQuality()
{
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[1]=example(Column::gold,3,"g",-13.0);
    snapshots[0]->metrics[1]={-18.0,Quality::low_confidence,"partial support"};
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[1]=receipt(*snapshots[1]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(view.columns[0].values[1].numeric);
    QVERIFY(view.columns[0].values[1].quality_text=="LOW SUPPORT");
    QVERIFY(view.prepared_to_gold[1].numeric);
    QVERIFY(view.prepared_to_gold[1].quality_text=="LOW SUPPORT");
    QVERIFY(!view.production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::monoNotApplicableWidthHasNoNumeric()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"mono",-18.0);
    snapshots[0]->metrics[5]={std::nullopt,Quality::not_applicable,"mono"};
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(!view.columns[0].values[5].numeric);
    QVERIFY(view.columns[0].values[5].value_text=="NOT APPLICABLE");
    QVERIFY(view.columns[0].values[5].quality_text=="NOT APPLICABLE");
}

void M21UiReadonlyProjectionCandidateTest::invalidPresentCellWithoutScalarNeverRenders()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->metrics[1]={std::nullopt,Quality::present,{}};
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(!view.columns[0].values[1].numeric);
    QVERIFY(view.columns[0].values[1].value_text=="INVALID");
    QVERIFY(view.columns[0].values[1].quality_text=="INVALID");
}

void M21UiReadonlyProjectionCandidateTest::nanMetricRemainsInvalidNotZero()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->metrics[1]={std::nan(""),Quality::present,{}};
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(!view.columns[0].values[1].numeric);
    QVERIFY(view.columns[0].values[1].value_text=="INVALID");
    QVERIFY(!view.production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::actualZeroIsNumericNotUnavailable()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",0.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(view.columns[0].values[1].numeric);
    QVERIFY(view.columns[0].values[1].value_text=="0.00");
    QVERIFY(view.columns[0].values[1].quality_text=="CURRENT");
}

void M21UiReadonlyProjectionCandidateTest::negativeGoldMinusCurrentDeltaRetainsSign()
{
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[1]=example(Column::gold,3,"g",-20.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    owners[1]=receipt(*snapshots[1]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(view.prepared_to_gold[1].numeric);
    QVERIFY(view.prepared_to_gold[1].value_text=="-2.00");
    QVERIFY(view.prepared_to_gold[1].unit_text=="dB");
}

void M21UiReadonlyProjectionCandidateTest::infiniteSampleNeverBecomesReadout()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->metrics[1]={std::numeric_limits<double>::infinity(),
                               Quality::present,{}};
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto view=project_readonly(compose(snapshots),owners);
    QVERIFY(!view.columns[0].values[1].numeric);
    QVERIFY(view.columns[0].values[1].quality_text=="INVALID");
}

void M21UiReadonlyProjectionCandidateTest::invalidLifecycleMasksEveryReadout()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->lifecycle=Lifecycle::invalid;
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(r.columns[0].state=="INVALID");
    for(const auto& v:r.columns[0].values) {
        QVERIFY(!v.numeric);
        QVERIFY(v.value_text=="INVALID");
        QVERIFY(v.quality_text=="INVALID");
    }
}

void M21UiReadonlyProjectionCandidateTest::matchingRegionReceiptKeepsRegionScope()
{
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"region",-18.0);
    snapshots[0]->analysis_scope_label="Region";
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto r=project_readonly(compose(snapshots),owners);
    QVERIFY(r.columns[0].candidate_matched);
    QVERIFY(r.columns[0].scope_text=="Region");
    QVERIFY(r.columns[0].values[1].numeric);
    QVERIFY(!r.production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::allSixSupportedRatesUseExactMatchingOnly()
{
    const std::array<std::uint32_t,6> valid{44100,48000,88200,96000,
                                            176400,192000};
    for(const auto rate:valid) {
        Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
        snapshots[0]->sample_rate_hz=rate;
        Receipts owners{}; owners[0]=receipt(*snapshots[0]);
        const auto r=project_readonly(compose(snapshots),owners);
        QVERIFY(r.columns[0].candidate_matched);
        QVERIFY(r.columns[0].values[1].numeric);
        owners[0]->sample_rate_hz=rate==192000 ? 176400 : 192000;
        const auto mismatch=project_readonly(compose(snapshots),owners);
        QVERIFY(!mismatch.columns[0].candidate_matched);
    }
}

void M21UiReadonlyProjectionCandidateTest::decimalDisplayUsesStableDotAndDeltaSign()
{
    QVERIFY(decimal_text(1.234,false)=="1.23");
    QVERIFY(decimal_text(1.234,true)=="+1.23");
    QVERIFY(decimal_text(-2.345,true)=="-2.35" ||
            decimal_text(-2.345,true)=="-2.34");
    QVERIFY(decimal_text(std::numeric_limits<double>::infinity(),false)
            =="INVALID");
    QVERIFY(decimal_text(std::numeric_limits<double>::quiet_NaN(),false)
            =="INVALID");
}

void M21UiReadonlyProjectionCandidateTest::testFixtureRejectsArtificialProductionAuthorization()
{
    using namespace app::m21_private::ui_candidate::qt_review;
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    auto projection=project_readonly(compose(snapshots),owners);
    QVERIFY(projection.columns[0].candidate_matched);
    projection.production_authorized=true; // deliberately poisoned fixture
    const auto denied=candidate_test_fixture_envelope(
        projection,Column::prepared,ExplicitTestFixture{0x4D323147U});
    QVERIFY(!denied.value("ready").toBool());
    QVERIFY(!denied.value("fixtureMode").toBool());
    QVERIFY(denied.value("levels").toMap().isEmpty());
}

void M21UiReadonlyProjectionCandidateTest::processedColumnQtProjectionSelectsProcessedCorrectly()
{
    using namespace app::m21_private::ui_candidate::qt_review;
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[1]=example(Column::gold,3,"g",-13.0);
    snapshots[2]=example(Column::processed,9,"x",-14.0);
    Receipts owners{};
    for(std::size_t i=0;i<3;++i)owners[i]=receipt(*snapshots[i]);
    const auto projection=project_readonly(compose(snapshots),owners);
    const auto shown=candidate_test_fixture_envelope(
        projection,Column::processed,ExplicitTestFixture{0x4D323147U});
    QVERIFY(shown.value("ready").toBool());
    QCOMPARE(shown.value("levels").toMap().value("rms").toString(),
             QStringLiteral("-14.00"));
    const auto rows=shown.value("metrics").toList();
    QCOMPARE(rows.at(1).toMap().value("processed").toString(),
             QStringLiteral("-14.00"));
    QCOMPARE(rows.at(1).toMap().value("processedDelta").toString(),
             QStringLiteral("+1.00"));
    QVERIFY(!shown.value("productionAuthorized").toBool());
}

void M21UiReadonlyProjectionCandidateTest::unclaimedGoldColumnIsNeverRenderedInQtRows()
{
    using namespace app::m21_private::ui_candidate::qt_review;
    Slots snapshots{};
    snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[1]=example(Column::gold,3,"g",-13.0);
    Receipts owners{}; owners[0]=receipt(*snapshots[0]); // no Gold receipt
    const auto projection=project_readonly(compose(snapshots),owners);
    const auto shown=candidate_test_fixture_envelope(
        projection,Column::prepared,ExplicitTestFixture{0x4D323147U});
    QVERIFY(shown.value("ready").toBool());
    const auto row=shown.value("metrics").toList().at(1).toMap();
    QCOMPARE(row.value("prepared").toString(),QStringLiteral("-18.00"));
    QCOMPARE(row.value("gold").toString(),QStringLiteral("UNAVAILABLE"));
    QCOMPARE(row.value("preparedDelta").toString(),
             QStringLiteral("UNAVAILABLE"));
    QCOMPARE(row.value("goldQuality").toString(),QStringLiteral("MISSING"));
}

void M21UiReadonlyProjectionCandidateTest::qtRowsPreservePerMetricUnitsAndQualityNames()
{
    using namespace app::m21_private::ui_candidate::qt_review;
    Slots snapshots{}; snapshots[0]=example(Column::prepared,7,"p",-18.0);
    snapshots[0]->metrics[5]={std::nullopt,Quality::not_applicable,"mono"};
    Receipts owners{}; owners[0]=receipt(*snapshots[0]);
    const auto projection=project_readonly(compose(snapshots),owners);
    const auto shown=candidate_test_fixture_envelope(
        projection,Column::prepared,ExplicitTestFixture{0x4D323147U});
    const auto rows=shown.value("metrics").toList();
    QCOMPARE(rows.size(),9);
    QCOMPARE(rows.at(1).toMap().value("unit").toString(),
             QStringLiteral("dBFS"));
    QCOMPARE(rows.at(1).toMap().value("preparedQuality").toString(),
             QStringLiteral("CURRENT"));
    QCOMPARE(rows.at(5).toMap().value("prepared").toString(),
             QStringLiteral("NOT APPLICABLE"));
    QCOMPARE(rows.at(5).toMap().value("preparedQuality").toString(),
             QStringLiteral("NOT APPLICABLE"));
}

void M21UiReadonlyProjectionCandidateTest::displayZeroSuppressesSignedRoundedZero()
{
    // Display rounding is cosmetic; exact unrounded M21 delta is preserved.
    QVERIFY(decimal_text(-0.0001,true)=="0.00");
    QVERIFY(decimal_text(0.0001,true)=="0.00");
    QVERIFY(decimal_text(-0.0001,false)=="0.00");
    QVERIFY(decimal_text(0.0001,false)=="0.00");
    QVERIFY(decimal_text(0.015,true)=="+0.01" ||
            decimal_text(0.015,true)=="+0.02");
    QVERIFY(decimal_text(-0.02,true)=="-0.02");
}

void M21UiReadonlyProjectionCandidateTest::newReviewEpochStoreStartsMasked()
{
    ReviewEpochStore store;
    QVERIFY(store.selected()==Column::prepared);
    QVERIFY(!store.snapshot().production_authorized);
    for(const auto& col:store.snapshot().columns) {
        QVERIFY(!col.candidate_matched);
        QVERIFY(col.state=="MISSING");
        QVERIFY(col.scope_text=="SCOPE UNAVAILABLE");
    }
    QVERIFY(!store.unpublished().value("ready").toBool());
    QVERIFY(!store.unpublished().value("fixtureMode").toBool());
    QVERIFY(store.unpublished().value("levels").toMap().isEmpty());
    QVERIFY(!store.test_preview(
        qt_review::ExplicitTestFixture{0x4D323147U}).value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStorePublishesOnlyAfterExactExpectedGeneration()
{
    ReviewEpochStore store;
    const auto s=example(Column::prepared,7,"p7",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(s,receipt(s)));
    const auto current=store.snapshot();
    QVERIFY(current.columns[0].candidate_matched);
    QVERIFY(current.columns[0].state=="CURRENT");
    QVERIFY(current.columns[0].values[1].value_text=="-18.00");
    QVERIFY(!current.production_authorized);
    QVERIFY(!store.unpublished().value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreNewGenerationStalesOldPrepared()
{
    ReviewEpochStore store;
    const auto old=example(Column::prepared,7,"p7",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(old,receipt(old)));
    QVERIFY(store.begin(Column::prepared,8));
    const auto changed=store.snapshot();
    QVERIFY(changed.columns[0].state=="STALE");
    QVERIFY(!changed.columns[0].candidate_matched);
    QVERIFY(!changed.columns[0].values[1].numeric);
    QVERIFY(changed.columns[0].values[1].value_text=="STALE");
    QVERIFY(!store.test_preview(
        qt_review::ExplicitTestFixture{0x4D323147U}).value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreLateCompletionCannotRestoreOldPrepared()
{
    ReviewEpochStore store;
    const auto stale=example(Column::prepared,7,"p7",-18.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(stale,receipt(stale)));
    QVERIFY(store.begin(Column::prepared,8));
    QVERIFY(!store.receive(stale,receipt(stale)));
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
    QVERIFY(store.snapshot().columns[0].state=="STALE");
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreFreshReplacementWinsAfterStaleCompletion()
{
    ReviewEpochStore store;
    const auto old=example(Column::prepared,7,"p7",-18.0);
    const auto newer=example(Column::prepared,8,"p8",-15.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(old,receipt(old)));
    QVERIFY(store.begin(Column::prepared,8));
    QVERIFY(!store.receive(old,receipt(old)));
    QVERIFY(store.receive(newer,receipt(newer)));
    QVERIFY(store.snapshot().columns[0].candidate_matched);
    QVERIFY(store.snapshot().columns[0].values[1].value_text=="-15.00");
    QVERIFY(!store.receive(old,receipt(old)));
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreDuplicateCompletionCannotReplaceFirstCurrent()
{
    ReviewEpochStore store;
    const auto first=example(Column::prepared,7,"p7",-18.0);
    const auto second=example(Column::prepared,7,"p7",-10.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.receive(first,receipt(first)));
    QVERIFY(!store.receive(second,receipt(second)));
    QCOMPARE(QString::fromStdString(store.snapshot().columns[0].values[1].value_text),
             QStringLiteral("-18.00"));
    QVERIFY(!store.snapshot().production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreIndependentGoldSurvivesPreparedReanalysis()
{
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p7",-18.0);
    const auto g=example(Column::gold,3,"gold",-13.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.receive(g,receipt(g)));
    QVERIFY(store.snapshot().prepared_to_gold[1].numeric);
    QVERIFY(store.begin(Column::prepared,8));
    const auto changed=store.snapshot();
    QVERIFY(!changed.columns[0].candidate_matched);
    QVERIFY(changed.columns[1].candidate_matched);
    QVERIFY(changed.columns[1].values[1].value_text=="-13.00");
    QVERIFY(!changed.prepared_to_gold[1].numeric);
    QVERIFY(changed.prepared_to_gold[1].value_text=="STALE");
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreProcessedKeepsOwnGenerationDuringSourceChange()
{
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    const auto g=example(Column::gold,3,"g",-13.0);
    const auto x=example(Column::processed,9,"x",-14.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.begin(Column::processed,9));
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.receive(g,receipt(g)));
    QVERIFY(store.receive(x,receipt(x)));
    QVERIFY(store.begin(Column::prepared,8));
    const auto view=store.snapshot();
    QVERIFY(!view.prepared_to_gold[1].numeric);
    QVERIFY(view.processed_to_gold[1].numeric);
    QVERIFY(view.processed_to_gold[1].value_text=="+1.00");
    QVERIFY(view.columns[1].candidate_matched);
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreSelectionDoesNotMutateOtherPanels()
{
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    const auto g=example(Column::gold,3,"g",-13.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.receive(g,receipt(g)));
    QVERIFY(store.select(Column::gold));
    QVERIFY(store.selected()==Column::gold);
    QCOMPARE(store.test_preview(qt_review::ExplicitTestFixture{0x4D323147U})
             .value("levels").toMap().value("rms").toString(),
             QStringLiteral("-13.00"));
    QVERIFY(store.select(Column::prepared));
    QCOMPARE(store.test_preview(qt_review::ExplicitTestFixture{0x4D323147U})
             .value("levels").toMap().value("rms").toString(),
             QStringLiteral("-18.00"));
    QVERIFY(!store.unpublished().value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreRejectsPublishWithoutBeginningEpoch()
{
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    QVERIFY(!store.receive(p,receipt(p)));
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
    QVERIFY(store.snapshot().columns[0].state=="MISSING");
    QVERIFY(!store.unpublished().value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreRejectsZeroAndBackwardGenerations()
{
    ReviewEpochStore store;
    QVERIFY(!store.begin(Column::prepared,0));
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(!store.begin(Column::prepared,7));
    QVERIFY(!store.begin(Column::prepared,6));
    const auto p=example(Column::prepared,7,"p",-18.0);
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.snapshot().columns[0].candidate_matched);
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreRefusesWrongOwnerWithoutInstalling()
{
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"p",-18.0);
    auto claim=receipt(p);
    claim.exact_realization_id="another";
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(!store.receive(p,claim));
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
    QVERIFY(store.receive(p,receipt(p))); // same pending epoch remains usable
    QVERIFY(store.snapshot().columns[0].candidate_matched);
    QVERIFY(store.snapshot().columns[0].values[1].value_text=="-18.00");
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreRejectsWrongSourceScopeWithoutInstalling()
{
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"p",-18.0);
    auto claim=receipt(p);
    claim.absolute_scope_begin+=1;
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(!store.receive(p,claim));
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.snapshot().columns[0].candidate_matched);
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreInvalidOwnerClaimCannotMakeDataCurrent()
{
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"p",-18.0);
    auto claim=receipt(p);
    claim.canonical_owner_confirmed=false;
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(!store.receive(p,claim));
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
    QVERIFY(!store.test_preview(qt_review::ExplicitTestFixture{0x4D323147U})
             .value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreUncheckedFrameDomainIsRejected()
{
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"p",-18.0);
    auto claim=receipt(p);
    claim.frame_domain_checked_at_source=false;
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(!store.receive(p,claim));
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
    QVERIFY(!store.snapshot().production_authorized);
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreRejectsMalformedUpstreamCell()
{
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"p",-18.0);
    p.metrics[1]={std::nullopt,Quality::present,{}};
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(!store.receive(p,receipt(p))); // PrivatePublicationGuard checks storage
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
    QVERIFY(store.snapshot().columns[0].state=="MISSING");
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreRejectsUnsupportedSourceRate()
{
    ReviewEpochStore store;
    auto p=example(Column::prepared,7,"p",-18.0);
    p.sample_rate_hz=32000;
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(!store.receive(p,receipt(p)));
    QVERIFY(!store.snapshot().columns[0].candidate_matched);
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreInvalidSelectionKeepsLastPanel()
{
    ReviewEpochStore store;
    QVERIFY(store.select(Column::gold));
    QVERIFY(!store.select(static_cast<Column>(255)));
    QVERIFY(store.selected()==Column::gold);
    QVERIFY(!store.unpublished().value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreUnselectedColumnNeverLeaksIntoPreview()
{
    ReviewEpochStore store;
    const auto gold=example(Column::gold,3,"g",-13.0);
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.receive(gold,receipt(gold)));
    QVERIFY(store.selected()==Column::prepared);
    QVERIFY(!store.test_preview(qt_review::ExplicitTestFixture{0x4D323147U})
             .value("ready").toBool());
    QVERIFY(store.select(Column::gold));
    QVERIFY(store.test_preview(qt_review::ExplicitTestFixture{0x4D323147U})
            .value("ready").toBool());
    QVERIFY(!store.unpublished().value("ready").toBool());
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreGoldReanalysisStalesOnlyGold()
{
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    const auto g=example(Column::gold,3,"g",-13.0);
    const auto x=example(Column::processed,9,"x",-14.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.begin(Column::processed,9));
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.receive(g,receipt(g)));
    QVERIFY(store.receive(x,receipt(x)));
    QVERIFY(store.begin(Column::gold,4));
    const auto view=store.snapshot();
    QVERIFY(view.columns[0].candidate_matched);
    QVERIFY(view.columns[2].candidate_matched);
    QVERIFY(view.columns[1].state=="STALE");
    QVERIFY(!view.columns[1].candidate_matched);
    QVERIFY(view.prepared_to_gold[1].quality_text=="STALE");
    QVERIFY(view.processed_to_gold[1].quality_text=="STALE");
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreFreshGoldUpdatesBothDeltas()
{
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    const auto g=example(Column::gold,3,"g",-13.0);
    const auto x=example(Column::processed,9,"x",-14.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.begin(Column::processed,9));
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.receive(g,receipt(g)));
    QVERIFY(store.receive(x,receipt(x)));
    QVERIFY(store.begin(Column::gold,4));
    const auto newGold=example(Column::gold,4,"g-new",-12.0);
    QVERIFY(store.receive(newGold,receipt(newGold)));
    const auto v=store.snapshot();
    QVERIFY(v.prepared_to_gold[1].numeric);
    QVERIFY(v.processed_to_gold[1].numeric);
    QVERIFY(v.prepared_to_gold[1].value_text=="+6.00");
    QVERIFY(v.processed_to_gold[1].value_text=="+2.00");
    QVERIFY(v.gold_independent_scope);
}

void M21UiReadonlyProjectionCandidateTest::reviewStoreProcessedReanalysisDoesNotMaskPreparedDelta()
{
    ReviewEpochStore store;
    const auto p=example(Column::prepared,7,"p",-18.0);
    const auto g=example(Column::gold,3,"g",-13.0);
    const auto x=example(Column::processed,9,"x",-14.0);
    QVERIFY(store.begin(Column::prepared,7));
    QVERIFY(store.begin(Column::gold,3));
    QVERIFY(store.begin(Column::processed,9));
    QVERIFY(store.receive(p,receipt(p)));
    QVERIFY(store.receive(g,receipt(g)));
    QVERIFY(store.receive(x,receipt(x)));
    QVERIFY(store.begin(Column::processed,10));
    const auto v=store.snapshot();
    QVERIFY(v.columns[0].candidate_matched);
    QVERIFY(v.columns[1].candidate_matched);
    QVERIFY(!v.columns[2].candidate_matched);
    QVERIFY(v.prepared_to_gold[1].numeric);
    QVERIFY(v.prepared_to_gold[1].value_text=="+5.00");
    QVERIFY(!v.processed_to_gold[1].numeric);
    QVERIFY(v.processed_to_gold[1].quality_text=="STALE");
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M21UiReadonlyProjectionCandidateTest)
#include "test_m21_ui_readonly_projection_candidate.moc"
