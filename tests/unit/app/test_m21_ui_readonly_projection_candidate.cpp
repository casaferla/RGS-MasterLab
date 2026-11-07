// L1 M19–M21 standalone typed read-only projection, preproduction only.
// Uses the real M19/M20 Product-port DTO; no fake Phase-6 Product publisher,
// no QML Main integration, and no permission to report release-ready data.
#include "internal/m21_ui_readonly_projection_candidate.hpp"
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
    QVERIFY(!view.columns[0].values[1].numeric);
    QVERIFY(view.columns[0].values[1].value_text=="UNAVAILABLE");
    QVERIFY(view.prepared_to_gold[1].value_text=="UNAVAILABLE");
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
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M21UiReadonlyProjectionCandidateTest)
#include "test_m21_ui_readonly_projection_candidate.moc"
