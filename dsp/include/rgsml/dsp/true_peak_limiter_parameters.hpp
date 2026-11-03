#pragma once

#include <rgsml/core/result.hpp>

namespace rgsml::dsp {

// L1-M17 accepted Core-first: Ceiling is the ONLY persisted sonic control.
// Input is validated; invalid values must never be silently clamped.
class TruePeakLimiterParameters final {
public:
    [[nodiscard]] static rgsml::core::Result<TruePeakLimiterParameters>
    create(double ceiling_db_tp);

    [[nodiscard]] static rgsml::core::Result<TruePeakLimiterParameters>
    create_default() noexcept;

    [[nodiscard]] double ceiling_db_tp() const noexcept;

    friend bool operator==(
        const TruePeakLimiterParameters&,
        const TruePeakLimiterParameters&) = default;

private:
    explicit TruePeakLimiterParameters(double ceiling_db_tp) noexcept;
    double ceiling_db_tp_;
};

}  // namespace rgsml::dsp
