// L1 M19–M21 standalone typed read-only projection, preproduction only.
// Uses the real M19/M20 Product-port DTO; no fake Phase-6 Product publisher,
// no QML Main integration, and no permission to report release-ready data.
#include "internal/m21_ui_readonly_projection_candidate.hpp"
#include "internal/m21_qt_snapshot_review_candidate.hpp"
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

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M21UiReadonlyProjectionCandidateTest)
#include "test_m21_ui_readonly_projection_candidate.moc"
