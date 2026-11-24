#pragma once
#include "l3_m09_p01_weighted_atom_fit.hpp"
#include <rgsml/dsp/parametric_eq_response.hpp>

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeEqAtom final {
    core::Uuid band_id;
    AtomFilter filter;
    double frequency_hz{};
    double q_or_slope{};
};
struct NativeEqFitCell final {
    double frequency_hz{},residual_db{},frozen_normalized_weight{};
};
struct NativeEqAtomFit final {
    AtomFitResult fit;
    dsp::EqBandParameters payload;
};
// This adapter consumes an already eligible dictionary atom and full
// active comparison population. It never replaces an unavailable
// response cell by zero, or uses a +1 dB response as the bounded fit.
[[nodiscard]] inline std::optional<NativeEqAtomFit>
fit_native_eq_atom(const NativeEqAtom& atom,
    std::span<const NativeEqFitCell> exact_active_population,
    core::SampleRate sample_rate,bool stereo_side)
{
    if(atom.band_id.is_nil()||exact_active_population.empty()||
       !std::isfinite(atom.frequency_hz)||!std::isfinite(atom.q_or_slope)||
       atom.frequency_hz<20.0||
       atom.frequency_hz>std::min(20000.0,0.45*static_cast<double>(sample_rate.value()))||
       atom.q_or_slope<0.1)return {};
    if(atom.filter!=AtomFilter::bell&&atom.filter!=AtomFilter::low_shelf&&
       atom.filter!=AtomFilter::high_shelf)return {};
    const bool bell=atom.filter==AtomFilter::bell;
    if(atom.q_or_slope>(bell?(stereo_side?1.0:1.5):1.0))return {};
    const auto make=[&](double gain) {
        const auto type=bell?dsp::EqFilterType::BELL:
            atom.filter==AtomFilter::low_shelf?dsp::EqFilterType::LOW_SHELF:dsp::EqFilterType::HIGH_SHELF;
        const dsp::EqBandPayload shape=bell?
            dsp::EqBandPayload{dsp::BellPayload{atom.frequency_hz,gain,atom.q_or_slope}}:
            dsp::EqBandPayload{dsp::ShelfPayload{atom.frequency_hz,gain,atom.q_or_slope}};
        return dsp::EqBandParameters::create(atom.band_id,true,type,
            stereo_side?dsp::EqRouting::SIDE:dsp::EqRouting::STEREO,shape);
    };
    std::vector<double> frequencies;
    frequencies.reserve(exact_active_population.size());
    for(const auto& cell:exact_active_population) {
        if(!std::isfinite(cell.frequency_hz)||cell.frequency_hz<20.0||
           cell.frequency_hz>std::min(20000.0,0.45*static_cast<double>(sample_rate.value())))
            return {};
        frequencies.push_back(cell.frequency_hz);
    }
    const auto unit=make(1.0);
    if(!unit)return {};
    const auto unit_response=dsp::evaluate_band_response(*unit.value(),frequencies,sample_rate);
    if(!unit_response||unit_response.value()->size()!=frequencies.size())return {};
    std::vector<AtomProfileCell> cells;
    cells.reserve(frequencies.size());
    for(std::size_t i=0;i<frequencies.size();++i)
        cells.push_back({exact_active_population[i].residual_db,
            (*unit_response.value())[i].magnitude_db,
            exact_active_population[i].frozen_normalized_weight});
    const auto projection=project_atom_unweighted_g0(cells,stereo_side);
    if(!projection.limited_gain_db)return {};
    auto bounded=make(*projection.limited_gain_db);
    if(!bounded)return {};
    const auto response=dsp::evaluate_band_response(*bounded.value(),frequencies,sample_rate);
    if(!response||response.value()->size()!=frequencies.size())return {};
    AtomFitEvidence evidence{atom.filter,atom.frequency_hz,atom.q_or_slope,projection,{},true};
    evidence.exact_canonical_response_at_limited_gain.reserve(frequencies.size());
    for(const auto& point:*response.value())
        evidence.exact_canonical_response_at_limited_gain.push_back(point.magnitude_db);
    auto fit=evaluate_weighted_atom(cells,std::move(evidence));
    return NativeEqAtomFit{std::move(fit),std::move(*bounded.value())};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
