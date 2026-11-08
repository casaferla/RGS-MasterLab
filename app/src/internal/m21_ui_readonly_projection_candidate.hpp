#pragma once
// PREPRODUCTION ONLY — typed read-only M19/M20/M21-to-UI projection.
// This is NOT a Product ViewModel, NOT a canonical owner/epoch validator,
// and is deliberately NOT registered with Main.qml or a QML context.
// The expected owner receipts must ultimately be minted by real Phase-6
// Product owners, not by QML or this test-only consumer.
#include "m21_metrics_comparison_candidate.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace rgsml::app::m21_private::ui_candidate {

struct ExpectedOwnerReceipt final {
    Column column{Column::prepared};
    std::uint64_t generation_token{};
    std::string exact_realization_id{};
    std::string analysis_scope_label{};
    std::uint64_t absolute_scope_begin{};
    std::uint64_t scope_frames{};
    std::uint32_t sample_rate_hz{};
    // Assertions from the *external* owner; these booleans are not proof.
    // Candidate tests can exercise them; no production publisher consumes them.
    bool canonical_owner_confirmed{false};
    bool frame_domain_checked_at_source{false};
};

struct ReadOnlyCell final {
    std::string value_text{"UNAVAILABLE"};
    std::string unit_text{};
    std::string quality_text{"MISSING"};
    std::string reason{};
    bool numeric{false};
};

struct ReadOnlyColumn final {
    std::string state{"MISSING"};
    std::string scope_text{"SCOPE UNAVAILABLE"};
    std::array<ReadOnlyCell,9> values{};
    // Candidate-level provenance match ONLY. Not canonical release authority.
    bool candidate_matched{false};
};

struct ReadOnlyProjection final {
    std::array<ReadOnlyColumn,3> columns{};
    std::array<ReadOnlyCell,9> prepared_to_gold{};
    std::array<ReadOnlyCell,9> processed_to_gold{};
    bool gold_independent_scope{true};
    // Must remain false until a separate Product integration explicitly wires
    // true Phase-6 owner & domain verification. Never infer from test claims.
    bool production_authorized{false};
};

