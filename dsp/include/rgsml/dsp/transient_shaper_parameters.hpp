#pragma once
// L3-M03 Transient Shaper P01/P02 ACCEPTED/FROZEN.
// Exactly TWO public amounts: signed Attack and Sustain in dB.
// No public time constants, threshold, event classifier, dry/wet,
// clipping, lookahead or stereo link mode exists in v1.
#include <rgsml/core/result.hpp>

namespace rgsml::dsp {
class TransientShaperParameters final {
public:
    [[nodiscard]] static core::Result<TransientShaperParameters> create(
        double attack_amount_db,double sustain_amount_db);
    [[nodiscard]] static core::Result<TransientShaperParameters>
    create_default() {return create(0.0,0.0);}

    [[nodiscard]] double attack_amount_db() const noexcept {
        return attack_amount_db_;
    }
    [[nodiscard]] double sustain_amount_db() const noexcept {
        return sustain_amount_db_;
    }
    [[nodiscard]] bool neutral() const noexcept {
        return attack_amount_db_==0.0 && sustain_amount_db_==0.0;
    }
    friend bool operator==(const TransientShaperParameters&,
                           const TransientShaperParameters&)=default;
private:
    TransientShaperParameters(double attack,double sustain) noexcept
        :attack_amount_db_(attack),sustain_amount_db_(sustain) {}
    double attack_amount_db_{};
    double sustain_amount_db_{};
};
} // namespace rgsml::dsp
