#include <rgsml/dsp/stereo_ms_crossover_runtime.hpp>

#include <algorithm>
#include <cmath>

namespace rgsml::dsp {

StereoMsCrossoverRuntime::StereoMsCrossoverRuntime(
    StereoMsCrossoverDesign design) noexcept
    : design_(design)
{
}

void StereoMsCrossoverRuntime::reset() noexcept
{
    mid_low_ = {};
    mid_high_ = {};
    side_low_ = {};
    side_high_ = {};
}

double StereoMsCrossoverRuntime::section(
    double input, StereoMsFilterSection c, SectionDelay& s) noexcept
{
    // Phase-5 recovered normalized direct-form II transposed:
    // y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y.
    // For LR12, b2=a2=0 so z2 remains identically zero.
    const double output = c.b0 * input + s.z1;
    const double next_z1 = c.b1 * input - c.a1 * output + s.z2;
    const double next_z2 = c.b2 * input - c.a2 * output;
    s.z1 = next_z1;
    s.z2 = next_z2;
    return output;
}

double StereoMsCrossoverRuntime::cascade(
    double input, StereoMsFilterSection c, Cascade& states) noexcept
{
    // LR12: exactly 2 LP1 or HP1; LR24: exactly 2 LP2 or HP2.
    for (auto& section_state : states) {
        input = section(input, c, section_state);
    }
    return input;
}

StereoMsCrossoverFrame StereoMsCrossoverRuntime::process(
    double mid, double side, double beta) noexcept
{
    const double mid_low = cascade(mid, design_.low_section, mid_low_);
    double mid_high = cascade(mid, design_.high_section, mid_high_);
    const double side_low = cascade(side, design_.low_section, side_low_);
    double side_high = cascade(side, design_.high_section, side_high_);

    if (design_.invert_high_branch) {
        // Exactly one high-branch sign reversal on LR12, none on LR24.
        mid_high = -mid_high;
        side_high = -side_high;
    }

    return {mid_low + mid_high, beta * side_low + side_high};
}

bool StereoMsCrossoverRuntime::finite() const noexcept
{
    const auto cascade_finite = [](const Cascade& c) noexcept {
        return std::all_of(c.begin(), c.end(), [](const SectionDelay& s) {
            return std::isfinite(s.z1) && std::isfinite(s.z2);
        });
    };
    return cascade_finite(mid_low_) && cascade_finite(mid_high_)
        && cascade_finite(side_low_) && cascade_finite(side_high_);
}


std::array<double, StereoMsCrossoverRuntime::kCheckpointStateWords>
StereoMsCrossoverRuntime::snapshot_state() const noexcept
{
    std::array<double, kCheckpointStateWords> words{};
    std::size_t i = 0;
    const auto append = [&words, &i](const Cascade& c) noexcept {
        for (const auto& s : c) {
            words[i++] = s.z1;
            words[i++] = s.z2;
        }
    };
    append(mid_low_);
    append(mid_high_);
    append(side_low_);
    append(side_high_);
    return words;
}

bool StereoMsCrossoverRuntime::restore_state(
    const std::array<double, kCheckpointStateWords>& words) noexcept
{
    if (!std::all_of(words.begin(), words.end(), [](double value) noexcept {
            return std::isfinite(value);
        })) {
        return false;
    }

    // Stage a complete snapshot in a local copy and publish only after
    // validating every word. Invalid state leaves all prior delays intact.
    auto candidate = *this;
    std::size_t i = 0;
    const auto restore_cascade = [&words, &i](Cascade& c) noexcept {
        for (auto& s : c) {
            s.z1 = words[i++];
            s.z2 = words[i++];
        }
    };
    restore_cascade(candidate.mid_low_);
    restore_cascade(candidate.mid_high_);
    restore_cascade(candidate.side_low_);
    restore_cascade(candidate.side_high_);
    *this = candidate;
    return true;
}

}  // namespace rgsml::dsp