[[nodiscard]] inline std::string quality_label(Quality q) {
    switch(q) {
        case Quality::present: return "CURRENT";
        case Quality::low_confidence: return "LOW SUPPORT";
        case Quality::missing: return "MISSING";
        case Quality::stale: return "STALE";
        case Quality::invalid: return "INVALID";
        case Quality::not_applicable: return "NOT APPLICABLE";
        case Quality::incompatible: return "INCOMPATIBLE";
    }
    return "INVALID";
}
[[nodiscard]] inline std::string lifecycle_label(Lifecycle life) {
    switch(life) {
        case Lifecycle::current: return "CURRENT";
        case Lifecycle::stale: return "STALE";
        case Lifecycle::invalid: return "INVALID";
        case Lifecycle::missing: return "MISSING";
    }
    return "INVALID";
}
[[nodiscard]] inline std::string decimal_text(double value, bool delta) {
    if(!std::isfinite(value))return "INVALID";
    std::ostringstream out;
    out.imbue(std::locale::classic());
    if(delta && value>0.0)out << '+';
    out << std::fixed << std::setprecision(2) << value;
    const auto displayed=out.str();
    // A real nonzero sub-centibel delta may round to zero. Its display
    // must not suggest a negative/positive 0.00 value; the unrounded
    // upstream metric remains unchanged for all subsequent comparisons.
    if(displayed=="-0.00" || displayed=="+0.00")return "0.00";
    return displayed;
}
[[nodiscard]] inline ReadOnlyCell display_cell(const Cell& cell,
                                                std::size_t metric_index,
                                                bool is_delta=false) {
    ReadOnlyCell result;
    result.quality_text=quality_label(cell.quality);
    result.reason=cell.reason;
    if(metric_index>=kMetricSpecs.size()) {
        result.value_text="INVALID";
        result.quality_text="INVALID";
        return result;
    }
    result.unit_text=is_delta ? kMetricSpecs[metric_index].delta_unit
                              : kMetricSpecs[metric_index].absolute_unit;
    // The full-resource M16 value is not release-qualified in the current
    // M19 Product public status. Do not present a numeric TP even if an
    // upstream mock/synthetic cell were incorrectly marked "present".
    if(metric_index==static_cast<std::size_t>(Metric::true_peak)) {
        result.value_text="UNAVAILABLE";
        result.quality_text="M16 UNQUALIFIED";
        result.reason="No canonical M16 TP publication authority";
        result.numeric=false;
        return result;
    }
    if(!valid_cell_storage(cell)) {
        result.value_text="INVALID";
        result.quality_text="INVALID";
        result.reason="Invalid numeric/status storage";
        return result;
    }
    if(!usable(cell)) {
        result.value_text=result.quality_text;
        return result;
    }
    result.value_text=decimal_text(*cell.unrounded,is_delta);
    result.numeric=true;
    return result;
}
[[nodiscard]] inline bool receipt_matches(const Snapshot& snapshot,
                                           const ExpectedOwnerReceipt& receipt,
                                           std::size_t slot) {
    return receipt.canonical_owner_confirmed
        && receipt.frame_domain_checked_at_source
        && snapshot.lifecycle==Lifecycle::current
        && static_cast<std::size_t>(receipt.column)==slot
        && static_cast<std::size_t>(snapshot.column)==slot
        && receipt.generation_token>0
        && receipt.generation_token==snapshot.generation_token
        && !receipt.exact_realization_id.empty()
        && receipt.exact_realization_id==snapshot.resource_identity
        && (receipt.analysis_scope_label=="Full Track"
            || receipt.analysis_scope_label=="Region")
        && receipt.analysis_scope_label==snapshot.analysis_scope_label
        && receipt.absolute_scope_begin==snapshot.absolute_scope_begin
        && receipt.scope_frames>0
        && receipt.absolute_scope_begin<=
            (std::numeric_limits<std::uint64_t>::max)()-receipt.scope_frames
        && receipt.scope_frames==snapshot.scope_frames
        && receipt.sample_rate_hz==snapshot.sample_rate_hz
        && supported_private_rate(snapshot.sample_rate_hz);
}
[[nodiscard]] inline ReadOnlyProjection project_readonly(
    const Comparison& compared,
    const std::array<std::optional<ExpectedOwnerReceipt>,3>& expected) {
    ReadOnlyProjection result;
    result.gold_independent_scope=compared.independent_scopes;
    for(std::size_t i=0;i<3;++i) {
        auto& view=result.columns[i];
        if(!compared.columns[i])continue;
        const Snapshot& source=*compared.columns[i];
        view.state=lifecycle_label(source.lifecycle);
        if(source.lifecycle!=Lifecycle::current) {
            for(auto& val:view.values) {
                // A missing result is an unavailable numeric field, not 0.
                // Preserve the exact lifecycle separately from the readout.
                val.value_text=source.lifecycle==Lifecycle::missing
                    ? "UNAVAILABLE" : view.state;
                val.quality_text=view.state;
                val.numeric=false;
            }
            continue;
        }
        if(!expected[i] || !receipt_matches(source,*expected[i],i)) {
            // We cannot trust even a CURRENT-looking snapshot absent a
            // matching independent external receipt. Mask ALL numbers.
            view.state=expected[i] ? "STALE" : "MISSING";
            for(auto& val:view.values) {
                // Lifecycle describes why the measurement cannot be used;
                // the numeric readout itself must remain visibly unavailable.
                // A mismatched receipt is marked STALE, never refreshed.
                val.value_text=expected[i] ? "STALE" : "UNAVAILABLE";
                val.quality_text=view.state;
                val.numeric=false;
            }
            continue;
        }
        view.candidate_matched=true;
        view.scope_text=source.analysis_scope_label;
        for(std::size_t k=0;k<view.values.size();++k)
            view.values[k]=display_cell(source.metrics[k],k);
    }
    const bool prep=result.columns[0].candidate_matched;
    const bool gold=result.columns[1].candidate_matched;
    const bool proc=result.columns[2].candidate_matched;
    for(std::size_t k=0;k<kMetricSpecs.size();++k) {
        if(prep&&gold)
            result.prepared_to_gold[k]=display_cell(compared.prepared_to_gold[k],k,true);
        else {
            const auto quality=result.columns[0].state=="STALE"
                || result.columns[1].state=="STALE" ? "STALE" : "MISSING";
            result.prepared_to_gold[k].quality_text=quality;
            result.prepared_to_gold[k].value_text=quality=="STALE"
                ? "STALE" : "UNAVAILABLE";
            result.prepared_to_gold[k].numeric=false;
        }
        if(proc&&gold)
            result.processed_to_gold[k]=display_cell(compared.processed_to_gold[k],k,true);
        else {
            const auto quality=result.columns[2].state=="STALE"
                || result.columns[1].state=="STALE" ? "STALE" : "MISSING";
            result.processed_to_gold[k].quality_text=quality;
            result.processed_to_gold[k].value_text=quality=="STALE"
                ? "STALE" : "UNAVAILABLE";
            result.processed_to_gold[k].numeric=false;
        }
    }
    return result;
}

} // namespace rgsml::app::m21_private::ui_candidate
