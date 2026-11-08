#pragma once
// PRIVATE design-review contract for THREE INDEPENDENT M19/M20/M21 panels.
// No Product publication, Main.qml registration, canonical owner authority,
// audio processing, or production QML binding is implemented here.
// The only value-carrying method requires an explicit TEST-only fixture.
#include "m21_review_epoch_store_candidate.hpp"
#include "m21_qt_snapshot_review_candidate.hpp"
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace rgsml::app::m21_private::ui_candidate::panel_review {

struct IndependentPanelBundle final {
    std::uint64_t review_revision{};
    bool test_fixture_only{false};
    QVariantMap m19{};
    QVariantMap m20{};
    QVariantMap m21{};
};

// All three unpublished outputs remain masked even if the private store
// contains plausible values and has received matched external claims.
[[nodiscard]] inline IndependentPanelBundle unpublished_panels(
    const ReviewEpochStore& store) {
    IndependentPanelBundle result;
    result.review_revision=store.review_revision();
    result.m19=qt_review::masked_unpublished_envelope();
    result.m20=qt_review::masked_unpublished_envelope();
    result.m21=qt_review::masked_unpublished_envelope();
    return result;
}

// Review-only readout map for a read-only field. Never converts a missing
// cell to 0 or drops its quality. No feature inputs or mutable controls.
[[nodiscard]] inline QVariantMap metric_quality_fields(
    const ReadOnlyColumn& selected, std::size_t first,
    std::size_t last, const std::array<const char*,9>& keys)
{
    QVariantMap quality;
    const auto upper=last>selected.values.size() ? selected.values.size():last;
    for(std::size_t i=first;i<upper;++i) {
        const auto name=QString::fromLatin1(keys[i]);
        quality.insert(name,qt_review::as_qstring(selected.values[i].quality_text));
    }
    return quality;
}
[[nodiscard]] inline QVariantMap column_lifecycles(
    const ReadOnlyProjection& projection) {
    QVariantMap statuses;
    static constexpr std::array<const char*,3> keys{"prepared","gold","processed"};
    for(std::size_t col=0;col<3;++col) {
        statuses.insert(QString::fromLatin1(keys[col]),
                        qt_review::as_qstring(projection.columns[col].state));
    }
    return statuses;
}

// One coherent typed projection and one UI-selected column per capture.
// Reconstruct, rather than reuse any old QVariantMap across begin/receive.
// This is still only a fixture, never a Product/QML release signal.
[[nodiscard]] inline IndependentPanelBundle capture_test_panels(
    const ReviewEpochStore& store,
    qt_review::ExplicitTestFixture fixture)
{
    IndependentPanelBundle result=unpublished_panels(store);
    const auto projection=store.snapshot();
    const auto preview=qt_review::candidate_test_fixture_envelope(
        projection,store.selected(),fixture);
    if(!preview.value("fixtureMode").toBool()
       || preview.value("productionAuthorized").toBool()
       || !store.is_current_review(result.review_revision))return result;

    result.test_fixture_only=true;
    result.m19=preview;
    result.m20=preview;
    result.m21=preview;
    result.m19.insert("panelId",QStringLiteral("M19"));
    result.m20.insert("panelId",QStringLiteral("M20"));
    result.m21.insert("panelId",QStringLiteral("M21"));
    const auto idx=static_cast<std::size_t>(store.selected());
    if(idx>=projection.columns.size())return unpublished_panels(store);
    const auto& selected=projection.columns[idx];

    static constexpr std::array<const char*,9> keys{{
        "lufs","rms","samplePeak","truePeak","crest",
        "width","P10","P50","P90"
    }};
    result.m19.insert("levelsQuality",
        metric_quality_fields(selected,0,5,keys));
    result.m20.insert("stereoQuality",
        metric_quality_fields(selected,5,9,keys));
    result.m21.insert("columnLifecycles",column_lifecycles(projection));
    result.m21.insert("goldIndependentScope",
        projection.gold_independent_scope);

    // Never draw a fabricated spectrum or normalized shape from level
    // values. The M20 typed PSD publisher is a separate future Product gate.
    result.m20.insert("spectrum",QVariantList{});
    result.m20.insert("bands",QVariantList{});
    result.m20.insert("spectrumStatus",QStringLiteral("UNAVAILABLE"));
    result.m20.insert("tonalProfileStatus",QStringLiteral("UNAVAILABLE"));
    return result;
}

// Consumer must check before applying a queued frame. A late Qt callback
// with an older private revision is unusable, irrespective of displayed
// numeric text. This is NOT a substitute for canonical Product owner epochs.
[[nodiscard]] inline bool review_frame_current(
    const ReviewEpochStore& store, const IndependentPanelBundle& frame) {
    return frame.test_fixture_only
        && store.is_current_review(frame.review_revision)
        && !frame.m19.value("productionAuthorized").toBool()
        && !frame.m20.value("productionAuthorized").toBool()
        && !frame.m21.value("productionAuthorized").toBool();
}
} // namespace rgsml::app::m21_private::ui_candidate::panel_review
