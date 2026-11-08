#pragma once
// INTERNAL PREPRODUCTION ONLY: coherent, single-writer read-only analysis
// review epochs for separately owned PREPARED/GOLD/PROCESSED candidates.
// This is NOT a canonical owner, a thread-safe scheduler or a Product/QML
// publisher. Test-only preview methods NEVER grant production authority.
#include "m21_metrics_comparison_candidate.hpp"
#include "m21_qt_snapshot_review_candidate.hpp"
#include "m21_ui_readonly_projection_candidate.hpp"
#include <QVariantMap>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace rgsml::app::m21_private::ui_candidate {

class ReviewEpochStore final {
public:
    // Caller must marshal all methods onto a single writer; concurrent
    // callbacks are prohibited. A new generation invalidates *only* its
    // column, not independent GOLD or the other analyzer's result.
    [[nodiscard]] bool begin(Column column, std::uint64_t generation) {
        const auto i=static_cast<std::size_t>(column);
        if(i>=3 || !guard_.expect(column,generation))return false;
        receipts_[i].reset();
        return true;
    }

    // External receipts here are TEST CLAIMS only, not canonical Product
    // authorization. A result must match the exact currently expected
    // generation and scope. Late callbacks and duplicates are rejected by
    // PrivatePublicationGuard. No QML publication is performed.
    [[nodiscard]] bool receive(const Snapshot& data,
                               const ExpectedOwnerReceipt& claimed) {
        const auto i=static_cast<std::size_t>(data.column);
        if(i>=3 || !receipt_matches(data,claimed,i))return false;
        if(!guard_.publish(data))return false;
        receipts_[i]=claimed;
        return true;
    }

    [[nodiscard]] bool select(Column column) {
        if(static_cast<std::size_t>(column)>=3)return false;
        selected_=column;
        return true;
    }
    [[nodiscard]] Column selected() const {return selected_;}

    // Always recompute from current receipts; never cache a ready map across
    // a begin(), receive(), or selected-column transition.
    [[nodiscard]] ReadOnlyProjection snapshot() const {
        return project_readonly(guard_.view(),receipts_);
    }

    // A production caller gets NO numeric data regardless of receipts.
    [[nodiscard]] QVariantMap unpublished() const {
        return qt_review::masked_unpublished_envelope();
    }

    // An EXPLICIT TEST ONLY preview, always watermarked fixtureMode=true
    // when evidence exists. Never expose this from app Main.qml.
    [[nodiscard]] QVariantMap test_preview(
        qt_review::ExplicitTestFixture test_only) const {
        return qt_review::candidate_test_fixture_envelope(
            snapshot(),selected_,test_only);
    }

private:
    PrivatePublicationGuard guard_{};
    std::array<std::optional<ExpectedOwnerReceipt>,3> receipts_{};
    Column selected_{Column::prepared};
};
} // namespace rgsml::app::m21_private::ui_candidate
