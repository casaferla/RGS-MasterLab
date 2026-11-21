#pragma once
// L3-M09 P01 ACCEPTED_FROZEN: selected-grid Gaussian smoothing.
// Caller supplies the exact pre-authenticated selected feature grid.
// A null cell is MISSING, not zero, and splits a valid run. No
// interpolation, 1/12 fabrication from 1/3 fallback or cross-gap fit.
// This is an analytical seed preparation utility; not actual DSP audio.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
inline constexpr double kGaussianSigmaOctaves=1.0/3.0;
inline constexpr double kGaussianRadiusOctaves=1.0;
enum class GridFailure:std::uint8_t {
    none,empty_grid,illegal_spacing,nonfinite_observed_cell
};
struct SmoothedGrid final {
    std::vector<std::optional<double>> values;
    GridFailure failure{GridFailure::empty_grid};
    [[nodiscard]] constexpr bool authorizes_render_or_match()
        const noexcept {return false;}
};
// Non-duplicating-endpoint reflection: -1 -> 1, n -> n-2.
// Local indices remain inside ONE valid run; n=1 reflects to itself.
[[nodiscard]] inline std::size_t reflected_valid_run_index(
    std::int64_t index,std::size_t run_length) noexcept {
    if(run_length<=1U)return 0U;
    const auto last=static_cast<std::int64_t>(run_length-1U);
    const auto period=2*last;
    auto position=index%period;
    if(position<0)position+=period;
    if(position>last)position=period-position;
    return static_cast<std::size_t>(position);
}
[[nodiscard]] inline SmoothedGrid smooth_selected_valid_runs(
    std::span<const std::optional<double>> selected,
    double selected_spacing_octaves)
{
    if(selected.empty())return {{},GridFailure::empty_grid};
    // Do not accept unbounded/ill-formed spacing as a source grid.
    if(!std::isfinite(selected_spacing_octaves)
       ||selected_spacing_octaves<=0.0)
        return {{},GridFailure::illegal_spacing};
    // Keep the mandatory positive/finiteness preflight before dividing.
    // A volatile read also avoids MSVC C4723 constant-zero speculation
    // when a negative test intentionally supplies zero spacing.
    const volatile double checked_spacing=selected_spacing_octaves;
    const double radius_count=kGaussianRadiusOctaves/checked_spacing;
    if(!std::isfinite(radius_count)
       ||radius_count>static_cast<double>(
            std::numeric_limits<std::int32_t>::max()/2))
        return {{},GridFailure::illegal_spacing};
    for(const auto& cell:selected)
        if(cell&&!std::isfinite(*cell))
            return {{},GridFailure::nonfinite_observed_cell};
    const auto radius=static_cast<std::int64_t>(
        std::floor(radius_count));
    std::vector<double> taps;
    taps.reserve(static_cast<std::size_t>(2*radius+1));
    for(std::int64_t offset=-radius;offset<=radius;++offset) {
        const double octave_distance=
            static_cast<double>(offset)*selected_spacing_octaves;
        // Explicit physical radius test, not an assumed 12-bin radius.
        const double ratio=octave_distance/kGaussianSigmaOctaves;
        taps.push_back(std::abs(octave_distance)<=kGaussianRadiusOctaves
            ?std::exp(-0.5*ratio*ratio):0.0);
    }
    SmoothedGrid result;
    result.values.resize(selected.size());
    std::size_t begin=0U;
    while(begin<selected.size()) {
        if(!selected[begin]) {
            ++begin;
            continue;
        }
        std::size_t end=begin+1U;
        while(end<selected.size()&&selected[end])++end;
        const auto run_length=end-begin;
        for(std::size_t i=begin;i<end;++i) {
            if(run_length==1U) {
                result.values[i]=selected[i];
                continue;
            }
            double numerator=0.0;
            double denominator=0.0;
            for(std::int64_t offset=-radius;offset<=radius;++offset) {
                const auto w=taps[static_cast<std::size_t>(offset+radius)];
                const auto local=reflected_valid_run_index(
                    static_cast<std::int64_t>(i-begin)+offset,run_length);
                numerator+=w*(*selected[begin+local]);
                denominator+=w;
            }
            if(!std::isfinite(numerator)||!std::isfinite(denominator)
               ||denominator<=0.0) {
                result.values.clear();
                result.failure=GridFailure::nonfinite_observed_cell;
                return result;
            }
            result.values[i]=numerator/denominator;
        }
        begin=end;
    }
    result.failure=GridFailure::none;
    return result;
}
// P01 conservative auto-broad-action gate: three adjacent *already
// eligible* selected-grid cells, same signed motion strictly outside
// their frozen deadband. Callers MUST map ineligible evidence to null.
[[nodiscard]] inline bool has_three_adjacent_signed_action(
    std::span<const std::optional<double>> eligible_movement,
    double frozen_deadband) noexcept
{
    if(!std::isfinite(frozen_deadband)||frozen_deadband<0.0)
        return false;
    std::size_t consecutive=0U;
    int prior_sign=0;
    for(const auto& value:eligible_movement) {
        const int sign=value&&std::isfinite(*value)
            &&std::abs(*value)>frozen_deadband
            ?(*value>0.0?1:-1):0;
        consecutive=(sign!=0&&sign==prior_sign)?consecutive+1U
                    :(sign!=0?1U:0U);
        prior_sign=sign;
        if(consecutive>=3U)return true;
    }
    return false;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
