#pragma once

#include <rgsml/core/result.hpp>

namespace rgsml::dsp {

class GainParameters final {
public:
    [[nodiscard]] static rgsml::core::Result<GainParameters>
    create(double gain_db);

    [[nodiscard]] double gain_db() const noexcept;

    friend bool operator==(const GainParameters&, const GainParameters&) = default;

private:
    explicit GainParameters(double canonical_gain_db) noexcept;

    double gain_db_;
};

}  // namespace rgsml::dsp
